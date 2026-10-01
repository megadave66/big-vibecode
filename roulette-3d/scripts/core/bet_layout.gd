class_name BetLayout
extends RefCounted
## Full American bet map in layout units (see CONVENTIONS "Table layout coordinate system").
##
## Number n (1..36): column c = (n-1)/3, row r = (n-1)%3, cell = [c, c+1] x [r, r+1].
## 0 cell: [-1, 0] x [0, 1.5]. 00 cell: [-1, 0] x [1.5, 3].
##
## Hit zones come in two tiers. Zones inside one tier never overlap.
## Rects use Rect2.has_point: start edge inclusive, end edge exclusive.
##   Tier 1 (edge/corner zones, checked first): splits, corners, streets, six-lines,
##     trios, five-number. Each straddles a cell edge or corner by EDGE units.
##   Tier 2 (base zones, checked second): straight cells, columns, dozens, even-money boxes.
## spot_at() returns the tier-1 zone under the point if any, else the tier-2 zone, else null.
##
## Tier-1 geometry (E = EDGE):
##   corner (c, r)          square [c-E, c+E] x [r-E, r+E], c = 1..11, r = 1..2
##   split n|n+3 (edge x=c) [c-E, c+E] x [r+E, r+1-E]
##   split n|n+1 (edge y=r) [c+E, c+1-E] x [r-E, r+E]
##   street col c           [c+E, c+1-E] x [-E, E]   (outer line, row-0 side)
##   six-line at x=c        square [c-E, c+E] x [-E, E], c = 1..11
##   five-number            square [-E, E] x [-E, E]  (outer corner by 0 and 1)
##   0-1 split              [-E, E] x [E, 1-E]
##   trio 0-1-2             square at (0, 1)
##   0-2 split              [-E, E] x [1+E, 1.5-E]
##   trio 0-00-2            square at (0, 1.5)
##   00-2 split             [-E, E] x [1.5+E, 2-E]
##   trio 00-2-3            square at (0, 2)
##   00-3 split             [-E, E] x [2+E, 3-E]
##   0-00 split             [-1+E, -E] x [1.5-E, 1.5+E]

const EDGE := 0.125  # 1/8: exact in binary floats, so shared zone edges match exactly
const DZ := WheelLayout.DOUBLE_ZERO

var _spots: Array[BetSpot] = []
var _by_id: Dictionary = {}
var _edge_zones: Array[BetSpot] = []   # tier 1
var _base_zones: Array[BetSpot] = []   # tier 2


func _init() -> void:
	_build()


## All spots, in build order.
func spots() -> Array[BetSpot]:
	return _spots.duplicate()


func get_spot(id: String) -> BetSpot:
	return _by_id.get(id, null)


## Most specific spot under layout point p, or null when p is off the layout.
func spot_at(p: Vector2) -> BetSpot:
	for s in _edge_zones:
		if s.rect.has_point(p):
			return s
	for s in _base_zones:
		if s.rect.has_point(p):
			return s
	return null


## Spots in tier 1 (edge/corner zones). Exposed for tests and debug drawing.
func edge_zones() -> Array[BetSpot]:
	return _edge_zones.duplicate()


## Spots in tier 2 (cells and outside boxes).
func base_zones() -> Array[BetSpot]:
	return _base_zones.duplicate()


## Cell rect of a number (0, 37 or 1..36) in layout units.
static func cell_rect(n: int) -> Rect2:
	if n == WheelLayout.ZERO:
		return Rect2(-1, 0, 1, 1.5)
	if n == DZ:
		return Rect2(-1, 1.5, 1, 1.5)
	@warning_ignore("integer_division")
	var c := (n - 1) / 3
	var r := (n - 1) % 3
	return Rect2(c, r, 1, 1)


static func number_at_cell(c: int, r: int) -> int:
	return c * 3 + r + 1


# ---------------------------------------------------------------- build

func _add(tier1: bool, id: String, type: Payouts.BetType, nums: Array[int], label: String, rect: Rect2) -> void:
	assert(not _by_id.has(id), "duplicate spot id " + id)
	var s := BetSpot.new(id, type, nums, label, rect)
	_spots.append(s)
	_by_id[id] = s
	if tier1:
		_edge_zones.append(s)
	else:
		_base_zones.append(s)


## Id built from sorted numbers, e.g. "split_0_37".
static func _num_id(prefix: String, nums: Array[int]) -> String:
	var sorted := nums.duplicate()
	sorted.sort()
	var parts: PackedStringArray = [prefix]
	for n in sorted:
		parts.append(str(n))
	return "_".join(parts)


## Human label with 0 and 00 first, e.g. "Trio 0-00-2".
static func _num_label(word: String, nums: Array[int]) -> String:
	var ordered := nums.duplicate()
	ordered.sort_custom(func(a: int, b: int) -> bool: return _label_key(a) < _label_key(b))
	var parts: PackedStringArray = []
	for n in ordered:
		parts.append(WheelLayout.label(n))
	return "%s %s" % [word, "-".join(parts)]


static func _label_key(n: int) -> int:
	if n == WheelLayout.ZERO:
		return -2
	if n == DZ:
		return -1
	return n


static func _square(at: Vector2) -> Rect2:
	return Rect2(at.x - EDGE, at.y - EDGE, EDGE * 2.0, EDGE * 2.0)


static func _box(x0: float, y0: float, x1: float, y1: float) -> Rect2:
	return Rect2(x0, y0, x1 - x0, y1 - y0)


func _inside(type: Payouts.BetType, word: String, prefix: String, nums: Array[int], rect: Rect2) -> void:
	_add(true, _num_id(prefix, nums), type, nums, _num_label(word, nums), rect)


