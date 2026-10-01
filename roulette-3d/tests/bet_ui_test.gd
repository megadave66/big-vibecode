extends GdUnitTestSuite
## Section 3 part B: chip breakdown, BetOverlay (stacks, hover, win marks, animation), HUD.

const EPS := Vector3(0.0001, 0.0001, 0.0001)

var _layout: BetLayout


func before_test() -> void:
	_layout = BetLayout.new()


## Mapper (offset, as on the real table) -> SpotOverlay -> BetOverlay, all in the tree.
func _rig() -> Array:
	var mapper := TableLayoutMapper.new()
	mapper.position = Vector3(0.1, 0.76, -0.2)
	var spot_overlay := Node3D.new()
	spot_overlay.name = "SpotOverlay"
	mapper.add_child(spot_overlay)
	var overlay := BetOverlay.new()
	spot_overlay.add_child(overlay)
	add_child(mapper)
	auto_free(mapper)
	overlay.setup(_layout, mapper)
	return [mapper, overlay]


func _wait_flag(flag: Array, max_ms: int) -> void:
	var t0 := Time.get_ticks_msec()
	while not flag[0] and Time.get_ticks_msec() - t0 < max_ms:
		await get_tree().process_frame


# ------------------------------------------------------------ chip breakdown

func test_break_into_chips_cases() -> void:
	assert_array(ChipBreakdown.break_into_chips(0)).is_empty()
	assert_array(ChipBreakdown.break_into_chips(-5)).is_empty()
	assert_array(ChipBreakdown.break_into_chips(1)).is_equal([1])
	assert_array(ChipBreakdown.break_into_chips(4)).is_equal([1, 1, 1, 1])
	assert_array(ChipBreakdown.break_into_chips(5)).is_equal([5])
	assert_array(ChipBreakdown.break_into_chips(30)).is_equal([25, 5])
	assert_array(ChipBreakdown.break_into_chips(131)).is_equal([100, 25, 5, 1])
	assert_array(ChipBreakdown.break_into_chips(249)).is_equal([100, 100, 25, 5, 5, 5, 5, 1, 1, 1, 1])
	assert_array(ChipBreakdown.break_into_chips(1260)).is_equal(
			[100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 25, 25, 5, 5])


func test_break_into_chips_sums_and_is_minimal_order() -> void:
	for amount in range(0, 600):
		var chips := ChipBreakdown.break_into_chips(amount)
		var sum := 0
		var prev := 1000
		for c in chips:
			assert_bool(c in BetBook.DENOMINATIONS).is_true()
			assert_int(c).is_less_equal(prev)
			prev = c
			sum += c
		assert_int(sum).is_equal(amount)
		# Greedy: never 5 or more of a lower chip that a higher one could replace.
		assert_int(chips.count(1)).is_less(5)
		assert_int(chips.count(5)).is_less(5)
		assert_int(chips.count(25)).is_less(4)


# ------------------------------------------------------------ overlay stacks

func test_overlay_builds_chip_nodes_for_book() -> void:
	var r := _rig()
	var mapper: TableLayoutMapper = r[0]
	var overlay: BetOverlay = r[1]
	var book := BetBook.new()
	book.place("straight_17", 25)
	book.place("straight_17", 5)
	book.place("straight_17", 1)
	book.place("split_0_37", 5)
	book.place("corner_1_2_4_5", 100)
	book.place("corner_1_2_4_5", 25)
	book.place("five_number", 1)
	book.place("red", 100)
	overlay.show_stacks(book)
	assert_int(overlay.stack_ids().size()).is_equal(5)
	assert_int(overlay.chip_count("straight_17")).is_equal(3)
	assert_int(overlay.chip_count("split_0_37")).is_equal(1)
	assert_int(overlay.chip_count("corner_1_2_4_5")).is_equal(2)
	assert_int(overlay.chip_count("five_number")).is_equal(1)
	assert_int(overlay.chip_count("red")).is_equal(1)
	assert_int(overlay.total_chip_count()).is_equal(8)
	# Chip values bottom-up match the book.
	var st := overlay.stack_node("straight_17")
	var vals: Array = []
	for c in st.get_children():
		if c.has_meta("value"):
			vals.append(c.get_meta("value"))
	assert_array(vals).is_equal([25, 5, 1])
	# Stacks grow upward.
	assert_float((st.get_node("Chip2") as Node3D).position.y).is_greater((st.get_node("Chip0") as Node3D).position.y)
	# Total label only on stacks with more than one chip.
	assert_object(st.get_node_or_null("Total")).is_not_null()
	assert_str((st.get_node("Total") as Label3D).text).is_equal("$31")
	assert_object(overlay.stack_node("red").get_node_or_null("Total")).is_null()


