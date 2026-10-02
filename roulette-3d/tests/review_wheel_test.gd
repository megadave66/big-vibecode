extends GdUnitTestSuite
## Reviewer checks for section 2 (wheel). Independent of PocketMath where possible:
## positions come from the built nodes (fret colliders, labels, markers) in world space.

const WHEEL_SCENE := preload("res://scenes/wheel/wheel.tscn")


func _wheel(pos: Vector3, speed: float) -> RouletteWheel:
	var w: RouletteWheel = WHEEL_SCENE.instantiate()
	w.rotor_speed = speed
	w.rotor_speed_variation = 0.0
	w.position = pos
	add_child(w)
	return auto_free(w)


## Clockwise-from-above angle of a world point about the wheel's axis (independent formula).
func _cw_angle(w: Node3D, p: Vector3) -> float:
	var l := w.to_local(p)
	# From above (+Y looking down), -Z is "up" and +X is "right". Clockwise from -Z.
	return fposmod(atan2(l.x, -l.z), TAU)


func _label_number(t: String) -> int:
	return 37 if t == "00" else int(t)


func _labels_by_cw(w: RouletteWheel) -> Array:
	var items: Array = []
	for lab in w.get_rotor().get_node("Numbers").get_children():
		items.append([_cw_angle(w, (lab as Node3D).global_position), _label_number((lab as Label3D).text)])
	items.sort_custom(func(a, b): return a[0] < b[0])
	return items


func _rotate_to_zero_first(nums: Array) -> Array:
	var k := nums.find(0)
	return nums.slice(k) + nums.slice(0, k)


func test_world_label_order_is_pocket_order_clockwise_at_several_rotor_angles() -> void:
	var w := _wheel(Vector3(5, 0, 5), 0.0)
	for ang in [0.0, 0.7, 2.9, 4.4, 6.1]:
		w.set_rotor_angle(ang)
		await get_tree().physics_frame
		var nums: Array = []
		for it in _labels_by_cw(w):
			nums.append(it[1])
		assert_array(_rotate_to_zero_first(nums)).is_equal(Array(WheelLayout.POCKET_ORDER))


func test_each_label_sits_between_its_two_frets_and_colours_match() -> void:
	var w := _wheel(Vector3(-5, 0, 5), 0.0)
	w.set_rotor_angle(1.234)
	await get_tree().physics_frame
	var frets: Array = []
	for c in w.get_node("Frets").get_children():
		if c is CollisionShape3D:
			frets.append(_cw_angle(w, (c as Node3D).global_position))
	frets.sort()
	assert_int(frets.size()).is_equal(38)
	# Every gap between neighbouring frets holds exactly one label.
	var labels := _labels_by_cw(w)
	for k in 38:
		var lo: float = frets[k]
		var hi: float = frets[(k + 1) % 38] + (TAU if k == 37 else 0.0)
		var inside := 0
		for it in labels:
			var a: float = it[0]
			if a < lo:
				a += TAU
			if a > lo and a < hi:
				inside += 1
		assert_int(inside).is_equal(1)
	# Pocket ring vertex colours: triangle centroids coloured by the label in the same gap.
	var mesh := (w.get_rotor().get_node("PocketRing") as MeshInstance3D).mesh
	var arr := mesh.surface_get_arrays(0)
	var verts: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
	var cols: PackedColorArray = arr[Mesh.ARRAY_COLOR]
	var rotor := w.get_rotor()
	assert_int(verts.size()).is_greater(38 * 30)
	assert_int(cols.size()).is_equal(verts.size())
	var bad := 0
	for t in range(0, verts.size(), 3):
		var cen := rotor.global_transform * ((verts[t] + verts[t + 1] + verts[t + 2]) / 3.0)
		var a := _cw_angle(w, cen)
		# nearest label by angle
		var best := -1
		var bd := 99.0
		for it in labels:
			var d := absf(wrapf(a - float(it[0]), -PI, PI))
			if d < bd:
				bd = d
				best = it[1]
		var c := cols[t]
		var want := WheelLayout.color_of(best)
		var got: int
		if c.g > c.r and c.g > c.b:
			got = WheelLayout.PocketColor.GREEN
		elif c.r > 0.3:
			got = WheelLayout.PocketColor.RED
		else:
			got = WheelLayout.PocketColor.BLACK
		if got != want:
			bad += 1
	assert_int(bad).is_equal(0)


