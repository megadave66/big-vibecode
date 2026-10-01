class_name PocketMath
extends RefCounted
## Pure maths for the pocket ring. No scene access, so it is unit-testable.
##
## Angle convention (wheel-local and rotor-local space, Y up):
## - "Angle" phi is measured counter-clockwise when viewed from above, from the +X axis.
##   A point at angle phi and radius r sits at (r * cos(phi), y, -r * sin(phi)).
## - Godot's positive rotation about +Y is also counter-clockwise from above.
## - Pocket index i (0..37) has its centre at phi_i = FIRST_POCKET_ANGLE - i * STEP.
##   Index grows CLOCKWISE from above, so walking the ring clockwise reads WheelLayout.POCKET_ORDER.

const POCKET_COUNT := WheelLayout.POCKET_COUNT
const STEP := TAU / POCKET_COUNT
## Pocket 0 (the green "0") sits at -Z ("north" from above) in rotor-local space.
const FIRST_POCKET_ANGLE := PI / 2.0


## Counter-clockwise angle (radians, -PI..PI) of a local point, seen from above.
static func angle_of(local_point: Vector3) -> float:
	return atan2(-local_point.z, local_point.x)


## Horizontal distance of a local point from the wheel axis.
static func radius_of(local_point: Vector3) -> float:
	return Vector2(local_point.x, local_point.z).length()


## Centre angle of the pocket at ring index i (wraps).
static func angle_for_index(index: int) -> float:
	return wrapf(FIRST_POCKET_ANGLE - posmod(index, POCKET_COUNT) * STEP, -PI, PI)


## Ring index (0..37) of the pocket that contains a rotor-local angle.
## Pocket i covers [phi_i - STEP/2, phi_i + STEP/2).
static func index_for_angle(local_angle: float) -> int:
	var steps := (FIRST_POCKET_ANGLE - local_angle) / STEP
	return posmod(int(floor(steps + 0.5)), POCKET_COUNT)


## Wheel number (0..37, 37 = "00") for a rotor-local angle.
static func number_for_angle(local_angle: float) -> int:
	return WheelLayout.number_at(index_for_angle(local_angle))


## Wheel number under a world-frame angle when the rotor has turned by rotor_angle
## (counter-clockwise from above, radians).
static func number_for_world_angle(world_angle: float, rotor_angle: float) -> int:
	return number_for_angle(world_angle - rotor_angle)


## Wheel number for a point given in rotor-local coordinates.
static func number_for_local_point(local_point: Vector3) -> int:
	return number_for_angle(angle_of(local_point))


## Rotor-local angle of the centre of the pocket holding a number.
static func angle_for_number(number: int) -> float:
	return angle_for_index(WheelLayout.index_of(number))


## Rotor-local point at a given angle, radius and height.
static func point_at(angle: float, radius: float, height: float) -> Vector3:
	return Vector3(radius * cos(angle), height, -radius * sin(angle))


## True if a local point lies inside the pocket ring band (between the inner and
## outer pocket walls) and low enough to be between the frets.
static func is_in_pocket_ring(local_point: Vector3, inner_r: float, outer_r: float, max_height: float) -> bool:
	var r := radius_of(local_point)
	return r >= inner_r and r <= outer_r and local_point.y <= max_height


## Velocity of a rotor surface point at a local position (rotor spins about +Y
## at omega rad/s, counter-clockwise from above when omega > 0).
static func rotor_velocity_at(local_point: Vector3, omega: float) -> Vector3:
	return Vector3(0.0, omega, 0.0).cross(Vector3(local_point.x, 0.0, local_point.z))
