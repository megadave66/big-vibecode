extends GdUnitTestSuite
## Doctest-style checks of every resolution. Expected amounts are written out by hand
## (stake + stake * N) so a wrong odds table or wrong win rule fails here.

const T := Payouts.BetType
const DZ := 37  # "00"

const RED := [1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36]
const BLACK := [2, 4, 6, 8, 10, 11, 13, 15, 17, 20, 22, 24, 26, 28, 29, 31, 33, 35]


static func _w(id: String, type: int, numbers: Array, amount: int) -> Dictionary:
	return {"id": id, "type": type, "numbers": numbers, "amount": amount}


static func _range(a: int, b: int, step: int = 1) -> Array:
	return range(a, b + 1, step)


static func _evens() -> Array:
	return _range(2, 36, 2)


static func _odds() -> Array:
	return _range(1, 35, 2)


## Resolve one wager and check returned and winnings. expected_returned 0 = loss.
func _check(type: int, numbers: Array, amount: int, winning: int, expected_returned: int) -> void:
	var r := Resolver.resolve([_w("x", type, numbers, amount)], winning)
	assert_int(r["total_staked"]).is_equal(amount)
	assert_int(r["total_returned"]).is_equal(expected_returned)
	assert_int(r["net"]).is_equal(expected_returned - amount)
	assert_array(r["refunded"]).is_empty()
	if expected_returned > 0:
		assert_int(r["winners"].size()).is_equal(1)
		assert_int(r["losers"].size()).is_equal(0)
		assert_int(r["winners"][0]["returned"]).is_equal(expected_returned)
		assert_int(r["winners"][0]["winnings"]).is_equal(expected_returned - amount)
		assert_int(r["winners"][0]["amount"]).is_equal(amount)
	else:
		assert_int(r["winners"].size()).is_equal(0)
		assert_int(r["losers"].size()).is_equal(1)
		assert_int(r["losers"][0]["amount"]).is_equal(amount)


# --- odds come from Payouts (single source) --------------------------------

func test_payout_table_values() -> void:
	assert_int(Payouts.odds(T.STRAIGHT)).is_equal(35)
	assert_int(Payouts.odds(T.SPLIT)).is_equal(17)
	assert_int(Payouts.odds(T.STREET)).is_equal(11)
	assert_int(Payouts.odds(T.TRIO)).is_equal(11)
	assert_int(Payouts.odds(T.CORNER)).is_equal(8)
	assert_int(Payouts.odds(T.FIVE_NUMBER)).is_equal(6)
	assert_int(Payouts.odds(T.SIX_LINE)).is_equal(5)
	assert_int(Payouts.odds(T.DOZEN)).is_equal(2)
	assert_int(Payouts.odds(T.COLUMN)).is_equal(2)
	for t in [T.RED, T.BLACK, T.EVEN, T.ODD, T.LOW, T.HIGH]:
		assert_int(Payouts.odds(t)).is_equal(1)


# --- straights ---------------------------------------------------------------

func test_straight_win_and_loss() -> void:
	_check(T.STRAIGHT, [17], 10, 17, 360)
	_check(T.STRAIGHT, [17], 10, 18, 0)
	_check(T.STRAIGHT, [36], 1, 36, 36)


func test_straight_on_zero_and_double_zero() -> void:
	_check(T.STRAIGHT, [0], 5, 0, 180)
	_check(T.STRAIGHT, [0], 5, DZ, 0)
	_check(T.STRAIGHT, [DZ], 5, DZ, 180)
	_check(T.STRAIGHT, [DZ], 5, 0, 0)


func test_every_straight_wins_only_on_its_number() -> void:
	for n in 38:
		for w in 38:
			var r := Resolver.resolve([_w("s", T.STRAIGHT, [n], 1)], w)
			assert_int(r["total_returned"]).is_equal(36 if n == w else 0)


# --- splits ------------------------------------------------------------------

func test_split_regular() -> void:
	_check(T.SPLIT, [8, 11], 10, 8, 180)
	_check(T.SPLIT, [8, 11], 10, 11, 180)
	_check(T.SPLIT, [8, 11], 10, 9, 0)
	_check(T.SPLIT, [35, 36], 5, 36, 90)


