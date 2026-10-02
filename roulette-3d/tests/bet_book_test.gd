extends GdUnitTestSuite

func test_place_stacks_and_totals() -> void:
	var b := BetBook.new()
	assert_bool(b.place("straight_17", 5)).is_true()
	assert_bool(b.place("straight_17", 25)).is_true()
	assert_bool(b.place("red", 100)).is_true()
	assert_array(b.chips("straight_17")).is_equal([5, 25])
	assert_int(b.amount_on("straight_17")).is_equal(30)
	assert_int(b.total()).is_equal(130)

func test_rejects_bad_denomination() -> void:
	var b := BetBook.new()
	assert_bool(b.place("red", 3)).is_false()
	assert_bool(b.place("", 5)).is_false()
	assert_bool(b.is_empty()).is_true()

func test_remove_top_pops_last_chip() -> void:
	var b := BetBook.new()
	b.place("odd", 1)
	b.place("odd", 100)
	assert_int(b.remove_top("odd")).is_equal(100)
	assert_int(b.remove_top("odd")).is_equal(1)
	assert_int(b.remove_top("odd")).is_equal(0)
	assert_bool(b.is_empty()).is_true()

func test_clear() -> void:
	var b := BetBook.new()
	b.place("odd", 1)
	b.clear()
	assert_int(b.total()).is_equal(0)
	assert_dict(b.bets()).is_empty()
