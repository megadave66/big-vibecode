// Level model helpers. Owner: level-format section.
#include "core/Level.h"

#include <algorithm>

#include "core/PhysicsConstants.h"

namespace gd {

double speedValue(Speed s) {
    switch (s) {
        case Speed::Slow: return phys::kSpeedSlow;
        case Speed::Normal: return phys::kSpeedNormal;
        case Speed::Fast: return phys::kSpeedFast;
        case Speed::Faster: return phys::kSpeedFaster;
    }
    return phys::kSpeedNormal;
}

const char* toString(ObjType t) {
    switch (t) {
        case ObjType::Block: return "block";
        case ObjType::Platform: return "platform";
        case ObjType::Spike: return "spike";
        case ObjType::PortalGravity: return "portal_gravity";
        case ObjType::PortalMode: return "portal_mode";
        case ObjType::PortalSpeed: return "portal_speed";
        case ObjType::EndWall: return "end_wall";
        case ObjType::Deco: return "deco";
    }
    return "unknown";
}

// The sim's rules: the player starts with its left edge at x = 0. A portal fires when the left
// edge reaches portal.x (centre reaches portal.x + 0.5). The player wins when the left edge
// reaches endX - kPlayerSize. So the run covers left-edge positions 0 .. endX - kPlayerSize,
// with the speed changing at each speed portal's x.
double levelDurationSeconds(const Level& level) {
    std::vector<const Object*> portals;
    for (const Object& o : level.objects)
        if (o.type == ObjType::PortalSpeed) portals.push_back(&o);
    std::stable_sort(portals.begin(), portals.end(),
                     [](const Object* a, const Object* b) { return a->x < b->x; });

    const double runEnd = level.endX - phys::kPlayerSize;
    double t = 0.0, pos = 0.0;
    Speed speed = level.startSpeed;
    for (const Object* p : portals) {
        if (p->x >= runEnd) break;
        if (p->x > pos) {
            t += (p->x - pos) / speedValue(speed);
            pos = p->x;
        }
        speed = p->speed;
    }
    if (runEnd > pos) t += (runEnd - pos) / speedValue(speed);
    return t;
}

}  // namespace gd
