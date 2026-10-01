extends GdUnitTestSuite
## RoundFlow tests. spot_info is built here from hand-written wagers (no BetLayout dependency).

const T := Payouts.BetType
const P := RoundFlow.Phase
const DZ := 37

const RED := [1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36]
const BLACK := [2, 4, 6, 8, 10, 11, 13, 15, 17, 20, 22, 24, 26, 28, 29, 31, 33, 35]

var _spots: Dictionary = {}


func before() -> void:
	_spots.clear()
	for n in 38:
		_spots["straight_%d" % n] = {"type": T.STRAIGHT, "numbers": [n]}
	_spots["split_0_37"] = {"type": T.SPLIT, "numbers": [0, DZ]}
	_spots["five_number"] = {"type": T.FIVE_NUMBER, "numbers": [0, 1, 2, 3, DZ]}
	_spots["corner_1_2_4_5"] = {"type": T.CORNER, "numbers": [1, 2, 4, 5]}
	_spots["dozen_2"] = {"type": T.DOZEN, "numbers": range(13, 25)}
	_spots["red"] = {"type": T.RED, "numbers": RED}
	_spots["black"] = {"type": T.BLACK, "numbers": BLACK}
	# Malformed on purpose: must be rejected at placement.
	_spots["broken"] = {"type": T.SPLIT, "numbers": [1]}


func _info(id: String) -> Dictionary:
	return _spots.get(id, {})


func _flow(start: int = 1000) -> RoundFlow:
	return RoundFlow.new(_info, start)


## Lock, settle on n, resolve, next_round. Returns the resolve result.
func _spin(f: RoundFlow, n: int) -> Dictionary:
	assert_bool(f.lock_bets()).is_true()
	assert_bool(f.ball_settled(n)).is_true()
	var r := f.resolve()
	assert_bool(r.is_empty()).is_false()
	assert_bool(f.next_round()).is_true()
	return r


func test_happy_round_with_signals() -> void:
	var f := _flow()
	var phases: Array = []
	var balances: Array[int] = []
	var settled: Array[int] = []
	var results: Array = []
	var bets_events := [0]
	f.phase_changed.connect(func(p: int) -> void: phases.append(p))
	f.bankroll_changed.connect(func(b: int) -> void: balances.append(b))
	f.number_settled.connect(func(n: int) -> void: settled.append(n))
	f.round_resolved.connect(func(r: Dictionary) -> void: results.append(r))
	f.bets_changed.connect(func() -> void: bets_events[0] += 1)

	assert_int(f.phase).is_equal(P.BETTING)
	assert_bool(f.place_chip("straight_17", 25)).is_true()
	assert_bool(f.place_chip("red", 100)).is_true()
	assert_int(f.bankroll.balance).is_equal(875)
	assert_int(f.book.total()).is_equal(125)

	assert_bool(f.lock_bets()).is_true()
	assert_int(f.phase).is_equal(P.LOCKED)
	assert_bool(f.ball_settled(17)).is_true()
	assert_int(f.phase).is_equal(P.SETTLED)
	assert_int(f.winning_number).is_equal(17)
	# Money is not paid until resolve.
	assert_int(f.bankroll.balance).is_equal(875)

	var r := f.resolve()
	assert_int(f.phase).is_equal(P.RESOLVED)
	# 17 is black: straight returns 25 + 875 = 900, red swept.
	assert_int(r["total_returned"]).is_equal(900)
	assert_int(r["net"]).is_equal(775)
	assert_int(f.bankroll.balance).is_equal(1775)
	assert_bool(f.book.is_empty()).is_true()
	assert_dict(f.last_result).is_equal(r)

	assert_bool(f.next_round()).is_true()
	assert_int(f.phase).is_equal(P.BETTING)
	assert_int(f.winning_number).is_equal(-1)

	assert_array(phases).is_equal([P.LOCKED, P.SETTLED, P.RESOLVED, P.BETTING])
	assert_array(balances).is_equal([975, 875, 1775])
	assert_array(settled).is_equal([17])
	assert_int(results.size()).is_equal(1)
	assert_int(bets_events[0]).is_equal(3)  # two places + clear on resolve


