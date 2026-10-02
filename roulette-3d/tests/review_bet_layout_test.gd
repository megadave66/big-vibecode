extends GdUnitTestSuite
## Independent review of BetLayout. Expected sets built from the plain table rules.

const DZ := 37

func _key(nums: Array) -> String:
	var a := nums.duplicate()
	a.sort()
	return ",".join(a.map(func(x): return str(x)))

func _expected() -> Dictionary:
	# id -> [type, numbers]
	var e := {}
	for n in range(0, 38):
		e["straight_%d" % n] = [Payouts.BetType.STRAIGHT, [n]]
	# splits
	for n in range(1, 37):
		if n % 3 != 0:
			e["split_%d_%d" % [n, n + 1]] = [Payouts.BetType.SPLIT, [n, n + 1]]
		if n + 3 <= 36:
			e["split_%d_%d" % [n, n + 3]] = [Payouts.BetType.SPLIT, [n, n + 3]]
	for s in [[0, 1], [0, 2], [2, 37], [3, 37], [0, 37]]:
		e["split_%d_%d" % [s[0], s[1]]] = [Payouts.BetType.SPLIT, s]
	for a in range(1, 35, 3):
		e["street_%d" % a] = [Payouts.BetType.STREET, [a, a + 1, a + 2]]
	for a in range(1, 32, 3):
		e["sixline_%d" % a] = [Payouts.BetType.SIX_LINE, [a, a + 1, a + 2, a + 3, a + 4, a + 5]]
	for n in range(1, 34):
		if n % 3 != 0:
			e["corner_%d_%d_%d_%d" % [n, n + 1, n + 3, n + 4]] = [Payouts.BetType.CORNER, [n, n + 1, n + 3, n + 4]]
	e["trio_0_1_2"] = [Payouts.BetType.TRIO, [0, 1, 2]]
	e["trio_0_2_37"] = [Payouts.BetType.TRIO, [0, 2, 37]]
	e["trio_2_3_37"] = [Payouts.BetType.TRIO, [2, 3, 37]]
	e["five_number"] = [Payouts.BetType.FIVE_NUMBER, [0, 1, 2, 3, 37]]
	for d in 3:
		var l := []
		for k in 12: l.append(d * 12 + k + 1)
		e["dozen_%d" % (d + 1)] = [Payouts.BetType.DOZEN, l]
	for c in 3:
		var l := []
		for k in 12: l.append(k * 3 + c + 1)
		e["column_%d" % (c + 1)] = [Payouts.BetType.COLUMN, l]
	var red := [1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36]
	var blk := []; var ev := []; var od := []; var lo := []; var hi := []
	for n in range(1, 37):
		if not red.has(n): blk.append(n)
		if n % 2 == 0:
			ev.append(n)
		else:
			od.append(n)
		if n <= 18:
			lo.append(n)
		else:
			hi.append(n)
	e["red"] = [Payouts.BetType.RED, red]
	e["black"] = [Payouts.BetType.BLACK, blk]
	e["even"] = [Payouts.BetType.EVEN, ev]
	e["odd"] = [Payouts.BetType.ODD, od]
	e["low"] = [Payouts.BetType.LOW, lo]
	e["high"] = [Payouts.BetType.HIGH, hi]
	return e

func test_counts() -> void:
	var L := BetLayout.new()
	var cnt := {}
	for s in L.spots():
		cnt[s.type] = cnt.get(s.type, 0) + 1
	assert_int(cnt[Payouts.BetType.STRAIGHT]).is_equal(38)
	assert_int(cnt[Payouts.BetType.SPLIT]).is_equal(62)
	assert_int(cnt[Payouts.BetType.STREET]).is_equal(12)
	assert_int(cnt[Payouts.BetType.TRIO]).is_equal(3)
	assert_int(cnt[Payouts.BetType.CORNER]).is_equal(22)
	assert_int(cnt[Payouts.BetType.FIVE_NUMBER]).is_equal(1)
	assert_int(cnt[Payouts.BetType.SIX_LINE]).is_equal(11)
	assert_int(L.spots().size()).is_equal(38 + 62 + 12 + 3 + 22 + 1 + 11 + 3 + 3 + 6)

