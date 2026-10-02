extends GdUnitTestSuite
## Tests for scripts/audio/audio_manager.gd

const AudioScript := preload("res://scripts/audio/audio_manager.gd")
const TEST_PATH := "user://test_audio_settings.cfg"
const EXPECTED_NAMES: Array[String] = [
	"chip_place", "chip_remove", "ball_launch", "fret_click",
	"deflector_hit", "win", "lose", "ui_click",
]

var _saved: Dictionary = {}


func before_test() -> void:
	for bus in ["Master", "SFX", "Ambient"]:
		_saved[bus] = AudioServer.get_bus_volume_db(AudioServer.get_bus_index(bus))
	DirAccess.remove_absolute(ProjectSettings.globalize_path(TEST_PATH))


func after_test() -> void:
	for bus in _saved:
		var idx := AudioServer.get_bus_index(bus)
		AudioServer.set_bus_volume_db(idx, _saved[bus])
		AudioServer.set_bus_mute(idx, false)
	DirAccess.remove_absolute(ProjectSettings.globalize_path(TEST_PATH))


func _make() -> AudioManager:
	var audio: AudioManager = auto_free(AudioScript.new())
	audio.settings_path = TEST_PATH
	add_child(audio)
	return audio


func test_bus_volume_set_get() -> void:
	var audio := _make()
	audio.set_bus_volume("Master", 0.5)
	assert_float(audio.get_bus_volume("Master")).is_equal_approx(0.5, 0.01)
	audio.set_bus_volume("SFX", 0.75)
	assert_float(audio.get_bus_volume("SFX")).is_equal_approx(0.75, 0.01)
	audio.set_bus_volume("Ambient", 0.25)
	assert_float(audio.get_bus_volume("Ambient")).is_equal_approx(0.25, 0.01)
	audio.set_bus_volume("Master", 1.5)
	assert_float(audio.get_bus_volume("Master")).is_equal_approx(1.0, 0.01)
	audio.set_bus_volume("Master", -0.5)
	assert_float(audio.get_bus_volume("Master")).is_equal(0.0)


func test_unknown_names_do_not_crash() -> void:
	var audio := _make()
	audio.play_sfx("no_such_sound")
	audio.set_bus_volume("NoSuchBus", 0.5)
	assert_float(audio.get_bus_volume("NoSuchBus")).is_equal(1.0)
	assert_bool(audio.is_inside_tree()).is_true()


func test_known_sfx_paths_exist() -> void:
	var audio := _make()
	var names: Array = audio.sfx_names()
	for sfx_name in EXPECTED_NAMES:
		assert_array(names).contains([sfx_name])
		var variants: Array = audio.sfx_variants(sfx_name)
		assert_array(variants).is_not_empty()
		for path in variants:
			assert_bool(ResourceLoader.exists(path)).override_failure_message(
				"missing file for %s: %s" % [sfx_name, path]).is_true()
			assert_object(load(path)).is_not_null()
	assert_int(audio.sfx_variants("fret_click").size()).is_equal(3)
	assert_int(audio.sfx_variants("chip_place").size()).is_equal(2)
	assert_int(audio.sfx_variants("chip_remove").size()).is_equal(2)
	for sfx_name in EXPECTED_NAMES:
		audio.play_sfx(sfx_name)


func test_settings_persist_through_manager() -> void:
	var first := _make()
	first.set_bus_volume("Master", 0.8)
	first.set_bus_volume("SFX", 0.6)
	first.set_bus_volume("Ambient", 0.4)
	first.flush_settings()
	assert_bool(FileAccess.file_exists(TEST_PATH)).is_true()
	# Change the live buses, then let a fresh manager load the file.
	for bus in ["Master", "SFX", "Ambient"]:
		AudioServer.set_bus_volume_db(AudioServer.get_bus_index(bus), 0.0)
	var second := _make()
	assert_float(second.get_bus_volume("Master")).is_equal_approx(0.8, 0.01)
	assert_float(second.get_bus_volume("SFX")).is_equal_approx(0.6, 0.01)
	assert_float(second.get_bus_volume("Ambient")).is_equal_approx(0.4, 0.01)


func test_change_saves_after_delay() -> void:
	var audio := _make()
	audio.set_bus_volume("SFX", 0.3)
	assert_bool(FileAccess.file_exists(TEST_PATH)).is_false()
	await await_millis(900)
	assert_bool(FileAccess.file_exists(TEST_PATH)).is_true()
	var config := ConfigFile.new()
	assert_int(config.load(TEST_PATH)).is_equal(OK)
	assert_float(config.get_value("audio", "sfx_volume", 1.0)).is_equal_approx(0.3, 0.01)


func test_ball_roll_stream_loops() -> void:
	var audio := _make()
	var wav := audio.get_ball_roll_stream()
	assert_object(wav).is_not_null()
	assert_int(wav.loop_mode).is_equal(AudioStreamWAV.LOOP_FORWARD)
	assert_int(wav.format).is_equal(AudioStreamWAV.FORMAT_16_BITS)
	assert_int(wav.loop_end).is_greater(0)
	var amb := audio.get_ambient_stream()
	assert_int(amb.loop_mode).is_equal(AudioStreamWAV.LOOP_FORWARD)
	assert_int(amb.format).is_equal(AudioStreamWAV.FORMAT_16_BITS)


func test_ball_roll_start_stop() -> void:
	var audio := _make()
	audio.set_ball_roll(true, 0.5)
	assert_bool(audio.is_ball_rolling()).is_true()
	audio.set_ball_roll(false, 0.0)
	assert_bool(audio.is_ball_rolling()).is_false()