func test_locked_rejects_place_remove_clear() -> void:
	var f := _flow()
	f.place_chip("red", 25)
	f.lock_bets()
	assert_bool(f.place_chip("red", 5)).is_false()
	assert_bool(f.remove_chip("red")).is_false()
	assert_bool(f.clear_bets()).is_false()
	assert_int(f.book.total()).is_equal(25)
	assert_int(f.bankroll.balance).is_equal(975)
	# Same after settle and after resolve.
	f.ball_settled(3)
	assert_bool(f.place_chip("red", 5)).is_false()
	assert_bool(f.remove_chip("red")).is_false()
	assert_bool(f.clear_bets()).is_false()
	f.resolve()
	assert_bool(f.place_chip("red", 5)).is_false()
	assert_bool(f.clear_bets()).is_false()
	assert_int(f.bankroll.balance).is_equal(1025)


func test_invalid_transitions_change_nothing() -> void:
	var f := _flow()
	var phases: Array = []
	f.phase_changed.connect(func(p: int) -> void: phases.append(p))
	# From BETTING.
	assert_bool(f.ball_settled(5)).is_false()
	assert_dict(f.resolve()).is_empty()
	assert_bool(f.next_round()).is_false()
	assert_int(f.phase).is_equal(P.BETTING)
	# From LOCKED.
	f.lock_bets()
	assert_bool(f.lock_bets()).is_false()
	assert_dict(f.resolve()).is_empty()
	assert_bool(f.next_round()).is_false()
	assert_int(f.phase).is_equal(P.LOCKED)
	# From SETTLED.
	f.ball_settled(5)
	assert_bool(f.lock_bets()).is_false()
	assert_bool(f.ball_settled(6)).is_false()
	assert_bool(f.next_round()).is_false()
	assert_int(f.phase).is_equal(P.SETTLED)
	assert_array(f.history).is_equal([5])
	# From RESOLVED.
	f.resolve()
	assert_bool(f.lock_bets()).is_false()
	assert_bool(f.ball_settled(7)).is_false()
	assert_dict(f.resolve()).is_empty()
	assert_int(f.phase).is_equal(P.RESOLVED)
	assert_array(f.history).is_equal([5])
	assert_array(phases).is_equal([P.LOCKED, P.SETTLED, P.RESOLVED])


func test_ball_settled_rejects_invalid_numbers() -> void:
	var f := _flow()
	f.lock_bets()
	assert_bool(f.ball_settled(-1)).is_false()
	assert_bool(f.ball_settled(38)).is_false()
	assert_int(f.phase).is_equal(P.LOCKED)
	assert_array(f.history).is_empty()
	assert_bool(f.ball_settled(DZ)).is_true()
	assert_array(f.history).is_equal([DZ])


func test_place_rejects_unknown_spot_bad_chip_and_malformed_spot() -> void:
	var f := _flow()
	assert_bool(f.place_chip("nope", 5)).is_false()
	assert_bool(f.place_chip("red", 3)).is_false()
	assert_bool(f.place_chip("red", 0)).is_false()
	assert_bool(f.place_chip("broken", 5)).is_false()
	assert_bool(f.place_chip("", 5)).is_false()
	assert_int(f.bankroll.balance).is_equal(1000)
	assert_bool(f.book.is_empty()).is_true()


func test_remove_and_clear_refund() -> void:
	var f := _flow()
	f.place_chip("red", 5)
	f.place_chip("red", 100)
	f.place_chip("straight_0", 25)
	assert_int(f.bankroll.balance).is_equal(870)
	assert_bool(f.remove_chip("red")).is_true()  # top chip = 100
	assert_int(f.bankroll.balance).is_equal(970)
	assert_array(f.book.chips("red")).is_equal([5])
	assert_bool(f.remove_chip("black")).is_false()  # empty spot
	assert_int(f.bankroll.balance).is_equal(970)
	assert_bool(f.clear_bets()).is_true()
	assert_int(f.bankroll.balance).is_equal(1000)
	assert_bool(f.book.is_empty()).is_true()
	assert_bool(f.clear_bets()).is_true()  # clearing an empty book is harmless
	assert_int(f.bankroll.balance).is_equal(1000)


func test_affordability() -> void:
	var f := _flow()
	for i in 10:
		assert_bool(f.place_chip("red", 100)).is_true()
	assert_int(f.bankroll.balance).is_equal(0)
	assert_bool(f.place_chip("red", 1)).is_false()
	assert_bool(f.place_chip("black", 1)).is_false()
	assert_int(f.book.total()).is_equal(1000)
	f.remove_chip("red")
	assert_int(f.bankroll.balance).is_equal(100)
	assert_bool(f.place_chip("black", 100)).is_true()
	assert_bool(f.place_chip("black", 1)).is_false()


