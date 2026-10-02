// Static level checks. Owner: level-format section.
//
// Validator algorithm (all numbers come from PhysicsConstants.h; "x" is in blocks).
//
// State walk: portals sorted by x (stable) give the mode / gravity / speed in force for a
// player whose LEFT edge is at p: all portals with portal.x <= p have fired. An object whose
// left face is at S.x is first touched when the left edge is at S.x - kPlayerSize, so cube
// rules use the state at S.x - kPlayerSize.
//
// 1. palette         Re-checks every field of the Level struct: format==1, id 1..10, name 1..32
//                    chars, difficulty 1..10, music non-empty and relative, ceiling in
//                    [kMinCeiling,kMaxCeiling], x/y/w/h multiples of 0.5, x,y >= 0, w,h > 0,
//                    platform h == kPlatformHeight, spike 1x1, y + h <= ceiling, exactly one
//                    end_wall, no object with x beyond it, endX == end_wall.x.
//                    If this fails, the geometry rules below are skipped.
// 2. embedded-hazard A spike's 1x1 cell may not overlap (positive area) any block / platform.
// 3. climb           Cube sections only. Frame = gravity down as is; gravity up is mirrored
//                    (y' = ceiling - y, so block bottoms become tops). For every solid S: the
//                    reference floor is the highest top among the ground (0) and the solids T
//                    that start left of S.x, end right of S.x - W (W = half a jump length at the
//                    speed, cubeJumpLength/2) and whose top is not above S's top; a taller T
//                    counts only if it touches S's left face (S is then hidden behind it).
//                    If S's underside (after following a stack of solids that support it) is at
//                    least kPlayerSize above that floor the cube passes beneath: no check.
//                    Otherwise rise = S.top - floor must be <= kCubeMaxClimb + 0.05.
//                    (0.05 slack keeps a 2-block wall legal; kCubeJumpApex is ~2.23.)
// 4. spike-run       Cube sections. "Floor spikes" are dir=up (gravity down) or dir=down
//                    (gravity up). Sorted by x; two spikes are in one run when their floor
//                    heights differ by <= 0.5 and the gap between hitboxes
//                    (dx - kSpikeHitW) is < kPlayerSize + 0.2 (else the cube can land between
//                    them). Run size = (last.x - first.x) + 1 must be <= cubeMaxSpikeRun(speed).
// 5. ship-gap        Ship sections (columns from the mode portal x to the next mode portal x
//                    + kPlayerSize, or to endX). Every 0.1 block, take the free vertical gaps
//                    between ground, ceiling, solids and spike hitboxes. At least one gap must
//                    be >= kShipMinGap.
// 6. ship-slope      The ship centre may be anywhere in [gap.lo + 0.5, gap.hi - 0.5] of a usable
//                    gap (size >= kShipMinGap). We track the set R of centre heights the ship
//                    can hold, moving at most kShipMaxSlope per block of x: each 0.1 step, R is
//                    widened by kShipMaxSlope * 0.1 on both sides and intersected with the
//                    usable ranges of the new column. R starts as everything at the section
//                    start. If R becomes empty (the corridor moves away faster than the ship
//                    may follow), that is a failure; R is then reset and checking goes on.
// 7. duration        levelDurationSeconds in [kLevelMinSeconds, kLevelMaxSeconds].
// 8. start-clear     No block, platform or spike with x < kStartClearBlocks.
// 9. music           When dataDir is given, <dataDir>/assets/<music> must exist.
#include "core/Validator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "core/LevelLoader.h"
#include "core/PhysicsConstants.h"

