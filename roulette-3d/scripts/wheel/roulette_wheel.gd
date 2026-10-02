class_name RouletteWheel
extends Node3D
## American roulette wheel with a real physics ball.
##
## How it works
## - The bowl (ball track, apron, 8 diamond deflectors) is static collision geometry.
## - The rotor (number ring, 38 pockets, frets, cone, turret) is an AnimatableBody3D that
##   turns counter-clockwise (seen from above) at rotor_speed. The engine gives the
##   rotor surfaces a real velocity, so frets and pocket walls push the ball.
## - The ball is a RigidBody3D with CCD. launch_ball() places it on the track and gives it
##   a clockwise speed (opposite the rotor). Damping and contacts slow it down. When it
##   is too slow to stay against the wall it rolls down the apron, hits diamonds, crosses
##   the number ring, rattles over frets and comes to rest in a pocket.
## - No outcome is chosen. The number is read only after the ball has moved with the
##   rotor (relative speed ~0) for settle_hold_time while deep inside the pocket ring.
##   PocketMath maps the ball's rotor-local angle to a pocket via WheelLayout.POCKET_ORDER.
## - If nothing settles within time_limit the wheel emits ball_timed_out (no number) and
##   the ball can be launched again. Nothing is faked.
##
## Geometry and sizes live in WheelGeometry (1 unit = 1 m, y = 0 is the wheel base).

signal ball_launched
signal ball_settled(number: int)
## kind: "fret", "deflector", "rim" or "pocket". strength: 0..1.
signal ball_collided(kind: String, strength: float)
## Extra signal (not in the base contract): no settle within time_limit. No number is
## emitted. The game can call launch_ball() again.
signal ball_timed_out

const OUTER_RADIUS := WheelGeometry.OUTER_RADIUS

@export_group("Rotor")
## Rotor angular speed in rad/s. Positive = counter-clockwise seen from above.
## 2.2 rad/s is about 21 rpm, a typical casino rotor speed.
@export var rotor_speed: float = 2.2
## Gentle speed variation as a fraction of rotor_speed (0 = fixed speed).
@export_range(0.0, 0.2) var rotor_speed_variation: float = 0.03
## Period of that variation in seconds.
@export var rotor_variation_period: float = 11.0

@export_group("Launch")
## Ball launch speed range on the track (m/s). The rng picks a value in this range.
@export var launch_speed_min: float = 2.6
@export var launch_speed_max: float = 3.2
## Wheel-local angle where the ball is released (rad, counter-clockwise from +X).
@export var launch_angle: float = -PI / 2.0
## Random spread of the release angle (rad).
@export var launch_angle_jitter: float = 0.3

@export_group("Ball")
## Ball mass (kg). A real ball is 6-10 g, but Jolt replaces very small inertia values
## (below about 1e-6 kg m^2) with a much larger fallback, so the ball would not roll.
## 0.03 kg keeps the true sphere inertia. All other bodies are static or kinematic, so
## the mass value does not change the motion.
@export var ball_mass: float = 0.03
## Ball friction. Godot/Jolt combine friction as the MIN of the two bodies, so the
## surface values below decide most contacts.
@export var ball_friction: float = 1.0
## Ball bounce. Combined bounce = ball + surface (clamped to 1).
@export var ball_bounce: float = 0.3
## Linear damping (1/s). Stands in for air drag and rolling resistance. Together with
## the track contact losses it sets how long the ball orbits before it drops.
@export var ball_linear_damp: float = 0.08
## Angular damping (1/s). Kept high on purpose: Jolt lets a fast-spinning small sphere
## sink into a curved mesh wall (measured: penetration grows with spin), so the ball
## mostly slides on the polished track instead of rolling at 300 rad/s.
@export var ball_angular_damp: float = 3.0
## Continuous collision detection (swept motion) for the small, fast ball.
@export var ball_ccd: bool = true
## Start the ball already rolling on the track. Off by default (see ball_angular_damp).
@export var launch_rolling: bool = false

