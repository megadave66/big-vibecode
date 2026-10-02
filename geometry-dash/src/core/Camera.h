#pragma once
// Camera math for the game view. Pure logic, no SDL. Units: world blocks (y up) and screen
// pixels (y down).
//
// Horizontal: the player's centre stays at a fixed fraction of the view width (the "lead").
// Vertical: the view centre follows the player with a dead zone and smoothing, clamped so the
// view never shows more than a margin outside the world. In ship mode, when the whole corridor
// (ground to ceiling) fits in the view, the camera stops and frames the corridor.

#include "core/Level.h"

namespace gd {

struct CameraConfig {
    double viewW = 1280.0;          // view size in pixels
    double viewH = 720.0;
    double pxPerBlock = 52.0;       // 48..64 at 1280x720
    double leadFraction = 1.0 / 3.0;
    double groundMargin = 2.0;      // blocks of ground band visible below y = 0 (cube)
    double ceilingMargin = 1.0;     // blocks visible above the ceiling at most
    double deadZone = 1.5;          // blocks the player may move from the view centre before it follows
    double followRate = 7.0;        // 1/s, exponential smoothing of the vertical follow
};

double viewWidthBlocks(const CameraConfig& c);
double viewHeightBlocks(const CameraConfig& c);

// World x of the view's left edge for a player whose centre is at playerCenterX.
double cameraLeftX(const CameraConfig& c, double playerCenterX);

// Lowest / highest view-centre y the camera may take for this ceiling.
double cameraMinCenterY(const CameraConfig& c);
double cameraMaxCenterY(const CameraConfig& c, double ceiling);

// True when ground .. ceiling fits inside the view (used to frame the ship corridor).
bool corridorFits(const CameraConfig& c, double ceiling);

// Desired view-centre y given the current centre and the player's centre y.
double cameraTargetCenterY(const CameraConfig& c, double curCenterY, double playerCenterY,
                           GameMode mode, double ceiling);

class Camera {
public:
    explicit Camera(const CameraConfig& c = CameraConfig{}) : cfg_(c) {}

    const CameraConfig& config() const { return cfg_; }

    // Jump straight to the target (level start, respawn).
    void snapTo(double playerCenterX, double playerCenterY, GameMode mode, double ceiling);
    // Smooth follow; call once per rendered frame with the real frame time.
    void update(double dt, double playerCenterX, double playerCenterY, GameMode mode, double ceiling);

    double leftX() const { return leftX_; }
    double centerY() const { return centerY_; }

    // World -> screen pixels.
    double screenX(double wx) const { return (wx - leftX_) * cfg_.pxPerBlock; }
    double screenY(double wy) const { return cfg_.viewH * 0.5 - (wy - centerY_) * cfg_.pxPerBlock; }
    // Screen -> world.
    double worldX(double sx) const { return sx / cfg_.pxPerBlock + leftX_; }
    double worldY(double sy) const { return centerY_ + (cfg_.viewH * 0.5 - sy) / cfg_.pxPerBlock; }

    // Visible world rectangle.
    double viewLeft() const { return leftX_; }
    double viewRight() const { return leftX_ + viewWidthBlocks(cfg_); }
    double viewBottom() const { return centerY_ - viewHeightBlocks(cfg_) * 0.5; }
    double viewTop() const { return centerY_ + viewHeightBlocks(cfg_) * 0.5; }

private:
    CameraConfig cfg_;
    double leftX_ = 0.0;
    double centerY_ = 0.0;
};

}  // namespace gd