func test_overlay_stacks_sit_at_spot_centers_via_mapper() -> void:
	var r := _rig()
	var mapper: TableLayoutMapper = r[0]
	var overlay: BetOverlay = r[1]
	var book := BetBook.new()
	for id in ["straight_0", "straight_36", "split_0_37", "corner_1_2_4_5", "five_number", "street_34",
			"sixline_31", "trio_0_2_37", "red", "dozen_3", "column_2"]:
		book.place(id, 5)
	overlay.show_stacks(book)
	for id in book.spot_ids():
		var want := mapper.layout_to_world(_layout.get_spot(id).center)
		var got := overlay.stack_node(id).global_position
		assert_float(got.x).override_failure_message(id).is_equal_approx(want.x, 0.0001)
		assert_float(got.z).override_failure_message(id).is_equal_approx(want.z, 0.0001)
		assert_float(got.y).is_greater_equal(want.y)  # on or above the felt
		assert_float(got.y - want.y).is_less(0.01)


func test_show_stacks_rebuilds_and_clears() -> void:
	var overlay: BetOverlay = _rig()[1]
	var book := BetBook.new()
	book.place("odd", 1)
	book.place("even", 1)
	overlay.show_stacks(book)
	assert_int(overlay.total_chip_count()).is_equal(2)
	book.remove_top("odd")
	overlay.show_stacks(book)
	assert_int(overlay.total_chip_count()).is_equal(1)
	assert_object(overlay.stack_node("odd")).is_null()
	book.clear()
	overlay.show_stacks(book)
	assert_int(overlay.total_chip_count()).is_equal(0)
	assert_int(overlay.get_node("Stacks").get_child_count()).is_equal(0)


func test_unknown_spot_ids_are_skipped() -> void:
	var overlay: BetOverlay = _rig()[1]
	var book := BetBook.new()
	book.place("not_a_spot", 5)
	book.place("high", 5)
	overlay.show_stacks(book)
	assert_array(overlay.stack_ids()).is_equal(["high"])


# ------------------------------------------------------------ hover

func test_hover_on_corner_zone_highlights_corner() -> void:
	var r := _rig()
	var mapper: TableLayoutMapper = r[0]
	var overlay: BetOverlay = r[1]
	var s := overlay.hover_at(Vector2(1.02, 0.98), true)
	assert_str(s.id).is_equal("corner_1_2_4_5")
	assert_str(overlay.hovered_spot().id).is_equal("corner_1_2_4_5")
	var hm := overlay.hover_mesh()
	assert_bool(hm.visible).is_true()
	var want := mapper.layout_to_world(s.rect.get_center())
	assert_float(hm.global_position.x).is_equal_approx(want.x, 0.0001)
	assert_float(hm.global_position.z).is_equal_approx(want.z, 0.0001)
	var size: Vector2 = (hm.mesh as PlaneMesh).size
	assert_float(size.x).is_equal_approx(s.rect.size.x * TableLayoutMapper.CELL_SIZE, 0.00001)
	assert_float(size.y).is_equal_approx(s.rect.size.y * TableLayoutMapper.CELL_SIZE, 0.00001)


func test_hover_moves_and_hides() -> void:
	var overlay: BetOverlay = _rig()[1]
	assert_str(overlay.hover_at(Vector2(0.5, 0.5)).id).is_equal("straight_1")
	assert_str(overlay.hover_at(Vector2(-0.5, 1.5)).id).is_equal("split_0_37")
	assert_object(overlay.hover_at(Vector2(0.5, 0.5), false)).is_null()
	assert_bool(overlay.hover_mesh().visible).is_false()
	assert_object(overlay.hover_at(Vector2(20, 20), true)).is_null()
	assert_bool(overlay.hover_mesh().visible).is_false()
	overlay.hover_at(Vector2(0.5, 0.5))
	overlay.clear_hover()
	assert_object(overlay.hovered_spot()).is_null()


# ------------------------------------------------------------ winning marks

func test_highlight_winning_marks_every_spot_with_number() -> void:
	var overlay: BetOverlay = _rig()[1]
	for n in [17, 0, 37, 1, 36]:
		overlay.highlight_winning(n)
		var want: Array = []
		for s in _layout.spots():
			if s.numbers.has(n):
				want.append(s.id)
		want.sort()
		var got := overlay.winning_ids()
		got.sort()
		assert_array(got).is_equal(want)
	# 17: straight, 4 splits, 4 corners, street, 2 six-lines, dozen, column, black, odd, low.
	overlay.highlight_winning(17)
	assert_int(overlay.winning_ids().size()).is_equal(17)
	overlay.clear_winning()
	assert_array(overlay.winning_ids()).is_empty()


# ------------------------------------------------------------ resolution animation

