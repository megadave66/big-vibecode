class_name Resolver
extends RefCounted
## Settles a list of wagers against one winning number. Pure function, no state.
##
## A wager is {"id": String, "type": Payouts.BetType, "numbers": Array[int], "amount": int}.
## A wager wins if and only if the winning number is in wager.numbers.
## A winning wager returns stake + Payouts.winnings(type, stake). A losing wager returns 0 (swept).
##
## Invalid wagers (amount <= 0, unknown type, empty numbers, numbers out of 0..37, duplicate
## numbers, or a number count that does not fit the type) should never reach here: RoundFlow
## rejects them at placement. If one does, Resolver calls push_error() and REFUNDS it:
## it goes in "refunded" with returned = amount and winnings = 0, so the player loses nothing.
## A non-positive amount is refunded as 0 and adds 0 to total_staked.
## If the winning number itself is invalid, every wager is refunded the same way.
##
## Result keys: "winning", "winners", "losers", "refunded", "total_staked", "total_returned", "net".
## returned = amount + winnings; net = total_returned - total_staked.

## How many numbers each bet type must cover.
const NUMBER_COUNT := {
	Payouts.BetType.STRAIGHT: 1,
	Payouts.BetType.SPLIT: 2,
	Payouts.BetType.STREET: 3,
	Payouts.BetType.TRIO: 3,
	Payouts.BetType.CORNER: 4,
	Payouts.BetType.FIVE_NUMBER: 5,
	Payouts.BetType.SIX_LINE: 6,
	Payouts.BetType.DOZEN: 12,
	Payouts.BetType.COLUMN: 12,
	Payouts.BetType.RED: 18,
	Payouts.BetType.BLACK: 18,
	Payouts.BetType.EVEN: 18,
	Payouts.BetType.ODD: 18,
	Payouts.BetType.LOW: 18,
	Payouts.BetType.HIGH: 18,
}


## True if the type is known and the numbers fit it (count, range 0..37, no duplicates).
static func is_valid_shape(type: Variant, numbers: Variant) -> bool:
	if typeof(type) != TYPE_INT or not NUMBER_COUNT.has(type):
		return false
	if typeof(numbers) != TYPE_ARRAY:
		return false
	var arr: Array = numbers
	if arr.size() != NUMBER_COUNT[type]:
		return false
	var seen := {}
	for n in arr:
		if typeof(n) != TYPE_INT or not WheelLayout.is_valid(n) or seen.has(n):
			return false
		seen[n] = true
	return true


## True if the wager is well-formed and has a positive int amount.
static func is_valid_wager(wager: Dictionary) -> bool:
	var amount: Variant = wager.get("amount", 0)
	if typeof(amount) != TYPE_INT or int(amount) <= 0:
		return false
	return is_valid_shape(wager.get("type", -1), wager.get("numbers", []))


static func resolve(wagers: Array, winning: int) -> Dictionary:
	var winners: Array[Dictionary] = []
	var losers: Array[Dictionary] = []
	var refunded: Array[Dictionary] = []
	var total_staked := 0
	var total_returned := 0
	var winning_ok := WheelLayout.is_valid(winning)
	if not winning_ok:
		push_error("Resolver: invalid winning number %d; refunding all wagers" % winning)

	for w in wagers:
		var wager: Dictionary = w if typeof(w) == TYPE_DICTIONARY else {}
		var id: String = str(wager.get("id", ""))
		if not winning_ok or not is_valid_wager(wager):
			if winning_ok:
				push_error("Resolver: invalid wager %s; refunding" % str(w))
			var raw: Variant = wager.get("amount", 0)
			var back: int = maxi(int(raw), 0) if typeof(raw) == TYPE_INT else 0
			refunded.append({"id": id, "amount": back, "winnings": 0, "returned": back})
			total_staked += back
			total_returned += back
			continue
		var amount: int = wager["amount"]
		total_staked += amount
		if (wager["numbers"] as Array).has(winning):
			var win: int = Payouts.winnings(wager["type"], amount)
			winners.append({"id": id, "amount": amount, "winnings": win, "returned": amount + win})
			total_returned += amount + win
		else:
			losers.append({"id": id, "amount": amount})

	return {
		"winning": winning,
		"winners": winners,
		"losers": losers,
		"refunded": refunded,
		"total_staked": total_staked,
		"total_returned": total_returned,
		"net": total_returned - total_staked,
	}
