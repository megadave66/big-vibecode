extends GdUnitTestSuite

func test_ray_hits_felt_plane() -> void:
	var plane := Plane(Vector3.UP, 0.76)
	var hit: Variant = TableInput.ray_plane_hit(Vector3(1, 2.76, 0), Vector3(0, -1, 0), plane)
	assert_object(hit).is_not_null()
	assert_vector(hit as Vector3).is_equal_approx(Vector3(1, 0.76, 0), Vector3.ONE * 0.0001)
	# Oblique ray: 45 degrees down toward -Z from height 1 above the felt.
	var hit2: Variant = TableInput.ray_plane_hit(Vector3(0, 1.76, 1), Vector3(0, -1, -1).normalized(), plane)
	assert_vector(hit2 as Vector3).is_equal_approx(Vector3(0, 0.76, 0), Vector3.ONE * 0.0001)


func test_ray_misses_when_parallel_or_away() -> void:
	var plane := Plane(Vector3.UP, 0.76)
	assert_object(TableInput.ray_plane_hit(Vector3(0, 2, 0), Vector3(1, 0, 0), plane)).is_null()
	assert_object(TableInput.ray_plane_hit(Vector3(0, 2, 0), Vector3(0, 1, 0), plane)).is_null()


func test_hit_to_layout() -> void:
	var m := TableLayoutMapper.new()
	m.position = Vector3(0, 0.76, 0)
	add_child(m)
	auto_free(m)
	var target: Vector3 = m.layout_to_world(Vector2(4.5, 1.5))
	var hit: Variant = TableInput.ray_plane_hit(target + Vector3(0, 1, 0.5), Vector3(0, -1, -0.5).normalized(), m.felt_plane())
	var lp: Vector2 = m.world_to_layout(hit as Vector3)
	assert_float(lp.x).is_equal_approx(4.5, 0.001)
	assert_float(lp.y).is_equal_approx(1.5, 0.001)


func test_main_scene_has_required_nodes() -> void:
	var main: Node = auto_free(load("res://scenes/main.tscn").instantiate())
	add_child(main)
	for path in ["WorldEnvironment", "Lights", "CameraRig", "CameraRig/Camera3D", "Table", "Table/TableLayoutMapper",
			"Table/TableLayoutMapper/SpotOverlay", "WheelAnchor", "UI", "TableInput"]:
		assert_object(main.get_node_or_null(path)).override_failure_message("missing " + path).is_not_null()
	assert_bool(main.get_node("WheelAnchor") is Marker3D).is_true()
	assert_bool(main.get_node("UI") is CanvasLayer).is_true()


func test_camera_rig_views() -> void:
	var main: Node = auto_free(load("res://scenes/main.tscn").instantiate())
	add_child(main)
	var rig: CameraRig = main.get_node("CameraRig")
	assert_str(rig.current_view).is_equal("table")
	rig.set_view("wheel", true)
	assert_str(rig.current_view).is_equal("wheel")
	assert_vector(rig.transform.origin).is_equal_approx(CameraRig.VIEWS["wheel"][0], Vector3.ONE * 0.001)
	rig.toggle()
	assert_str(rig.current_view).is_equal("table")
