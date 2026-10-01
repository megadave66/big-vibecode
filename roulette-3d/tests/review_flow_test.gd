extends GdUnitTestSuite
## Section 4 review: RoundFlow + Resolver + Bankroll wired to the real BetLayout.

const OUTSIDE_IDS: Array[String] = [
	"dozen_1", "dozen_2", "dozen_3", "column_1", "column_2", "column_3",
	"red", "black", "even", "odd", "low", "high",
]

var _layout: BetLayout


func before() -> void:
	_layout = BetLayout.new()


func _spot_info() -> Callable:
	var layout := _layout
	return func(id: String) -> Dictionary:
		var s: BetSpot = layout.get_spot(id)
		return {} if s == null else {"type": s.type, "numbers": s.numbers}


func _flow(start: int = 1000) -> RoundFlow:
	return RoundFlow.new(_spot_info(), start)


func _play(flow: RoundFlow, n: int) -> Dictionary:
	assert_bool(flow.lock_bets()).is_true()
	assert_bool(flow.ball_settled(n)).is_true()
	return flow.resolve()


# Every spot accepts a chip (its BetLayout shape passes RoundFlow/Resolver validation).
func test_every_layout_spot_is_bettable() -> void:
	var spots := _layout.spots()
	assert_int(spots.size()).is_greater(100)
	var flow := _flow(1_000_000)
	for s in spots:
		assert_bool(flow.place_chip(s.id, 1)).override_failure_message("rejected " + s.id).is_true()
	assert_int(flow.book.total()).is_equal(spots.size())
	assert_int(flow.bankroll.balance).is_equal(1_000_000 - spots.size())


# Literal sweep: every spot x every winning number, one 1-chip bet per round.
func test_every_spot_every_number_single_bet() -> void:
	var bad := 0
	for s in _layout.spots():
		for n in range(38):
			var flow := _flow(100)
			flow.place_chip(s.id, 1)
			var r := _play(flow, n)
			var expected := (Payouts.odds(s.type) + 1) if s.numbers.has(n) else 0
			var ok: bool = r["total_returned"] == expected and r["total_staked"] == 1 \
				and flow.bankroll.balance == 99 + expected and (r["refunded"] as Array).is_empty() \
				and (r["winners"] as Array).size() == (1 if expected > 0 else 0) \
				and (r["losers"] as Array).size() == (0 if expected > 0 else 1)
			if not ok:
				bad += 1
				if bad < 10:
					push_error("mismatch %s on %d: %s" % [s.id, n, str(r)])
	assert_int(bad).is_equal(0)


# Same sweep with every spot covered at once; checks per-id results and bankroll totals.
func test_full_board_each_number() -> void:
	var spots := _layout.spots()
	for n in range(38):
		var flow := _flow(1_000_000)
		for s in spots:
			flow.place_chip(s.id, 1)
		var before := flow.bankroll.balance
		var r := _play(flow, n)
		var won := {}
		for w: Dictionary in r["winners"]:
			won[w["id"]] = w["returned"]
		var lost := {}
		for l: Dictionary in r["losers"]:
			lost[l["id"]] = true
		var expected_total := 0
		for s in spots:
			if s.numbers.has(n):
				var exp := Payouts.odds(s.type) + 1
				expected_total += exp
				assert_int(int(won.get(s.id, -1))).override_failure_message("%s on %d" % [s.id, n]).is_equal(exp)
			else:
				assert_bool(lost.has(s.id)).override_failure_message("%s on %d" % [s.id, n]).is_true()
		assert_int(won.size() + lost.size()).is_equal(spots.size())
		assert_int(r["total_returned"]).is_equal(expected_total)
		assert_int(r["net"]).is_equal(expected_total - spots.size())
		assert_int(flow.bankroll.balance).is_equal(before + expected_total)
		assert_bool(flow.book.is_empty()).is_true()