func test_zero_splits() -> void:
	# 0-00
	_check(T.SPLIT, [0, DZ], 10, 0, 180)
	_check(T.SPLIT, [0, DZ], 10, DZ, 180)
	_check(T.SPLIT, [0, DZ], 10, 1, 0)
	# 0-1
	_check(T.SPLIT, [0, 1], 10, 0, 180)
	_check(T.SPLIT, [0, 1], 10, 1, 180)
	_check(T.SPLIT, [0, 1], 10, DZ, 0)
	# 0-2
	_check(T.SPLIT, [0, 2], 10, 2, 180)
	_check(T.SPLIT, [0, 2], 10, 0, 180)
	_check(T.SPLIT, [0, 2], 10, 3, 0)
	# 00-2
	_check(T.SPLIT, [2, DZ], 10, DZ, 180)
	_check(T.SPLIT, [2, DZ], 10, 2, 180)
	_check(T.SPLIT, [2, DZ], 10, 0, 0)
	# 00-3
	_check(T.SPLIT, [3, DZ], 10, DZ, 180)
	_check(T.SPLIT, [3, DZ], 10, 3, 180)
	_check(T.SPLIT, [3, DZ], 10, 0, 0)
	_check(T.SPLIT, [3, DZ], 10, 2, 0)


# --- streets and trios -------------------------------------------------------

func test_streets() -> void:
	_check(T.STREET, [1, 2, 3], 10, 1, 120)
	_check(T.STREET, [1, 2, 3], 10, 3, 120)
	_check(T.STREET, [1, 2, 3], 10, 0, 0)
	_check(T.STREET, [1, 2, 3], 10, 4, 0)
	_check(T.STREET, [34, 35, 36], 25, 35, 300)
	_check(T.STREET, [34, 35, 36], 25, 33, 0)


func test_trios() -> void:
	# 0-1-2
	_check(T.TRIO, [0, 1, 2], 5, 0, 60)
	_check(T.TRIO, [0, 1, 2], 5, 1, 60)
	_check(T.TRIO, [0, 1, 2], 5, 2, 60)
	_check(T.TRIO, [0, 1, 2], 5, DZ, 0)
	_check(T.TRIO, [0, 1, 2], 5, 3, 0)
	# 0-00-2
	_check(T.TRIO, [0, 2, DZ], 5, 0, 60)
	_check(T.TRIO, [0, 2, DZ], 5, DZ, 60)
	_check(T.TRIO, [0, 2, DZ], 5, 2, 60)
	_check(T.TRIO, [0, 2, DZ], 5, 1, 0)
	# 00-2-3
	_check(T.TRIO, [2, 3, DZ], 5, DZ, 60)
	_check(T.TRIO, [2, 3, DZ], 5, 2, 60)
	_check(T.TRIO, [2, 3, DZ], 5, 3, 60)
	_check(T.TRIO, [2, 3, DZ], 5, 0, 0)
	_check(T.TRIO, [2, 3, DZ], 5, 1, 0)


# --- corners, five-number, six-line -------------------------------------------

func test_corners() -> void:
	_check(T.CORNER, [1, 2, 4, 5], 10, 1, 90)
	_check(T.CORNER, [1, 2, 4, 5], 10, 5, 90)
	_check(T.CORNER, [1, 2, 4, 5], 10, 3, 0)
	_check(T.CORNER, [32, 33, 35, 36], 5, 36, 45)
	_check(T.CORNER, [32, 33, 35, 36], 5, 34, 0)


func test_five_number_wins_on_each_member_at_6_to_1() -> void:
	var five := [0, 1, 2, 3, DZ]
	for n in five:
		_check(T.FIVE_NUMBER, five, 10, n, 70)
		_check(T.FIVE_NUMBER, five, 1, n, 7)


func test_five_number_loses_elsewhere() -> void:
	var five := [0, 1, 2, 3, DZ]
	_check(T.FIVE_NUMBER, five, 10, 4, 0)
	_check(T.FIVE_NUMBER, five, 10, 36, 0)
	for n in range(4, 37):
		_check(T.FIVE_NUMBER, five, 10, n, 0)


func test_six_lines() -> void:
	_check(T.SIX_LINE, [1, 2, 3, 4, 5, 6], 10, 1, 60)
	_check(T.SIX_LINE, [1, 2, 3, 4, 5, 6], 10, 6, 60)
	_check(T.SIX_LINE, [1, 2, 3, 4, 5, 6], 10, 7, 0)
	_check(T.SIX_LINE, [1, 2, 3, 4, 5, 6], 10, 0, 0)
	_check(T.SIX_LINE, [31, 32, 33, 34, 35, 36], 25, 31, 150)
	_check(T.SIX_LINE, [31, 32, 33, 34, 35, 36], 25, 30, 0)


# --- dozens and columns ---------------------------------------------------------

func test_dozen_boundaries() -> void:
	var d1 := _range(1, 12)
	var d2 := _range(13, 24)
	var d3 := _range(25, 36)
	_check(T.DOZEN, d1, 10, 1, 30)
	_check(T.DOZEN, d1, 10, 12, 30)
	_check(T.DOZEN, d1, 10, 13, 0)
	_check(T.DOZEN, d2, 10, 12, 0)
	_check(T.DOZEN, d2, 10, 13, 30)
	_check(T.DOZEN, d2, 10, 24, 30)
	_check(T.DOZEN, d2, 10, 25, 0)
	_check(T.DOZEN, d3, 10, 24, 0)
	_check(T.DOZEN, d3, 10, 25, 30)
	_check(T.DOZEN, d3, 10, 36, 30)