@export_group("Surfaces")
## Bowl = outer wall + ball track + apron, one smooth collider. Low friction: polished wood.
@export var bowl_friction: float = 0.01
@export var bowl_bounce: float = 0.2
@export var deflector_friction: float = 0.2
@export var deflector_bounce: float = 0.55
@export var rotor_friction: float = 0.5
@export var rotor_bounce: float = 0.3
@export var fret_friction: float = 0.3
@export var fret_bounce: float = 0.45

@export_group("Settle")
## Max speed of the ball relative to the rotor (m/s) that counts as "at rest".
@export var settle_speed: float = 0.04
## The ball must stay at rest in the pocket ring this long (s) before we read the number.
@export var settle_hold_time: float = 0.5
## Settle aid: below this relative speed (m/s), while the ball already rests on a pocket
## floor (centre within 2 mm of rest height, well below the fret tops), extra damping pulls its velocity towards the rotor's.
## It cannot move the ball to another pocket: it only removes relative motion.
@export var settle_aid_speed: float = 0.25
## Strength of the settle aid (1/s).
@export var settle_aid_rate: float = 2.0
## Fail-safe: give up after this many seconds in play. Emits ball_timed_out, no number.
@export var time_limit: float = 45.0

@export_group("Build")
## Angular segments of the track/wall collider. More = smoother wall, less energy loss.
@export var track_collision_segments: int = 720
@export var visual_segments: int = 192

var settled_number: int = -1
var last_spin_time: float = 0.0
var timed_out: bool = false

var _rotor: AnimatableBody3D
var _frets: AnimatableBody3D
var _ball: WheelBall
var _rotor_angle: float = 0.0
var _time: float = 0.0  # rotor variation clock (restart_rotor resets it)
var _clock: float = 0.0  # monotonic clock for rate limiting
var _in_play: bool = false
var _spin_time: float = 0.0
var _hold: float = 0.0
var _labels: Dictionary = {}
var _highlight: MeshInstance3D
var _highlight_mat: StandardMaterial3D
var _highlight_number: int = -1
var _last_any_hit: float = -1.0
var _last_kind_hit: Dictionary = {}

var _mats: Dictionary = {}


func _ready() -> void:
	_build_materials()
	_build_bowl()
	_build_rotor()
	_build_ball()
	_build_highlight()
	_apply_rotor_transform()
	reset_ball()


# --- Public API ---------------------------------------------------------------

## Release the ball on the track with a random speed (from rng). Ignored while a ball is in play.
func launch_ball(rng: RandomNumberGenerator = null) -> void:
	if _in_play:
		return
	if rng == null:
		rng = RandomNumberGenerator.new()
		rng.randomize()
	var a := launch_angle + rng.randf_range(-launch_angle_jitter, launch_angle_jitter)
	var speed := rng.randf_range(launch_speed_min, launch_speed_max)
	var r := WheelGeometry.WALL_R - WheelGeometry.BALL_RADIUS - 0.0005
	var y := WheelGeometry.track_floor_y(r) + WheelGeometry.BALL_RADIUS * 1.03 + 0.0005
	var local_pos := PocketMath.point_at(a, r, y)
	# Counter-clockwise tangent at angle a is (-sin a, 0, -cos a). The ball goes the other way.
	var ccw := Vector3(-sin(a), 0.0, -cos(a))
	var dir := -ccw if rotor_speed >= 0.0 else ccw
	var v_world := global_basis * (dir * speed)
	settled_number = -1
	timed_out = false
	_spin_time = 0.0
	_hold = 0.0
	var w_world := Vector3.ZERO
	# Start it rolling on the track floor (no slip) so it does not lose speed to skidding.
	if launch_rolling:
		w_world = global_basis.y.cross(v_world) / WheelGeometry.BALL_RADIUS
	# Unfreeze first: changing freeze afterwards would clear the launch velocity.
	_ball.freeze = false
	_place_ball(Transform3D(Basis.IDENTITY, to_global(local_pos)), v_world, w_world)
	_in_play = true
	ball_launched.emit()


func is_ball_in_play() -> bool:
	return _in_play