func test_five_number_edge_cases() -> void:
	var s := _layout.get_spot("five_number")
	assert_object(s).is_not_null()
	assert_int(s.type).is_equal(Payouts.BetType.FIVE_NUMBER)
	assert_array(s.numbers).contains_exactly([0, 1, 2, 3, 37])
	for n in range(38):
		var flow := _flow(1000)
		flow.place_chip("five_number", 5)
		flow.place_chip("five_number", 1)
		var r := _play(flow, n)
		if n in [0, 1, 2, 3, 37]:
			assert_int(r["total_returned"]).is_equal(6 * 7)
			assert_int(flow.bankroll.balance).is_equal(1000 + 6 * 6)
		else:
			assert_int(r["total_returned"]).is_equal(0)
			assert_int(flow.bankroll.balance).is_equal(1000 - 6)
	# Five-number is its own spot, distinct from the trios that share its numbers.
	for trio in ["trio_0_1_2", "trio_0_2_37", "trio_2_3_37"]:
		var t := _layout.get_spot(trio)
		assert_object(t).override_failure_message(trio).is_not_null()
		assert_int(t.type).is_equal(Payouts.BetType.TRIO)


func test_zero_and_double_zero_lose_all_outside_bets() -> void:
	for n in [0, 37]:
		var flow := _flow(1000)
		for id in OUTSIDE_IDS:
			assert_bool(flow.place_chip(id, 5)).override_failure_message(id).is_true()
		flow.place_chip("straight_%d" % n, 1)
		var r := _play(flow, n)
		assert_int((r["losers"] as Array).size()).is_equal(OUTSIDE_IDS.size())
		assert_int((r["winners"] as Array).size()).is_equal(1)
		assert_int(r["total_returned"]).is_equal(36)
		assert_int(flow.bankroll.balance).is_equal(1000 - 61 + 36)


func test_no_hard_coded_odds_outside_payouts() -> void:
	var strict := RegEx.create_from_string("\\b(35|17|11|8|6|5)\\b")
	# Resolver.NUMBER_COUNT lines hold number counts, not odds; checked against the layout below.
	var count_line := RegEx.create_from_string("^\\s*Payouts\\.BetType\\.\\w+: \\d+,\\s*$")
	for s in _layout.spots():
		assert_int(s.numbers.size()).override_failure_message(s.id).is_equal(Resolver.NUMBER_COUNT[s.type])
	var ratio := RegEx.create_from_string("\\b(35|17|11|8|6|5|2|1)\\s*:\\s*1\\b")
	for path in ["res://scripts/core/resolver.gd", "res://scripts/core/bankroll.gd", "res://scripts/core/round_flow.gd"]:
		var text := FileAccess.get_file_as_string(path)
		assert_bool(text.is_empty()).is_false()
		for line in text.split("\n"):
			if line.strip_edges().begins_with("#") or count_line.search(line) != null:
				continue
			assert_object(strict.search(line)).override_failure_message("%s: %s" % [path, line]).is_null()
	for path in _gd_files("res://scripts"):
		if path.ends_with("/payouts.gd"):
			continue
		var text := FileAccess.get_file_as_string(path)
		assert_object(ratio.search(text)).override_failure_message(path).is_null()


func _gd_files(dir: String) -> Array[String]:
	var out: Array[String] = []
	for f in DirAccess.get_files_at(dir):
		if f.ends_with(".gd"):
			out.append(dir + "/" + f)
	for d in DirAccess.get_directories_at(dir):
		out.append_array(_gd_files(dir + "/" + d))
	return out


func test_bankroll_never_negative_random_play() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 4242
	var ids: Array[String] = []
	for s in _layout.spots():
		ids.append(s.id)
	var flow := _flow(500)
	var rounds := 0
	var neg := [false]
	flow.bankroll_changed.connect(func(b: int) -> void:
		if b < 0:
			neg[0] = true)
	for round_i in range(300):
		for k in range(rng.randi_range(0, 12)):
			var op := rng.randi_range(0, 9)
			var id := ids[rng.randi_range(0, ids.size() - 1)]
			if op < 8:
				var chip: int = BetBook.DENOMINATIONS[rng.randi_range(0, 3)]
				var before := flow.bankroll.balance
				var ok := flow.place_chip(id, chip)
				assert_bool(ok).is_equal(chip <= before)
			elif op == 8:
				flow.remove_chip(id)
			else:
				flow.clear_bets()
			assert_int(flow.bankroll.balance).is_greater_equal(0)
		# Money is conserved: balance + chips on table equals the pre-round total.
		var pre := flow.bankroll.balance + flow.book.total()
		var r := _play(flow, rng.randi_range(0, 37))
		assert_int(flow.bankroll.balance).is_equal(pre + r["net"])
		assert_int(flow.bankroll.balance).is_greater_equal(0)
		assert_bool(flow.next_round()).is_true()
		rounds += 1
		if flow.bankroll.balance == 0:
			assert_bool(flow.place_chip("red", 1)).is_false()
			break
	assert_bool(neg[0]).is_false()
	assert_int(rounds).is_greater(5)
	# Direct bankroll abuse.
	var b := Bankroll.new(3)
	assert_bool(b.debit(4)).is_false()
	assert_bool(b.debit(-1)).is_false()
	assert_bool(b.credit(-10)).is_false()
	assert_int(b.balance).is_equal(3)


