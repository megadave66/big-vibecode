class_name WheelGeometry
extends RefCounted
## Dimensions (metres) and procedural mesh/collider builders for the roulette wheel.
## 1 unit = 1 m. Wheel-local origin = centre of the wheel base, y = 0 is the bottom
## (it sits on the table). Profiles are lists of (radius, height) points, ordered from
## the outside of the wheel towards the centre, so the "up/visible" side is on the left
## of the travel direction (normal = (dy, -dr)).

# --- Overall ---------------------------------------------------------------
## Outer radius of the wooden bowl (footprint on the table).
const OUTER_RADIUS := 0.42
const RIM_TOP_Y := 0.145

# --- Bowl (static) ---------------------------------------------------------
## Outer wall of the ball track. The ball runs against this wall while it is fast.
const WALL_R := 0.385
const WALL_BOTTOM_Y := 0.115
## Track floor slopes down towards the centre (about 12 degrees).
const TRACK_INNER_R := 0.345
const TRACK_INNER_Y := 0.1065
## Lower bowl ("apron") where the diamonds sit (about 22 degrees).
const APRON_INNER_R := 0.272
const APRON_INNER_Y := 0.077
const DEFLECTOR_R := 0.315
const DEFLECTOR_COUNT := 8

# --- Rotor (spinning) ------------------------------------------------------
const ROTOR_R := 0.268
const ROTOR_TOP_Y := 0.074
## Number ring: slopes down from ROTOR_R to POCKET_OUTER_R (about 20 degrees).
const POCKET_OUTER_R := 0.232
const RING_INNER_Y := 0.061
const POCKET_INNER_R := 0.19
const POCKET_FLOOR_Y := 0.048
## Frets are as high as the ring's inner edge.
const FRET_TOP_Y := 0.061
const FRET_THICKNESS := 0.0025
## Central cone rises to the turret.
const CONE_TOP_R := 0.05
const CONE_TOP_Y := 0.11

# --- Ball ------------------------------------------------------------------
const BALL_RADIUS := 0.0095

## Invisible containment lid above the track (keeps a wild bounce inside the bowl).
const LID_Y := 0.152


static func track_floor_y(r: float) -> float:
	var t := (WALL_R - r) / (WALL_R - TRACK_INNER_R)
	return lerpf(WALL_BOTTOM_Y, TRACK_INNER_Y, t)


static func apron_y(r: float) -> float:
	var t := (TRACK_INNER_R - r) / (TRACK_INNER_R - APRON_INNER_R)
	return lerpf(TRACK_INNER_Y, APRON_INNER_Y, t)


static func ring_y(r: float) -> float:
	var t := (ROTOR_R - r) / (ROTOR_R - POCKET_OUTER_R)
	return lerpf(ROTOR_TOP_Y, RING_INNER_Y, t)


## Radius of the pocket centres (middle of the pocket floor).
static func pocket_center_r() -> float:
	return (POCKET_INNER_R + POCKET_OUTER_R) * 0.5


## Ball-centre height when it rests on the pocket floor.
static func pocket_rest_y() -> float:
	return POCKET_FLOOR_Y + BALL_RADIUS


# --- Profiles ----------------------------------------------------------------
static func wall_profile() -> PackedVector2Array:
	# Rim top edge -> small overhanging lip -> vertical wall.
	return PackedVector2Array([
		Vector2(WALL_R, RIM_TOP_Y), Vector2(WALL_R - 0.008, RIM_TOP_Y - 0.001),
		Vector2(WALL_R - 0.008, RIM_TOP_Y - 0.005), Vector2(WALL_R, RIM_TOP_Y - 0.009),
		Vector2(WALL_R, WALL_BOTTOM_Y),
	])


static func track_profile() -> PackedVector2Array:
	return PackedVector2Array([Vector2(WALL_R, WALL_BOTTOM_Y), Vector2(TRACK_INNER_R, TRACK_INNER_Y)])


static func apron_profile() -> PackedVector2Array:
	return PackedVector2Array([
		Vector2(TRACK_INNER_R, TRACK_INNER_Y), Vector2(APRON_INNER_R, APRON_INNER_Y),
		Vector2(APRON_INNER_R, 0.03), Vector2(0.06, 0.03),
	])


static func outer_shell_profile() -> PackedVector2Array:
	return PackedVector2Array([
		Vector2(OUTER_RADIUS, 0.0), Vector2(OUTER_RADIUS, RIM_TOP_Y - 0.014),
		Vector2(OUTER_RADIUS - 0.012, RIM_TOP_Y), Vector2(WALL_R, RIM_TOP_Y),
	])


static func rotor_side_profile() -> PackedVector2Array:
	return PackedVector2Array([Vector2(ROTOR_R, 0.02), Vector2(ROTOR_R, ROTOR_TOP_Y)])


static func number_ring_profile() -> PackedVector2Array:
	return PackedVector2Array([Vector2(ROTOR_R, ROTOR_TOP_Y), Vector2(POCKET_OUTER_R, RING_INNER_Y)])


static func pocket_profile() -> PackedVector2Array:
	# Outer pocket wall -> floor -> inner pocket wall.
	return PackedVector2Array([
		Vector2(POCKET_OUTER_R, RING_INNER_Y), Vector2(POCKET_OUTER_R, POCKET_FLOOR_Y),
		Vector2(POCKET_INNER_R, POCKET_FLOOR_Y), Vector2(POCKET_INNER_R, FRET_TOP_Y),
	])


static func cone_profile() -> PackedVector2Array:
	return PackedVector2Array([Vector2(POCKET_INNER_R, FRET_TOP_Y), Vector2(CONE_TOP_R, CONE_TOP_Y),
		Vector2(0.02, CONE_TOP_Y + 0.004)])