## Park the ball, frozen, on the track at the release point. Cancels a spin in play.
func reset_ball() -> void:
	_in_play = false
	_hold = 0.0
	_spin_time = 0.0
	var r := WheelGeometry.WALL_R - WheelGeometry.BALL_RADIUS - 0.0005
	var y := WheelGeometry.track_floor_y(r) + WheelGeometry.BALL_RADIUS * 1.03
	_ball.freeze = true
	_place_ball(Transform3D(Basis.IDENTITY, to_global(PocketMath.point_at(launch_angle, r, y))),
			Vector3.ZERO, Vector3.ZERO)


## Teleport the ball. Setting global_transform on an active RigidBody3D is not reliable:
## in release exports the physics engine kept the old pose, so the ball stayed in its last
## pocket and every spin gave the same number. Writing the state on the physics server
## applies at once; the node properties keep the scene side in step.
func _place_ball(tf: Transform3D, linear: Vector3, angular: Vector3) -> void:
	var rid := _ball.get_rid()
	PhysicsServer3D.body_set_state(rid, PhysicsServer3D.BODY_STATE_TRANSFORM, tf)
	PhysicsServer3D.body_set_state(rid, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY, linear)
	PhysicsServer3D.body_set_state(rid, PhysicsServer3D.BODY_STATE_ANGULAR_VELOCITY, angular)
	_ball.global_transform = tf
	_ball.linear_velocity = linear
	_ball.angular_velocity = angular


func highlight_pocket(number: int) -> void:
	clear_highlight()
	if not WheelLayout.is_valid(number):
		return
	_highlight_number = number
	_highlight.transform = Transform3D(Basis(Vector3.UP, PocketMath.angle_for_number(number)), Vector3.ZERO)
	_highlight.visible = true
	var label: Label3D = _labels[number]
	label.modulate = Color(1, 1, 1)
	label.outline_modulate = Color(0.08, 0.05, 0.0, 1.0)


func clear_highlight() -> void:
	if _highlight_number >= 0:
		var label: Label3D = _labels[_highlight_number]
		label.modulate = Color(1, 1, 1)
		label.outline_modulate = Color(0, 0, 0, 0)
	_highlight_number = -1
	if _highlight:
		_highlight.visible = false


## World position of the centre of a pocket (ball-rest height). Moves with the rotor.
func get_pocket_world_position(number: int) -> Vector3:
	var local := PocketMath.point_at(PocketMath.angle_for_number(number), WheelGeometry.pocket_center_r(),
			WheelGeometry.pocket_rest_y())
	return global_transform * (_rotor_local_transform() * local)


# --- Extra helpers (tests, tools, camera) -----------------------------------------

func get_ball() -> RigidBody3D:
	return _ball


func get_rotor() -> AnimatableBody3D:
	return _rotor


## Current rotor angle (rad, counter-clockwise from above).
func get_rotor_angle() -> float:
	return _rotor_angle


## Set the rotor angle (for repeatable tests and tools).
func set_rotor_angle(angle: float) -> void:
	_rotor_angle = angle
	_apply_rotor_transform()


## Put the rotor at an angle and restart its speed-variation clock, so a following
## launch_ball(rng) with the same seed starts from the same state (repeatability checks).
func restart_rotor(angle: float = 0.0) -> void:
	_time = 0.0
	set_rotor_angle(angle)


func get_spin_time() -> float:
	return _spin_time


## Ball centre in rotor-local coordinates.
## Uses the rotor angle the physics engine is using (AnimatableBody3D with
## sync_to_physics may report its node transform one step late).
func get_ball_rotor_local_position() -> Vector3:
	return _rotor_local_transform().affine_inverse() * to_local(_ball.global_position)


func _rotor_local_transform() -> Transform3D:
	return Transform3D(Basis(Vector3.UP, _rotor_angle), Vector3.ZERO)


## Current rotor speed (rad/s) including the gentle variation.
func get_current_rotor_speed() -> float:
	if rotor_speed_variation <= 0.0 or rotor_variation_period <= 0.0:
		return rotor_speed
	return rotor_speed * (1.0 + rotor_speed_variation * sin(TAU * _time / rotor_variation_period))


# --- Simulation -----------------------------------------------------------------

func _physics_process(delta: float) -> void:
	var w := get_current_rotor_speed()
	if _in_play:
		_update_ball(delta, w)
	_time += delta
	_clock += delta
	_rotor_angle = wrapf(_rotor_angle + w * delta, 0.0, TAU)
	_apply_rotor_transform()