func _snapshot(flow: RoundFlow) -> Dictionary:
	return {
		"phase": flow.phase, "balance": flow.bankroll.balance, "bets": flow.book.bets().duplicate(),
		"history": flow.history.duplicate(), "winning": flow.winning_number,
	}


func _count_signals(flow: RoundFlow) -> Array:
	var c := [0]
	var bump := func(_a: Variant = null) -> void: c[0] += 1
	flow.phase_changed.connect(bump)
	flow.bets_changed.connect(bump)
	flow.bankroll_changed.connect(bump)
	flow.number_settled.connect(bump)
	flow.round_resolved.connect(bump)
	return c


func test_non_betting_phases_reject_every_bet_edit() -> void:
	var flow := _flow(1000)
	flow.place_chip("straight_17", 25)
	flow.place_chip("red", 5)
	var steps := [func() -> void: flow.lock_bets(), func() -> void: flow.ball_settled(17), func() -> void: flow.resolve()]
	for step: Callable in steps:
		step.call()
		var snap := _snapshot(flow)
		var c := _count_signals(flow)
		for s in _layout.spots():
			for chip in BetBook.DENOMINATIONS:
				assert_bool(flow.place_chip(s.id, chip)).is_false()
			assert_bool(flow.remove_chip(s.id)).is_false()
		assert_bool(flow.clear_bets()).is_false()
		assert_dict(_snapshot(flow)).is_equal(snap)
		assert_int(c[0]).is_equal(0)
		_disconnect_all(flow)


func _disconnect_all(flow: RoundFlow) -> void:
	for sig in ["phase_changed", "bets_changed", "bankroll_changed", "number_settled", "round_resolved"]:
		for conn in flow.get_signal_connection_list(sig):
			flow.disconnect(sig, conn["callable"])


func test_invalid_transitions_change_nothing() -> void:
	var flow := _flow(1000)
	flow.place_chip("split_0_37", 5)
	# BETTING
	_expect_rejected(flow, [
		func() -> bool: return flow.ball_settled(5),
		func() -> bool: return not flow.resolve().is_empty(),
		func() -> bool: return flow.next_round(),
	])
	flow.lock_bets()
	# LOCKED (also invalid settle numbers)
	_expect_rejected(flow, [
		func() -> bool: return flow.lock_bets(),
		func() -> bool: return not flow.resolve().is_empty(),
		func() -> bool: return flow.next_round(),
		func() -> bool: return flow.ball_settled(-1),
		func() -> bool: return flow.ball_settled(38),
	])
	flow.ball_settled(37)
	# SETTLED
	_expect_rejected(flow, [
		func() -> bool: return flow.lock_bets(),
		func() -> bool: return flow.ball_settled(1),
		func() -> bool: return flow.next_round(),
	])
	var r := flow.resolve()
	assert_int(r["total_returned"]).is_equal(5 * 18)
	assert_int(flow.bankroll.balance).is_equal(1000 - 5 + 90)
	# RESOLVED
	_expect_rejected(flow, [
		func() -> bool: return flow.lock_bets(),
		func() -> bool: return flow.ball_settled(1),
		func() -> bool: return not flow.resolve().is_empty(),
	])
	assert_bool(flow.next_round()).is_true()
	assert_int(flow.phase).is_equal(RoundFlow.Phase.BETTING)
	assert_array(flow.history).contains_exactly([37])


func _expect_rejected(flow: RoundFlow, calls: Array) -> void:
	for f: Callable in calls:
		var snap := _snapshot(flow)
		var c := _count_signals(flow)
		assert_bool(f.call()).is_false()
		assert_dict(_snapshot(flow)).is_equal(snap)
		assert_int(c[0]).is_equal(0)
		_disconnect_all(flow)
