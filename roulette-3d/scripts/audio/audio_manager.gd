extends Node
class_name AudioManager
## Central audio manager. Autoload name: "Audio" (the class_name must stay AudioManager).
## Plays SFX from a pool, runs the ambient loop and the ball-roll loop,
## and keeps the three bus volumes (Master, SFX, Ambient) saved between runs.

const DEFAULT_SETTINGS_PATH := "user://settings.cfg"
const POOL_SIZE := 8
const SAVE_DELAY := 0.5
const MIX_RATE := 22050
const BUSES: Array[String] = ["Master", "SFX", "Ambient"]
const DEFAULT_VOLUMES := {"Master": 1.0, "SFX": 1.0, "Ambient": 0.5}
const AMBIENT_PLAYER_DB := -10.0

## Where settings are saved. Tests set this before adding the node to the tree.
var settings_path: String = DEFAULT_SETTINGS_PATH

## SFX name -> list of variants. One variant is picked at random per play.
var _sfx_paths: Dictionary = {
	"chip_place": ["res://assets/audio/chip_place_1.ogg", "res://assets/audio/chip_place_2.ogg"],
	"chip_remove": ["res://assets/audio/chip_remove_1.ogg", "res://assets/audio/chip_remove_2.ogg"],
	"ball_launch": ["res://assets/audio/ball_launch.ogg"],
	"fret_click": [
		"res://assets/audio/fret_click_1.ogg",
		"res://assets/audio/fret_click_2.ogg",
		"res://assets/audio/fret_click_3.ogg",
	],
	"deflector_hit": ["res://assets/audio/deflector_hit.ogg"],
	"win": ["res://assets/audio/win_chime.ogg"],
	"lose": ["res://assets/audio/lose_buzz.ogg"],
	"ui_click": ["res://assets/audio/ui_click.ogg"],
}

var _volumes: Dictionary = {}
var _sfx_pool: Array[AudioStreamPlayer] = []
var _pool_index: int = 0
var _stream_cache: Dictionary = {}
var _ambient_player: AudioStreamPlayer
var _ambient_stream: AudioStreamWAV
var _ball_roll_player: AudioStreamPlayer
var _ball_roll_stream: AudioStreamWAV
var _save_timer: Timer
var _dirty: bool = false
var _rng := RandomNumberGenerator.new()


func _ready() -> void:
	_rng.randomize()
	_volumes = DEFAULT_VOLUMES.duplicate()

	for i in POOL_SIZE:
		var player := AudioStreamPlayer.new()
		player.bus = "SFX"
		add_child(player)
		_sfx_pool.append(player)

	_ambient_stream = _make_ambient_stream()
	_ambient_player = AudioStreamPlayer.new()
	_ambient_player.bus = "Ambient"
	_ambient_player.stream = _ambient_stream
	_ambient_player.volume_db = AMBIENT_PLAYER_DB
	add_child(_ambient_player)

	_ball_roll_stream = _make_ball_roll_stream()
	_ball_roll_player = AudioStreamPlayer.new()
	_ball_roll_player.bus = "SFX"
	_ball_roll_player.stream = _ball_roll_stream
	_ball_roll_player.volume_db = -40.0
	add_child(_ball_roll_player)

	_save_timer = Timer.new()
	_save_timer.one_shot = true
	_save_timer.wait_time = SAVE_DELAY
	_save_timer.timeout.connect(flush_settings)
	add_child(_save_timer)

	_load_settings()


## Names that play_sfx accepts.
func sfx_names() -> Array:
	return _sfx_paths.keys()


## All file paths behind one SFX name (empty if the name is unknown).
func sfx_variants(sfx_name: String) -> Array:
	return _sfx_paths.get(sfx_name, [])


func get_ball_roll_stream() -> AudioStreamWAV:
	return _ball_roll_stream


func get_ambient_stream() -> AudioStreamWAV:
	return _ambient_stream


func play_sfx(sfx_name: String, volume_db: float = 0.0, pitch: float = 1.0) -> void:
	var variants: Array = _sfx_paths.get(sfx_name, [])
	if variants.is_empty():
		push_warning("Audio: unknown SFX '%s'" % sfx_name)
		return
	var path: String = variants[_rng.randi() % variants.size()]
	var stream := _get_stream(path)
	if stream == null:
		push_warning("Audio: cannot load '%s'" % path)
		return
	var player := _next_player()
	player.stream = stream
	player.volume_db = volume_db
	player.pitch_scale = maxf(pitch, 0.01)
	player.play()


## Start or stop the rolling-ball loop. Intensity 0..1 drives volume and pitch.
func set_ball_roll(active: bool, intensity: float = 1.0) -> void:
	if _ball_roll_player == null:
		return
	if not active:
		if _ball_roll_player.playing:
			_ball_roll_player.stop()
		return
	var k := clampf(intensity, 0.0, 1.0)
	_ball_roll_player.pitch_scale = lerpf(0.7, 1.35, k)
	_ball_roll_player.volume_db = lerpf(-34.0, -8.0, k)
	if not _ball_roll_player.playing:
		_ball_roll_player.play()


func is_ball_rolling() -> bool:
	return _ball_roll_player != null and _ball_roll_player.playing


func start_ambient() -> void:
	if _ambient_player != null and not _ambient_player.playing:
		_ambient_player.play()


func stop_ambient() -> void:
	if _ambient_player != null:
		_ambient_player.stop()


## Set a bus volume (linear 0..1). Saves after a short delay.
func set_bus_volume(bus: String, linear_volume: float) -> void:
	var idx := AudioServer.get_bus_index(bus)
	if idx < 0:
		push_warning("Audio: unknown bus '%s'" % bus)
		return
	var v := clampf(linear_volume, 0.0, 1.0)
	AudioServer.set_bus_mute(idx, v <= 0.0)
	AudioServer.set_bus_volume_db(idx, linear_to_db(v) if v > 0.0 else -80.0)
	_volumes[bus] = v
	_request_save()