func test_animate_resolution_finishes_and_clears() -> void:
	var overlay: BetOverlay = _rig()[1]
	var book := BetBook.new()
	book.place("straight_17", 5)
	book.place("red", 25)
	book.place("black", 25)
	overlay.show_stacks(book)
	var wagers: Array = []
	for id in book.spot_ids():
		var s := _layout.get_spot(id)
		wagers.append({"id": id, "type": s.type, "numbers": s.numbers, "amount": book.amount_on(id)})
	var result := Resolver.resolve(wagers, 17)
	var done := [false]
	overlay.resolution_animation_finished.connect(func() -> void: done[0] = true)
	var t0 := Time.get_ticks_msec()
	overlay.animate_resolution(result)
	assert_bool(overlay.is_animating()).is_true()
	# Payout stack for the winner exists during the animation (5 * 35 = 175 -> 100, 25, 25, 25).
	var pay: Node3D = overlay.get_node("Animating").get_node_or_null("Payout_straight_17")
	assert_object(pay).is_not_null()
	var vals: Array = []
	for c in pay.get_children():
		if c.has_meta("value"):
			vals.append(c.get_meta("value"))
	assert_array(vals).is_equal([100, 25, 25, 25])
	# A book change during the animation is held, then applied.
	var next := BetBook.new()
	next.place("odd", 1)
	overlay.show_stacks(next)
	assert_int(overlay.total_chip_count()).is_equal(0)
	await _wait_flag(done, 4000)
	var elapsed := Time.get_ticks_msec() - t0
	assert_bool(done[0]).is_true()
	assert_int(elapsed).is_less(3000)
	assert_bool(overlay.is_animating()).is_false()
	assert_int(overlay.get_node("Animating").get_child_count()).is_equal(0)
	assert_array(overlay.stack_ids()).is_equal(["odd"])


func test_animate_resolution_with_no_bets_still_finishes() -> void:
	var overlay: BetOverlay = _rig()[1]
	var done := [false]
	overlay.resolution_animation_finished.connect(func() -> void: done[0] = true)
	overlay.animate_resolution(Resolver.resolve([], 5))
	await _wait_flag(done, 2000)
	assert_bool(done[0]).is_true()


# ------------------------------------------------------------ HUD

func _hud() -> Hud:
	var hud: Hud = (load("res://scenes/ui/hud.tscn") as PackedScene).instantiate()
	add_child(hud)
	auto_free(hud)
	return hud


func test_tooltip_text_uses_payouts_odds() -> void:
	for id in ["straight_17", "split_0_37", "corner_1_2_4_5", "five_number", "trio_0_2_37", "street_1",
			"sixline_1", "dozen_2", "column_3", "red", "low"]:
		var s := _layout.get_spot(id)
		assert_str(Hud.tooltip_text(s)).is_equal(s.label + " pays " + Payouts.odds_text(s.type))
	assert_str(Hud.tooltip_text(_layout.get_spot("corner_1_2_4_5"))).is_equal("Corner 1-2-4-5 pays 8:1")
	assert_str(Hud.tooltip_text(_layout.get_spot("five_number"))).ends_with("pays 6:1")


func test_hud_tooltip_show_hide() -> void:
	var hud := _hud()
	var s := _layout.get_spot("split_0_37")
	hud.show_spot_tooltip(s, Vector2(400, 300))
	assert_bool(hud.tooltip_visible()).is_true()
	assert_str(hud.tooltip_label.text).is_equal("Split 0-00 pays 17:1")
	hud.show_spot_tooltip(null, Vector2.ZERO)
	assert_bool(hud.tooltip_visible()).is_false()
	hud.show_tooltip("x", Vector2(10, 10))
	hud.hide_tooltip()
	assert_bool(hud.tooltip_visible()).is_false()


func test_hud_buttons_emit_signals() -> void:
	var hud := _hud()
	var got: Array = []
	hud.spin_pressed.connect(func() -> void: got.append("spin"))
	hud.clear_pressed.connect(func() -> void: got.append("clear"))
	hud.settings_pressed.connect(func() -> void: got.append("settings"))
	hud.how_to_play_pressed.connect(func() -> void: got.append("how"))
	hud.chip_selected.connect(func(v: int) -> void: got.append(v))
	hud.spin_button.pressed.emit()
	hud.clear_button.pressed.emit()
	hud.settings_button.pressed.emit()
	hud.how_to_play_button.pressed.emit()
	(hud.chip_buttons[25] as Button).pressed.emit()
	assert_array(got).is_equal(["spin", "clear", "settings", "how", 25])
	assert_int(hud.selected_chip()).is_equal(25)


func test_hud_chip_selection_remembered_and_exclusive() -> void:
	var hud := _hud()
	assert_int(hud.selected_chip()).is_equal(1)
	hud.select_chip(100)
	assert_int(hud.selected_chip()).is_equal(100)
	for v in Hud.CHIP_VALUES:
		assert_bool((hud.chip_buttons[v] as Button).button_pressed).is_equal(v == 100)
	hud.select_chip(7)  # not a chip: ignored
	assert_int(hud.selected_chip()).is_equal(100)