namespace gd {
namespace {

constexpr double kEps = 1e-9;
constexpr double kClimbSlack = 0.05;     // see rule 3
constexpr double kLandMargin = 0.2;      // see rule 4
constexpr double kShipStep = 0.1;        // see rule 5

std::string fmt(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3g", v);
    return buf;
}

bool isHalf(double v) { return std::isfinite(v) && v * 2.0 == std::round(v * 2.0); }

bool isSolid(const Object& o) { return o.type == ObjType::Block || o.type == ObjType::Platform; }

struct State {
    GameMode mode;
    Gravity gravity;
    Speed speed;
};

class Timeline {
public:
    explicit Timeline(const Level& lv) {
        State cur{lv.startMode, lv.startGravity, lv.startSpeed};
        events_.push_back({-1e18, cur});
        std::vector<const Object*> portals;
        for (const Object& o : lv.objects)
            if (o.type == ObjType::PortalGravity || o.type == ObjType::PortalMode ||
                o.type == ObjType::PortalSpeed)
                portals.push_back(&o);
        std::stable_sort(portals.begin(), portals.end(),
                         [](const Object* a, const Object* b) { return a->x < b->x; });
        for (const Object* p : portals) {
            if (p->type == ObjType::PortalGravity) cur.gravity = p->gravity;
            if (p->type == ObjType::PortalMode) cur.mode = p->mode;
            if (p->type == ObjType::PortalSpeed) cur.speed = p->speed;
            events_.push_back({p->x, cur});
            if (p->type == ObjType::PortalMode) modeChanges_.push_back({p->x, cur.mode});
        }
    }
    // State for a player whose left edge is at p.
    State at(double p) const {
        auto it = std::upper_bound(events_.begin(), events_.end(), p,
                                   [](double v, const Ev& e) { return v < e.x; });
        return std::prev(it)->state;
    }
    struct ModeChange { double x; GameMode mode; };
    const std::vector<ModeChange>& modeChanges() const { return modeChanges_; }

private:
    struct Ev { double x; State state; };
    std::vector<Ev> events_;
    std::vector<ModeChange> modeChanges_;
};

// A solid in the cube's frame (floor at 0, gravity pointing down).
struct Box { double x0, x1, lo, hi; };

Box frameBox(const Object& o, bool flip, double ceiling) {
    Box b{o.x, o.x + o.w, o.y, o.y + o.h};
    if (flip) {
        b.lo = ceiling - (o.y + o.h);
        b.hi = ceiling - o.y;
    }
    return b;
}

class Checker {
public:
    Checker(const Level& lv, const std::string& dataDir, ValidationReport& rep)
        : lv_(lv), dataDir_(dataDir), rep_(rep), tl_(lv) {}

    void run() {
        palette();
        if (!rep_.issues.empty()) return;  // geometry rules need a sane palette
        embedded();
        climb();
        spikeRun();
        ship();
        duration();
        startClear();
        music();
    }

private:
    void add(const char* rule, double x, std::string msg) {
        rep_.issues.push_back({rule, x, std::move(msg)});
    }

