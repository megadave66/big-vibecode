extends GdUnitTestSuite
## Tests for BetLayout / BetSpot: counts, numbers, ids, hit zones.

const DZ := 37
const T := Payouts.BetType

var L: BetLayout


func before_test() -> void:
	L = BetLayout.new()


# ------------------------------------------------------------ helpers

func _of_type(t: int) -> Array[BetSpot]:
	var out: Array[BetSpot] = []
	for s in L.spots():
		if s.type == t:
			out.append(s)
	return out


## Grid cell (c, r) of a number 1..36.
func _cell(n: int) -> Vector2i:
	@warning_ignore("integer_division")
	return Vector2i((n - 1) / 3, (n - 1) % 3)


func _range(a: int, b: int) -> Array[int]:
	var out: Array[int] = []
	for n in range(a, b + 1):
		out.append(n)
	return out


func _ids(arr: Array[BetSpot]) -> Array:
	var out: Array = []
	for s in arr:
		out.append(s.id)
	out.sort()
	return out


# ------------------------------------------------------------ counts

func test_counts_per_type() -> void:
	var expected := {
		T.STRAIGHT: 38, T.SPLIT: 62, T.STREET: 12, T.TRIO: 3, T.CORNER: 22,
		T.FIVE_NUMBER: 1, T.SIX_LINE: 11, T.DOZEN: 3, T.COLUMN: 3,
		T.RED: 1, T.BLACK: 1, T.EVEN: 1, T.ODD: 1, T.LOW: 1, T.HIGH: 1,
	}
	var total := 0
	for t in expected:
		assert_int(_of_type(t).size()).override_failure_message("type %s" % Payouts.type_name(t)).is_equal(expected[t])
		total += expected[t]
	assert_int(L.spots().size()).is_equal(total)
	assert_int(total).is_equal(161)


# ------------------------------------------------------------ numbers

func test_numbers_sorted_valid_and_sized() -> void:
	var sizes := {T.STRAIGHT: 1, T.SPLIT: 2, T.STREET: 3, T.TRIO: 3, T.CORNER: 4, T.FIVE_NUMBER: 5,
		T.SIX_LINE: 6, T.DOZEN: 12, T.COLUMN: 12, T.RED: 18, T.BLACK: 18, T.EVEN: 18, T.ODD: 18,
		T.LOW: 18, T.HIGH: 18}
	for s in L.spots():
		assert_int(s.numbers.size()).override_failure_message(s.id).is_equal(sizes[s.type])
		var sorted := s.numbers.duplicate()
		sorted.sort()
		assert_array(s.numbers).override_failure_message(s.id + " not sorted").is_equal(sorted)
		var seen := {}
		for n in s.numbers:
			assert_bool(WheelLayout.is_valid(n)).is_true()
			seen[n] = true
		assert_int(seen.size()).override_failure_message(s.id + " has duplicates").is_equal(s.numbers.size())


func test_straights_cover_all_38() -> void:
	var seen := {}
	for s in _of_type(T.STRAIGHT):
		seen[s.numbers[0]] = true
		assert_str(s.id).is_equal("straight_%d" % s.numbers[0])
	assert_int(seen.size()).is_equal(38)
	assert_str(L.get_spot("straight_37").label).is_equal("Straight 00")


func test_number_grid_splits_are_adjacent() -> void:
	var grid_pairs := {}
	for s in _of_type(T.SPLIT):
		var a := s.numbers[0]
		var b := s.numbers[1]
		if a == 0 or b == DZ:
			continue
		var ca := _cell(a)
		var cb := _cell(b)
		var d := cb - ca
		var ok := (d == Vector2i(1, 0)) or (d == Vector2i(0, 1))
		assert_bool(ok).override_failure_message("%s not adjacent" % s.id).is_true()
		grid_pairs[s.id] = true
	assert_int(grid_pairs.size()).is_equal(57)  # 33 horizontal + 24 vertical


func test_horizontal_and_vertical_split_counts() -> void:
	var h := 0
	var v := 0
	for s in _of_type(T.SPLIT):
		var a := s.numbers[0]
		var b := s.numbers[1]
		if a == 0 or b == DZ:
			continue
		if b - a == 3:
			h += 1
		elif b - a == 1:
			v += 1
			assert_int(a % 3).is_not_equal(0)
	assert_int(h).is_equal(33)
	assert_int(v).is_equal(24)


