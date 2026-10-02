class_name WheelSpinBatch
extends Node3D
## Test/tool helper: runs many real physics spins on several wheels at once.
## Wheels sit far apart (spacing) so their balls never interact.
## Each spin: restart_rotor(0), launch_ball(rng seeded with the spin seed), wait for
## ball_settled or ball_timed_out. Results are honest raw outcomes.

signal finished(results: Array)

const WHEEL_SCENE := preload("res://scenes/wheel/wheel.tscn")

## Distance between wheels (m).
@export var spacing: float = 2.0
## Restart the rotor before every launch, so the seed alone decides the start state.
@export var restart_rotor_each_spin: bool = true
## With restart: put the rotor at a random angle drawn from the spin's rng (a dealer
## launches at an arbitrary moment). Off = rotor always at angle 0 (pure launch chaos).
@export var random_rotor_phase: bool = true

## Property overrides applied to every wheel before it enters the tree (tuning aid).
var overrides: Dictionary = {}
## One Dictionary per spin: {seed, number, time, drop_time, timed_out, in_ring, on_floor, local_pos, wheel, hits}.
var results: Array = []

var _queue: Array[int] = []
var _wheels: Array[RouletteWheel] = []
var _current: Dictionary = {}  # wheel index -> Dictionary (result being filled)
var _done: bool = false


## Start running all seeds on `parallel` wheels. Await `finished` (or poll is_done()).
func run(seeds: Array[int], parallel: int) -> void:
	_queue = seeds.duplicate()
	results.clear()
	_done = false
	var count := mini(parallel, maxi(1, seeds.size()))
	var side := int(ceil(sqrt(count)))
	for i in count:
		var w: RouletteWheel = WHEEL_SCENE.instantiate()
		w.name = "Wheel_%d" % i
		w.position = Vector3((i % side) * spacing, 0.0, (i / side) * spacing)
		for key in overrides:
			w.set(key, overrides[key])
		add_child(w)
		w.ball_settled.connect(_on_settled.bind(i))
		w.ball_timed_out.connect(_on_timed_out.bind(i))
		w.ball_collided.connect(_on_collided.bind(i))
		_wheels.append(w)
	for i in count:
		_launch_next(i)


func is_done() -> bool:
	return _done


func get_wheels() -> Array[RouletteWheel]:
	return _wheels


func _launch_next(i: int) -> void:
	if _queue.is_empty():
		_current.erase(i)
		if _current.is_empty() and not _done:
			_done = true
			finished.emit(results)
		return
	var seed_value: int = _queue.pop_front()
	var w := _wheels[i]
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_value
	if restart_rotor_each_spin:
		w.restart_rotor(rng.randf() * TAU if random_rotor_phase else 0.0)
	_current[i] = {"seed": seed_value, "number": -1, "time": 0.0, "drop_time": -1.0,
			"timed_out": false, "in_ring": false, "wheel": i, "hits": {}}
	w.launch_ball(rng)


func _physics_process(_delta: float) -> void:
	for i in _current:
		var res: Dictionary = _current[i]
		if res["drop_time"] < 0.0:
			var w := _wheels[i]
			var lp := w.to_local(w.get_ball().global_position)
			if PocketMath.radius_of(lp) < WheelGeometry.TRACK_INNER_R - WheelGeometry.BALL_RADIUS:
				res["drop_time"] = w.get_spin_time()


func _finish(i: int) -> void:
	var res: Dictionary = _current[i]
	results.append(res)
	_current.erase(i)
	# Launch the next one on the following frame so signal handlers finish first.
	_current[i] = {"seed": -1, "number": -1, "time": 0.0, "drop_time": 0.0, "timed_out": false,
			"in_ring": false, "wheel": i, "hits": {}}
	call_deferred("_launch_next", i)


func _on_settled(number: int, i: int) -> void:
	var w := _wheels[i]
	var res: Dictionary = _current[i]
	res["number"] = number
	res["time"] = w.last_spin_time
	var lp := w.get_ball_rotor_local_position()
	res["in_ring"] = PocketMath.is_in_pocket_ring(lp, WheelGeometry.POCKET_INNER_R, WheelGeometry.POCKET_OUTER_R,
			WheelGeometry.FRET_TOP_Y)
	res["local_pos"] = lp
	res["on_floor"] = is_resting_on_pocket_floor(lp)
	_finish(i)


func _on_timed_out(i: int) -> void:
	var w := _wheels[i]
	var res: Dictionary = _current[i]
	res["timed_out"] = true
	res["time"] = w.last_spin_time
	res["local_pos"] = w.get_ball_rotor_local_position()
	_finish(i)


func _on_collided(kind: String, _strength: float, i: int) -> void:
	if not _current.has(i):
		return
	var hits: Dictionary = _current[i]["hits"]
	hits[kind] = int(hits.get(kind, 0)) + 1


## Independent rest check (not the settle trigger): ball centre at rest height on the
## pocket floor, and its radius between the inner and outer pocket walls.
static func is_resting_on_pocket_floor(lp: Vector3) -> bool:
	var r := PocketMath.radius_of(lp)
	var tol := 0.002
	return absf(lp.y - WheelGeometry.pocket_rest_y()) <= 0.0015 \
			and r >= WheelGeometry.POCKET_INNER_R + WheelGeometry.BALL_RADIUS - tol \
			and r <= WheelGeometry.POCKET_OUTER_R - WheelGeometry.BALL_RADIUS + tol


## Summary text: count, timeouts, distribution, timing.
static func summarize(res: Array) -> String:
	var counts := {}
	var timeouts := 0
	var outside := 0
	var total_t := 0.0
	var min_t := INF
	var max_t := 0.0
	var drop_sum := 0.0
	var drop_n := 0
	var hit_tot := {}
	for r in res:
		if r["timed_out"]:
			timeouts += 1
			continue
		if not r["in_ring"]:
			outside += 1
		counts[r["number"]] = int(counts.get(r["number"], 0)) + 1
		total_t += r["time"]
		min_t = minf(min_t, r["time"])
		max_t = maxf(max_t, r["time"])
		if r["drop_time"] >= 0.0:
			drop_sum += r["drop_time"]
			drop_n += 1
		for k in r["hits"]:
			hit_tot[k] = int(hit_tot.get(k, 0)) + int(r["hits"][k])
	var settled := res.size() - timeouts
	var lines := PackedStringArray()
	lines.append("spins=%d settled=%d timeouts=%d outside_ring=%d" % [res.size(), settled, timeouts, outside])
	if settled > 0:
		lines.append("settle time s: mean=%.2f min=%.2f max=%.2f | mean drop-off-track time=%.2f" % [
			total_t / settled, min_t, max_t, drop_sum / maxf(1.0, drop_n)])
		var per_spin := PackedStringArray()
		for k in hit_tot:
			per_spin.append("%s=%.1f" % [k, float(hit_tot[k]) / settled])
		lines.append("collision events per spin: " + ", ".join(per_spin))
	lines.append("distinct pockets hit: %d / 38" % counts.size())
	var row := PackedStringArray()
	for n in WheelLayout.POCKET_ORDER:
		row.append("%s:%d" % [WheelLayout.label(n), int(counts.get(n, 0))])
	lines.append("by wheel order: " + " ".join(row))
	if settled > 0:
		var expected := float(settled) / 38.0
		var chi2 := 0.0
		for n in WheelLayout.POCKET_ORDER:
			var o := float(counts.get(n, 0))
			chi2 += (o - expected) * (o - expected) / expected
		lines.append("chi-square vs uniform (37 dof, 5%% critical ~52.2): %.1f" % chi2)
	return "\n".join(lines)