    void palette() {
        const Level& L = lv_;
        if (L.format != 1) add("palette", -1, "format must be 1");
        if (L.id < 1 || L.id > 10) add("palette", -1, "id must be 1..10");
        if (L.name.empty() || L.name.size() > 32 * 4) add("palette", -1, "name must be 1..32 characters");
        if (L.difficulty < 1 || L.difficulty > 10) add("palette", -1, "difficulty must be 1..10");
        if (L.music.empty()) add("palette", -1, "music must not be empty");
        else if (L.music[0] == '/' || L.music.find("..") != std::string::npos)
            add("palette", -1, "music must be a relative path under assets/");
        if (!(L.ceiling >= phys::kMinCeiling && L.ceiling <= phys::kMaxCeiling))
            add("palette", -1, "ceiling must be " + fmt(phys::kMinCeiling) + ".." + fmt(phys::kMaxCeiling));

        int ends = 0;
        double endX = 0;
        for (const Object& o : L.objects)
            if (o.type == ObjType::EndWall) {
                ++ends;
                endX = o.x;
            }
        if (ends != 1) add("palette", -1, "exactly one end_wall required (found " + std::to_string(ends) + ")");
        else if (std::fabs(endX - L.endX) > kEps) add("palette", endX, "endX does not match the end_wall x");

        for (const Object& o : L.objects) {
            const std::string nm = toString(o.type);
            const double x = std::isfinite(o.x) ? o.x : -1;
            if (!isHalf(o.x) || !isHalf(o.y) || !isHalf(o.w) || !isHalf(o.h))
                add("palette", x, nm + ": x, y, w, h must be multiples of 0.5");
            if (o.x < 0 || o.y < 0) add("palette", x, nm + ": x and y must be >= 0");
            if (!(o.w > 0) || !(o.h > 0)) add("palette", x, nm + ": w and h must be > 0");
            if (o.type == ObjType::Platform && std::fabs(o.h - phys::kPlatformHeight) > kEps)
                add("palette", x, "platform h must be " + fmt(phys::kPlatformHeight));
            if (o.type == ObjType::Spike && (o.w != 1 || o.h != 1))
                add("palette", x, "spike must be 1x1");
            if (o.type != ObjType::EndWall && o.y + o.h > L.ceiling + kEps)
                add("palette", x, nm + ": top is above the ceiling");
            if (ends == 1 && o.type != ObjType::EndWall && o.x > endX + kEps)
                add("palette", x, nm + ": is right of the end_wall");
        }
    }

    void embedded() {
        for (const Object& s : lv_.objects) {
            if (s.type != ObjType::Spike) continue;
            for (const Object& b : lv_.objects) {
                if (!isSolid(b)) continue;
                const bool ox = s.x < b.x + b.w - kEps && b.x < s.x + 1 - kEps;
                const bool oy = s.y < b.y + b.h - kEps && b.y < s.y + 1 - kEps;
                if (ox && oy) {
                    add("embedded-hazard", s.x,
                        "spike at y=" + fmt(s.y) + " overlaps " + toString(b.type) + " at x=" +
                            fmt(b.x) + " y=" + fmt(b.y));
                    break;
                }
            }
        }
    }

    void climb() {
        const double ceil = lv_.ceiling;
        for (std::size_t i = 0; i < lv_.objects.size(); ++i) {
            const Object& S = lv_.objects[i];
            if (!isSolid(S)) continue;
            const State st = tl_.at(S.x - phys::kPlayerSize);
            if (st.mode != GameMode::Cube) continue;
            const bool flip = st.gravity == Gravity::Up;
            const Box s = frameBox(S, flip, ceil);
            const double window = phys::cubeJumpLength(speedValue(st.speed)) * 0.5;

            std::vector<Box> others;
            for (std::size_t j = 0; j < lv_.objects.size(); ++j) {
                if (j == i || !isSolid(lv_.objects[j])) continue;
                others.push_back(frameBox(lv_.objects[j], flip, ceil));
            }
            double floorTop = 0.0;
            for (const Box& t : others) {
                if (!(t.x0 < s.x0 - kEps)) continue;
                if (t.hi <= s.hi + kEps) {
                    if (t.x1 > s.x0 - window) floorTop = std::max(floorTop, t.hi);
                } else if (t.x1 >= s.x0 - kEps) {
                    floorTop = std::max(floorTop, t.hi);  // taller neighbour hides S's face
                }
            }
            // Follow solids that carry S from below (stacked blocks form one tall wall).
            double under = s.lo;
            for (bool grew = true; grew;) {
                grew = false;
                for (const Box& t : others) {
                    if (t.lo < under - kEps && t.hi >= under - kEps && t.x0 <= s.x0 + kEps &&
                        t.x1 > s.x0 + kEps) {
                        under = t.lo;
                        grew = true;
                    }
                }
            }
            if (under - floorTop >= phys::kPlayerSize - kEps) continue;  // cube passes beneath
            const double rise = s.hi - floorTop;
            const double limit = phys::kCubeMaxClimb + kClimbSlack;
            if (rise > limit + kEps)
                add("climb", S.x,
                    std::string(toString(S.type)) + " top is " + fmt(rise) +
                        " above the floor before it (max " + fmt(limit) + ")");
        }
    }