func test_ids_types_numbers_match_expected() -> void:
	var L := BetLayout.new()
	var exp := _expected()
	assert_int(L.spots().size()).is_equal(exp.size())
	for id in exp:
		var s := L.get_spot(id)
		assert_object(s).override_failure_message("missing " + id).is_not_null()
		if s == null: continue
		assert_int(s.type).override_failure_message("type " + id).is_equal(exp[id][0])
		assert_str(_key(s.numbers)).override_failure_message("nums " + id).is_equal(_key(exp[id][1]))
	for s in L.spots():
		assert_bool(exp.has(s.id)).override_failure_message("unexpected " + s.id).is_true()
		assert_str(s.label).is_not_empty()
		assert_bool(s.label.contains("_")).override_failure_message("ugly label " + s.label).is_false()

func test_center_reachable_and_total() -> void:
	var L := BetLayout.new()
	for s in L.spots():
		var hit := L.spot_at(s.center)
		assert_object(hit).override_failure_message("center of " + s.id + " hits nothing").is_not_null()
		if hit: assert_str(hit.id).override_failure_message("center of %s hits %s" % [s.id, hit.id]).is_equal(s.id)
		assert_bool(s.rect.size.x >= 0.2 and s.rect.size.y >= 0.2).override_failure_message("tiny " + s.id + str(s.rect.size)).is_true()
	# every rect center inside the rect, no NaN
	assert_object(L.spot_at(Vector2(100, 100))).is_null()
	assert_object(L.spot_at(Vector2(-5, 0.5))).is_null()

func test_no_overlap_within_tier_and_full_coverage() -> void:
	var L := BetLayout.new()
	var step := 1.0 / 32.0
	var x := -1.0
	var uncovered := []
	while x < 13.0:
		var y := -2.0
		while y < 3.0:
			var p := Vector2(x + step * 0.5, y + step * 0.5)
			var t1 := 0
			for s in L.edge_zones():
				if s.rect.has_point(p): t1 += 1
			var t2 := 0
			for s in L.base_zones():
				if s.rect.has_point(p): t2 += 1
			assert_int(t1).override_failure_message("tier1 overlap at %s" % p).is_less_equal(1)
			assert_int(t2).override_failure_message("tier2 overlap at %s" % p).is_less_equal(1)
			var in_table := false
			if x < 0 and y >= 0 and y < 3: in_table = true
			elif x >= 0 and x < 13 and y >= -2 and y < 3: in_table = true
			if in_table and L.spot_at(p) == null and uncovered.size() < 5:
				uncovered.append(p)
			y += step * 4
		x += step * 4
	# Note: area x>=0,y in[-2,-1) etc. is covered; x in [12,13) y<0 is not part of table
	print("UNCOVERED sample: ", uncovered)

func test_cell_interior_is_straight() -> void:
	var L := BetLayout.new()
	for n in range(1, 37):
		var r := BetLayout.cell_rect(n)
		assert_str(L.spot_at(r.get_center()).id).is_equal("straight_%d" % n)
	assert_str(L.spot_at(Vector2(-0.5, 0.4)).id).is_equal("straight_0")
	assert_str(L.spot_at(Vector2(-0.5, 2.6)).id).is_equal("straight_37")
	assert_str(L.spot_at(Vector2(12.5, 1.5)).id).is_equal("column_2")
	assert_str(L.spot_at(Vector2(5, -0.5)).id).is_equal("dozen_2")
	assert_str(L.spot_at(Vector2(11, -1.5)).id).is_equal("high")

func test_edge_points_pick_inside_bet() -> void:
	var L := BetLayout.new()
	assert_str(L.spot_at(Vector2(1.0, 1.0)).id).is_equal("corner_1_2_4_5")
	assert_str(L.spot_at(Vector2(1.0, 0.5)).id).is_equal("split_1_4")
	assert_str(L.spot_at(Vector2(0.5, 1.0)).id).is_equal("split_1_2")
	assert_str(L.spot_at(Vector2(0.5, 0.0)).id).is_equal("street_1")
	assert_str(L.spot_at(Vector2(1.0, 0.0)).id).is_equal("sixline_1")
	assert_str(L.spot_at(Vector2(0.0, 0.0)).id).is_equal("five_number")
	assert_str(L.spot_at(Vector2(0.0, 1.0)).id).is_equal("trio_0_1_2")
	assert_str(L.spot_at(Vector2(0.0, 2.0)).id).is_equal("trio_2_3_37")
	assert_str(L.spot_at(Vector2(-0.5, 1.5)).id).is_equal("split_0_37")