func _process(_delta: float) -> void:
	if _highlight and _highlight.visible:
		var t := Time.get_ticks_msec() / 1000.0
		_highlight_mat.emission_energy_multiplier = 1.6 + 1.0 * sin(t * 5.0)


func _apply_rotor_transform() -> void:
	var tf := _rotor_local_transform()
	if _rotor:
		_rotor.transform = tf
	if _frets:
		_frets.transform = tf


func _update_ball(delta: float, w: float) -> void:
	_spin_time += delta
	var lp := to_local(_ball.global_position)
	var inv := global_basis.inverse()
	var lv := inv * _ball.linear_velocity
	var rel := lv - PocketMath.rotor_velocity_at(lp, w)
	var deep := PocketMath.is_in_pocket_ring(lp, WheelGeometry.POCKET_INNER_R, WheelGeometry.POCKET_OUTER_R,
			WheelGeometry.FRET_TOP_Y)
	var rel_speed := rel.length()
	# The aid needs the ball resting on a pocket floor (centre at most 2 mm above rest height),
	# far below the fret tops, so it can only act once the pocket is already decided.
	var on_floor := PocketMath.is_in_pocket_ring(lp, WheelGeometry.POCKET_INNER_R, WheelGeometry.POCKET_OUTER_R,
			WheelGeometry.pocket_rest_y() + 0.002)
	if on_floor and rel_speed < settle_aid_speed:
		# Settle aid: damp only the motion relative to the rotor. It never adds motion.
		_ball.apply_central_force(global_basis * (-rel * settle_aid_rate * _ball.mass))
		var rel_ang := (inv * _ball.angular_velocity) - Vector3(0.0, w, 0.0)
		var inertia := 0.4 * _ball.mass * WheelGeometry.BALL_RADIUS * WheelGeometry.BALL_RADIUS
		_ball.apply_torque(global_basis * (-rel_ang * settle_aid_rate * inertia))
	if deep and rel_speed < settle_speed:
		_hold += delta
	else:
		_hold = 0.0
	if _hold >= settle_hold_time:
		var number := PocketMath.number_for_local_point(get_ball_rotor_local_position())
		_in_play = false
		settled_number = number
		last_spin_time = _spin_time
		ball_settled.emit(number)
	elif _spin_time >= time_limit:
		_in_play = false
		timed_out = true
		last_spin_time = _spin_time
		ball_timed_out.emit()


func _on_ball_impacted(kind: String, strength: float) -> void:
	# Rate limit for audio: at most one event per 30 ms, and per kind one per 60 ms.
	if _clock - _last_any_hit < 0.03:
		return
	if _clock - float(_last_kind_hit.get(kind, -1.0)) < 0.06:
		return
	_last_any_hit = _clock
	_last_kind_hit[kind] = _clock
	ball_collided.emit(kind, strength)


# --- Build ----------------------------------------------------------------------

func _build_materials() -> void:
	var grain := NoiseTexture2D.new()
	var noise := FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_PERLIN
	noise.frequency = 0.02
	noise.fractal_octaves = 3
	grain.noise = noise
	grain.width = 512
	grain.height = 64
	grain.seamless = true
	var ramp := Gradient.new()
	ramp.set_color(0, Color(0.72, 0.72, 0.72))
	ramp.set_color(1, Color(1, 1, 1))
	grain.color_ramp = ramp

	_mats["wood"] = _mat(Color(0.36, 0.14, 0.06), 0.0, 0.35, grain)
	_mats["track"] = _mat(Color(0.62, 0.38, 0.18), 0.0, 0.22, grain)
	_mats["apron"] = _mat(Color(0.17, 0.08, 0.04), 0.0, 0.25, grain)
	_mats["cone"] = _mat(Color(0.42, 0.19, 0.07), 0.0, 0.3, grain)
	_mats["chrome"] = _mat(Color(0.9, 0.9, 0.93), 0.85, 0.22, null)
	_mats["gold"] = _mat(Color(0.92, 0.74, 0.38), 1.0, 0.2, null)
	# Diamonds: brass that still reads as gold under dim reflections.
	var dia := _mat(Color(0.95, 0.78, 0.4), 0.55, 0.3, null)
	dia.emission_enabled = true
	dia.emission = Color(0.35, 0.26, 0.1)
	dia.emission_energy_multiplier = 0.4
	_mats["diamond"] = dia
	_mats["dark"] = _mat(Color(0.05, 0.04, 0.03), 0.0, 0.7, null)
	var vc := _mat(Color.WHITE, 0.0, 0.4, null)
	vc.vertex_color_use_as_albedo = true
	vc.vertex_color_is_srgb = true
	_mats["vcol"] = vc
	var ball := _mat(Color(0.97, 0.97, 0.95), 0.0, 0.12, null)
	ball.clearcoat_enabled = true
	_mats["ball"] = ball
	for k in ["wood", "track", "apron", "cone"]:
		var m: StandardMaterial3D = _mats[k]
		m.clearcoat_enabled = true
		m.clearcoat_roughness = 0.15
		m.uv1_triplanar = true
		m.uv1_scale = Vector3(1.5, 30.0, 1.5)


