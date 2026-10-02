extends GdUnitTestSuite
## Settle invariants with real headless physics (Jolt, 120 ticks/s, real time).
## 200 spins on 50 wheels at once (4 rounds). Each spin must end in exactly one valid
## pocket, within the time limit, with the ball resting inside the pocket ring.
## The test prints the pocket distribution and spin times.

const SPINS := 200
const PARALLEL := 50
const FIRST_SEED := 5000
## Real-time budget for the whole batch (s). Spins take about 7-8 s of simulated time.
const BUDGET_S := 240.0


func test_every_spin_settles_in_exactly_one_pocket() -> void:
	var batch := WheelSpinBatch.new()
	batch.spacing = 2.0
	var settled_signals := [0]
	var timeouts := [0]
	add_child(batch)
	var seeds: Array[int] = []
	for k in SPINS:
		seeds.append(FIRST_SEED + k)
	batch.run(seeds, PARALLEL)
	for w in batch.get_wheels():
		w.ball_settled.connect(func(_n: int) -> void: settled_signals[0] += 1)
		w.ball_timed_out.connect(func() -> void: timeouts[0] += 1)
	var start := Time.get_ticks_msec()
	while not batch.is_done() and (Time.get_ticks_msec() - start) / 1000.0 < BUDGET_S:
		await get_tree().physics_frame
	var wall := (Time.get_ticks_msec() - start) / 1000.0
	print(WheelSpinBatch.summarize(batch.results))
	print("real time for %d spins: %.1f s" % [SPINS, wall])

	assert_bool(batch.is_done()).override_failure_message("batch did not finish in %.0f s" % BUDGET_S).is_true()
	assert_int(batch.results.size()).is_equal(SPINS)
	assert_int(timeouts[0]).is_equal(0)
	assert_int(settled_signals[0]).is_equal(SPINS)
	var seen_seeds := {}
	var half := PocketMath.STEP * 0.5
	for r in batch.results:
		seen_seeds[r["seed"]] = true
		assert_bool(r["timed_out"]).is_false()
		assert_bool(WheelLayout.is_valid(r["number"])).is_true()
		assert_float(r["time"]).is_less(60.0)
		assert_bool(r["in_ring"]).override_failure_message("seed %d rests outside the pocket ring: %s" % [r["seed"], r["local_pos"]]).is_true()
		# Exactly one pocket: the ball centre is clear of both frets, so it touches one pocket only.
		var lp: Vector3 = r["local_pos"]
		# Independent of the settle trigger: centre at rest height, radius on the pocket floor.
		assert_bool(WheelSpinBatch.is_resting_on_pocket_floor(lp)).override_failure_message(
				"seed %d not resting on a pocket floor: %s" % [r["seed"], lp]).is_true()
		assert_float(lp.y).is_equal_approx(WheelGeometry.pocket_rest_y(), 0.0015)
		var a := PocketMath.angle_of(lp)
		var centre := PocketMath.angle_for_number(r["number"])
		var off := absf(wrapf(a - centre, -PI, PI))
		var fret_half_angle := (WheelGeometry.FRET_THICKNESS * 0.5) / PocketMath.radius_of(lp)
		assert_float(off).is_less(half - fret_half_angle)
		assert_float(lp.y).is_less(WheelGeometry.FRET_TOP_Y)
	assert_int(seen_seeds.size()).is_equal(SPINS)
	batch.queue_free()
