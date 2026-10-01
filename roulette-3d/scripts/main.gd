extends Node3D
## Scene root and integration point. Wires wheel, bet layout, round flow, HUD and audio.
## Game rules live in scripts/core/*; this file only routes signals.

const WHEEL_SCENE := preload("res://scenes/wheel/wheel.tscn")
const HUD_SCENE := preload("res://scenes/ui/hud.tscn")
const SETTINGS_SCENE := preload("res://scenes/ui/settings_screen.tscn")
const HOW_TO_PLAY_SCENE := preload("res://scenes/ui/how_to_play_screen.tscn")

## Seconds to show the winning pocket on the wheel before the camera returns to the table.
const SHOW_WINNER_TIME := 1.6

@onready var camera_rig: CameraRig = $CameraRig
@onready var table: Node3D = $Table
@onready var wheel_anchor: Marker3D = $WheelAnchor
@onready var table_input: TableInput = $TableInput
@onready var ui: CanvasLayer = $UI

var layout: BetLayout
var flow: RoundFlow
var wheel: RouletteWheel
var overlay: BetOverlay
var hud: Hud
var rng := RandomNumberGenerator.new()
var _modal: Control = null
var _resolving := false


func _ready() -> void:
	rng.randomize()
	layout = BetLayout.new()
	flow = RoundFlow.new(_spot_info)

	wheel = WHEEL_SCENE.instantiate()
	wheel_anchor.add_child(wheel)

	var mapper: TableLayoutMapper = table.get_node("TableLayoutMapper")
	overlay = BetOverlay.new()
	overlay.name = "BetOverlay"
	mapper.get_node("SpotOverlay").add_child(overlay)
	overlay.setup(layout, mapper)

	hud = HUD_SCENE.instantiate()
	ui.add_child(hud)

	table_input.table_clicked.connect(_on_table_clicked)
	table_input.table_hovered.connect(_on_table_hovered)
	flow.bets_changed.connect(_on_bets_changed)
	flow.bankroll_changed.connect(hud.set_bankroll)
	flow.phase_changed.connect(_on_phase_changed)
	hud.spin_pressed.connect(spin)
	hud.clear_pressed.connect(_on_clear_pressed)
	hud.settings_pressed.connect(_open_modal.bind(SETTINGS_SCENE))
	hud.how_to_play_pressed.connect(_open_modal.bind(HOW_TO_PLAY_SCENE))
	wheel.ball_settled.connect(_on_ball_settled)
	wheel.ball_timed_out.connect(_on_ball_timed_out)
	wheel.ball_collided.connect(_on_ball_collided)

	hud.set_bankroll(flow.bankroll.balance)
	hud.set_total_bet(0)
	hud.set_phase(flow.phase)
	_audio("start_ambient")
	print("Roulette 3D ready: %d bet spots, bankroll %d" % [layout.spots().size(), flow.bankroll.balance])


func _spot_info(id: String) -> Dictionary:
	var s := layout.get_spot(id)
	if s == null:
		return {}
	return {"type": s.type, "numbers": s.numbers}


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("spin") and _modal == null:
		spin()
		get_viewport().set_input_as_handled()
	elif event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE and _modal != null:
		_close_modal()
		get_viewport().set_input_as_handled()


## Start a spin: lock bets ("no more bets"), then launch the ball.
func spin() -> void:
	if _modal != null or not flow.lock_bets():
		return
	overlay.clear_hover()
	hud.hide_tooltip()
	wheel.clear_highlight()
	camera_rig.set_view("wheel")
	wheel.launch_ball(rng)
	_audio("play_sfx", ["ball_launch"])


func _process(_delta: float) -> void:
	if wheel != null and wheel.is_ball_in_play():
		var speed := wheel.get_ball().linear_velocity.length()
		_audio("set_ball_roll", [speed > 0.05, clampf(speed / 3.0, 0.0, 1.0)])
	else:
		_audio("set_ball_roll", [false, 0.0])


