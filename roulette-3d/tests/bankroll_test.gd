extends GdUnitTestSuite


func test_starts_at_1000() -> void:
	assert_int(Bankroll.STARTING).is_equal(1000)
	assert_int(Bankroll.new().balance).is_equal(1000)
	assert_int(Bankroll.new(250).balance).is_equal(250)


func test_negative_start_clamps_to_zero() -> void:
	assert_int(Bankroll.new(-5).balance).is_equal(0)


func test_debit_and_credit() -> void:
	var b := Bankroll.new()
	assert_bool(b.debit(100)).is_true()
	assert_int(b.balance).is_equal(900)
	assert_bool(b.credit(250)).is_true()
	assert_int(b.balance).is_equal(1150)


func test_cannot_overdraw() -> void:
	var b := Bankroll.new(100)
	assert_bool(b.can_afford(100)).is_true()
	assert_bool(b.can_afford(101)).is_false()
	assert_bool(b.debit(101)).is_false()
	assert_int(b.balance).is_equal(100)
	assert_bool(b.debit(100)).is_true()
	assert_int(b.balance).is_equal(0)
	assert_bool(b.debit(1)).is_false()
	assert_int(b.balance).is_equal(0)


func test_rejects_negative_amounts() -> void:
	var b := Bankroll.new()
	assert_bool(b.can_afford(-1)).is_false()
	assert_bool(b.debit(-50)).is_false()
	assert_bool(b.credit(-50)).is_false()
	assert_int(b.balance).is_equal(1000)


func test_balance_is_read_only() -> void:
	var b := Bankroll.new()
	b.balance = 5
	assert_int(b.balance).is_equal(1000)


func test_changed_signal() -> void:
	var b := Bankroll.new()
	var seen: Array[int] = []
	b.changed.connect(func(v: int) -> void: seen.append(v))
	b.debit(300)
	b.credit(50)
	b.debit(5000)  # rejected: no signal
	b.debit(0)     # no change: no signal
	b.credit(0)
	assert_array(seen).is_equal([700, 750])
