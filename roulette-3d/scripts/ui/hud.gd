class_name Hud
extends Control
## Casino HUD. Top bar: bankroll, total bet, last round, status banner, Settings / How to play.
## Bottom bar: recent numbers, chip selector (1/5/25/100), Clear, SPIN.
## The middle of the screen stays free so the betting layout is never covered.
## Built in code so the scene file stays tiny. Instance scenes/ui/hud.tscn under the UI CanvasLayer.

signal chip_selected(value: int)
signal spin_pressed
signal clear_pressed
signal settings_pressed
signal how_to_play_pressed

const CHIP_VALUES: Array[int] = [1, 5, 25, 100]
const HISTORY_MAX := 14
const GOLD := Color(0.95, 0.78, 0.36)
const GOLD_DIM := Color(0.72, 0.58, 0.3)
const PANEL_BG := Color(0.07, 0.02, 0.03, 1.0)
const TEXT := Color(0.97, 0.93, 0.84)
const WIN_TXT := Color(0.45, 0.95, 0.5)
const LOSS_TXT := Color(1.0, 0.45, 0.4)
const POCKET_COLORS := {
	WheelLayout.PocketColor.GREEN: Color(0.06, 0.5, 0.22),
	WheelLayout.PocketColor.RED: Color(0.78, 0.08, 0.09),
	WheelLayout.PocketColor.BLACK: Color(0.07, 0.07, 0.08),
}

var bankroll_label: Label
var bet_label: Label
var last_label: Label
var status_label: Label
var history_box: HBoxContainer
var spin_button: Button
var clear_button: Button
var settings_button: Button
var how_to_play_button: Button
var chip_buttons: Dictionary = {}     # value -> Button
var tooltip_panel: PanelContainer
var tooltip_label: Label

var _selected_chip := 1
var _history: Array[int] = []
var _built := false


func _ready() -> void:
	_build()


## Spot hover text: "<label> pays <odds>", odds from Payouts.
static func tooltip_text(spot: BetSpot) -> String:
	return "%s pays %s" % [spot.label, Payouts.odds_text(spot.type)]


static func number_text(n: int) -> String:
	var c: WheelLayout.PocketColor = WheelLayout.color_of(n)
	var word: String = {WheelLayout.PocketColor.GREEN: "Green", WheelLayout.PocketColor.RED: "Red",
		WheelLayout.PocketColor.BLACK: "Black"}[c]
	return "%s %s" % [WheelLayout.label(n), word]


static func money(n: int) -> String:
	var s := str(absi(n))
	var out := ""
	while s.length() > 3:
		out = "," + s.substr(s.length() - 3) + out
		s = s.substr(0, s.length() - 3)
	return ("-" if n < 0 else "") + "$" + s + out


# ------------------------------------------------------------ public API

func set_bankroll(balance: int) -> void:
	_build()
	bankroll_label.text = money(balance)


func set_total_bet(amount: int) -> void:
	_build()
	bet_label.text = money(amount)


## Net result of the last round (total_returned - total_staked).
func set_last_result(net: int) -> void:
	_build()
	if net > 0:
		last_label.text = "+" + money(net)
		last_label.add_theme_color_override("font_color", WIN_TXT)
	elif net < 0:
		last_label.text = "-" + money(-net)
		last_label.add_theme_color_override("font_color", LOSS_TXT)
	else:
		last_label.text = money(0)
		last_label.add_theme_color_override("font_color", TEXT)


## Phase from RoundFlow.Phase. BETTING enables buttons; other phases disable them.
func set_phase(phase: int) -> void:
	_build()
	match phase:
		RoundFlow.Phase.BETTING:
			set_status("Place your bets")
			set_buttons_enabled(true)
		RoundFlow.Phase.LOCKED:
			set_status("No more bets")
			set_buttons_enabled(false)
		_:
			set_buttons_enabled(false)


func set_status(text: String, color: Color = GOLD) -> void:
	_build()
	status_label.text = text
	status_label.add_theme_color_override("font_color", color)


