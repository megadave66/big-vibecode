class_name Bankroll
extends RefCounted
## Session-only player money in whole dollars. Can never go negative.

signal changed(balance: int)

const STARTING := 1000

## Read-only. Use debit() and credit() to change it.
var balance: int:
	get:
		return _balance
	set(_value):
		push_error("Bankroll.balance is read-only; use debit()/credit()")

var _balance: int = STARTING


func _init(start: int = STARTING) -> void:
	_balance = maxi(start, 0)


func can_afford(amount: int) -> bool:
	return amount >= 0 and amount <= _balance


## Take money out. Rejects negative amounts and overdrafts (returns false, no change).
func debit(amount: int) -> bool:
	if not can_afford(amount):
		return false
	if amount == 0:
		return true
	_balance -= amount
	changed.emit(_balance)
	return true


## Add money. Rejects negative amounts (returns false, no change).
func credit(amount: int) -> bool:
	if amount < 0:
		return false
	if amount == 0:
		return true
	_balance += amount
	changed.emit(_balance)
	return true
