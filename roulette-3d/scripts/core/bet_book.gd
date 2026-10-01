class_name BetBook
extends RefCounted
## Chips on bet spots. Pure stacking; no bankroll or phase rules here (RoundFlow owns those).
## Each spot holds a stack (Array[int]) of chip denominations, bottom first.

signal changed

const DENOMINATIONS: Array[int] = [1, 5, 25, 100]

var _stacks: Dictionary = {}  # spot_id (String) -> Array[int]


static func is_denomination(value: int) -> bool:
	return DENOMINATIONS.has(value)


## Push one chip on a spot. Returns false for an invalid denomination or empty id.
func place(spot_id: String, chip: int) -> bool:
	if spot_id.is_empty() or not is_denomination(chip):
		return false
	if not _stacks.has(spot_id):
		_stacks[spot_id] = [] as Array[int]
	(_stacks[spot_id] as Array).append(chip)
	changed.emit()
	return true


## Pop the top chip from a spot. Returns its value, or 0 if the spot is empty.
func remove_top(spot_id: String) -> int:
	if not _stacks.has(spot_id):
		return 0
	var stack: Array = _stacks[spot_id]
	var chip: int = stack.pop_back()
	if stack.is_empty():
		_stacks.erase(spot_id)
	changed.emit()
	return chip


func chips(spot_id: String) -> Array[int]:
	var out: Array[int] = []
	if _stacks.has(spot_id):
		out.assign(_stacks[spot_id])
	return out


func amount_on(spot_id: String) -> int:
	var sum := 0
	for c in chips(spot_id):
		sum += c
	return sum


func total() -> int:
	var sum := 0
	for id in _stacks:
		sum += amount_on(id)
	return sum


func spot_ids() -> Array[String]:
	var out: Array[String] = []
	for id in _stacks:
		out.append(id)
	return out


## spot_id -> total amount, for every spot with chips.
func bets() -> Dictionary:
	var out := {}
	for id in _stacks:
		out[id] = amount_on(id)
	return out


func is_empty() -> bool:
	return _stacks.is_empty()


func clear() -> void:
	if _stacks.is_empty():
		return
	_stacks.clear()
	changed.emit()