func _build() -> void:
	var E := EDGE

	# --- Tier 1: zero-area bets along x = 0 and the 0/00 border.
	_add(true, "five_number", Payouts.BetType.FIVE_NUMBER, [0, DZ, 1, 2, 3] as Array[int],
			"Five Number 0-00-1-2-3", _square(Vector2(0, 0)))
	_inside(Payouts.BetType.SPLIT, "Split", "split", [0, 1], _box(-E, E, E, 1 - E))
	_inside(Payouts.BetType.TRIO, "Trio", "trio", [0, 1, 2], _square(Vector2(0, 1)))
	_inside(Payouts.BetType.SPLIT, "Split", "split", [0, 2], _box(-E, 1 + E, E, 1.5 - E))
	_inside(Payouts.BetType.TRIO, "Trio", "trio", [0, DZ, 2], _square(Vector2(0, 1.5)))
	_inside(Payouts.BetType.SPLIT, "Split", "split", [DZ, 2], _box(-E, 1.5 + E, E, 2 - E))
	_inside(Payouts.BetType.TRIO, "Trio", "trio", [DZ, 2, 3], _square(Vector2(0, 2)))
	_inside(Payouts.BetType.SPLIT, "Split", "split", [DZ, 3], _box(-E, 2 + E, E, 3 - E))
	_inside(Payouts.BetType.SPLIT, "Split", "split", [0, DZ], _box(-1 + E, 1.5 - E, -E, 1.5 + E))

	# --- Tier 1: number grid edges and corners.
	for c in 12:
		for r in 3:
			var n := number_at_cell(c, r)
			# Horizontal split n | n+3 on edge x = c+1.
			if c < 11:
				_inside(Payouts.BetType.SPLIT, "Split", "split", [n, n + 3], _box(c + 1 - E, r + E, c + 1 + E, r + 1 - E))
			# Vertical split n | n+1 on edge y = r+1.
			if r < 2:
				_inside(Payouts.BetType.SPLIT, "Split", "split", [n, n + 1], _box(c + E, r + 1 - E, c + 1 - E, r + 1 + E))
			# Corner at (c+1, r+1).
			if c < 11 and r < 2:
				_inside(Payouts.BetType.CORNER, "Corner", "corner", [n, n + 1, n + 3, n + 4], _square(Vector2(c + 1, r + 1)))
		# Street for column c on the outer line y = 0.
		var a := number_at_cell(c, 0)
		_add(true, "street_%d" % a, Payouts.BetType.STREET, [a, a + 1, a + 2] as Array[int],
				"Street %d-%d-%d" % [a, a + 1, a + 2], _box(c + E, -E, c + 1 - E, E))
		# Six-line at x = c+1 on the outer line.
		if c < 11:
			var six: Array[int] = []
			for k in 6:
				six.append(a + k)
			_add(true, "sixline_%d" % a, Payouts.BetType.SIX_LINE, six,
					"Six Line %d-%d" % [a, a + 5], _square(Vector2(c + 1, 0)))

	# --- Tier 2: straights.
	_add(false, "straight_0", Payouts.BetType.STRAIGHT, [0] as Array[int], "Straight 0", cell_rect(0))
	_add(false, "straight_%d" % DZ, Payouts.BetType.STRAIGHT, [DZ] as Array[int], "Straight 00", cell_rect(DZ))
	for n in range(1, 37):
		_add(false, "straight_%d" % n, Payouts.BetType.STRAIGHT, [n] as Array[int], "Straight %d" % n, cell_rect(n))

	# --- Tier 2: columns ("2 to 1"), x in [12, 13], one per row.
	for r in 3:
		var col: Array[int] = []
		for c in 12:
			col.append(number_at_cell(c, r))
		_add(false, "column_%d" % (r + 1), Payouts.BetType.COLUMN, col, "Column %d" % (r + 1), Rect2(12, r, 1, 1))

	# --- Tier 2: dozens, y in [-1, 0].
	var dozen_names := ["1st 12", "2nd 12", "3rd 12"]
	for d in 3:
		var doz: Array[int] = []
		for k in range(1, 13):
			doz.append(d * 12 + k)
		_add(false, "dozen_%d" % (d + 1), Payouts.BetType.DOZEN, doz, "%s (%d-%d)" % [dozen_names[d], d * 12 + 1, d * 12 + 12],
				Rect2(d * 4, -1, 4, 1))

	# --- Tier 2: even-money strip, y in [-2, -1].
	var outs: Array = [
		["low", Payouts.BetType.LOW], ["even", Payouts.BetType.EVEN], ["red", Payouts.BetType.RED],
		["black", Payouts.BetType.BLACK], ["odd", Payouts.BetType.ODD], ["high", Payouts.BetType.HIGH],
	]
	for i in outs.size():
		var t: Payouts.BetType = outs[i][1]
		var nums: Array[int] = []
		for n in range(1, 37):
			if _even_money_has(t, n):
				nums.append(n)
		_add(false, outs[i][0], t, nums, Payouts.type_name(t), Rect2(i * 2, -2, 2, 1))


static func _even_money_has(t: Payouts.BetType, n: int) -> bool:
	match t:
		Payouts.BetType.LOW:
			return n <= 18
		Payouts.BetType.HIGH:
			return n >= 19
		Payouts.BetType.EVEN:
			return n % 2 == 0
		Payouts.BetType.ODD:
			return n % 2 == 1
		Payouts.BetType.RED:
			return WheelLayout.RED_NUMBERS.has(n)
		Payouts.BetType.BLACK:
			return not WheelLayout.RED_NUMBERS.has(n)
	return false
