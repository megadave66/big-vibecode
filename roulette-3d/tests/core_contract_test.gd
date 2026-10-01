extends GdUnitTestSuite

func test_pocket_order_has_38_unique_valid_numbers() -> void:
	var seen := {}
	for n in WheelLayout.POCKET_ORDER:
		assert_bool(WheelLayout.is_valid(n)).is_true()
		seen[n] = true
	assert_int(WheelLayout.POCKET_ORDER.size()).is_equal(38)
	assert_int(seen.size()).is_equal(38)

func test_zero_and_double_zero_are_opposite() -> void:
	assert_int(WheelLayout.index_of(0)).is_equal(0)
	assert_int(WheelLayout.index_of(WheelLayout.DOUBLE_ZERO)).is_equal(19)

func test_american_wheel_alternates_colour_except_greens() -> void:
	var order := WheelLayout.POCKET_ORDER
	for i in order.size():
		var a := order[i]
		var b := order[(i + 1) % order.size()]
		if WheelLayout.color_of(a) == WheelLayout.PocketColor.GREEN or WheelLayout.color_of(b) == WheelLayout.PocketColor.GREEN:
			continue
		assert_int(WheelLayout.color_of(a)).is_not_equal(WheelLayout.color_of(b))

func test_american_wheel_number_opposite_pairs() -> void:
	# On the American wheel each number sits opposite its "consecutive" partner: 1-2, 3-4, ... 35-36.
	for n in range(1, 37, 2):
		var i := WheelLayout.index_of(n)
		assert_int(WheelLayout.number_at(i + 19)).is_equal(n + 1)

func test_labels_and_colours() -> void:
	assert_str(WheelLayout.label(37)).is_equal("00")
	assert_int(WheelLayout.color_of(0)).is_equal(WheelLayout.PocketColor.GREEN)
	assert_int(WheelLayout.color_of(1)).is_equal(WheelLayout.PocketColor.RED)
	assert_int(WheelLayout.color_of(2)).is_equal(WheelLayout.PocketColor.BLACK)
	assert_int(WheelLayout.RED_NUMBERS.size()).is_equal(18)

func test_payout_table() -> void:
	assert_int(Payouts.odds(Payouts.BetType.STRAIGHT)).is_equal(35)
	assert_int(Payouts.odds(Payouts.BetType.SPLIT)).is_equal(17)
	assert_int(Payouts.odds(Payouts.BetType.STREET)).is_equal(11)
	assert_int(Payouts.odds(Payouts.BetType.TRIO)).is_equal(11)
	assert_int(Payouts.odds(Payouts.BetType.CORNER)).is_equal(8)
	assert_int(Payouts.odds(Payouts.BetType.FIVE_NUMBER)).is_equal(6)
	assert_int(Payouts.odds(Payouts.BetType.SIX_LINE)).is_equal(5)
	assert_int(Payouts.odds(Payouts.BetType.DOZEN)).is_equal(2)
	assert_int(Payouts.odds(Payouts.BetType.COLUMN)).is_equal(2)
	for t in [Payouts.BetType.RED, Payouts.BetType.BLACK, Payouts.BetType.EVEN, Payouts.BetType.ODD, Payouts.BetType.LOW, Payouts.BetType.HIGH]:
		assert_int(Payouts.odds(t)).is_equal(1)
	assert_str(Payouts.odds_text(Payouts.BetType.STRAIGHT)).is_equal("35:1")
	assert_int(Payouts.winnings(Payouts.BetType.FIVE_NUMBER, 5)).is_equal(30)