func _mat(color: Color, metallic: float, roughness: float, tex: Texture2D) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.metallic = metallic
	m.roughness = roughness
	if tex:
		m.albedo_texture = tex
	return m


func _phys(friction: float, bounce: float) -> PhysicsMaterial:
	var pm := PhysicsMaterial.new()
	pm.friction = friction
	pm.bounce = bounce
	return pm


func _mesh_node(parent: Node3D, node_name: String, mesh: Mesh, mat_key: String) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.name = node_name
	mi.mesh = mesh
	mi.material_override = _mats[mat_key]
	parent.add_child(mi)
	return mi


func _static_body(node_name: String, kind: String, friction: float, bounce: float) -> StaticBody3D:
	var b := StaticBody3D.new()
	b.name = node_name
	b.set_meta("kind", kind)
	b.physics_material_override = _phys(friction, bounce)
	add_child(b)
	return b


func _shape_node(parent: Node3D, node_name: String, shape: Shape3D, kind: String) -> CollisionShape3D:
	var cs := CollisionShape3D.new()
	cs.name = node_name
	cs.shape = shape
	cs.set_meta("kind", kind)
	parent.add_child(cs)
	return cs


func _build_bowl() -> void:
	var segs := visual_segments
	var shell := Node3D.new()
	shell.name = "BowlVisual"
	add_child(shell)
	_mesh_node(shell, "OuterShell", WheelGeometry.revolve_mesh(WheelGeometry.outer_shell_profile(), segs), "wood")
	_mesh_node(shell, "Wall", WheelGeometry.revolve_mesh(WheelGeometry.wall_profile(), segs), "wood")
	_mesh_node(shell, "Track", WheelGeometry.revolve_mesh(WheelGeometry.track_profile(), segs), "track")
	_mesh_node(shell, "Apron", WheelGeometry.revolve_mesh(WheelGeometry.apron_profile(), segs), "apron")
	# Thin brass line where the track meets the apron.
	var trim := PackedVector2Array([Vector2(WheelGeometry.TRACK_INNER_R + 0.002, WheelGeometry.track_floor_y(WheelGeometry.TRACK_INNER_R + 0.002) + 0.0003),
		Vector2(WheelGeometry.TRACK_INNER_R - 0.002, WheelGeometry.apron_y(WheelGeometry.TRACK_INNER_R - 0.002) + 0.0003)])
	_mesh_node(shell, "TrackTrim", WheelGeometry.revolve_mesh(trim, segs), "gold")

	# One collider for wall + track + apron, so the ball never meets a seam between bodies.
	var bowl_prof := WheelGeometry.wall_profile()
	bowl_prof.append_array(WheelGeometry.track_profile().slice(1))
	bowl_prof.append_array(WheelGeometry.apron_profile().slice(1))
	var bowl := _static_body("Bowl", "rim", bowl_friction, bowl_bounce)
	_shape_node(bowl, "Shape", WheelGeometry.revolve_shape(bowl_prof, track_collision_segments), "rim")
	var lid := _static_body("Lid", "rim", 0.1, 0.2)
	_shape_node(lid, "Shape", WheelGeometry.revolve_shape(WheelGeometry.lid_profile(), 96), "rim")

	# Eight diamond deflectors on the apron, alternating long-radial and long-tangential.
	var defl := _static_body("Deflectors", "deflector", deflector_friction, deflector_bounce)
	var d2 := Vector2(WheelGeometry.APRON_INNER_R - WheelGeometry.TRACK_INNER_R,
			WheelGeometry.APRON_INNER_Y - WheelGeometry.TRACK_INNER_Y).normalized()
	var n2 := Vector2(d2.y, -d2.x)
	for k in WheelGeometry.DEFLECTOR_COUNT:
		var a := TAU * (k + 0.5) / WheelGeometry.DEFLECTOR_COUNT
		var down := Vector3(d2.x * cos(a), d2.y, -d2.x * sin(a)).normalized()
		var nrm := Vector3(n2.x * cos(a), n2.y, -n2.x * sin(a)).normalized()
		var tangent := Vector3(-sin(a), 0.0, -cos(a))
		var x_axis := down if k % 2 == 0 else tangent
		var basis := Basis(x_axis, nrm, x_axis.cross(nrm))
		var pos := PocketMath.point_at(a, WheelGeometry.DEFLECTOR_R, WheelGeometry.apron_y(WheelGeometry.DEFLECTOR_R))
		var pts := WheelGeometry.deflector_points(0.017 if k % 2 == 0 else 0.015, 0.0065, 0.008)
		var shape := ConvexPolygonShape3D.new()
		shape.points = pts
		var cs := _shape_node(defl, "Diamond_%d" % k, shape, "deflector")
		cs.transform = Transform3D(basis, pos)
		var mi := _mesh_node(shell, "Diamond_%d" % k, WheelGeometry.deflector_mesh(pts), "diamond")
		mi.transform = Transform3D(basis, pos)


