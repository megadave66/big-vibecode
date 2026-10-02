class_name Payouts
extends RefCounted
## SINGLE SOURCE of payout odds. UI tooltips and the resolver both read from here.
## Payout is "N to 1": a winning bet of A returns A (stake) + A * N.

enum BetType {
	STRAIGHT,     # 1 number
	SPLIT,        # 2 numbers (incl. 0-00, 0-1, 0-2, 00-2, 00-3)
	STREET,       # 3 numbers in a row of the layout
	TRIO,         # 0-1-2, 0-00-2, 00-2-3
	CORNER,       # 4 numbers
	FIVE_NUMBER,  # 0, 00, 1, 2, 3
	SIX_LINE,     # 6 numbers (two adjacent streets)
	DOZEN,        # 1-12, 13-24, 25-36
	COLUMN,       # 12 numbers
	RED,
	BLACK,
	EVEN,
	ODD,
	LOW,          # 1-18
	HIGH,         # 19-36
}

const ODDS := {
	BetType.STRAIGHT: 35,
	BetType.SPLIT: 17,
	BetType.STREET: 11,
	BetType.TRIO: 11,
	BetType.CORNER: 8,
	BetType.FIVE_NUMBER: 6,
	BetType.SIX_LINE: 5,
	BetType.DOZEN: 2,
	BetType.COLUMN: 2,
	BetType.RED: 1,
	BetType.BLACK: 1,
	BetType.EVEN: 1,
	BetType.ODD: 1,
	BetType.LOW: 1,
	BetType.HIGH: 1,
}

const NAMES := {
	BetType.STRAIGHT: "Straight",
	BetType.SPLIT: "Split",
	BetType.STREET: "Street",
	BetType.TRIO: "Trio",
	BetType.CORNER: "Corner",
	BetType.FIVE_NUMBER: "Five Number",
	BetType.SIX_LINE: "Six Line",
	BetType.DOZEN: "Dozen",
	BetType.COLUMN: "Column",
	BetType.RED: "Red",
	BetType.BLACK: "Black",
	BetType.EVEN: "Even",
	BetType.ODD: "Odd",
	BetType.LOW: "Low (1-18)",
	BetType.HIGH: "High (19-36)",
}


static func odds(type: BetType) -> int:
	return ODDS[type]


## "35:1" style string for tooltips.
static func odds_text(type: BetType) -> String:
	return "%d:1" % ODDS[type]


static func type_name(type: BetType) -> String:
	return NAMES[type]


## Net winnings (excluding returned stake) for a winning bet.
static func winnings(type: BetType, amount: int) -> int:
	return amount * ODDS[type]
