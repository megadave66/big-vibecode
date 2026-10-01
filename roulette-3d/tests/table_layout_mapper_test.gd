extends GdUnitTestSuite

func _mapper() -> TableLayoutMapper:
	var m := TableLayoutMapper.new()
	m.position = Vector3(0.3, 0.76, -0.1)
	add_child(m)
	return auto_free(m)


func test_round_trip_points() -> void:
	var m := _mapper()
	var pts: Array[Vector2] = [Vector2(0, 0), Vector2(5.5, 1.5), Vector2(12.5, 2.5), Vector2(-1, 0), Vector2(-1, 1.5),
		Vector2(0, 3), Vector2(-1, 3), Vector2(-0.5, 0.75), Vector2(-0.5, 2.25), Vector2(3, -1.5), Vector2(13, -2)]
	for p in pts:
		var back: Vector2 = m.world_to_layout(m.layout_to_world(p))
		assert_float(back.x).is_equal_approx(p.x, 0.0001)
		assert_float(back.y).is_equal_approx(p.y, 0.0001)


func test_orientation_and_scale() -> void:
	var m := _mapper()
	var o: Vector3 = m.layout_to_world(Vector2.ZERO)
	var x1: Vector3 = m.layout_to_world(Vector2(1, 0))
	var y1: Vector3 = m.layout_to_world(Vector2(0, 1))
	assert_vector(o).is_equal_approx(Vector3(0.3, 0.76, -0.1), Vector3.ONE * 0.0001)
	assert_float(x1.x - o.x).is_equal_approx(TableLayoutMapper.CELL_SIZE, 0.0001)
	# Positive layout y runs away from the player (player is on +Z).
	assert_float(y1.z - o.z).is_equal_approx(-TableLayoutMapper.CELL_SIZE, 0.0001)
	assert_float(y1.y).is_equal_approx(0.76, 0.0001)


func test_cell_size_range() -> void:
	assert_float(TableLayoutMapper.CELL_SIZE).is_between(0.07, 0.09)