func test_rotor_turns_ccw_and_ball_launches_cw() -> void:
	var w := _wheel(Vector3(5, 0, -5), 2.2)
	await get_tree().physics_frame
	var m := w.get_rotor().get_node("Pockets/Pocket_0") as Node3D
	var p0 := w.to_local(m.global_position)
	for i in 30:
		await get_tree().physics_frame
	var p1 := w.to_local(m.global_position)
	assert_float(Vector3(p0.x, 0, p0.z).cross(Vector3(p1.x, 0, p1.z)).y).is_greater(0.0)  # CCW from above
	var rng := RandomNumberGenerator.new()
	rng.seed = 3
	w.launch_ball(rng)
	await get_tree().physics_frame
	var b := w.get_ball()
	var lp := w.to_local(b.global_position)
	var lv := w.global_basis.inverse() * b.linear_velocity
	assert_float(Vector3(lp.x, 0, lp.z).cross(Vector3(lv.x, 0, lv.z)).y).is_less(0.0)  # CW from above
	# The physics rotor transform must agree with the angle used to read the pocket.
	var st := PhysicsServer3D.body_get_state(w.get_rotor().get_rid(), PhysicsServer3D.BODY_STATE_TRANSFORM) as Transform3D
	var phys_angle := (w.global_basis.inverse() * st.basis).get_euler().y
	var diff := absf(wrapf(phys_angle - w.get_rotor_angle(), -PI, PI))
	print("rotor angle: engine %.4f, script %.4f, diff %.4f rad (pocket step %.4f)" % [phys_angle, w.get_rotor_angle(), diff, PocketMath.STEP])
	assert_float(diff).is_less(PocketMath.STEP * 0.1)


## Place a still ball in every pocket (centre and both sides), using only fret collider
## positions to pick the spot, and the label between those frets as the expected number.
func _drop_test(speed: float, rotor_angles: Array, offsets: Array) -> void:
	var wheels: Array[RouletteWheel] = []
	for i in 38:
		wheels.append(_wheel(Vector3((i % 7) * 2.0, 0, 20.0 + (i / 7) * 2.0), speed))
	await get_tree().physics_frame
	var fails := PackedStringArray()
	var total := 0
	for ang in rotor_angles:
		for off in offsets:
			var got := {}
			var want := {}
			for i in 38:
				var w := wheels[i]
				w.reset_ball()
				w.set_rotor_angle(ang)
			await get_tree().physics_frame
			for i in 38:
				var w := wheels[i]
				var frets: Array = []
				for c in w.get_node("Frets").get_children():
					if c is CollisionShape3D:
						frets.append(_cw_angle(w, (c as Node3D).global_position))
				frets.sort()
				var lo: float = frets[i]
				var hi: float = frets[(i + 1) % 38] + (TAU if i == 37 else 0.0)
				var mid := (lo + hi) * 0.5 + float(off) * (hi - lo)
				# Expected: the label in this fret gap.
				for it in _labels_by_cw(w):
					var a: float = it[0]
					if a < lo:
						a += TAU
					if a > lo and a < hi:
						want[i] = it[1]
				var r := WheelGeometry.pocket_center_r()
				var local := Vector3(r * sin(mid), WheelGeometry.pocket_rest_y() + 0.002, -r * cos(mid))
				w.launch_ball()
				var b := w.get_ball()
				b.global_transform = Transform3D(Basis.IDENTITY, w.to_global(local))
				var v := Vector3(0, w.rotor_speed, 0).cross(Vector3(local.x, 0, local.z))
				b.linear_velocity = w.global_basis * v
				b.angular_velocity = w.global_basis * Vector3(0, w.rotor_speed, 0)
				w.ball_settled.connect(func(n: int) -> void: got[i] = n, CONNECT_ONE_SHOT)
			var t := 0
			while got.size() < 38 and t < 360:
				await get_tree().physics_frame
				t += 1
			for i in 38:
				total += 1
				if not got.has(i) or got[i] != want.get(i, -2):
					fails.append("speed=%.1f rotor=%.2f off=%.2f pocket=%s got=%s" % [speed, ang, off,
						WheelLayout.label(want.get(i, -2)), str(got.get(i, "none"))])
	print("static drop: %d placements, %d mismatches" % [total, fails.size()])
	for f in fails:
		print("  ", f)
	assert_int(fails.size()).is_equal(0)


func test_static_ball_in_each_pocket_frozen_rotor() -> void:
	await _drop_test(0.0, [0.0, 1.3, 3.05, 5.5], [0.0, -0.18, 0.18])


func test_static_ball_in_each_pocket_spinning_rotor() -> void:
	await _drop_test(2.2, [0.4, 2.6], [0.0, -0.18, 0.18])