func _build_rotor() -> void:
	_rotor = AnimatableBody3D.new()
	_rotor.name = "Rotor"
	_rotor.sync_to_physics = true
	_rotor.set_meta("kind", "pocket")
	_rotor.physics_material_override = _phys(rotor_friction, rotor_bounce)
	add_child(_rotor)
	_frets = AnimatableBody3D.new()
	_frets.name = "Frets"
	_frets.sync_to_physics = true
	_frets.set_meta("kind", "fret")
	_frets.physics_material_override = _phys(fret_friction, fret_bounce)
	add_child(_frets)

	var step := PocketMath.STEP
	# Collider: number ring + pockets + cone, segments aligned to the pocket edges.
	var prof := WheelGeometry.number_ring_profile()
	prof.append_array(WheelGeometry.pocket_profile().slice(1))
	prof.append_array(WheelGeometry.cone_profile().slice(1))
	var a_start := PocketMath.FIRST_POCKET_ANGLE + step * 0.5
	var shape := ConcavePolygonShape3D.new()
	shape.backface_collision = false
	shape.set_faces(WheelGeometry.revolve_faces(prof, a_start, a_start - TAU, WheelLayout.POCKET_COUNT * 6))
	_shape_node(_rotor, "Surface", shape, "pocket")
	var turret_shape := CylinderShape3D.new()
	turret_shape.radius = 0.03
	turret_shape.height = 0.08
	var tcs := _shape_node(_rotor, "Turret", turret_shape, "pocket")
	tcs.position = Vector3(0, WheelGeometry.CONE_TOP_Y + 0.04, 0)

	# Coloured number ring and pockets (one mesh, vertex colours).
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var pockets := Node3D.new()
	pockets.name = "Pockets"
	_rotor.add_child(pockets)
	var numbers := Node3D.new()
	numbers.name = "Numbers"
	_rotor.add_child(numbers)
	for i in WheelLayout.POCKET_COUNT:
		var n := WheelLayout.number_at(i)
		var c := _pocket_color(n)
		var a := PocketMath.angle_for_index(i)
		WheelGeometry.add_revolve(st, WheelGeometry.number_ring_profile(), a + step * 0.5, a - step * 0.5, 6, c)
		WheelGeometry.add_revolve(st, WheelGeometry.pocket_profile(), a + step * 0.5, a - step * 0.5, 6, c.darkened(0.15))
		var marker := Marker3D.new()
		marker.name = "Pocket_%s" % WheelLayout.label(n)
		marker.set_meta("number", n)
		marker.set_meta("index", i)
		marker.position = PocketMath.point_at(a, WheelGeometry.pocket_center_r(), WheelGeometry.pocket_rest_y())
		pockets.add_child(marker)
		numbers.add_child(_make_label(n, a))
		# Fret on the clockwise edge of this pocket (between index i and i + 1).
		var fa := a - step * 0.5
		var box := BoxShape3D.new()
		var flen := WheelGeometry.POCKET_OUTER_R - WheelGeometry.POCKET_INNER_R
		var fh := WheelGeometry.FRET_TOP_Y - WheelGeometry.POCKET_FLOOR_Y
		box.size = Vector3(flen, fh, WheelGeometry.FRET_THICKNESS)
		var ftf := Transform3D(Basis(Vector3.UP, fa),
				PocketMath.point_at(fa, WheelGeometry.pocket_center_r(), WheelGeometry.POCKET_FLOOR_Y + fh * 0.5))
		var fcs := _shape_node(_frets, "Fret_%d" % i, box, "fret")
		fcs.transform = ftf
		var bm := BoxMesh.new()
		bm.size = box.size + Vector3(0.0, 0.001, 0.0)
		var fmi := _mesh_node(_frets, "FretMesh_%d" % i, bm, "chrome")
		fmi.transform = ftf
	_mesh_node(_rotor, "PocketRing", st.commit(), "vcol")
	_mesh_node(_rotor, "Side", WheelGeometry.revolve_mesh(WheelGeometry.rotor_side_profile(), visual_segments), "gold")
	_mesh_node(_rotor, "Cone", WheelGeometry.revolve_mesh(WheelGeometry.cone_profile(), visual_segments), "cone")
	# Thin chrome rings at the pocket-ring edges.
	var outer_trim := PackedVector2Array([Vector2(WheelGeometry.ROTOR_R + 0.0005, WheelGeometry.ROTOR_TOP_Y + 0.0012),
		Vector2(WheelGeometry.ROTOR_R - 0.003, WheelGeometry.ring_y(WheelGeometry.ROTOR_R - 0.003) + 0.0012)])
	_mesh_node(_rotor, "OuterTrim", WheelGeometry.revolve_mesh(outer_trim, visual_segments), "chrome")
	_build_turret()