## Banner like "17 Red", tinted by pocket colour.
func announce_number(n: int) -> void:
	var banner := {WheelLayout.PocketColor.GREEN: Color(0.45, 1.0, 0.55),
		WheelLayout.PocketColor.RED: Color(1.0, 0.42, 0.38),
		WheelLayout.PocketColor.BLACK: Color(0.96, 0.96, 0.96)}
	set_status(number_text(n), banner[WheelLayout.color_of(n)])


func push_history(number: int) -> void:
	_build()
	_history.append(number)
	while _history.size() > HISTORY_MAX:
		_history.pop_front()
	_rebuild_history()


func set_history(numbers: Array) -> void:
	_build()
	_history.clear()
	for n in numbers.slice(maxi(0, numbers.size() - HISTORY_MAX)):
		_history.append(int(n))
	_rebuild_history()


func history() -> Array[int]:
	return _history.duplicate()


## Enables / disables SPIN and Clear. Chip selection always stays available.
func set_buttons_enabled(enabled: bool) -> void:
	_build()
	spin_button.disabled = not enabled
	clear_button.disabled = not enabled


func selected_chip() -> int:
	return _selected_chip


func select_chip(value: int) -> void:
	_build()
	if not chip_buttons.has(value):
		return
	_selected_chip = value
	for v in chip_buttons:
		(chip_buttons[v] as Button).set_pressed_no_signal(v == value)
	chip_selected.emit(value)


func show_tooltip(text: String, screen_pos: Vector2) -> void:
	_build()
	tooltip_label.text = text
	tooltip_panel.visible = true
	tooltip_panel.reset_size()
	var vp := get_viewport_rect().size
	var sz := tooltip_panel.get_combined_minimum_size()
	var p := screen_pos + Vector2(18, 18)
	p.x = clampf(p.x, 4.0, maxf(4.0, vp.x - sz.x - 4.0))
	if p.y + sz.y > vp.y - 4.0:
		p.y = screen_pos.y - sz.y - 12.0
	tooltip_panel.position = p


func show_spot_tooltip(spot: BetSpot, screen_pos: Vector2) -> void:
	if spot == null:
		hide_tooltip()
	else:
		show_tooltip(tooltip_text(spot), screen_pos)


func hide_tooltip() -> void:
	_build()
	tooltip_panel.visible = false


func tooltip_visible() -> bool:
	return tooltip_panel != null and tooltip_panel.visible


# ------------------------------------------------------------ build

func _panel_style(radius: int = 12) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = PANEL_BG
	sb.border_color = GOLD_DIM
	sb.set_border_width_all(2)
	sb.set_corner_radius_all(radius)
	sb.content_margin_left = 18
	sb.content_margin_right = 18
	sb.content_margin_top = 8
	sb.content_margin_bottom = 8
	sb.shadow_color = Color(0, 0, 0, 0.5)
	sb.shadow_size = 6
	return sb


func _label(text: String, size: int, color: Color = TEXT) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0, 0.8))
	l.add_theme_constant_override("outline_size", 4)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return l


func _stat(parent: Control, caption: String, value: String) -> Label:
	var v := VBoxContainer.new()
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_theme_constant_override("separation", -2)
	v.custom_minimum_size.x = 150
	v.add_child(_label(caption, 14, GOLD_DIM))
	var val := _label(value, 30)
	v.add_child(val)
	parent.add_child(v)
	return val


func _button_style(bg: Color, border: Color, radius: int, bw: int = 2) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg
	sb.border_color = border
	sb.set_border_width_all(bw)
	sb.set_corner_radius_all(radius)
	sb.content_margin_left = 16
	sb.content_margin_right = 16
	sb.content_margin_top = 6
	sb.content_margin_bottom = 6
	return sb


