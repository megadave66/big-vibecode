extends SceneTree
## Headless spin simulator for the roulette wheel. Runs N real physics spins and prints
## the pocket distribution, timeouts and timing.
##
## Usage (from the project root):
##   godot --headless --path . --fixed-fps 120 -s res://tools/sim_spins.gd -- --spins=200
## Options (after the "--"):
##   --spins=N      number of spins (default 100)
##   --parallel=P   wheels simulated at the same time (default 20)
##   --seed=S       first seed; spin k uses seed S + k (default 1)
##   --fixed-rotor  rotor always starts at angle 0 (default: random phase from the seed)
##   --repeat       run every seed twice (separate batches) and report same-seed repeatability.
##                  Honest result: inside ONE process only about 10% of seeds repeat their pocket
##                  (the second batch has a different body history, and the chaotic bounce phase
##                  amplifies tiny float differences). Two separate processes with the same
##                  arguments give bit-identical results.
##   --list         print one line per spin
##   --set=prop:val override a RouletteWheel export (float), e.g. --set=wall_friction:0.02
##   --trace        print the first wheel's ball state every 0.1 s (tuning aid)
##   --trace-dt=S   same, every S seconds
## --fixed-fps 120 makes every rendered frame one physics step (120 ticks/s), so the sim
## runs as fast as the CPU allows. Without it the sim runs in real time.

var _spins := 100
var _parallel := 20
var _seed := 1
var _repeat := false
var _list := false
var _trace := false
var _overrides := {}
var _fixed_rotor := false
var _trace_t := 0.0
var _trace_dt := 0.1
var _batch: WheelSpinBatch
var _first: Array = []
var _phase := 0
var _t0 := 0


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--spins="):
			_spins = int(a.get_slice("=", 1))
		elif a.begins_with("--parallel="):
			_parallel = int(a.get_slice("=", 1))
		elif a.begins_with("--seed="):
			_seed = int(a.get_slice("=", 1))
		elif a == "--repeat":
			_repeat = true
		elif a == "--fixed-rotor":
			_fixed_rotor = true
		elif a == "--list":
			_list = true
		elif a == "--trace":
			_trace = true
		elif a.begins_with("--trace-dt="):
			_trace = true
			_trace_dt = float(a.get_slice("=", 1))
		elif a.begins_with("--set="):
			var kv := a.get_slice("=", 1)
			_overrides[kv.get_slice(":", 0)] = str_to_var(kv.get_slice(":", 1))
	print("sim_spins: spins=%d parallel=%d first_seed=%d physics_ticks=%d rotor_phase=%s" % [
		_spins, _parallel, _seed, Engine.physics_ticks_per_second, "fixed 0" if _fixed_rotor else "random per seed"])
	_t0 = Time.get_ticks_msec()
	_start_batch.call_deferred()


func _seeds() -> Array[int]:
	var s: Array[int] = []
	for k in _spins:
		s.append(_seed + k)
	return s


func _start_batch() -> void:
	_batch = WheelSpinBatch.new()
	root.add_child(_batch)
	_batch.finished.connect(_on_finished)
	_batch.overrides = _overrides
	_batch.random_rotor_phase = not _fixed_rotor
	if not _overrides.is_empty():
		print("overrides: ", _overrides)
	_batch.run(_seeds(), _parallel)


func _on_finished(results: Array) -> void:
	var sorted := results.duplicate()
	sorted.sort_custom(func(a, b): return a["seed"] < b["seed"])
	var wall := (Time.get_ticks_msec() - _t0) / 1000.0
	if _phase == 0:
		if _list:
			for r in sorted:
				print("seed=%d number=%s time=%.2f drop=%.2f timed_out=%s in_ring=%s hits=%s" % [
					r["seed"], WheelLayout.label(r["number"]) if r["number"] >= 0 else "-", r["time"],
					r["drop_time"], r["timed_out"], r["in_ring"], r["hits"]])
		print(WheelSpinBatch.summarize(sorted))
		print("wall clock: %.1f s" % wall)
	if _repeat and _phase == 0:
		_first = sorted
		_phase = 1
		_batch.queue_free()
		_start_batch.call_deferred()
		return
	if _repeat:
		var same := 0
		for k in sorted.size():
			if sorted[k]["number"] == _first[k]["number"] and sorted[k]["seed"] == _first[k]["seed"]:
				same += 1
		print("repeatability: %d / %d seeds gave the same pocket on the second run" % [same, sorted.size()])
		print("wall clock total: %.1f s" % wall)
	quit(0)


func _physics_process(delta: float) -> bool:
	if not _trace or _batch == null or _batch.get_wheels().is_empty():
		return false
	var w: RouletteWheel = _batch.get_wheels()[0]
	if not w.is_ball_in_play():
		return false
	_trace_t += delta
	if _trace_t < _trace_dt - 0.0001:
		return false
	_trace_t = 0.0
	var b := w.get_ball()
	var lp := w.to_local(b.global_position)
	var rel := b.linear_velocity - PocketMath.rotor_velocity_at(lp, w.get_current_rotor_speed())
	print("t=%.2f r=%.4f y=%.4f speed=%.3f rel=%.3f spin=%.1f" % [w.get_spin_time(), PocketMath.radius_of(lp), lp.y,
		b.linear_velocity.length(), rel.length(), b.angular_velocity.length()])
	return false