func _on_ball_settled(number: int) -> void:
	if not flow.ball_settled(number):
		return
	wheel.highlight_pocket(number)
	hud.announce_number(number)
	hud.push_history(number)
	overlay.highlight_winning(number, layout)
	await get_tree().create_timer(SHOW_WINNER_TIME).timeout
	camera_rig.set_view("table")
	await get_tree().create_timer(0.8).timeout
	_resolve_round()


func _resolve_round() -> void:
	_resolving = true
	var result := flow.resolve()
	overlay.animate_resolution(result)
	if result.get("winners", []).size() > 0:
		_audio("play_sfx", ["win"])
	elif result.get("losers", []).size() > 0:
		_audio("play_sfx", ["lose"])
	await overlay.resolution_animation_finished
	hud.set_last_result(int(result.get("net", 0)))
	overlay.clear_winning()
	overlay.show_stacks(flow.book)
	_resolving = false
	flow.next_round()


## Honest fail-safe: the ball did not settle in time. No result is invented; the same
## locked bets ride on a fresh launch.
func _on_ball_timed_out() -> void:
	hud.set_status("Re-spin: ball did not settle")
	wheel.reset_ball()
	wheel.launch_ball(rng)


func _on_ball_collided(kind: String, strength: float) -> void:
	var vol := linear_to_db(clampf(strength, 0.05, 1.0))
	match kind:
		"fret", "pocket":
			_audio("play_sfx", ["fret_click", vol, rng.randf_range(0.9, 1.15)])
		"deflector", "rim":
			_audio("play_sfx", ["deflector_hit", vol, rng.randf_range(0.95, 1.05)])


func _on_table_clicked(layout_pos: Vector2, button_index: int) -> void:
	if _modal != null:
		return
	var spot := layout.spot_at(layout_pos)
	if spot == null:
		return
	if button_index == MOUSE_BUTTON_LEFT:
		if flow.place_chip(spot.id, hud.selected_chip()):
			_audio("play_sfx", ["chip_place"])
	elif button_index == MOUSE_BUTTON_RIGHT:
		if flow.remove_chip(spot.id):
			_audio("play_sfx", ["chip_remove"])


func _on_table_hovered(layout_pos: Vector2, on_table: bool) -> void:
	if _modal != null or not flow.is_betting_open():
		overlay.clear_hover()
		hud.hide_tooltip()
		return
	var spot := overlay.hover_at(layout_pos, on_table)
	hud.show_spot_tooltip(spot, get_viewport().get_mouse_position())


func _on_bets_changed() -> void:
	if not _resolving:
		overlay.show_stacks(flow.book)
	hud.set_total_bet(flow.book.total())


func _on_clear_pressed() -> void:
	if flow.clear_bets():
		_audio("play_sfx", ["chip_remove"])


func _on_phase_changed(phase: int) -> void:
	hud.set_phase(phase)
	if phase == RoundFlow.Phase.BETTING:
		hud.set_total_bet(flow.book.total())


func _open_modal(scene: PackedScene) -> void:
	if _modal != null:
		return
	_audio("play_sfx", ["ui_click"])
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.6)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	var center := CenterContainer.new()
	center.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.add_child(center)
	var screen: Control = scene.instantiate()
	center.add_child(screen)
	screen.closed.connect(_close_modal)
	ui.add_child(dim)
	_modal = dim
	hud.hide_tooltip()
	overlay.clear_hover()


func _close_modal() -> void:
	if _modal == null:
		return
	_modal.queue_free()
	_modal = null
	_audio("play_sfx", ["ui_click"])


## Call the Audio autoload if present (it is absent in some unit tests).
func _audio(method: String, args: Array = []) -> void:
	var audio := get_node_or_null("/root/Audio")
	if audio != null and audio.has_method(method):
		audio.callv(method, args)