func _text_button(text: String, size: int, bg: Color, min_size: Vector2) -> Button:
	var b := Button.new()
	b.text = text
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = min_size
	b.add_theme_font_size_override("font_size", size)
	b.add_theme_color_override("font_color", TEXT)
	b.add_theme_color_override("font_hover_color", Color.WHITE)
	b.add_theme_color_override("font_disabled_color", Color(0.55, 0.5, 0.45))
	b.add_theme_stylebox_override("normal", _button_style(bg, GOLD_DIM, 10))
	b.add_theme_stylebox_override("hover", _button_style(bg.lightened(0.15), GOLD, 10))
	b.add_theme_stylebox_override("pressed", _button_style(bg.darkened(0.2), GOLD, 10))
	b.add_theme_stylebox_override("disabled", _button_style(bg.darkened(0.5), Color(0.3, 0.27, 0.22), 10))
	return b


func _chip_button(value: int, group: ButtonGroup) -> Button:
	var b := Button.new()
	b.text = "$%d" % value
	b.toggle_mode = true
	b.button_group = group
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(72, 72)
	b.tooltip_text = "Select $%d chip" % value
	var body := ChipVisual.body_color(value)
	var accent := ChipVisual.accent_color(value)
	var txt := Color(0.1, 0.1, 0.12) if body.get_luminance() > 0.6 else Color(0.98, 0.96, 0.9)
	b.add_theme_font_size_override("font_size", 20)
	for k in ["font_color", "font_hover_color", "font_pressed_color", "font_hover_pressed_color"]:
		b.add_theme_color_override(k, txt)
	var normal := _button_style(body, accent, 36, 6)
	var hover := _button_style(body.lightened(0.12), accent, 36, 6)
	var sel := _button_style(body.lightened(0.08), GOLD, 36, 6)
	sel.expand_margin_left = 5
	sel.expand_margin_right = 5
	sel.expand_margin_top = 5
	sel.expand_margin_bottom = 5
	sel.shadow_color = Color(1.0, 0.8, 0.3, 0.6)
	sel.shadow_size = 10
	b.add_theme_stylebox_override("normal", normal)
	b.add_theme_stylebox_override("hover", hover)
	b.add_theme_stylebox_override("pressed", sel)
	b.add_theme_stylebox_override("hover_pressed", sel)
	b.pressed.connect(_on_chip_pressed.bind(value))
	return b