func test_column_boundaries() -> void:
	var c1 := _range(1, 34, 3)   # 1,4,...,34
	var c2 := _range(2, 35, 3)   # 2,5,...,35
	var c3 := _range(3, 36, 3)   # 3,6,...,36
	_check(T.COLUMN, c1, 10, 34, 30)
	_check(T.COLUMN, c1, 10, 35, 0)
	_check(T.COLUMN, c1, 10, 36, 0)
	_check(T.COLUMN, c2, 10, 34, 0)
	_check(T.COLUMN, c2, 10, 35, 30)
	_check(T.COLUMN, c2, 10, 36, 0)
	_check(T.COLUMN, c3, 10, 34, 0)
	_check(T.COLUMN, c3, 10, 35, 0)
	_check(T.COLUMN, c3, 10, 36, 30)
	_check(T.COLUMN, c1, 10, 1, 30)
	_check(T.COLUMN, c3, 10, 3, 30)


# --- even money ---------------------------------------------------------------

func test_even_money_wins_and_losses() -> void:
	_check(T.RED, RED, 20, 1, 40)
	_check(T.RED, RED, 20, 2, 0)
	_check(T.BLACK, BLACK, 20, 2, 40)
	_check(T.BLACK, BLACK, 20, 1, 0)
	_check(T.EVEN, _evens(), 20, 36, 40)
	_check(T.EVEN, _evens(), 20, 35, 0)
	_check(T.ODD, _odds(), 20, 35, 40)
	_check(T.ODD, _odds(), 20, 36, 0)
	_check(T.LOW, _range(1, 18), 20, 18, 40)
	_check(T.LOW, _range(1, 18), 20, 19, 0)
	_check(T.HIGH, _range(19, 36), 20, 19, 40)
	_check(T.HIGH, _range(19, 36), 20, 18, 0)


func test_zero_and_double_zero_lose_all_outside_bets() -> void:
	var outside := [
		_w("red", T.RED, RED, 10),
		_w("black", T.BLACK, BLACK, 10),
		_w("even", T.EVEN, _evens(), 10),
		_w("odd", T.ODD, _odds(), 10),
		_w("low", T.LOW, _range(1, 18), 10),
		_w("high", T.HIGH, _range(19, 36), 10),
		_w("dozen_1", T.DOZEN, _range(1, 12), 10),
		_w("dozen_2", T.DOZEN, _range(13, 24), 10),
		_w("dozen_3", T.DOZEN, _range(25, 36), 10),
		_w("column_1", T.COLUMN, _range(1, 34, 3), 10),
		_w("column_2", T.COLUMN, _range(2, 35, 3), 10),
		_w("column_3", T.COLUMN, _range(3, 36, 3), 10),
	]
	for z in [0, DZ]:
		var r := Resolver.resolve(outside, z)
		assert_int(r["total_staked"]).is_equal(120)
		assert_int(r["total_returned"]).is_equal(0)
		assert_int(r["net"]).is_equal(-120)
		assert_array(r["winners"]).is_empty()
		assert_int(r["losers"].size()).is_equal(12)


# --- full board sweep ---------------------------------------------------------

## Fixed board, 100 staked in total:
## straight 17 x10, split 0-00 x5, five-number x5, red x20, odd x10, dozen_1 x10,
## column_1 x10, corner 1-2-4-5 x5, high x25.
static func _board() -> Array:
	return [
		_w("straight_17", T.STRAIGHT, [17], 10),
		_w("split_0_37", T.SPLIT, [0, DZ], 5),
		_w("five_number", T.FIVE_NUMBER, [0, 1, 2, 3, DZ], 5),
		_w("red", T.RED, RED, 20),
		_w("odd", T.ODD, _odds(), 10),
		_w("dozen_1", T.DOZEN, _range(1, 12), 10),
		_w("column_1", T.COLUMN, _range(1, 34, 3), 10),
		_w("corner_1_2_4_5", T.CORNER, [1, 2, 4, 5], 5),
		_w("high", T.HIGH, _range(19, 36), 25),
	]


## Hand-computed net for winning numbers 0..37 (index 37 = "00").
const EXPECTED_NET := [
	25, 100, 10, 25, 5, 35, -70, 20, -70, -10,      # 0..9
	-40, -50, -30, -50, -60, -80, -30, 280, -60, 40, # 10..19
	-50, 10, -20, 10, -50, 40, -50, 10, -20, -30,    # 20..29
	-10, 0, -10, -30, 20, -30, -10,                  # 30..36
	25,                                              # 00
]