func _build_turret() -> void:
	var t := Node3D.new()
	t.name = "Turret"
	t.position = Vector3(0, WheelGeometry.CONE_TOP_Y, 0)
	_rotor.add_child(t)
	var base := CylinderMesh.new()
	base.top_radius = 0.018
	base.bottom_radius = 0.034
	base.height = 0.022
	_mesh_node(t, "Base", base, "chrome").position = Vector3(0, 0.011, 0)
	var stem := CylinderMesh.new()
	stem.top_radius = 0.007
	stem.bottom_radius = 0.009
	stem.height = 0.05
	_mesh_node(t, "Stem", stem, "chrome").position = Vector3(0, 0.045, 0)
	var top := SphereMesh.new()
	top.radius = 0.011
	top.height = 0.022
	_mesh_node(t, "Knob", top, "chrome").position = Vector3(0, 0.074, 0)
	for k in 2:
		var arm := CylinderMesh.new()
		arm.top_radius = 0.004
		arm.bottom_radius = 0.004
		arm.height = 0.075
		var a := k * PI / 2.0
		var mi := _mesh_node(t, "Arm_%d" % k, arm, "chrome")
		mi.transform = Transform3D(Basis(Vector3.UP, a) * Basis(Vector3.FORWARD, PI / 2.0), Vector3(0, 0.06, 0))
	for k in 4:
		var knob := SphereMesh.new()
		knob.radius = 0.007
		knob.height = 0.014
		var a := k * PI / 2.0
		_mesh_node(t, "ArmKnob_%d" % k, knob, "chrome").position = PocketMath.point_at(a, 0.0375, 0.06)


