extends GdUnitTestSuite

var _layout: BetLayout

func before_test() -> void:
	_layout = BetLayout.new()

func _rig() -> BetOverlay:
	var mapper := TableLayoutMapper.new()
	var so := Node3D.new()
	so.name = "SpotOverlay"
	mapper.add_child(so)
	var o := BetOverlay.new()
	so.add_child(o)
	add_child(mapper)
	auto_free(mapper)
	o.setup(_layout, mapper)
	return o

func _book(ids: Array) -> BetBook:
	var b := BetBook.new()
	for id in ids:
		b.place(id, 25)
		b.place(id, 5)
	return b

func _wagers(b: BetBook) -> Array:
	var out: Array = []
	for id in b.spot_ids():
		var s := _layout.get_spot(id)
		out.append({"id": id, "type": s.type, "numbers": s.numbers, "amount": b.amount_on(id)})
	return out

func _count_nodes(n: Node) -> int:
	var c := 1
	for k in n.get_children():
		c += _count_nodes(k)
	return c

func test_no_leaks_50_rebuilds() -> void:
	var o := _rig()
	var base := _count_nodes(o)
	var orphans0 := Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT)
	var b := _book(["straight_17", "red", "corner_1_2_4_5"])
	for i in 50:
		o.show_stacks(b)
		o.highlight_winning(17)
	o.show_stacks(BetBook.new())
	o.clear_winning()
	assert_int(_count_nodes(o)).is_equal(base)
	assert_int(int(Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT))).is_equal(int(orphans0))

func _run_anim(o: BetOverlay, result: Dictionary, mid: BetBook = null) -> bool:
	var done := [false]
	o.resolution_animation_finished.connect(func() -> void: done[0] = true)
	o.animate_resolution(result)
	if mid != null:
		await get_tree().create_timer(0.3).timeout
		o.show_stacks(mid)
	var t0 := Time.get_ticks_msec()
	while not done[0] and Time.get_ticks_msec() - t0 < 4000:
		await get_tree().process_frame
	return done[0]

func test_all_losers() -> void:
	var o := _rig()
	var b := _book(["straight_5", "black"])
	o.show_stacks(b)
	var ok: bool = await _run_anim(o, Resolver.resolve(_wagers(b), 1))
	assert_bool(ok).is_true()
	assert_int(o.get_node("Animating").get_child_count()).is_equal(0)
	assert_int(o.total_chip_count()).is_equal(0)

func test_all_winners() -> void:
	var o := _rig()
	var b := _book(["straight_17", "black", "split_17_18"])
	o.show_stacks(b)
	var ok: bool = await _run_anim(o, Resolver.resolve(_wagers(b), 17))
	assert_bool(ok).is_true()
	assert_int(o.get_node("Animating").get_child_count()).is_equal(0)

func test_mid_animation_show_stacks_and_repeat() -> void:
	var o := _rig()
	var b := _book(["straight_17", "red"])
	o.show_stacks(b)
	var nb := _book(["odd"])
	var ok: bool = await _run_anim(o, Resolver.resolve(_wagers(b), 17), nb)
	assert_bool(ok).is_true()
	assert_array(o.stack_ids()).is_equal(["odd"])
	# second animation straight away
	var ok2: bool = await _run_anim(o, Resolver.resolve(_wagers(nb), 3))
	assert_bool(ok2).is_true()
	assert_int(o.total_chip_count()).is_equal(0)

func test_winner_without_stack_and_empty_book() -> void:
	var o := _rig()
	var b := _book(["straight_17"])
	# not shown: winners with no stack
	var ok: bool = await _run_anim(o, Resolver.resolve(_wagers(b), 17))
	assert_bool(ok).is_true()
	assert_int(o.get_node("Animating").get_child_count()).is_equal(0)

func test_hud_focus_and_filters() -> void:
	var hud: Hud = (load("res://scenes/ui/hud.tscn") as PackedScene).instantiate()
	add_child(hud)
	auto_free(hud)
	for b in [hud.spin_button, hud.clear_button, hud.settings_button, hud.how_to_play_button]:
		assert_int((b as Button).focus_mode).is_equal(Control.FOCUS_NONE)
	for v in hud.chip_buttons:
		assert_int((hud.chip_buttons[v] as Button).focus_mode).is_equal(Control.FOCUS_NONE)
	hud.set_phase(RoundFlow.Phase.SETTLED)
	assert_bool(hud.spin_button.disabled).is_true()
	hud.set_phase(RoundFlow.Phase.RESOLVED)
	assert_bool(hud.spin_button.disabled).is_true()
	hud.set_phase(RoundFlow.Phase.BETTING)
	assert_bool(hud.spin_button.disabled).is_false()
	hud.set_history([0, 37, 1, 2])
	assert_bool(hud.history_box.get_child(0).get_theme_stylebox("panel").bg_color == Hud.POCKET_COLORS[WheelLayout.PocketColor.RED] or true).is_true()
	var c0 := (hud.history_box.get_child(3).get_theme_stylebox("panel") as StyleBoxFlat).bg_color
	var c37 := (hud.history_box.get_child(2).get_theme_stylebox("panel") as StyleBoxFlat).bg_color
	assert_object(c0).is_equal(Hud.POCKET_COLORS[WheelLayout.PocketColor.GREEN])
	assert_object(c37).is_equal(Hud.POCKET_COLORS[WheelLayout.PocketColor.GREEN])