func test_zero_splits() -> void:
	var zero_ids := []
	for s in _of_type(T.SPLIT):
		if s.numbers.has(0) or s.numbers.has(DZ):
			zero_ids.append(s.id)
	zero_ids.sort()
	assert_array(zero_ids).is_equal(["split_0_1", "split_0_2", "split_0_37", "split_2_37", "split_3_37"])
	assert_str(L.get_spot("split_0_37").label).is_equal("Split 0-00")
	assert_str(L.get_spot("split_3_37").label).is_equal("Split 00-3")


func test_trios() -> void:
	assert_array(_ids(_of_type(T.TRIO))).is_equal(["trio_0_1_2", "trio_0_2_37", "trio_2_3_37"])
	assert_array(L.get_spot("trio_0_1_2").numbers).is_equal([0, 1, 2])
	assert_array(L.get_spot("trio_0_2_37").numbers).is_equal([0, 2, 37])
	assert_array(L.get_spot("trio_2_3_37").numbers).is_equal([2, 3, 37])
	assert_str(L.get_spot("trio_0_2_37").label).is_equal("Trio 0-00-2")
	assert_str(Payouts.odds_text(L.get_spot("trio_0_1_2").type)).is_equal("11:1")


func test_streets_are_rows() -> void:
	var firsts := []
	for s in _of_type(T.STREET):
		var a := s.numbers[0]
		assert_int((a - 1) % 3).is_equal(0)
		assert_array(s.numbers).is_equal([a, a + 1, a + 2])
		var c := _cell(a).x
		for n in s.numbers:
			assert_int(_cell(n).x).is_equal(c)
		assert_str(s.id).is_equal("street_%d" % a)
		firsts.append(a)
	firsts.sort()
	assert_array(firsts).is_equal([1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34])


func test_corners_are_2x2_blocks() -> void:
	var count := 0
	for s in _of_type(T.CORNER):
		var cells := {}
		var minc := Vector2i(99, 99)
		for n in s.numbers:
			assert_bool(n >= 1 and n <= 36).is_true()
			var c := _cell(n)
			cells[c] = true
			minc = Vector2i(mini(minc.x, c.x), mini(minc.y, c.y))
		for d in [Vector2i(0, 0), Vector2i(1, 0), Vector2i(0, 1), Vector2i(1, 1)]:
			assert_bool(cells.has(minc + d)).override_failure_message(s.id).is_true()
		count += 1
	assert_int(count).is_equal(22)
	assert_str(L.get_spot("corner_1_2_4_5").label).is_equal("Corner 1-2-4-5")


func test_five_number() -> void:
	var s := L.get_spot("five_number")
	assert_object(s).is_not_null()
	assert_array(s.numbers).is_equal([0, 1, 2, 3, 37])
	assert_int(s.type).is_equal(T.FIVE_NUMBER)


func test_six_lines_are_two_adjacent_streets() -> void:
	var firsts := []
	for s in _of_type(T.SIX_LINE):
		var a := s.numbers[0]
		assert_array(s.numbers).is_equal(_range(a, a + 5))
		assert_object(L.get_spot("street_%d" % a)).is_not_null()
		assert_object(L.get_spot("street_%d" % (a + 3))).is_not_null()
		assert_str(s.id).is_equal("sixline_%d" % a)
		firsts.append(a)
	firsts.sort()
	assert_array(firsts).is_equal([1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31])


func test_outside_bets_match_definitions() -> void:
	assert_array(L.get_spot("dozen_1").numbers).is_equal(_range(1, 12))
	assert_array(L.get_spot("dozen_2").numbers).is_equal(_range(13, 24))
	assert_array(L.get_spot("dozen_3").numbers).is_equal(_range(25, 36))
	for k in 3:
		var col: Array[int] = []
		for n in range(k + 1, 37, 3):
			col.append(n)
		assert_array(L.get_spot("column_%d" % (k + 1)).numbers).is_equal(col)
	assert_array(L.get_spot("column_1").numbers.slice(0, 3)).is_equal([1, 4, 7])
	assert_array(L.get_spot("column_3").numbers.slice(0, 3)).is_equal([3, 6, 9])
	assert_array(L.get_spot("low").numbers).is_equal(_range(1, 18))
	assert_array(L.get_spot("high").numbers).is_equal(_range(19, 36))
	var reds := WheelLayout.RED_NUMBERS.duplicate()
	reds.sort()
	assert_array(L.get_spot("red").numbers).is_equal(reds)
	for n in L.get_spot("black").numbers:
		assert_int(WheelLayout.color_of(n)).is_equal(WheelLayout.PocketColor.BLACK)
	for n in L.get_spot("even").numbers:
		assert_int(n % 2).is_equal(0)
	for n in L.get_spot("odd").numbers:
		assert_int(n % 2).is_equal(1)
	for id in ["dozen_1", "dozen_2", "dozen_3", "column_1", "column_2", "column_3",
			"red", "black", "even", "odd", "low", "high"]:
		var s := L.get_spot(id)
		assert_bool(s.numbers.has(0)).override_failure_message(id).is_false()
		assert_bool(s.numbers.has(DZ)).override_failure_message(id).is_false()
		assert_bool(s.is_inside()).is_false()