func get_bus_volume(bus: String) -> float:
	var idx := AudioServer.get_bus_index(bus)
	if idx < 0:
		push_warning("Audio: unknown bus '%s'" % bus)
		return 1.0
	if AudioServer.is_bus_mute(idx):
		return 0.0
	return db_to_linear(AudioServer.get_bus_volume_db(idx))


## Write settings now (cancels any pending delayed save).
func flush_settings() -> void:
	if _save_timer != null:
		_save_timer.stop()
	_dirty = false
	var config := ConfigFile.new()
	for bus in BUSES:
		config.set_value("audio", bus.to_lower() + "_volume", float(_volumes.get(bus, 1.0)))
	var err := config.save(settings_path)
	if err != OK:
		push_warning("Audio: cannot save settings (%d)" % err)


func _request_save() -> void:
	_dirty = true
	if _save_timer != null and is_inside_tree():
		_save_timer.start()


func _load_settings() -> void:
	var config := ConfigFile.new()
	var has_file := config.load(settings_path) == OK
	for bus in BUSES:
		var v: float = DEFAULT_VOLUMES[bus]
		if has_file:
			v = float(config.get_value("audio", bus.to_lower() + "_volume", v))
		set_bus_volume(bus, v)
	_dirty = false
	if _save_timer != null:
		_save_timer.stop()


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST or what == NOTIFICATION_EXIT_TREE:
		if _dirty:
			flush_settings()
	if what == NOTIFICATION_EXIT_TREE:
		# Stop looping players so their playbacks are released before shutdown (avoids leak warnings).
		for p in [_ambient_player, _ball_roll_player]:
			if p != null:
				p.stop()
				p.stream = null
		_ambient_stream = null
		_ball_roll_stream = null


func _get_stream(path: String) -> AudioStream:
	if _stream_cache.has(path):
		return _stream_cache[path]
	var stream := load(path) as AudioStream
	if stream != null:
		_stream_cache[path] = stream
	return stream


## Prefer an idle player. If all are busy, steal in round-robin order.
func _next_player() -> AudioStreamPlayer:
	for p in _sfx_pool:
		if not p.playing:
			return p
	var p := _sfx_pool[_pool_index]
	_pool_index = (_pool_index + 1) % _sfx_pool.size()
	return p


# ---- procedural sounds -------------------------------------------------

## Soft room murmur: low-passed noise, a faint low hum, slow swells.
## 3 s mono loop, built once. The noise filter wraps round, so the loop is seamless.
func _make_ambient_stream() -> AudioStreamWAV:
	var seconds := 3
	var n := MIX_RATE * seconds
	var rng := RandomNumberGenerator.new()
	rng.seed = 7731
	var noise := PackedFloat32Array()
	noise.resize(n)
	for i in n:
		noise[i] = rng.randf_range(-1.0, 1.0)
	var low := _circular_lowpass(noise, 0.04, 2)
	var low2 := _circular_lowpass(noise, 0.012, 2)
	var out := PackedFloat32Array()
	out.resize(n)
	for i in n:
		var t := float(i) / MIX_RATE
		# Whole cycles per loop keep the ends matched.
		var swell := 0.75 + 0.25 * sin(TAU * t / seconds)
		var murmur := (low[i] * 5.0 + low2[i] * 7.0) * swell
		var hum := sin(TAU * 54.0 * t) * 0.05 + sin(TAU * 108.0 * t) * 0.02
		out[i] = murmur * 0.35 + hum
	return _to_wav(out, 0.5)


## Rolling ball: band-limited rumble with a quick turning pulse. 1 s mono loop.
func _make_ball_roll_stream() -> AudioStreamWAV:
	var n := MIX_RATE
	var rng := RandomNumberGenerator.new()
	rng.seed = 4242
	var noise := PackedFloat32Array()
	noise.resize(n)
	for i in n:
		noise[i] = rng.randf_range(-1.0, 1.0)
	var low := _circular_lowpass(noise, 0.12, 2)
	var mid := _circular_lowpass(noise, 0.35, 1)
	var out := PackedFloat32Array()
	out.resize(n)
	for i in n:
		var t := float(i) / MIX_RATE
		var pulse := 0.7 + 0.3 * sin(TAU * 7.0 * t)
		out[i] = (low[i] * 3.0 + (mid[i] - low[i]) * 1.2) * pulse
	return _to_wav(out, 0.8)


## One-pole low-pass that runs round the loop so the join has no click.
func _circular_lowpass(src: PackedFloat32Array, alpha: float, passes: int) -> PackedFloat32Array:
	var n := src.size()
	var cur := src.duplicate()
	for p in passes:
		var y := cur[n - 1]
		# Two laps: the first settles the state, the second writes the result.
		for lap in 2:
			for i in n:
				y += alpha * (cur[i] - y)
				if lap == 1:
					cur[i] = y
	return cur


## Normalise to `peak`, convert to 16-bit mono, loop forward.
func _to_wav(samples: PackedFloat32Array, peak: float) -> AudioStreamWAV:
	var n := samples.size()
	var top := 0.0001
	for s in samples:
		top = maxf(top, absf(s))
	var scale := peak / top
	var bytes := PackedByteArray()
	bytes.resize(n * 2)
	for i in n:
		bytes.encode_s16(i * 2, int(clampf(samples[i] * scale, -1.0, 1.0) * 32767.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = MIX_RATE
	wav.stereo = false
	wav.data = bytes
	wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
	wav.loop_begin = 0
	wav.loop_end = n
	return wav
