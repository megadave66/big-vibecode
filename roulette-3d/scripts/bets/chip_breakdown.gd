class_name ChipBreakdown
extends RefCounted
## Splits an amount into chips, largest first (greedy by 100 / 25 / 5 / 1).

const DENOMS_DESC: Array[int] = [100, 25, 5, 1]


## break_into_chips(131) -> [100, 25, 5, 1]. Amount <= 0 gives [].
static func break_into_chips(amount: int) -> Array[int]:
	var out: Array[int] = []
	var left := amount
	for d in DENOMS_DESC:
		while left >= d:
			out.append(d)
			left -= d
	return out