static func lid_profile() -> PackedVector2Array:
	return PackedVector2Array([Vector2(OUTER_RADIUS, LID_Y), Vector2(0.07, LID_Y)])


# --- Builders ----------------------------------------------------------------
static func _p3(rr: float, y: float, a: float) -> Vector3:
	return Vector3(rr * cos(a), y, -rr * sin(a))


static func _n3(n2: Vector2, a: float) -> Vector3:
	return Vector3(n2.x * cos(a), n2.y, -n2.x * sin(a)).normalized()


## Adds a surface of revolution to a SurfaceTool (PRIMITIVE_TRIANGLES).
## Angles a0..a1 are counter-clockwise from above (see PocketMath).
static func add_revolve(st: SurfaceTool, profile: PackedVector2Array, a0: float, a1: float, segs: int, color: Color) -> void:
	for j in profile.size() - 1:
		var p0 := profile[j]
		var p1 := profile[j + 1]
		var d := p1 - p0
		var n2 := Vector2(d.y, -d.x).normalized()
		for k in segs:
			var aa := lerpf(a0, a1, float(k) / segs)
			var ab := lerpf(a0, a1, float(k + 1) / segs)
			var quad := [
				[_p3(p0.x, p0.y, aa), _n3(n2, aa)], [_p3(p0.x, p0.y, ab), _n3(n2, ab)],
				[_p3(p1.x, p1.y, ab), _n3(n2, ab)], [_p3(p1.x, p1.y, aa), _n3(n2, aa)],
			]
			_add_tri(st, quad[0], quad[1], quad[2], color)
			_add_tri(st, quad[0], quad[2], quad[3], color)


## Godot front faces are clockwise seen from the front. Reorder to match the normal.
static func _add_tri(st: SurfaceTool, a: Array, b: Array, c: Array, color: Color) -> void:
	var pa: Vector3 = a[0]
	var pb: Vector3 = b[0]
	var pc: Vector3 = c[0]
	var want: Vector3 = (a[1] + b[1] + c[1])
	var geo := (pb - pa).cross(pc - pa)
	if geo.length_squared() < 1e-14:
		return
	var verts := [a, b, c] if geo.dot(want) < 0.0 else [a, c, b]
	for v in verts:
		st.set_color(color)
		st.set_normal(v[1])
		st.add_vertex(v[0])


## Triangle soup (for ConcavePolygonShape3D) of a surface of revolution.
## Front faces point to the visible side of the profile (same winding rule as meshes),
## so one-sided collision works from the side the ball is on.
static func revolve_faces(profile: PackedVector2Array, a0: float, a1: float, segs: int) -> PackedVector3Array:
	var out := PackedVector3Array()
	for j in profile.size() - 1:
		var p0 := profile[j]
		var p1 := profile[j + 1]
		var d := p1 - p0
		var n2 := Vector2(d.y, -d.x).normalized()
		for k in segs:
			var aa := lerpf(a0, a1, float(k) / segs)
			var ab := lerpf(a0, a1, float(k + 1) / segs)
			var n := _n3(n2, (aa + ab) * 0.5)
			var v0 := _p3(p0.x, p0.y, aa)
			var v1 := _p3(p0.x, p0.y, ab)
			var v2 := _p3(p1.x, p1.y, ab)
			var v3 := _p3(p1.x, p1.y, aa)
			_append_tri(out, v0, v1, v2, n)
			_append_tri(out, v0, v2, v3, n)
	return out


static func _append_tri(out: PackedVector3Array, a: Vector3, b: Vector3, c: Vector3, n: Vector3) -> void:
	var geo := (b - a).cross(c - a)
	if geo.length_squared() < 1e-14:
		return
	if geo.dot(n) < 0.0:
		out.append_array([a, b, c])
	else:
		out.append_array([a, c, b])


static func revolve_mesh(profile: PackedVector2Array, segs: int, color: Color = Color.WHITE) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	add_revolve(st, profile, 0.0, TAU, segs, color)
	return st.commit()


static func revolve_shape(profile: PackedVector2Array, segs: int) -> ConcavePolygonShape3D:
	var shape := ConcavePolygonShape3D.new()
	shape.backface_collision = false
	shape.set_faces(revolve_faces(profile, 0.0, TAU, segs))
	return shape


## Points of a diamond deflector (a rhombus frustum) in its own frame:
## X = long axis, Y = surface normal, Z = short axis. Slightly sunk into the bowl.
static func deflector_points(half_len: float, half_wid: float, height: float) -> PackedVector3Array:
	var sink := -0.003
	var top := 0.45
	return PackedVector3Array([
		Vector3(half_len, sink, 0), Vector3(0, sink, half_wid), Vector3(-half_len, sink, 0), Vector3(0, sink, -half_wid),
		Vector3(half_len * top, height, 0), Vector3(0, height, half_wid * top),
		Vector3(-half_len * top, height, 0), Vector3(0, height, -half_wid * top),
	])


## Flat-shaded mesh for a convex point set given as faces (index lists).
static func deflector_mesh(pts: PackedVector3Array) -> ArrayMesh:
	var faces := [[4, 5, 6, 7], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]]
	var centroid := Vector3.ZERO
	for p in pts:
		centroid += p
	centroid /= pts.size()
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for f in faces:
		var a := pts[f[0]]
		var b := pts[f[1]]
		var c := pts[f[2]]
		var d := pts[f[3]]
		var n := (b - a).cross(c - a).normalized()
		var mid := (a + b + c + d) * 0.25
		if n.dot(mid - centroid) < 0.0:
			n = -n
		_add_tri(st, [a, n], [b, n], [c, n], Color.WHITE)
		_add_tri(st, [a, n], [c, n], [d, n], Color.WHITE)
	return st.commit()
