class_name RoundFlow
extends RefCounted
## One roulette round as a phase machine. Pure logic, no scene dependency.
##
## BETTING  -> lock_bets()      -> LOCKED   ("no more bets": place/remove/clear rejected)
## LOCKED   -> ball_settled(n)  -> SETTLED  (n recorded and appended to history)
## SETTLED  -> resolve()        -> RESOLVED (total_returned credited, book cleared)
## RESOLVED -> next_round()     -> BETTING
## Any other call is rejected: it returns false (or {}) and changes nothing.
##
## Money: a chip is debited when placed and refunded when removed (BETTING only).
## Losing chips are swept. No table minimum; a round with zero bets may spin.
## spot_info(id) -> {"type": Payouts.BetType, "numbers": Array[int]} or {} for an unknown spot.

signal phase_changed(phase: Phase)
signal bets_changed
signal bankroll_changed(balance: int)
signal number_settled(number: int)
signal round_resolved(result: Dictionary)

enum Phase { BETTING, LOCKED, SETTLED, RESOLVED }

var phase: Phase = Phase.BETTING
var bankroll: Bankroll
var book: BetBook
## Winning numbers, oldest first.
var history: Array[int] = []
## Winning number of the current round, or -1 before the ball settles.
var winning_number: int = -1
## Resolver result of the most recent resolve(), or {}.
var last_result: Dictionary = {}

var _spot_info: Callable


func _init(spot_info: Callable, start_bankroll: int = Bankroll.STARTING) -> void:
	_spot_info = spot_info
	bankroll = Bankroll.new(start_bankroll)
	book = BetBook.new()
	# Bound methods, not lambdas: a lambda capturing self would make a reference cycle.
	bankroll.changed.connect(_on_bankroll_changed)
	book.changed.connect(_on_book_changed)


func is_betting_open() -> bool:
	return phase == Phase.BETTING


## Spot info for an id, or {} if unknown or malformed.
func info_for(spot_id: String) -> Dictionary:
	if not _spot_info.is_valid():
		return {}
	var info: Variant = _spot_info.call(spot_id)
	if typeof(info) != TYPE_DICTIONARY or (info as Dictionary).is_empty():
		return {}
	if not Resolver.is_valid_shape(info.get("type", -1), info.get("numbers", [])):
		return {}
	return info


func place_chip(spot_id: String, chip: int) -> bool:
	if phase != Phase.BETTING or not BetBook.is_denomination(chip):
		return false
	if info_for(spot_id).is_empty() or not bankroll.can_afford(chip):
		return false
	if not bankroll.debit(chip):
		return false
	if not book.place(spot_id, chip):
		bankroll.credit(chip)
		return false
	return true


## Remove the top chip from a spot and refund it.
func remove_chip(spot_id: String) -> bool:
	if phase != Phase.BETTING:
		return false
	var chip := book.remove_top(spot_id)
	if chip <= 0:
		return false
	bankroll.credit(chip)
	return true


## Remove every chip and refund the total. Returns false outside BETTING.
func clear_bets() -> bool:
	if phase != Phase.BETTING:
		return false
	var total := book.total()
	book.clear()
	bankroll.credit(total)
	return true


## "No more bets". Allowed with zero bets.
func lock_bets() -> bool:
	if phase != Phase.BETTING:
		return false
	_set_phase(Phase.LOCKED)
	return true


func ball_settled(number: int) -> bool:
	if phase != Phase.LOCKED or not WheelLayout.is_valid(number):
		return false
	winning_number = number
	history.append(number)
	number_settled.emit(number)
	_set_phase(Phase.SETTLED)
	return true


## Current bets as Resolver wagers.
func wagers() -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	var bets := book.bets()
	for id: String in bets:
		var info := info_for(id)
		out.append({
			"id": id,
			"type": info.get("type", -1),
			"numbers": info.get("numbers", []),
			"amount": bets[id],
		})
	return out


## Settle the round. Returns the Resolver result, or {} outside SETTLED.
func resolve() -> Dictionary:
	if phase != Phase.SETTLED:
		return {}
	var result := Resolver.resolve(wagers(), winning_number)
	bankroll.credit(result["total_returned"])
	book.clear()
	last_result = result
	_set_phase(Phase.RESOLVED)
	round_resolved.emit(result)
	return result


func next_round() -> bool:
	if phase != Phase.RESOLVED:
		return false
	winning_number = -1
	_set_phase(Phase.BETTING)
	return true


func _on_bankroll_changed(balance: int) -> void:
	bankroll_changed.emit(balance)


func _on_book_changed() -> void:
	bets_changed.emit()


func _set_phase(p: Phase) -> void:
	phase = p
	phase_changed.emit(p)
