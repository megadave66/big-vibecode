extends PanelContainer
## Settings modal: volume sliders for the Master, SFX and Ambient buses.

signal closed

const BUS_NAMES: Array[String] = ["Master", "SFX", "Ambient"]

@onready var master_slider: HSlider = %MasterSlider
@onready var sfx_slider: HSlider = %SFXSlider
@onready var ambient_slider: HSlider = %AmbientSlider
@onready var master_value: Label = %MasterValue
@onready var sfx_value: Label = %SFXValue
@onready var ambient_value: Label = %AmbientValue


func _ready() -> void:
	_init_slider(master_slider, master_value, "Master")
	_init_slider(sfx_slider, sfx_value, "SFX")
	_init_slider(ambient_slider, ambient_value, "Ambient")
	master_slider.value_changed.connect(_on_slider_changed.bind("Master", master_value))
	sfx_slider.value_changed.connect(_on_slider_changed.bind("SFX", sfx_value))
	ambient_slider.value_changed.connect(_on_slider_changed.bind("Ambient", ambient_value))
	%CloseButton.pressed.connect(_on_close_button_pressed)


func _init_slider(slider: HSlider, label: Label, bus: String) -> void:
	slider.set_value_no_signal(_get_volume(bus))
	label.text = _percent(slider.value)


func _audio() -> Node:
	return get_node_or_null("/root/Audio")


func _get_volume(bus: String) -> float:
	var audio := _audio()
	if audio != null:
		return audio.get_bus_volume(bus)
	var idx := AudioServer.get_bus_index(bus)
	if idx < 0:
		return 1.0
	return 0.0 if AudioServer.is_bus_mute(idx) else db_to_linear(AudioServer.get_bus_volume_db(idx))


func _set_volume(bus: String, value: float) -> void:
	var audio := _audio()
	if audio != null:
		audio.set_bus_volume(bus, value)
		return
	var idx := AudioServer.get_bus_index(bus)
	if idx >= 0:
		AudioServer.set_bus_mute(idx, value <= 0.0)
		AudioServer.set_bus_volume_db(idx, linear_to_db(value) if value > 0.0 else -80.0)


func _percent(value: float) -> String:
	return "%d%%" % roundi(value * 100.0)


func _on_slider_changed(value: float, bus: String, label: Label) -> void:
	label.text = _percent(value)
	_set_volume(bus, value)


func _on_close_button_pressed() -> void:
	closed.emit()
	queue_free()
