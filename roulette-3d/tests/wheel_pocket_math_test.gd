extends GdUnitTestSuite
## Pocket maths (angle -> number) and the pocket ring order of the built wheel scene.

const WHEEL_SCENE := preload("res://scenes/wheel/wheel.tscn")
const EPS := 1e-4


func test_every_pocket_centre_maps_back_to_its_number() -> void:
	for i in 38:
		var n := WheelLayout.number_at(i)
		var a := PocketMath.angle_for_index(i)
		assert_int(PocketMath.index_for_angle(a)).is_equal(i)
		assert_int(PocketMath.number_for_angle(a)).is_equal(n)
		assert_float(PocketMath.angle_for_number(n)).is_equal_approx(a, EPS)


func test_pocket_edges_stay_inside_their_pocket() -> void:
	var half := PocketMath.STEP * 0.5
	for i in 38:
		var a := PocketMath.angle_for_index(i)
		assert_int(PocketMath.index_for_angle(a + half - EPS)).is_equal(i)
		assert_int(PocketMath.index_for_angle(a - half + EPS)).is_equal(i)
		# Just past the clockwise edge is the next pocket in POCKET_ORDER.
		assert_int(PocketMath.index_for_angle(a - half - EPS)).is_equal((i + 1) % 38)
		# Just past the counter-clockwise edge is the previous one.
		assert_int(PocketMath.index_for_angle(a + half + EPS)).is_equal((i + 37) % 38)


func test_zero_is_at_minus_z_and_order_runs_clockwise() -> void:
	assert_int(PocketMath.number_for_local_point(Vector3(0, 0, -1))).is_equal(0)
	# Clockwise from above = decreasing counter-clockwise angle = next number in POCKET_ORDER.
	assert_int(PocketMath.number_for_angle(PI / 2.0 - PocketMath.STEP)).is_equal(28)
	assert_int(PocketMath.number_for_angle(PI / 2.0 + PocketMath.STEP)).is_equal(2)
	# 00 sits opposite 0.
	assert_int(PocketMath.number_for_local_point(Vector3(0, 0, 1))).is_equal(37)


func test_angle_wraps_around() -> void:
	for i in 38:
		var a := PocketMath.angle_for_index(i)
		for k in [-3, -1, 1, 2, 7]:
			assert_int(PocketMath.index_for_angle(a + k * TAU)).is_equal(i)
	# The atan2 seam (-PI/PI) is at -X. Angles just either side must agree with their wrapped twin.
	assert_int(PocketMath.number_for_angle(PI - 0.001)).is_equal(PocketMath.number_for_angle(-PI - 0.001))
	assert_int(PocketMath.number_for_angle(-PI + 0.001)).is_equal(PocketMath.number_for_angle(PI + 0.001))


func test_rotor_rotation_offsets() -> void:
	for rotor in [0.0, 0.37, 1.0, PI, 4.2, -2.5, 25.0]:
		for i in 38:
			var n := WheelLayout.number_at(i)
			var world := PocketMath.angle_for_number(n) + float(rotor)
			assert_int(PocketMath.number_for_world_angle(world, rotor)).is_equal(n)
	# Rotating the rotor counter-clockwise by one step brings the previous pocket under a fixed world angle.
	var fixed_world := PI / 2.0
	assert_int(PocketMath.number_for_world_angle(fixed_world, 0.0)).is_equal(0)
	assert_int(PocketMath.number_for_world_angle(fixed_world, PocketMath.STEP)).is_equal(28)


func test_local_point_helpers() -> void:
	for i in 38:
		var a := PocketMath.angle_for_index(i)
		var p := PocketMath.point_at(a, 0.21, 0.05)
		assert_float(PocketMath.radius_of(p)).is_equal_approx(0.21, EPS)
		assert_int(PocketMath.number_for_local_point(p)).is_equal(WheelLayout.number_at(i))
	assert_bool(PocketMath.is_in_pocket_ring(Vector3(0.2, 0.05, 0.0), 0.19, 0.232, 0.061)).is_true()
	assert_bool(PocketMath.is_in_pocket_ring(Vector3(0.25, 0.05, 0.0), 0.19, 0.232, 0.061)).is_false()
	assert_bool(PocketMath.is_in_pocket_ring(Vector3(0.2, 0.07, 0.0), 0.19, 0.232, 0.061)).is_false()


