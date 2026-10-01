#include "GameScreen.h"

#include <algorithm>
#include <cmath>

#include "core/Interp.h"
#include "core/LevelLoader.h"
#include "core/PhysicsConstants.h"

namespace gd {

namespace {
Color portalColor(const Object& o) {
    switch (o.type) {
        case ObjType::PortalGravity: return o.gravity == Gravity::Up ? Color{255, 220, 50} : Color{60, 140, 255};
        case ObjType::PortalMode: return o.mode == GameMode::Ship ? Color{255, 100, 200} : Color{80, 230, 100};
        case ObjType::PortalSpeed:
            switch (o.speed) {
                case Speed::Slow: return Color{255, 170, 40};
                case Speed::Normal: return Color{80, 200, 255};
                case Speed::Fast: return Color{90, 235, 90};
                case Speed::Faster: return Color{255, 90, 190};
            }
            break;
        default: break;
    }
    return Color{255, 255, 255};
}
}  // namespace

bool GameScreen::load(const GameConfig& cfg, std::string& error) {
    cfg_ = cfg;
    std::vector<std::string> errs;
    auto lvl = loadLevelFromFile(cfg.levelFile, errs);
    if (!lvl) {
        error = "cannot load " + cfg.levelFile;
        for (const auto& e : errs) error += "\n  " + e;
        return false;
    }
    sim_.reset();  // the old Sim points at the old level
    level_ = std::move(*lvl);
    sim_ = std::make_unique<Sim>(level_);
    sim_->setInvincible(cfg.fly);
    practice_.clear();
    practiceMode_ = cfg.practice;
    attempts_ = 1;
    jumps_ = 0;
    deathTicks_ = winTicks_ = 0;
    completeReported_ = false;
    paused_ = false;
    bestProgress_ = 0;
    particles_.reseed(static_cast<std::uint32_t>(level_.id * 7919 + 17));
    particles_.clear();
    musicPath_ = assetPath(cfg.dataDir, level_.music);
    sim_->reset();
    prev_ = sim_->player();
    const PlayerState& p = sim_->player();
    camera_.snapTo(p.x + 0.5, p.y + 0.5, p.mode, level_.ceiling);
    startMusic();
    return true;
}

void GameScreen::startMusic() {
    if (audio_ && !musicPath_.empty()) audio_->playMusic(musicPath_);
}

int GameScreen::percent() const {
    return std::clamp(static_cast<int>(std::floor(progress() * 100.0)), 0, 100);
}

GameResult GameScreen::makeResult(bool completed) const {
    GameResult r;
    r.levelId = level_.id;
    r.practice = practiceMode_;
    r.completed = completed;
    r.attempts = attempts_;
    r.progress = completed ? 1.0 : progress();
    r.seconds = sim_ ? sim_->tick() * phys::kDt : 0.0;
    return r;
}

PlayerPose GameScreen::pose(double alpha) const {
    const PlayerState& c = sim_->player();
    PlayerPose p;
    p.x = lerp(prev_.x, c.x, alpha);
    p.y = lerp(prev_.y, c.y, alpha);
    p.rotation = lerpAngleDeg(prev_.rotation, c.rotation, alpha);
    p.mode = c.mode;
    p.gravity = c.gravity;
    p.grounded = c.grounded;
    return p;
}

void GameScreen::feedInput(bool held) {
    heldNow_ = held;
    latch_ = latch_ || held;
}

void GameScreen::endFrame(int ticksRun) {
    if (ticksRun > 0) latch_ = heldNow_;
}

void GameScreen::placeCheckpoint() {
    if (!sim_ || !practiceMode_ || deathTicks_ > 0 || sim_->won()) return;
    if (practice_.add(*sim_) && audio_) audio_->playSfx(Sfx::Checkpoint);
}

void GameScreen::removeCheckpoint() {
    if (practiceMode_) practice_.removeLast();
}

void GameScreen::restart() {
    if (!sim_) return;
    ++attempts_;
    jumps_ = 0;
    practice_.clear();
    deathTicks_ = winTicks_ = 0;
    particles_.clear();
    sim_->reset();
    prev_ = sim_->player();
    const PlayerState& p = sim_->player();
    camera_.snapTo(p.x + 0.5, p.y + 0.5, p.mode, level_.ceiling);
    bestProgress_ = 0;
    startMusic();
    if (hooks_.onAttempt) hooks_.onAttempt();
}

void GameScreen::setPractice(bool on) {
    if (!sim_ || on == practiceMode_) return;
    practiceMode_ = on;
    --attempts_;  // restart() adds one; toggling is not a new attempt
    restart();
}

void GameScreen::setPaused(bool paused) {
    if (paused == paused_) return;
    paused_ = paused;
    if (!audio_) return;
    if (paused) audio_->pauseMusic();
    else audio_->resumeMusic();
}

void GameScreen::respawn() {
    deathTicks_ = 0;
    ++attempts_;
    if (practiceMode_) {
        practice_.respawn(*sim_);  // last checkpoint, or a fresh start when there is none
        if (practice_.count() == 0) bestProgress_ = 0;
    } else {
        sim_->reset();
        startMusic();
        bestProgress_ = 0;
        jumps_ = 0;
    }
    prev_ = sim_->player();
    const PlayerState& p = sim_->player();
    camera_.snapTo(p.x + 0.5, p.y + 0.5, p.mode, level_.ceiling);
    if (hooks_.onAttempt) hooks_.onAttempt();
}

void GameScreen::onDeathEvent() {
    const PlayerState& p = sim_->player();
    deathX_ = p.x + 0.5;
    deathY_ = p.y + 0.5;
    deathTicks_ = kDeathTicks;
    const Color c = {255, 205, 50};
    particles_.burst(deathX_, deathY_, 26, c, 3.0, 11.0, 0.35, 0.7, 0.28, -22.0);
    particles_.burst(deathX_, deathY_, 12, Color{255, 255, 255}, 2.0, 6.0, 0.25, 0.5, 0.2, -10.0);
    particles_.ring(deathX_, deathY_, Color{255, 230, 120}, 0.8, 5.5, 0.4);
    if (audio_) {
        audio_->playSfx(Sfx::Death);
        if (!practiceMode_) audio_->stopMusic(0);
    }
    if (hooks_.onDeath) hooks_.onDeath(makeResult(false));
}

void GameScreen::emitTrail() {
    const PlayerState& p = sim_->player();
    const int t = p.tick;
    const double cx = p.x + 0.5, cy = p.y + 0.5;
    if (p.mode == GameMode::Ship) {
        if (t % 2 == 0) {
            const double a = p.rotation * 3.14159265358979323846 / 180.0;
            const double bx = cx - std::cos(a) * 0.62, by = cy - std::sin(a) * 0.62;
            particles_.trail(bx, by, Color{255, 170, 50}, 0.3, 0.3, p.vx);
        }
    } else if (t % 4 == 0) {
        const double fy = p.gravity == Gravity::Down ? p.y + 0.12 : p.y + 0.88;
        particles_.trail(p.x + 0.1, p.grounded ? fy : cy, Color{255, 230, 130}, p.grounded ? 0.16 : 0.26, 0.4, p.vx);
    }
}

void GameScreen::handleEvents() {
    for (const SimEvent& e : sim_->events()) {
        switch (e.type) {
            case SimEventType::Portal: {
                if (e.objectIndex < 0 || e.objectIndex >= static_cast<int>(level_.objects.size())) break;
                const Object& o = level_.objects[e.objectIndex];
                const Color c = portalColor(o);
                particles_.ring(o.x + 0.5, o.y + o.h * 0.5, c, 0.6, 3.6, 0.35);
                particles_.burst(o.x + 0.5, o.y + o.h * 0.5, 14, c, 2.0, 7.0, 0.25, 0.5, 0.2, 0.0);
                if (audio_) audio_->playSfx(Sfx::Portal);
                break;
            }
            case SimEventType::Land: {
                const PlayerState& p = sim_->player();
                const double fy = p.gravity == Gravity::Down ? p.y : p.y + 1.0;
                const double dir = p.gravity == Gravity::Down ? 1.0 : -1.0;
                for (int i = 0; i < 5; ++i) {
                    Particle q;
                    q.x = p.x + 0.5 + particles_.rng().range(-0.4, 0.4);
                    q.y = fy;
                    q.vx = particles_.rng().range(-2.0, 1.0);
                    q.vy = dir * particles_.rng().range(1.0, 3.0);
                    q.maxLife = q.life = particles_.rng().range(0.2, 0.4);
                    q.size0 = 0.14;
                    q.size1 = 0.03;
                    q.color = Color{230, 235, 255};
                    q.alpha = 180;
                    particles_.emit(q);
                }
                break;
            }
            case SimEventType::Jump: ++jumps_; break;
            default: break;
        }
    }
}

void GameScreen::tick() {
    if (!sim_ || paused_) return;
    particles_.update(phys::kDt);

    if (deathTicks_ > 0) {
        prev_ = sim_->player();
        if (--deathTicks_ == 0) respawn();
        return;
    }
    if (sim_->won()) {
        prev_ = sim_->player();
        ++winTicks_;
        return;
    }

    prev_ = sim_->player();
    bool held = latch_;
    if (cfg_.useReplay) {
        const auto t = static_cast<std::size_t>(sim_->tick());
        held = t < cfg_.replayHeld.size() ? cfg_.replayHeld[t] : false;
    }
    sim_->step(held);
    bestProgress_ = std::max(bestProgress_, sim_->progress());
    handleEvents();

    if (sim_->dead()) {
        onDeathEvent();
        return;
    }
    if (sim_->won()) {
        const PlayerState& p = sim_->player();
        const Color cols[] = {{255, 90, 90}, {255, 210, 60}, {90, 230, 120}, {80, 190, 255}, {220, 120, 255}};
        for (const Color& c : cols)
            particles_.burst(p.x + 0.5, p.y + 0.5, 14, c, 4.0, 13.0, 0.7, 1.4, 0.3, -9.0);
        particles_.ring(p.x + 0.5, p.y + 0.5, Color{255, 255, 255}, 1.0, 9.0, 0.7);
        if (audio_) {
            audio_->stopMusic(400);
            audio_->playSfx(Sfx::LevelComplete);
        }
        if (!completeReported_) {
            completeReported_ = true;
            if (hooks_.onComplete) hooks_.onComplete(makeResult(true));
        }
        return;
    }
    emitTrail();
}

void GameScreen::frame(double dt, double alpha) {
    if (!sim_) return;
    animTime_ += dt;
    const PlayerPose p = pose(alpha);
    camera_.update(dt, p.x + 0.5, p.y + 0.5, p.mode, level_.ceiling);
}

}  // namespace gd
