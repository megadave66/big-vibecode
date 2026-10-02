#include "core/Camera.h"

#include <algorithm>
#include <cmath>

#include "core/Interp.h"

namespace gd {

double viewWidthBlocks(const CameraConfig& c) { return c.viewW / c.pxPerBlock; }
double viewHeightBlocks(const CameraConfig& c) { return c.viewH / c.pxPerBlock; }

double cameraLeftX(const CameraConfig& c, double playerCenterX) {
    return playerCenterX - viewWidthBlocks(c) * c.leadFraction;
}

double cameraMinCenterY(const CameraConfig& c) {
    return -c.groundMargin + viewHeightBlocks(c) * 0.5;
}

double cameraMaxCenterY(const CameraConfig& c, double ceiling) {
    return std::max(cameraMinCenterY(c), ceiling + c.ceilingMargin - viewHeightBlocks(c) * 0.5);
}

bool corridorFits(const CameraConfig& c, double ceiling) {
    return ceiling <= viewHeightBlocks(c) - 0.5;  // leaves at least a quarter block on each side
}

double cameraTargetCenterY(const CameraConfig& c, double curCenterY, double playerCenterY,
                           GameMode mode, double ceiling) {
    if (mode == GameMode::Ship && corridorFits(c, ceiling)) return ceiling * 0.5;
    double target = curCenterY;
    if (playerCenterY > curCenterY + c.deadZone) target = playerCenterY - c.deadZone;
    else if (playerCenterY < curCenterY - c.deadZone) target = playerCenterY + c.deadZone;
    return clampd(target, cameraMinCenterY(c), cameraMaxCenterY(c, ceiling));
}

void Camera::snapTo(double px, double py, GameMode mode, double ceiling) {
    leftX_ = cameraLeftX(cfg_, px);
    // Start from the lowest framing and let the follow rule pull it to the player.
    centerY_ = cameraTargetCenterY(cfg_, cameraMinCenterY(cfg_), py, mode, ceiling);
}

void Camera::update(double dt, double px, double py, GameMode mode, double ceiling) {
    leftX_ = cameraLeftX(cfg_, px);
    const double target = cameraTargetCenterY(cfg_, centerY_, py, mode, ceiling);
    const double k = 1.0 - std::exp(-cfg_.followRate * std::max(0.0, dt));
    centerY_ += (target - centerY_) * k;
}

}  // namespace gd
