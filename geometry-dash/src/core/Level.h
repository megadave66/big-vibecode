#pragma once
// Level data model + object palette. Contract shared by loader, validator, sim, renderer.
// See docs/LEVEL-FORMAT.md. Owner: level-format section (keep this interface stable).

#include <cstdint>
#include <string>
#include <vector>

namespace gd {

enum class GameMode : std::uint8_t { Cube, Ship };
enum class Gravity : std::uint8_t { Down, Up };
enum class Speed : std::uint8_t { Slow, Normal, Fast, Faster };
enum class SpikeDir : std::uint8_t { Up, Down };
enum class DecoKind : std::uint8_t { Pillar, Chain, Star, Arrow, Glow };

enum class ObjType : std::uint8_t {
    Block,
    Platform,
    Spike,
    PortalGravity,
    PortalMode,
    PortalSpeed,
    EndWall,
    Deco,
};

struct Object {
    ObjType type = ObjType::Block;
    double x = 0, y = 0;
    double w = 1, h = 1;          // Platform: h = kPlatformHeight. Spike: 1x1. Portals: 1 x kPortalDrawHeight.
    SpikeDir spikeDir = SpikeDir::Up;
    Gravity gravity = Gravity::Down;   // PortalGravity
    GameMode mode = GameMode::Cube;    // PortalMode
    Speed speed = Speed::Normal;       // PortalSpeed
    DecoKind deco = DecoKind::Pillar;  // Deco
};

struct Color {
    std::uint8_t r = 0, g = 0, b = 0;
};

struct Level {
    int format = 1;
    int id = 0;
    std::string name;
    int difficulty = 1;
    std::string music;                  // relative to assets/
    GameMode startMode = GameMode::Cube;
    Speed startSpeed = Speed::Normal;
    Gravity startGravity = Gravity::Down;
    double ceiling = 12.0;
    Color bg{32, 58, 143}, ground{26, 47, 110}, accent{95, 208, 255};
    std::vector<Object> objects;        // sorted by x after load
    double endX = 0;                    // x of the end_wall
};

double speedValue(Speed s);             // blocks per second, from PhysicsConstants.h
const char* toString(ObjType t);
// Duration of the level in seconds, following speed portals from start to end wall.
double levelDurationSeconds(const Level& level);

}  // namespace gd