func test_partial_affordability() -> void:
	var f := _flow(30)
	assert_bool(f.place_chip("red", 100)).is_false()
	assert_bool(f.place_chip("red", 25)).is_true()
	assert_bool(f.place_chip("red", 25)).is_false()
	assert_bool(f.place_chip("red", 5)).is_true()
	assert_bool(f.place_chip("red", 1)).is_false()
	assert_int(f.bankroll.balance).is_equal(0)


func test_zero_bets_can_spin() -> void:
	var f := _flow()
	var r := _spin(f, 0)
	assert_int(r["total_staked"]).is_equal(0)
	assert_int(r["net"]).is_equal(0)
	assert_int(f.bankroll.balance).is_equal(1000)
	assert_array(f.history).is_equal([0])


func test_multi_round_bankroll_math() -> void:
	var f := _flow()
	# Round 1: five-number 100 + red 25 (875 left). Ball 00: five-number pays 6:1 -> 700.
	assert_bool(f.place_chip("five_number", 100)).is_true()
	assert_bool(f.place_chip("red", 25)).is_true()
	assert_int(f.bankroll.balance).is_equal(875)
	var r := _spin(f, DZ)
	assert_int(r["total_returned"]).is_equal(700)
	assert_int(r["net"]).is_equal(575)
	assert_int(f.bankroll.balance).is_equal(1575)

	# Round 2: straight 17 x30 (25+5), dozen_2 x100 (1445 left). Ball 17: 1080 + 300.
	f.place_chip("straight_17", 25)
	f.place_chip("straight_17", 5)
	f.place_chip("dozen_2", 100)
	assert_int(f.bankroll.balance).is_equal(1445)
	r = _spin(f, 17)
	assert_int(r["total_returned"]).is_equal(1380)
	assert_int(f.bankroll.balance).is_equal(2825)

	# Round 3: split 0-00 x5 (2820 left). Ball 0: 5 + 85 = 90.
	f.place_chip("split_0_37", 5)
	r = _spin(f, 0)
	assert_int(r["total_returned"]).is_equal(90)
	assert_int(f.bankroll.balance).is_equal(2910)

	# Round 4: all-in on black (29 x 100 + 1 x 5 + 1 x 5). Ball 0: black loses, bankroll 0.
	for i in 29:
		assert_bool(f.place_chip("black", 100)).is_true()
	assert_bool(f.place_chip("black", 5)).is_true()
	assert_bool(f.place_chip("black", 5)).is_true()
	assert_int(f.bankroll.balance).is_equal(0)
	r = _spin(f, 0)
	assert_int(r["total_staked"]).is_equal(2910)
	assert_int(r["total_returned"]).is_equal(0)
	assert_int(r["net"]).is_equal(-2910)
	assert_int(f.bankroll.balance).is_equal(0)

	# Round 5: broke. Betting is rejected, but a no-bet spin still works.
	assert_bool(f.place_chip("red", 1)).is_false()
	assert_bool(f.book.is_empty()).is_true()
	r = _spin(f, 22)
	assert_int(r["net"]).is_equal(0)
	assert_int(f.bankroll.balance).is_equal(0)
	assert_int(f.phase).is_equal(P.BETTING)

	# History is oldest first.
	assert_array(f.history).is_equal([DZ, 17, 0, 0, 22])


func test_five_number_round_on_each_member() -> void:
	for n in [0, DZ, 1, 2, 3]:
		var f := _flow()
		f.place_chip("five_number", 5)
		var r := _spin(f, n)
		assert_int(r["total_returned"]).is_equal(35)
		assert_int(f.bankroll.balance).is_equal(1030)
	var g := _flow()
	g.place_chip("five_number", 5)
	_spin(g, 4)
	assert_int(g.bankroll.balance).is_equal(995)


func test_wagers_built_from_book_and_spot_info() -> void:
	var f := _flow()
	f.place_chip("corner_1_2_4_5", 25)
	f.place_chip("corner_1_2_4_5", 1)
	var ws := f.wagers()
	assert_int(ws.size()).is_equal(1)
	assert_dict(ws[0]).is_equal({"id": "corner_1_2_4_5", "type": T.CORNER, "numbers": [1, 2, 4, 5], "amount": 26})