    void spikeRun() {
        const double ceil = lv_.ceiling;
        for (int g = 0; g < 2; ++g) {
            const Gravity grav = g == 0 ? Gravity::Down : Gravity::Up;
            const SpikeDir floorDir = g == 0 ? SpikeDir::Up : SpikeDir::Down;
            struct Sp { double x, lo; };
            std::vector<Sp> sp;
            for (const Object& o : lv_.objects) {
                if (o.type != ObjType::Spike || o.spikeDir != floorDir) continue;
                const State st = tl_.at(o.x - phys::kPlayerSize);
                if (st.mode != GameMode::Cube || st.gravity != grav) continue;
                sp.push_back({o.x, g == 0 ? o.y : ceil - o.y - 1.0});
            }
            std::stable_sort(sp.begin(), sp.end(), [](const Sp& a, const Sp& b) { return a.x < b.x; });
            std::size_t i = 0;
            while (i < sp.size()) {
                std::size_t j = i;
                while (j + 1 < sp.size() && std::fabs(sp[j + 1].lo - sp[j].lo) <= 0.5 + kEps &&
                       sp[j + 1].x - sp[j].x - phys::kSpikeHitW <
                           phys::kPlayerSize + kLandMargin - kEps)
                    ++j;
                const double size = sp[j].x - sp[i].x + 1.0;
                const State st = tl_.at(sp[i].x - phys::kPlayerSize);
                const double maxRun = phys::cubeMaxSpikeRun(speedValue(st.speed));
                if (size > maxRun + kEps)
                    add("spike-run", sp[i].x,
                        "run of floor spikes is " + fmt(size) + " blocks long (max " + fmt(maxRun) +
                            " at this speed)");
                i = j + 1;
            }
        }
    }

    struct Obst { double x0, x1, lo, hi; };
    struct Gap { double lo, hi; };  // allowed range of the ship centre

    static std::vector<Gap> mergeRanges(std::vector<Gap> v) {
        std::sort(v.begin(), v.end(), [](const Gap& a, const Gap& b) { return a.lo < b.lo; });
        std::vector<Gap> out;
        for (const Gap& g : v) {
            if (!out.empty() && g.lo <= out.back().hi + kEps) out.back().hi = std::max(out.back().hi, g.hi);
            else out.push_back(g);
        }
        return out;
    }

