extends GdUnitTestSuite
## End-to-end: main scene, real bets, real physics spin, real settle, real payout.
## The wheel's pocket detection feeds RoundFlow directly; nothing is stubbed.

const MAIN := "res://scenes/main.tscn"
const BUDGET_S := 60.0


func _await_phase(main: Node, phase: int, budget_s: float) -> bool:
	var start := Time.get_ticks_msec()
	while main.flow.phase != phase:
		if (Time.get_ticks_msec() - start) / 1000.0 > budget_s:
			return false
		await get_tree().process_frame
	return true


func test_full_round_with_real_physics_pays_correctly() -> void:
	var runner := scene_runner(MAIN)
	var main: Node = runner.scene()
	await runner.simulate_frames(5)
	var flow: RoundFlow = main.flow
	assert_int(flow.bankroll.balance).is_equal(1000)
	assert_int(flow.phase).is_equal(RoundFlow.Phase.BETTING)

	# A spread of bets covering inside, five-number and outside bets.
	var bets := {"straight_17": 5, "split_0_37": 1, "corner_1_2_4_5": 25, "five_number": 5,
		"red": 100, "dozen_2": 25, "column_3": 25, "sixline_31": 5, "trio_0_1_2": 1}
	for id in bets:
		var amount: int = bets[id]
		for chip in ChipBreakdown.break_into_chips(amount):
			assert_bool(flow.place_chip(id, chip)).override_failure_message("place %s %d" % [id, chip]).is_true()
	var staked := flow.book.total()
	assert_int(staked).is_equal(192)
	assert_int(flow.bankroll.balance).is_equal(1000 - staked)
	var wagers := flow.wagers()

	main.spin()
	assert_int(flow.phase).is_equal(RoundFlow.Phase.LOCKED)
	assert_bool(main.wheel.is_ball_in_play()).is_true()
	# No more bets.
	assert_bool(flow.place_chip("black", 1)).is_false()
	assert_bool(flow.remove_chip("red")).is_false()

	assert_bool(await _await_phase(main, RoundFlow.Phase.SETTLED, BUDGET_S)).override_failure_message("ball did not settle").is_true()
	var n: int = flow.winning_number
	assert_bool(WheelLayout.is_valid(n)).is_true()
	assert_int(main.wheel.settled_number).is_equal(n)

	assert_bool(await _await_phase(main, RoundFlow.Phase.BETTING, 20.0)).override_failure_message("round did not finish").is_true()
	var expected := Resolver.resolve(wagers, n)
	print("integration spin: number=%s net=%d" % [WheelLayout.label(n), expected["net"]])
	assert_int(flow.bankroll.balance).is_equal(1000 - staked + int(expected["total_returned"]))
	assert_bool(flow.book.is_empty()).is_true()
	assert_array(flow.history).is_equal([n])
	assert_int(main.hud.history().size()).is_equal(1)


func test_spin_with_no_bets_is_allowed() -> void:
	var runner := scene_runner(MAIN)
	var main: Node = runner.scene()
	await runner.simulate_frames(5)
	main.spin()
	assert_int(main.flow.phase).is_equal(RoundFlow.Phase.LOCKED)
	assert_bool(await _await_phase(main, RoundFlow.Phase.BETTING, BUDGET_S + 20.0)).is_true()
	assert_int(main.flow.bankroll.balance).is_equal(1000)
	assert_int(main.flow.history.size()).is_equal(1)


func test_left_click_places_and_right_click_removes_via_table_input() -> void:
	var runner := scene_runner(MAIN)
	var main: Node = runner.scene()
	await runner.simulate_frames(5)
	main.hud.select_chip(25)
	var spot: BetSpot = main.layout.get_spot("straight_17")
	main.table_input.table_clicked.emit(spot.center, MOUSE_BUTTON_LEFT)
	main.table_input.table_clicked.emit(spot.center, MOUSE_BUTTON_LEFT)
	assert_int(main.flow.book.amount_on("straight_17")).is_equal(50)
	assert_int(main.flow.bankroll.balance).is_equal(950)
	main.table_input.table_clicked.emit(spot.center, MOUSE_BUTTON_RIGHT)
	assert_int(main.flow.book.amount_on("straight_17")).is_equal(25)
	assert_int(main.flow.bankroll.balance).is_equal(975)
	await runner.simulate_frames(2)
	assert_int(main.overlay.chip_count("straight_17")).is_equal(1)
