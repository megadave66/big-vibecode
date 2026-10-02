class_name WheelLayout
extends RefCounted
## Single source of truth for the American wheel (38 pockets).
## Numbers are ints. Double zero "00" is stored as DOUBLE_ZERO (37).

const ZERO := 0
const DOUBLE_ZERO := 37
const POCKET_COUNT := 38

## Real American wheel order, clockwise when viewed from above, starting at 0.
const POCKET_ORDER: Array[int] = [
	0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1,
	37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2,
]

const RED_NUMBERS: Array[int] = [1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36]

enum PocketColor { GREEN, RED, BLACK }


static func label(number: int) -> String:
	if number == DOUBLE_ZERO:
		return "00"
	return str(number)


static func color_of(number: int) -> PocketColor:
	if number == ZERO or number == DOUBLE_ZERO:
		return PocketColor.GREEN
	return PocketColor.RED if RED_NUMBERS.has(number) else PocketColor.BLACK


static func is_valid(number: int) -> bool:
	return number >= 0 and number <= DOUBLE_ZERO


## Index (0..37) of a number around the wheel, or -1 if invalid.
static func index_of(number: int) -> int:
	return POCKET_ORDER.find(number)


## Number at a wheel index; index wraps.
static func number_at(index: int) -> int:
	return POCKET_ORDER[posmod(index, POCKET_COUNT)]