    void ship() {
        // Sections where mode == ship: [enter, exit + kPlayerSize), clipped to [0, endX].
        std::vector<std::pair<double, double>> sections;
        double enter = 0;
        bool inShip = lv_.startMode == GameMode::Ship;
        for (const auto& mc : tl_.modeChanges()) {
            if (mc.mode == GameMode::Ship && !inShip) {
                inShip = true;
                enter = mc.x;
            } else if (mc.mode == GameMode::Cube && inShip) {
                inShip = false;
                sections.push_back({enter, mc.x + phys::kPlayerSize});
            }
        }
        if (inShip) sections.push_back({enter, lv_.endX});

        for (auto [a, b] : sections) {
            b = std::min(b, lv_.endX);
            std::vector<Obst> obs;
            for (const Object& o : lv_.objects) {
                if (isSolid(o)) {
                    obs.push_back({o.x, o.x + o.w, o.y, o.y + o.h});
                } else if (o.type == ObjType::Spike) {
                    const double y0 = o.spikeDir == SpikeDir::Up
                                          ? o.y + phys::kSpikeHitY
                                          : o.y + 1.0 - phys::kSpikeHitY - phys::kSpikeHitH;
                    obs.push_back({o.x + phys::kSpikeHitX, o.x + phys::kSpikeHitX + phys::kSpikeHitW,
                                   y0, y0 + phys::kSpikeHitH});
                }
            }
            std::vector<Gap> prev;
            bool failing = false;  // inside a stretch already reported
            const int n = static_cast<int>(std::floor((b - a) / kShipStep + kEps));
            for (int k = 0; k < n; ++k) {
                const double xs = a + (k + 0.5) * kShipStep;
                std::vector<std::pair<double, double>> iv;
                for (const Obst& o : obs)
                    if (o.x0 < xs && xs < o.x1) iv.push_back({o.lo, o.hi});
                std::sort(iv.begin(), iv.end());
                std::vector<Gap> usable;
                double cur = 0.0, best = 0.0;
                auto gap = [&](double lo, double hi) {
                    best = std::max(best, hi - lo);
                    if (hi - lo >= phys::kShipMinGap - kEps)
                        usable.push_back({lo + phys::kPlayerSize / 2, hi - phys::kPlayerSize / 2});
                };
                for (const auto& [lo, hi] : iv) {
                    if (lo > cur) gap(cur, lo);
                    cur = std::max(cur, hi);
                }
                if (lv_.ceiling > cur) gap(cur, lv_.ceiling);

                if (usable.empty()) {
                    if (!failing)
                        add("ship-gap", xs - kShipStep / 2,
                            "free corridor is only " + fmt(best) + " blocks (min " +
                                fmt(phys::kShipMinGap) + ")");
                    failing = true;
                    prev.clear();
                    continue;
                }
                std::vector<Gap> reach;
                if (prev.empty()) {
                    reach = usable;
                    failing = false;
                } else {
                    const double grow = phys::kShipMaxSlope * kShipStep;
                    for (const Gap& u : usable)
                        for (const Gap& r : prev) {
                            const double lo = std::max(u.lo, r.lo - grow);
                            const double hi = std::min(u.hi, r.hi + grow);
                            if (lo <= hi + kEps) reach.push_back({lo, hi});
                        }
                    if (reach.empty()) {
                        if (!failing)
                            add("ship-slope", xs - kShipStep / 2,
                                "corridor moves faster than " + fmt(phys::kShipMaxSlope) +
                                    " blocks per block of x");
                        failing = true;
                        reach = usable;
                    } else {
                        failing = false;
                    }
                }
                prev = mergeRanges(std::move(reach));
            }
        }
    }

    void duration() {
        rep_.durationSeconds = levelDurationSeconds(lv_);
        if (rep_.durationSeconds < phys::kLevelMinSeconds || rep_.durationSeconds > phys::kLevelMaxSeconds)
            add("duration", -1,
                "duration " + fmt(rep_.durationSeconds) + " s is outside " + fmt(phys::kLevelMinSeconds) +
                    ".." + fmt(phys::kLevelMaxSeconds) + " s");
    }

    void startClear() {
        for (const Object& o : lv_.objects)
            if ((isSolid(o) || o.type == ObjType::Spike) && o.x < phys::kStartClearBlocks)
                add("start-clear", o.x,
                    std::string(toString(o.type)) + " inside the first " + fmt(phys::kStartClearBlocks) +
                        " blocks");
    }

    void music() {
        if (dataDir_.empty()) return;
        std::error_code ec;
        if (!std::filesystem::exists(assetPath(dataDir_, lv_.music), ec))
            add("music", -1, "music file not found: " + assetPath(dataDir_, lv_.music));
    }

    const Level& lv_;
    const std::string& dataDir_;
    ValidationReport& rep_;
    Timeline tl_;
};

}  // namespace

ValidationReport validateLevel(const Level& level, const std::string& dataDir) {
    ValidationReport rep;
    rep.durationSeconds = levelDurationSeconds(level);
    Checker(level, dataDir, rep).run();
    std::stable_sort(rep.issues.begin(), rep.issues.end(),
                     [](const ValidationIssue& a, const ValidationIssue& b) { return a.x < b.x; });
    return rep;
}

}  // namespace gd