func test_rotor_velocity_is_counter_clockwise_for_positive_speed() -> void:
	var v := PocketMath.rotor_velocity_at(Vector3(1, 0, 0), 2.0)
	assert_vector(v).is_equal_approx(Vector3(0, 0, -2), Vector3.ONE * EPS)


func _wheel() -> RouletteWheel:
	var w: RouletteWheel = WHEEL_SCENE.instantiate()
	add_child(w)
	return auto_free(w)


func test_scene_pocket_ring_is_pocket_order_clockwise_from_above() -> void:
	var w := _wheel()
	var pockets := w.get_rotor().get_node("Pockets")
	assert_int(pockets.get_child_count()).is_equal(38)
	var by_number := {}
	for m in pockets.get_children():
		by_number[int(m.get_meta("number"))] = (m as Node3D).position
	assert_int(by_number.size()).is_equal(38)
	# Sort pockets by clockwise angle from pocket 0, using only node positions.
	var a0 := PocketMath.angle_of(by_number[0])
	var nums: Array = by_number.keys()
	nums.sort_custom(func(x, y):
		return fposmod(a0 - PocketMath.angle_of(by_number[x]) + 1e-6, TAU) < fposmod(a0 - PocketMath.angle_of(by_number[y]) + 1e-6, TAU))
	assert_array(nums).is_equal(Array(WheelLayout.POCKET_ORDER))
	# Independent check: consecutive pockets turn clockwise seen from above (+Y), i.e. cross.y < 0.
	for i in 38:
		var p: Vector3 = by_number[WheelLayout.number_at(i)]
		var q: Vector3 = by_number[WheelLayout.number_at(i + 1)]
		assert_float(p.cross(q).y).is_less(0.0)


func test_scene_labels_match_pockets() -> void:
	var w := _wheel()
	var labels := w.get_rotor().get_node("Numbers")
	assert_int(labels.get_child_count()).is_equal(38)
	for lab in labels.get_children():
		var l := lab as Label3D
		var n := 37 if l.text == "00" else int(l.text)
		assert_int(PocketMath.number_for_local_point(l.position)).is_equal(n)


func test_scene_has_frets_and_deflectors() -> void:
	var w := _wheel()
	var frets := 0
	for c in w.get_node("Frets").get_children():
		if c is CollisionShape3D:
			frets += 1
	assert_int(frets).is_equal(38)
	var diamonds := 0
	for c in w.get_node("Deflectors").get_children():
		if c is CollisionShape3D:
			diamonds += 1
	assert_int(diamonds).is_equal(8)
	assert_bool(w.get_ball().continuous_cd).is_true()


func test_pocket_world_position_follows_rotor() -> void:
	var w := _wheel()
	for rotor in [0.0, 1.0, 3.5]:
		w.set_rotor_angle(rotor)
		for n in [0, 37, 1, 14, 36]:
			var p := w.to_local(w.get_pocket_world_position(n))
			assert_int(PocketMath.number_for_world_angle(PocketMath.angle_of(p), rotor)).is_equal(n)
			assert_float(PocketMath.radius_of(p)).is_equal_approx(WheelGeometry.pocket_center_r(), EPS)


func test_interface_and_highlight() -> void:
	var w := _wheel()
	for s in ["ball_launched", "ball_settled", "ball_collided"]:
		assert_bool(w.has_signal(s)).is_true()
	assert_float(RouletteWheel.OUTER_RADIUS).is_equal_approx(0.42, 0.05)
	assert_bool(w.is_ball_in_play()).is_false()
	var hl := w.get_rotor().get_node("Highlight") as MeshInstance3D
	assert_bool(hl.visible).is_false()
	w.highlight_pocket(17)
	assert_bool(hl.visible).is_true()
	assert_int(PocketMath.number_for_angle(hl.rotation.y)).is_equal(17)
	w.clear_highlight()
	assert_bool(hl.visible).is_false()
	w.highlight_pocket(99)
	assert_bool(hl.visible).is_false()