func test_hud_values_phase_and_history() -> void:
	var hud := _hud()
	hud.set_bankroll(1250)
	assert_str(hud.bankroll_label.text).is_equal("$1,250")
	hud.set_total_bet(35)
	assert_str(hud.bet_label.text).is_equal("$35")
	hud.set_last_result(-35)
	assert_str(hud.last_label.text).is_equal("-$35")
	hud.set_last_result(350)
	assert_str(hud.last_label.text).is_equal("+$350")
	hud.set_phase(RoundFlow.Phase.LOCKED)
	assert_str(hud.status_label.text).is_equal("No more bets")
	assert_bool(hud.spin_button.disabled).is_true()
	assert_bool(hud.clear_button.disabled).is_true()
	hud.set_phase(RoundFlow.Phase.BETTING)
	assert_str(hud.status_label.text).is_equal("Place your bets")
	assert_bool(hud.spin_button.disabled).is_false()
	hud.announce_number(17)
	assert_str(hud.status_label.text).is_equal("17 Black")
	hud.announce_number(37)
	assert_str(hud.status_label.text).is_equal("00 Green")
	hud.set_status("Hello")
	assert_str(hud.status_label.text).is_equal("Hello")
	for n in range(20):
		hud.push_history(n)
	assert_int(hud.history().size()).is_equal(Hud.HISTORY_MAX)
	assert_int(hud.history()[-1]).is_equal(19)
	assert_int(hud.history_box.get_child_count()).is_equal(Hud.HISTORY_MAX)
	assert_int(hud.history_box.get_child(0).get_meta("number")).is_equal(19)  # newest first
	hud.set_buttons_enabled(false)
	assert_bool(hud.spin_button.disabled).is_true()


func test_hud_money_format() -> void:
	assert_str(Hud.money(0)).is_equal("$0")
	assert_str(Hud.money(999)).is_equal("$999")
	assert_str(Hud.money(1000)).is_equal("$1,000")
	assert_str(Hud.money(1234567)).is_equal("$1,234,567")
	assert_str(Hud.money(-25)).is_equal("-$25")


func test_hud_root_does_not_block_table_clicks() -> void:
	var hud := _hud()
	assert_int(hud.mouse_filter).is_equal(Control.MOUSE_FILTER_IGNORE)
	assert_int(hud.tooltip_panel.mouse_filter).is_equal(Control.MOUSE_FILTER_IGNORE)


func test_resolution_signal_never_emitted_synchronously() -> void:
	# In tree, empty result.
	var overlay: BetOverlay = _rig()[1]
	var done := [false]
	overlay.resolution_animation_finished.connect(func() -> void: done[0] = true)
	overlay.animate_resolution({})
	assert_bool(done[0]).is_false()
	await _wait_flag(done, 2000)
	assert_bool(done[0]).is_true()
	# Not in tree.
	var loose := BetOverlay.new()
	auto_free(loose)
	loose.setup(_layout, null)
	var done2 := [false]
	loose.resolution_animation_finished.connect(func() -> void: done2[0] = true)
	loose.animate_resolution(Resolver.resolve([], 5))
	assert_bool(done2[0]).is_false()
	assert_bool(loose.is_animating()).is_true()
	await _wait_flag(done2, 2000)
	assert_bool(done2[0]).is_true()
	assert_bool(loose.is_animating()).is_false()


func test_second_call_while_animating_does_not_hang() -> void:
	var overlay: BetOverlay = _rig()[1]
	var book := BetBook.new()
	book.place("red", 5)
	overlay.show_stacks(book)
	var s := _layout.get_spot("red")
	var result := Resolver.resolve([{"id": "red", "type": s.type, "numbers": s.numbers, "amount": 5}], 1)
	var emits := [0]
	overlay.resolution_animation_finished.connect(func() -> void: emits[0] += 1)
	overlay.animate_resolution(result)
	overlay.animate_resolution(result)  # ignored; the running animation's emit covers it
	assert_int(emits[0]).is_equal(0)
	var t0 := Time.get_ticks_msec()
	while emits[0] == 0 and Time.get_ticks_msec() - t0 < 4000:
		await get_tree().process_frame
	assert_int(emits[0]).is_equal(1)
	assert_bool(overlay.is_animating()).is_false()
	# Awaiting the signal right after the call works (main.gd pattern).
	overlay.show_stacks(book)
	var ok := [false]
	var waiter := func() -> void:
		await overlay.resolution_animation_finished
		ok[0] = true
	overlay.animate_resolution(result)
	waiter.call()
	await _wait_flag(ok, 4000)
	assert_bool(ok[0]).is_true()
