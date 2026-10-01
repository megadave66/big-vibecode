class_name BetSpot
extends RefCounted
## One place on the table where a bet can be put.
## rect is the hit zone in layout units. center is where the chip is drawn.

var id: String
var type: Payouts.BetType
var numbers: Array[int] = []   # sorted ascending, 37 = "00"
var label: String
var rect: Rect2
var center: Vector2


func _init(p_id: String, p_type: Payouts.BetType, p_numbers: Array[int], p_label: String, p_rect: Rect2) -> void:
	id = p_id
	type = p_type
	numbers = p_numbers.duplicate()
	numbers.sort()
	label = p_label
	rect = p_rect
	center = p_rect.get_center()


## True if this is an inside bet (placed on numbers, edges or corners).
func is_inside() -> bool:
	return type in [
		Payouts.BetType.STRAIGHT, Payouts.BetType.SPLIT, Payouts.BetType.STREET,
		Payouts.BetType.TRIO, Payouts.BetType.CORNER, Payouts.BetType.FIVE_NUMBER,
		Payouts.BetType.SIX_LINE,
	]


func odds_text() -> String:
	return Payouts.odds_text(type)


func _to_string() -> String:
	return "BetSpot(%s)" % id