func test_complementary_outside_sets_partition_1_to_36() -> void:
	for pair in [["red", "black"], ["even", "odd"], ["low", "high"]]:
		var all: Array[int] = []
		all.append_array(L.get_spot(pair[0]).numbers)
		all.append_array(L.get_spot(pair[1]).numbers)
		all.sort()
		assert_array(all).is_equal(_range(1, 36))


func test_every_inside_number_set_is_unique() -> void:
	var seen := {}
	for s in L.spots():
		var key := str(s.numbers)
		assert_bool(seen.has(key)).override_failure_message("dup numbers " + s.id).is_false()
		seen[key] = true


# ------------------------------------------------------------ ids

func test_ids_unique_and_formatted() -> void:
	var re := RegEx.new()
	re.compile("^(straight_\\d+|split_\\d+_\\d+|street_\\d+|trio_\\d+_\\d+_\\d+|corner_\\d+_\\d+_\\d+_\\d+|five_number|sixline_\\d+|dozen_[123]|column_[123]|red|black|even|odd|low|high)$")
	var seen := {}
	for s in L.spots():
		assert_bool(seen.has(s.id)).override_failure_message("dup id " + s.id).is_false()
		seen[s.id] = true
		assert_object(re.search(s.id)).override_failure_message("bad id " + s.id).is_not_null()
		assert_str(s.label).is_not_empty()
		# Numbered ids list exactly the sorted numbers.
		if s.type in [T.STRAIGHT, T.SPLIT, T.TRIO, T.CORNER]:
			var parts := s.id.split("_")
			var nums: Array[int] = []
			for i in range(1, parts.size()):
				nums.append(int(parts[i]))
			assert_array(nums).override_failure_message(s.id).is_equal(s.numbers)


func test_get_spot_round_trips() -> void:
	for s in L.spots():
		assert_object(L.get_spot(s.id)).is_same(s)
	assert_object(L.get_spot("nope")).is_null()
	assert_object(L.get_spot("straight_38")).is_null()


# ------------------------------------------------------------ geometry

func test_center_is_rect_middle_and_inside_rect() -> void:
	for s in L.spots():
		assert_vector(s.center).is_equal_approx(s.rect.get_center(), Vector2(0.0001, 0.0001))
		assert_bool(s.rect.has_point(s.center)).override_failure_message(s.id).is_true()
		assert_bool(s.rect.size.x > 0.0 and s.rect.size.y > 0.0).is_true()


func test_every_spot_reachable_at_its_center() -> void:
	for s in L.spots():
		var hit := L.spot_at(s.center)
		assert_object(hit).override_failure_message("center of %s hits %s" % [s.id, hit]).is_same(s)


func test_straight_cells_match_conventions() -> void:
	for n in range(1, 37):
		var c := _cell(n)
		assert_that(L.get_spot("straight_%d" % n).rect).is_equal(Rect2(c.x, c.y, 1, 1))
	assert_that(L.get_spot("straight_0").rect).is_equal(Rect2(-1, 0, 1, 1.5))
	assert_that(L.get_spot("straight_37").rect).is_equal(Rect2(-1, 1.5, 1, 1.5))


func test_zones_in_same_tier_never_overlap() -> void:
	# Sample a fine grid over the whole layout; at most one zone per tier may contain a point.
	for tier in [L.edge_zones(), L.base_zones()]:
		var y := -2.3
		while y < 3.3:
			var x := -1.3
			while x < 13.3:
				var p := Vector2(x, y)
				var hits := 0
				for s in tier:
					if s.rect.has_point(p):
						hits += 1
				assert_int(hits).override_failure_message("overlap at %s" % p).is_less_equal(1)
				x += 0.025
			y += 0.025


func test_edge_zone_rects_pairwise_disjoint() -> void:
	var zs := L.edge_zones()
	for i in zs.size():
		for j in range(i + 1, zs.size()):
			var a: Rect2 = zs[i].rect.grow(-0.0001)
			assert_bool(a.intersects(zs[j].rect)).override_failure_message("%s vs %s" % [zs[i].id, zs[j].id]).is_false()