func _build() -> void:
	if _built:
		return
	_built = true
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	# ---- top bar
	var top := PanelContainer.new()
	top.name = "TopBar"
	top.add_theme_stylebox_override("panel", _panel_style())
	top.set_anchors_preset(Control.PRESET_TOP_WIDE)
	top.offset_left = 14
	top.offset_right = -14
	top.offset_top = 10
	top.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(top)
	var th := HBoxContainer.new()
	th.add_theme_constant_override("separation", 22)
	th.mouse_filter = Control.MOUSE_FILTER_IGNORE
	top.add_child(th)
	bankroll_label = _stat(th, "BANKROLL", money(Bankroll.STARTING))
	bet_label = _stat(th, "TOTAL BET", money(0))
	last_label = _stat(th, "LAST ROUND", "—")
	status_label = _label("Place your bets", 34, GOLD)
	status_label.name = "Status"
	status_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	status_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	status_label.add_theme_constant_override("outline_size", 6)
	th.add_child(status_label)
	settings_button = _text_button("Settings", 18, Color(0.18, 0.06, 0.07), Vector2(120, 48))
	settings_button.name = "SettingsButton"
	settings_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	settings_button.pressed.connect(func() -> void: settings_pressed.emit())
	th.add_child(settings_button)
	how_to_play_button = _text_button("How to play", 18, Color(0.18, 0.06, 0.07), Vector2(140, 48))
	how_to_play_button.name = "HowToPlayButton"
	how_to_play_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	how_to_play_button.pressed.connect(func() -> void: how_to_play_pressed.emit())
	th.add_child(how_to_play_button)

	# ---- bottom bar
	var bottom := PanelContainer.new()
	bottom.name = "BottomBar"
	bottom.add_theme_stylebox_override("panel", _panel_style())
	bottom.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	bottom.offset_left = 14
	bottom.offset_right = -14
	bottom.offset_bottom = -10
	bottom.grow_vertical = Control.GROW_DIRECTION_BEGIN
	bottom.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(bottom)
	var bh := HBoxContainer.new()
	bh.add_theme_constant_override("separation", 16)
	bh.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bottom.add_child(bh)
	# history (left)
	var hv := VBoxContainer.new()
	hv.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hv.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	hv.add_child(_label("RECENT NUMBERS", 14, GOLD_DIM))
	history_box = HBoxContainer.new()
	history_box.name = "History"
	history_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	history_box.add_theme_constant_override("separation", 4)
	history_box.custom_minimum_size.y = 38
	hv.add_child(history_box)
	bh.add_child(hv)
	# chips (centre)
	var cv := VBoxContainer.new()
	cv.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cv.alignment = BoxContainer.ALIGNMENT_CENTER
	var cap := _label("SELECT CHIP", 14, GOLD_DIM)
	cap.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cv.add_child(cap)
	var chips := HBoxContainer.new()
	chips.name = "Chips"
	chips.mouse_filter = Control.MOUSE_FILTER_IGNORE
	chips.add_theme_constant_override("separation", 18)
	var group := ButtonGroup.new()
	for v in CHIP_VALUES:
		var b := _chip_button(v, group)
		b.name = "Chip%d" % v
		chips.add_child(b)
		chip_buttons[v] = b
	(chip_buttons[_selected_chip] as Button).set_pressed_no_signal(true)
	cv.add_child(chips)
	bh.add_child(cv)
	# actions (right)
	var right := HBoxContainer.new()
	right.mouse_filter = Control.MOUSE_FILTER_IGNORE
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.alignment = BoxContainer.ALIGNMENT_END
	right.add_theme_constant_override("separation", 14)
	clear_button = _text_button("Clear bets", 20, Color(0.22, 0.08, 0.08), Vector2(150, 64))
	clear_button.name = "ClearButton"
	clear_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	clear_button.pressed.connect(func() -> void: clear_pressed.emit())
	right.add_child(clear_button)
	spin_button = _text_button("SPIN", 30, Color(0.55, 0.38, 0.08), Vector2(190, 72))
	spin_button.name = "SpinButton"
	spin_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	spin_button.tooltip_text = "Launch the ball (Space)"
	spin_button.pressed.connect(func() -> void: spin_pressed.emit())
	right.add_child(spin_button)
	bh.add_child(right)

	# ---- tooltip
	tooltip_panel = PanelContainer.new()
	tooltip_panel.name = "Tooltip"
	var ts := _panel_style(8)
	ts.bg_color = Color(0.04, 0.03, 0.03, 0.94)
	ts.border_color = GOLD
	ts.content_margin_left = 12
	ts.content_margin_right = 12
	ts.content_margin_top = 6
	ts.content_margin_bottom = 6
	tooltip_panel.add_theme_stylebox_override("panel", ts)
	tooltip_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	tooltip_panel.visible = false
	tooltip_panel.z_index = 10
	tooltip_label = _label("", 20, TEXT)
	tooltip_panel.add_child(tooltip_label)
	add_child(tooltip_panel)


func _rebuild_history() -> void:
	for c in history_box.get_children():
		history_box.remove_child(c)
		c.free()  # immediate: removed nodes must not linger as orphans
	# Newest first, on the left.
	for i in range(_history.size() - 1, -1, -1):
		var n := _history[i]
		var p := PanelContainer.new()
		p.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var sb := StyleBoxFlat.new()
		sb.bg_color = POCKET_COLORS[WheelLayout.color_of(n)]
		sb.set_corner_radius_all(6)
		sb.border_color = GOLD if i == _history.size() - 1 else Color(0.85, 0.85, 0.8, 0.5)
		sb.set_border_width_all(2 if i == _history.size() - 1 else 1)
		p.add_theme_stylebox_override("panel", sb)
		p.custom_minimum_size = Vector2(38, 36)
		var l := _label(WheelLayout.label(n), 18, Color.WHITE)
		l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		p.add_child(l)
		p.set_meta("number", n)
		history_box.add_child(p)


func _on_chip_pressed(value: int) -> void:
	_selected_chip = value
	chip_selected.emit(value)