func test_full_sweep_fixed_board() -> void:
	var board := _board()
	for n in 38:
		var r := Resolver.resolve(board, n)
		assert_int(r["total_staked"]).is_equal(100)
		assert_int(r["net"]) \
			.override_failure_message("winning %s: net %d, expected %d" % [WheelLayout.label(n), r["net"], EXPECTED_NET[n]]) \
			.is_equal(EXPECTED_NET[n])
		assert_int(r["winners"].size() + r["losers"].size()).is_equal(board.size())
		assert_int(r["winning"]).is_equal(n)


func test_sweep_detail_on_one() -> void:
	# 1 is red, odd, 1st dozen, column 1, corner 1-2-4-5, five-number.
	var r := Resolver.resolve(_board(), 1)
	var by_id := {}
	for w in r["winners"]:
		by_id[w["id"]] = w["returned"]
	assert_dict(by_id).is_equal({
		"five_number": 35, "red": 40, "odd": 20, "dozen_1": 30, "column_1": 30, "corner_1_2_4_5": 45,
	})
	assert_int(r["total_returned"]).is_equal(200)


func test_empty_wagers() -> void:
	var r := Resolver.resolve([], 5)
	assert_int(r["total_staked"]).is_equal(0)
	assert_int(r["total_returned"]).is_equal(0)
	assert_int(r["net"]).is_equal(0)


# --- defensive validation -------------------------------------------------------

func test_shape_validation() -> void:
	assert_bool(Resolver.is_valid_shape(T.STRAIGHT, [5])).is_true()
	assert_bool(Resolver.is_valid_shape(T.STRAIGHT, [5, 6])).is_false()
	assert_bool(Resolver.is_valid_shape(T.SPLIT, [5])).is_false()
	assert_bool(Resolver.is_valid_shape(T.SPLIT, [5, 5])).is_false()
	assert_bool(Resolver.is_valid_shape(T.STREET, [1, 2, 3])).is_true()
	assert_bool(Resolver.is_valid_shape(T.TRIO, [0, 1])).is_false()
	assert_bool(Resolver.is_valid_shape(T.CORNER, [1, 2, 4])).is_false()
	assert_bool(Resolver.is_valid_shape(T.FIVE_NUMBER, [0, 1, 2, 3])).is_false()
	assert_bool(Resolver.is_valid_shape(T.SIX_LINE, [1, 2, 3, 4, 5])).is_false()
	assert_bool(Resolver.is_valid_shape(T.DOZEN, _range(1, 11))).is_false()
	assert_bool(Resolver.is_valid_shape(T.COLUMN, _range(1, 34, 3))).is_true()
	assert_bool(Resolver.is_valid_shape(T.RED, RED)).is_true()
	assert_bool(Resolver.is_valid_shape(T.EVEN, _range(2, 34, 2))).is_false()
	assert_bool(Resolver.is_valid_shape(T.STRAIGHT, [38])).is_false()
	assert_bool(Resolver.is_valid_shape(T.STRAIGHT, [-1])).is_false()
	assert_bool(Resolver.is_valid_shape(99, [1])).is_false()
	assert_bool(Resolver.is_valid_shape(T.STRAIGHT, [])).is_false()


func test_invalid_wagers_are_refunded_not_paid() -> void:
	var bad := [
		_w("zero_amount", T.STRAIGHT, [5], 0),
		_w("neg_amount", T.STRAIGHT, [5], -10),
		_w("no_numbers", T.STRAIGHT, [], 10),
		_w("wrong_count", T.SPLIT, [5], 10),
		_w("out_of_range", T.STRAIGHT, [40], 10),
	]
	var r := Resolver.resolve(bad + [_w("good", T.STRAIGHT, [5], 1)], 5)
	assert_int(r["refunded"].size()).is_equal(5)
	assert_int(r["winners"].size()).is_equal(1)
	# Refunds: 0 + 0 + 10 + 10 + 10 back, winnings 0. Good bet returns 36.
	assert_int(r["total_staked"]).is_equal(31)
	assert_int(r["total_returned"]).is_equal(66)
	assert_int(r["net"]).is_equal(35)
	for f in r["refunded"]:
		assert_int(f["winnings"]).is_equal(0)
		assert_int(f["returned"]).is_equal(f["amount"])


func test_invalid_winning_number_refunds_everything() -> void:
	var r := Resolver.resolve(_board(), 38)
	assert_int(r["net"]).is_equal(0)
	assert_int(r["total_returned"]).is_equal(100)
	assert_array(r["winners"]).is_empty()
	assert_array(r["losers"]).is_empty()
	r = Resolver.resolve(_board(), -1)
	assert_int(r["net"]).is_equal(0)