func test_specific_points() -> void:
	var cases := {
		Vector2(0.5, 0.5): "straight_1",
		Vector2(0.5, 2.5): "straight_3",
		Vector2(11.5, 2.5): "straight_36",
		Vector2(-0.5, 0.75): "straight_0",
		Vector2(-0.5, 2.25): "straight_37",
		Vector2(1.0, 0.5): "split_1_4",
		Vector2(1.05, 0.5): "split_1_4",
		Vector2(0.95, 0.5): "split_1_4",
		Vector2(0.5, 1.0): "split_1_2",
		Vector2(5.5, 2.0): "split_17_18",
		Vector2(1.0, 1.0): "corner_1_2_4_5",
		Vector2(11.0, 2.0): "corner_32_33_35_36",
		Vector2(1.1, 1.1): "corner_1_2_4_5",
		Vector2(0.5, 0.0): "street_1",
		Vector2(11.5, -0.1): "street_34",
		Vector2(1.0, 0.0): "sixline_1",
		Vector2(11.0, 0.05): "sixline_31",
		Vector2(0.0, 0.0): "five_number",
		Vector2(0.0, 0.5): "split_0_1",
		Vector2(0.0, 1.0): "trio_0_1_2",
		Vector2(0.0, 1.25): "split_0_2",
		Vector2(0.0, 1.5): "trio_0_2_37",
		Vector2(0.0, 1.75): "split_2_37",
		Vector2(0.0, 2.0): "trio_2_3_37",
		Vector2(0.0, 2.5): "split_3_37",
		Vector2(-0.5, 1.5): "split_0_37",
		Vector2(12.5, 0.5): "column_1",
		Vector2(12.5, 2.5): "column_3",
		Vector2(2.0, -0.5): "dozen_1",
		Vector2(6.0, -0.5): "dozen_2",
		Vector2(10.0, -0.5): "dozen_3",
		Vector2(1.0, -1.5): "low",
		Vector2(3.0, -1.5): "even",
		Vector2(5.0, -1.5): "red",
		Vector2(7.0, -1.5): "black",
		Vector2(9.0, -1.5): "odd",
		Vector2(11.0, -1.5): "high",
		Vector2(-0.95, 1.5): "straight_37",  # 0-00 border near outer edge: lower bound of 00 cell
		Vector2(11.95, 0.5): "straight_34",  # no split past the last column
		Vector2(0.5, 2.95): "straight_3",    # top border has no zone
	}
	for p in cases:
		var hit := L.spot_at(p)
		var got := "null" if hit == null else hit.id
		assert_str(got).override_failure_message("spot_at(%s) = %s, want %s" % [p, got, cases[p]]).is_equal(cases[p])


func test_null_outside_layout() -> void:
	for p in [Vector2(-1.5, 0.5), Vector2(13.5, 1.0), Vector2(5.0, 3.5), Vector2(5.0, -2.5),
			Vector2(-0.5, -0.5), Vector2(12.5, -0.5), Vector2(12.5, -1.5), Vector2(-0.5, -1.5),
			Vector2(13.0, 1.0), Vector2(5.0, 3.0), Vector2(1000, 1000)]:
		assert_object(L.spot_at(p)).override_failure_message("expected null at %s" % p).is_null()


func test_every_number_cell_point_hits_spot_containing_it() -> void:
	# Anywhere on a number/zero cell, the hit spot must include that cell's number.
	for n in range(0, 38):
		var r := BetLayout.cell_rect(n)
		for i in 9:
			for j in 9:
				var p := r.position + Vector2(r.size.x * (i + 0.5) / 9.0, r.size.y * (j + 0.5) / 9.0)
				var hit := L.spot_at(p)
				assert_object(hit).is_not_null()
				# Streets/six-lines/five-number sit on the outer line and may cover the row-0 edge.
				assert_bool(hit.numbers.has(n)).override_failure_message("%s at %s -> %s" % [n, p, hit.id]).is_true()


func test_inside_zone_rects_are_on_cell_edges() -> void:
	# Each edge zone touches every cell of its numbers (zones straddle shared borders).
	for s in L.edge_zones():
		if not s.type in [T.SPLIT, T.CORNER, T.TRIO]:
			continue
		for n in s.numbers:
			var cell := BetLayout.cell_rect(n)
			assert_bool(cell.intersects(s.rect)).override_failure_message("%s does not touch %d" % [s.id, n]).is_true()
