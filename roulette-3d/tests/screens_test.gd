extends GdUnitTestSuite
## Tests for the settings and how-to-play screens.

const SETTINGS := preload("res://scenes/ui/settings_screen.tscn")
const HOWTO := preload("res://scenes/ui/how_to_play_screen.tscn")

var _saved: Dictionary = {}


func before_test() -> void:
	for bus in ["Master", "SFX", "Ambient"]:
		_saved[bus] = db_to_linear(AudioServer.get_bus_volume_db(AudioServer.get_bus_index(bus)))


func after_test() -> void:
	for bus in _saved:
		# Restore through the autoload so the saved settings file is not polluted.
		var audio := get_node_or_null("/root/Audio")
		if audio != null:
			audio.set_bus_volume(bus, _saved[bus])
			audio.flush_settings()
		else:
			AudioServer.set_bus_volume_db(AudioServer.get_bus_index(bus), linear_to_db(_saved[bus]))


func _show(scene: PackedScene) -> PanelContainer:
	var screen: PanelContainer = auto_free(scene.instantiate())
	add_child(screen)
	await get_tree().process_frame
	return screen


func test_both_screens_instantiate() -> void:
	var s := await _show(SETTINGS)
	var h := await _show(HOWTO)
	assert_object(s).is_not_null()
	assert_object(h).is_not_null()
	assert_bool(s.is_inside_tree()).is_true()
	assert_bool(h.is_inside_tree()).is_true()


func test_how_to_play_lists_every_bet_type() -> void:
	var h := await _show(HOWTO)
	var text: String = (h.get_node("%RulesText") as RichTextLabel).text
	for t in Payouts.BetType.values():
		assert_str(text).contains(Payouts.type_name(t))
		assert_str(text).contains(Payouts.odds_text(t))
	assert_str(text).contains("38 pockets")
	assert_str(text).contains("$1,000")
	assert_str(text).contains("0-00-1-2-3")
	assert_str(text).not_contains("number keys")


func test_how_to_play_panel_has_size() -> void:
	var h := await _show(HOWTO)
	assert_float(h.custom_minimum_size.x).is_greater_equal(900.0)
	var rules: RichTextLabel = h.get_node("%RulesText")
	assert_bool(rules.fit_content).is_true()
	assert_float(rules.size.y).is_greater(100.0)


func test_sliders_change_bus_volumes() -> void:
	var s := await _show(SETTINGS)
	var cases := {"%MasterSlider": "Master", "%SFXSlider": "SFX", "%AmbientSlider": "Ambient"}
	for path in cases:
		var slider: HSlider = s.get_node(path)
		slider.value = 0.5
		var idx := AudioServer.get_bus_index(cases[path])
		assert_float(db_to_linear(AudioServer.get_bus_volume_db(idx))).is_equal_approx(0.5, 0.02)
	(s.get_node("%MasterValue") as Label).text
	assert_str((s.get_node("%MasterValue") as Label).text).is_equal("50%")


func test_sliders_start_at_current_values() -> void:
	AudioServer.set_bus_volume_db(AudioServer.get_bus_index("SFX"), linear_to_db(0.3))
	var s := await _show(SETTINGS)
	assert_float((s.get_node("%SFXSlider") as HSlider).value).is_equal_approx(0.3, 0.02)


func test_close_signals() -> void:
	for scene in [SETTINGS, HOWTO]:
		var screen: PanelContainer = scene.instantiate()
		add_child(screen)
		await get_tree().process_frame
		var got: Array[bool] = [false]
		screen.closed.connect(func() -> void: got[0] = true)
		(screen.get_node("%CloseButton") as Button).pressed.emit()
		assert_bool(got[0]).is_true()
