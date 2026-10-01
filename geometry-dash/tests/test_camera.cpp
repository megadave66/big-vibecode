#include <doctest/doctest.h>

#include <cmath>

#include "core/Camera.h"
#include "core/Interp.h"

using namespace gd;

TEST_CASE("camera: player sits at the lead fraction of the view") {
    CameraConfig c;
    Camera cam(c);
    cam.snapTo(100.5, 0.5, GameMode::Cube, 12.0);
    // Player centre x = 100.5 -> screen x = 1/3 of the width.
    CHECK(cam.screenX(100.5) == doctest::Approx(c.viewW / 3.0));
    cam.update(1.0 / 60.0, 200.5, 0.5, GameMode::Cube, 12.0);
    CHECK(cam.screenX(200.5) == doctest::Approx(c.viewW / 3.0));
}

TEST_CASE("camera: y is flipped and the world/screen mapping round-trips") {
    Camera cam;
    cam.snapTo(10.0, 0.5, GameMode::Cube, 12.0);
    CHECK(cam.screenY(5.0) < cam.screenY(1.0));  // higher in the world = smaller screen y
    for (double wx : {-3.0, 0.0, 17.25}) CHECK(cam.worldX(cam.screenX(wx)) == doctest::Approx(wx));
    for (double wy : {-2.0, 0.0, 9.5}) CHECK(cam.worldY(cam.screenY(wy)) == doctest::Approx(wy));
    // One block is pxPerBlock pixels.
    CHECK(cam.screenX(1.0) - cam.screenX(0.0) == doctest::Approx(cam.config().pxPerBlock));
    CHECK(cam.screenY(0.0) - cam.screenY(1.0) == doctest::Approx(cam.config().pxPerBlock));
}

TEST_CASE("camera: block size is in the 48..64 px range at 1280x720") {
    CameraConfig c;
    CHECK(c.pxPerBlock >= 48.0);
    CHECK(c.pxPerBlock <= 64.0);
}

TEST_CASE("camera: cube on the ground shows the ground band and stays put inside the dead zone") {
    CameraConfig c;
    const double lo = cameraMinCenterY(c);
    // Player centre 0.5 is within the dead zone of the lowest framing: no target change.
    CHECK(cameraTargetCenterY(c, lo, 0.5, GameMode::Cube, 12.0) == doctest::Approx(lo));
    Camera cam(c);
    cam.snapTo(0.5, 0.5, GameMode::Cube, 12.0);
    CHECK(cam.viewBottom() == doctest::Approx(-c.groundMargin));
    CHECK(cam.viewBottom() < 0.0);
}

TEST_CASE("camera: follows the player above the dead zone and clamps at the ceiling") {
    CameraConfig c;
    const double ceiling = 20.0;
    const double cur = cameraMinCenterY(c);
    const double high = cur + c.deadZone + 3.0;
    CHECK(cameraTargetCenterY(c, cur, high, GameMode::Cube, ceiling) == doctest::Approx(high - c.deadZone));
    // Very high player: clamp to the max centre.
    CHECK(cameraTargetCenterY(c, cur, 500.0, GameMode::Cube, ceiling) == doctest::Approx(cameraMaxCenterY(c, ceiling)));
    // Very low player: clamp to the min centre.
    CHECK(cameraTargetCenterY(c, 10.0, -50.0, GameMode::Cube, ceiling) == doctest::Approx(cameraMinCenterY(c)));
    // Max never drops below min even for a short ceiling.
    CHECK(cameraMaxCenterY(c, 3.0) >= cameraMinCenterY(c));
}

TEST_CASE("camera: smooth follow converges without overshoot") {
    CameraConfig c;
    Camera cam(c);
    const double ceiling = 20.0;
    cam.snapTo(0.5, 0.5, GameMode::Cube, ceiling);
    double prev = cam.centerY();
    for (int i = 0; i < 600; ++i) {
        cam.update(1.0 / 60.0, 0.5 + i * 0.17, 12.0, GameMode::Cube, ceiling);
        CHECK(cam.centerY() >= prev - 1e-12);
        prev = cam.centerY();
    }
    CHECK(cam.centerY() == doctest::Approx(12.0 - c.deadZone).epsilon(0.01));
}

TEST_CASE("camera: ship frames the corridor when it fits, follows when it does not") {
    CameraConfig c;
    const double ceiling = 12.0;
    REQUIRE(corridorFits(c, ceiling));
    // Whatever the ship height, the target is the corridor centre.
    CHECK(cameraTargetCenterY(c, 5.0, 1.0, GameMode::Ship, ceiling) == doctest::Approx(6.0));
    CHECK(cameraTargetCenterY(c, 5.0, 11.0, GameMode::Ship, ceiling) == doctest::Approx(6.0));
    Camera cam(c);
    cam.snapTo(0.5, 1.5, GameMode::Ship, ceiling);
    CHECK(cam.viewBottom() < 0.0);
    CHECK(cam.viewTop() > ceiling);
    // A tall world does not fit: ship follows like the cube.
    CHECK_FALSE(corridorFits(c, 20.0));
    CHECK(cameraTargetCenterY(c, 10.0, 18.0, GameMode::Ship, 20.0) > 10.0);
}

TEST_CASE("camera: dt of zero does not move the vertical position") {
    Camera cam;
    cam.snapTo(0.5, 0.5, GameMode::Cube, 20.0);
    const double y = cam.centerY();
    cam.update(0.0, 0.5, 15.0, GameMode::Cube, 20.0);
    CHECK(cam.centerY() == doctest::Approx(y));
}

TEST_CASE("interp: lerp and shortest-way angle lerp") {
    CHECK(lerp(2.0, 6.0, 0.25) == doctest::Approx(3.0));
    CHECK(angleDiffDeg(350.0, 10.0) == doctest::Approx(20.0));
    CHECK(angleDiffDeg(10.0, 350.0) == doctest::Approx(-20.0));
    CHECK(lerpAngleDeg(350.0, 10.0, 0.5) == doctest::Approx(360.0));
    CHECK(lerpAngleDeg(90.0, 180.0, 0.5) == doctest::Approx(135.0));
    CHECK(lerpAngleDeg(30.0, 30.0, 0.7) == doctest::Approx(30.0));
    CHECK(clampd(5.0, 0.0, 1.0) == doctest::Approx(1.0));
}