func _make_label(n: int, a: float) -> Label3D:
	var lab := Label3D.new()
	lab.name = "Label_%s" % WheelLayout.label(n)
	lab.text = WheelLayout.label(n)
	lab.font_size = 64
	lab.pixel_size = 0.00026
	lab.outline_size = 8
	lab.outline_modulate = Color(0, 0, 0, 0)
	lab.alpha_cut = Label3D.ALPHA_CUT_DISCARD
	lab.double_sided = false
	var r := (WheelGeometry.ROTOR_R + WheelGeometry.POCKET_OUTER_R) * 0.5
	var d := Vector2(WheelGeometry.POCKET_OUTER_R - WheelGeometry.ROTOR_R,
			WheelGeometry.RING_INNER_Y - WheelGeometry.ROTOR_TOP_Y)
	var n2 := Vector2(d.y, -d.x).normalized()
	var z_axis := Vector3(n2.x * cos(a), n2.y, -n2.x * sin(a)).normalized()
	var x_axis := Vector3(-sin(a), 0.0, -cos(a))
	var y_axis := z_axis.cross(x_axis).normalized()
	lab.transform = Transform3D(Basis(x_axis, y_axis, z_axis),
			PocketMath.point_at(a, r, WheelGeometry.ring_y(r)) + z_axis * 0.0008)
	_labels[n] = lab
	return lab


func _pocket_color(n: int) -> Color:
	match WheelLayout.color_of(n):
		WheelLayout.PocketColor.GREEN:
			return Color(0.02, 0.42, 0.16)
		WheelLayout.PocketColor.RED:
			return Color(0.72, 0.04, 0.04)
		_:
			return Color(0.035, 0.035, 0.04)


func _build_ball() -> void:
	_ball = WheelBall.new()
	_ball.name = "Ball"
	_ball.mass = ball_mass
	_ball.continuous_cd = ball_ccd
	_ball.can_sleep = false
	_ball.freeze_mode = RigidBody3D.FREEZE_MODE_STATIC
	_ball.linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	_ball.linear_damp = ball_linear_damp
	_ball.angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	_ball.angular_damp = ball_angular_damp
	_ball.physics_material_override = _phys(ball_friction, ball_bounce)
	var cs := CollisionShape3D.new()
	var sphere := SphereShape3D.new()
	sphere.radius = WheelGeometry.BALL_RADIUS
	cs.shape = sphere
	_ball.add_child(cs)
	var sm := SphereMesh.new()
	sm.radius = WheelGeometry.BALL_RADIUS
	sm.height = WheelGeometry.BALL_RADIUS * 2.0
	var mi := MeshInstance3D.new()
	mi.mesh = sm
	mi.material_override = _mats["ball"]
	_ball.add_child(mi)
	add_child(_ball)
	_ball.impacted.connect(_on_ball_impacted)


func _build_highlight() -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var half := PocketMath.STEP * 0.5
	var lift := PackedVector2Array()
	for p in WheelGeometry.number_ring_profile():
		lift.append(p + Vector2(0, 0.0012))
	var pocket := PackedVector2Array()
	for p in WheelGeometry.pocket_profile():
		pocket.append(Vector2(clampf(p.x, WheelGeometry.POCKET_INNER_R + 0.0008, WheelGeometry.POCKET_OUTER_R - 0.0008), p.y + 0.0008))
	WheelGeometry.add_revolve(st, lift, half * 0.92, -half * 0.92, 6, Color.WHITE)
	WheelGeometry.add_revolve(st, pocket, half * 0.92, -half * 0.92, 6, Color.WHITE)
	_highlight_mat = StandardMaterial3D.new()
	_highlight_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_highlight_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_highlight_mat.albedo_color = Color(1.0, 0.85, 0.3, 0.55)
	_highlight_mat.emission_enabled = true
	_highlight_mat.emission = Color(1.0, 0.8, 0.25)
	_highlight_mat.emission_energy_multiplier = 2.0
	_highlight_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	_highlight = MeshInstance3D.new()
	_highlight.name = "Highlight"
	_highlight.mesh = st.commit()
	_highlight.material_override = _highlight_mat
	_highlight.visible = false
	_highlight.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_rotor.add_child(_highlight)
