#pragma once
// The gameplay screen: owns the level, the Sim, practice checkpoints, particles and camera.
// No SDL here (drawing is in GameRenderer). It talks to Audio and to the menus through hooks.
//
// MENUS HOOKS: set `hooks().onComplete` / `hooks().onDeath` to record progress (attempts, best %).
// The App moves to AppState::Complete / Paused; the Menus module draws those screens and may call
// restart(), setPractice(), setPaused() on this object.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Audio.h"
#include "core/Camera.h"
#include "core/Level.h"
#include "core/Particles.h"
#include "core/Practice.h"
#include "core/Sim.h"

namespace gd {

struct GameConfig {
    std::string dataDir;
    std::string levelFile;          // full path to the level JSON
    bool practice = false;
    bool fly = false;
    bool useReplay = false;
    std::vector<bool> replayHeld;   // held state per tick (see heldPerTick)
};

struct GameResult {
    int levelId = 0;
    bool practice = false;
    bool completed = false;
    int attempts = 0;               // attempt number of this run (1-based)
    double progress = 0.0;          // 0..1 at the moment of the event
    double seconds = 0.0;           // sim time of the run (ticks / tick rate)
};

struct GameHooks {
    std::function<void(const GameResult&)> onComplete;   // once, when the end wall is reached
    std::function<void(const GameResult&)> onDeath;      // every death
    std::function<void()> onAttempt;                     // each restart / respawn (not the first attempt)
};

// Interpolated player pose for drawing.
struct PlayerPose {
    double x = 0, y = 0;     // bottom-left of the 1x1 box
    double rotation = 0;     // degrees, counter-clockwise (y up)
    GameMode mode = GameMode::Cube;
    Gravity gravity = Gravity::Down;
    bool grounded = false;
};

class GameScreen {
public:
    static constexpr int kDeathTicks = 96;          // 0.4 s at 240 Hz
    static constexpr int kCompleteDelayTicks = 192; // 0.8 s of win burst before the overlay

    explicit GameScreen(Audio* audio = nullptr) : audio_(audio) {}

    // Loads the level and starts attempt 1. Returns false with `error` set on failure.
    bool load(const GameConfig& cfg, std::string& error);
    bool loaded() const { return sim_ != nullptr; }

    GameHooks& hooks() { return hooks_; }

    // ---- input (call once per frame, before ticks) ----
    void feedInput(bool held);
    void endFrame(int ticksRun);     // call after the frame's ticks
    void placeCheckpoint();          // practice only
    void removeCheckpoint();         // practice only
    void restart();                  // back to the start of the level (counts as a new attempt)
    void setPractice(bool on);       // restarts the run
    void setPaused(bool paused);

    // ---- stepping ----
    void tick();                     // one fixed sim tick (240 Hz)
    void frame(double dt, double alpha);  // once per rendered frame: camera + animation time

    // ---- state for the renderer / menus ----
    const Level& level() const { return level_; }
    const Sim& sim() const { return *sim_; }
    PlayerPose pose(double alpha) const;
    const Camera& camera() const { return camera_; }
    const ParticleSystem& particles() const { return particles_; }
    const Practice& practice() const { return practice_; }
    bool practiceMode() const { return practiceMode_; }
    bool fly() const { return cfg_.fly; }
    bool paused() const { return paused_; }
    int attempts() const { return attempts_; }
    int jumps() const { return jumps_; }   // jumps in the current attempt
    bool dying() const { return deathTicks_ > 0; }
    double deathX() const { return deathX_; }
    double deathY() const { return deathY_; }
    int deathTicksLeft() const { return deathTicks_; }
    bool won() const { return sim_ && sim_->won(); }
    bool showComplete() const { return won() && winTicks_ >= kCompleteDelayTicks; }
    int runTick() const { return sim_ ? sim_->tick() : 0; }
    double animTime() const { return animTime_; }
    double progress() const { return sim_ ? sim_->progress() : 0.0; }
    int percent() const;
    GameResult makeResult(bool completed) const;

private:
    void onDeathEvent();
    void respawn();
    void startMusic();
    void handleEvents();
    void emitTrail();

    Audio* audio_;
    GameConfig cfg_;
    GameHooks hooks_;
    Level level_;
    std::unique_ptr<Sim> sim_;
    Practice practice_;
    ParticleSystem particles_;
    Camera camera_;
    PlayerState prev_;
    bool practiceMode_ = false;
    bool paused_ = false;
    bool heldNow_ = false;
    bool latch_ = false;
    int attempts_ = 1;
    int jumps_ = 0;
    int deathTicks_ = 0;
    int winTicks_ = 0;
    bool completeReported_ = false;
    double deathX_ = 0, deathY_ = 0;
    double animTime_ = 0;
    double bestProgress_ = 0;
    std::string musicPath_;
};

}  // namespace gd
