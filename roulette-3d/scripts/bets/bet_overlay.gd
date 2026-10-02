class_name BetOverlay
extends Node3D
## 3D bet visuals on the felt: hover highlight, chip stacks, winning-spot marks,
## and the sweep / pay animation. Place under Table/TableLayoutMapper/SpotOverlay.
##
## Usage:
##   overlay.setup(layout, mapper)
##   overlay.hover_at(layout_pos, on_table)   # from TableInput.table_hovered
##   overlay.show_stacks(book)                # after every BetBook change
##   overlay.highlight_winning(n)             # when the ball settles
##   overlay.animate_resolution(result)       # Resolver result; await resolution_animation_finished
## While the resolution animation runs, show_stacks() calls are held and applied when it ends.

signal resolution_animation_finished

## Height above the felt for flat marks (painted table marks go up to ~0.0036 m).
const MARK_Y := 0.0040
const CHIP_Y := 0.0040
const CHIP_GAP := 0.0003
## Payout stack sits this far (layout units) beside the winning stack.
const PAYOUT_OFFSET := Vector2(0.5, 0.0)
## Dealer side is +y in layout units; player side is -y.
const DEALER_Y := 5.5
const PLAYER_Y := -4.0
## Animation timing (seconds). Total stays under 2.5 s.
const T_SWEEP := 0.6
const T_PAY_DELAY := 0.45
const T_PAY := 0.55
const T_HOLD_END := 1.55
const T_RETURN := 0.6

const HOVER_COLOR := Color(1.0, 0.85, 0.3, 0.55)
const WIN_COLOR := Color(1.0, 0.9, 0.35, 0.55)
const WIN_SOFT_COLOR := Color(1.0, 0.95, 0.6, 0.22)

var layout: BetLayout
var mapper: TableLayoutMapper

var _hover_mesh: MeshInstance3D
var _hover_spot: BetSpot
var _stacks_root: Node3D
var _marks_root: Node3D
var _anim_root: Node3D
var _stacks: Dictionary = {}          # spot_id -> Node3D stack
var _animating := false
var _pending_book: BetBook = null
var _win_tween: Tween
var _win_mat: StandardMaterial3D
var _win_soft_mat: StandardMaterial3D
var _hover_mat: StandardMaterial3D


func _init() -> void:
	_stacks_root = Node3D.new()
	_stacks_root.name = "Stacks"
	add_child(_stacks_root)
	_marks_root = Node3D.new()
	_marks_root.name = "WinMarks"
	add_child(_marks_root)
	_anim_root = Node3D.new()
	_anim_root.name = "Animating"
	add_child(_anim_root)
	_hover_mat = _flat_mat(HOVER_COLOR)
	_win_mat = _flat_mat(WIN_COLOR)
	_win_soft_mat = _flat_mat(WIN_SOFT_COLOR)
	_hover_mesh = MeshInstance3D.new()
	_hover_mesh.name = "Hover"
	_hover_mesh.mesh = PlaneMesh.new()
	_hover_mesh.material_override = _hover_mat
	_hover_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_hover_mesh.visible = false
	add_child(_hover_mesh)


func setup(p_layout: BetLayout, p_mapper: TableLayoutMapper) -> void:
	layout = p_layout
	mapper = p_mapper


# ------------------------------------------------------------ positions

## Layout point -> position in this node's local space (on the felt, plus height y).
func layout_to_overlay(p: Vector2, y: float = 0.0) -> Vector3:
	var v: Vector3
	if mapper != null and mapper.is_inside_tree() and is_inside_tree():
		v = to_local(mapper.layout_to_world(p))
	elif mapper != null:
		v = mapper.layout_to_local(p)
	else:
		v = Vector3(p.x * TableLayoutMapper.CELL_SIZE, 0.0, -p.y * TableLayoutMapper.CELL_SIZE)
	return v + Vector3(0.0, y, 0.0)


func _cell() -> float:
	return TableLayoutMapper.CELL_SIZE


func _flat_mat(c: Color) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.albedo_color = c
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	return m


func _quad_for(spot: BetSpot, mat: Material, y: float) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var pm := PlaneMesh.new()
	pm.size = spot.rect.size * _cell()
	mi.mesh = pm
	mi.material_override = mat
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.position = layout_to_overlay(spot.rect.get_center(), y)
	return mi


# ------------------------------------------------------------ hover

## Highlight the spot under layout_pos. Returns the spot, or null (highlight hidden).
func hover_at(layout_pos: Vector2, on_table: bool = true) -> BetSpot:
	var spot: BetSpot = null
	if on_table and layout != null:
		spot = layout.spot_at(layout_pos)
	_set_hover(spot)
	return spot


func clear_hover() -> void:
	_set_hover(null)


func hovered_spot() -> BetSpot:
	return _hover_spot


func hover_mesh() -> MeshInstance3D:
	return _hover_mesh


func _set_hover(spot: BetSpot) -> void:
	_hover_spot = spot
	if spot == null:
		_hover_mesh.visible = false
		return
	(_hover_mesh.mesh as PlaneMesh).size = spot.rect.size * _cell()
	_hover_mesh.position = layout_to_overlay(spot.rect.get_center(), MARK_Y + 0.0002)
	_hover_mesh.visible = true


# ------------------------------------------------------------ stacks

## Rebuild all chip stacks from the book. Held while the resolution animation runs.
func show_stacks(book: BetBook) -> void:
	if _animating:
		_pending_book = book
		return
	_clear_children(_stacks_root)
	_stacks.clear()
	if book == null or layout == null:
		return
	for id in book.spot_ids():
		var spot := layout.get_spot(id)
		if spot == null:
			continue
		var chips: Array[int] = book.chips(id)
		if chips.is_empty():
			continue
		var st := _make_stack(chips, spot.center, hash(id))
		st.name = "Stack_" + id
		st.set_meta("spot_id", id)
		_stacks_root.add_child(st)
		_stacks[id] = st


## Stack node for a spot, or null.
func stack_node(spot_id: String) -> Node3D:
	return _stacks.get(spot_id, null)


func stack_ids() -> Array:
	return _stacks.keys()


## Number of chip nodes in one stack.
func chip_count(spot_id: String) -> int:
	var st := stack_node(spot_id)
	return 0 if st == null else _chips_in(st).size()


func total_chip_count() -> int:
	var n := 0
	for id in _stacks:
		n += chip_count(id)
	return n


func _chips_in(stack: Node3D) -> Array[Node3D]:
	var out: Array[Node3D] = []
	for c in stack.get_children():
		if c.has_meta("value"):
			out.append(c)
	return out


## Stack of chips (bottom first) at layout point. Seed gives stable jitter per spot.
func _make_stack(chips: Array[int], at: Vector2, seed_value: int) -> Node3D:
	var st := Node3D.new()
	st.position = layout_to_overlay(at, CHIP_Y)
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_value
	var total := 0
	for i in chips.size():
		var chip := ChipVisual.make_chip(chips[i])
		chip.name = "Chip%d" % i
		chip.position = Vector3(rng.randf_range(-0.0007, 0.0007), i * (ChipVisual.HEIGHT + CHIP_GAP),
				rng.randf_range(-0.0007, 0.0007))
		chip.rotation.y = rng.randf_range(0.0, TAU)
		st.add_child(chip)
		total += chips[i]
	if chips.size() > 1:
		var lbl := Label3D.new()
		lbl.name = "Total"
		lbl.text = "$%d" % total
		lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		lbl.no_depth_test = true
		lbl.fixed_size = false
		lbl.pixel_size = 0.0004
		lbl.font_size = 48
		lbl.outline_size = 14
		lbl.modulate = Color(1.0, 0.93, 0.6)
		lbl.outline_modulate = Color(0, 0, 0, 0.9)
		lbl.render_priority = 2
		lbl.position = Vector3(0, chips.size() * (ChipVisual.HEIGHT + CHIP_GAP) + 0.008, 0)
		st.add_child(lbl)
	return st


func _clear_children(n: Node) -> void:
	for c in n.get_children():
		n.remove_child(c)
		c.free()  # immediate: removed nodes must not linger as orphans


# ------------------------------------------------------------ winning marks

## Mark every spot that contains number (straight cell strongest). Pulses until cleared.
func highlight_winning(number: int, p_layout: BetLayout = null) -> void:
	clear_winning()
	var lay := p_layout if p_layout != null else layout
	if lay == null:
		return
	for s in lay.spots():
		if not s.numbers.has(number):
			continue
		var strong := s.type == Payouts.BetType.STRAIGHT
		var q := _quad_for(s, _win_mat if strong else _win_soft_mat, MARK_Y + (0.0001 if strong else 0.00005))
		q.name = "Win_" + s.id
		q.set_meta("spot_id", s.id)
		_marks_root.add_child(q)
	if is_inside_tree():
		_win_tween = create_tween().set_loops()
		_win_tween.tween_method(_set_win_alpha, 0.25, 1.0, 0.45).set_trans(Tween.TRANS_SINE)
		_win_tween.tween_method(_set_win_alpha, 1.0, 0.25, 0.45).set_trans(Tween.TRANS_SINE)


func clear_winning() -> void:
	if _win_tween != null:
		_win_tween.kill()
		_win_tween = null
	_set_win_alpha(1.0)
	_clear_children(_marks_root)


## Spot ids currently marked as winning.
func winning_ids() -> Array:
	var out: Array = []
	for q in _marks_root.get_children():
		out.append(q.get_meta("spot_id"))
	return out


func _set_win_alpha(k: float) -> void:
	_win_mat.albedo_color.a = WIN_COLOR.a * k
	_win_soft_mat.albedo_color.a = WIN_SOFT_COLOR.a * k


# ------------------------------------------------------------ resolution animation

func is_animating() -> bool:
	return _animating


## Sweep losers toward the dealer, pay winners with a matched stack beside them,
## then slide winners + payout back to the player. Emits resolution_animation_finished.
## Uses the stacks from the last show_stacks(); afterwards no stacks remain
## (unless a show_stacks() call arrived during the animation, which is then applied).
## The signal is never emitted inside this call, so `animate_resolution(r)` then
## `await resolution_animation_finished` is always safe. Every path ends with one emit:
## - normal: when the tween ends (~2.2 s);
## - empty result or not in the tree: on a later frame (deferred);
## - called again while running: no new animation starts; the running one's single
##   emit wakes every awaiter, so a second caller does not hang either.
func animate_resolution(result: Dictionary) -> void:
	if _animating:
		return
	_animating = true
	# Move current stacks into the animation layer.
	var by_id: Dictionary = {}
	for id in _stacks:
		var st: Node3D = _stacks[id]
		_stacks_root.remove_child(st)
		_anim_root.add_child(st)
		by_id[id] = st
	_stacks.clear()
	if not is_inside_tree():
		_finish_animation.call_deferred()
		return
	var tw := create_tween().set_parallel(true)
	tw.tween_interval(0.1)
	for l in result.get("losers", []):
		var st: Node3D = by_id.get(l.get("id", ""), null)
		if st == null:
			continue
		var target := st.position + layout_to_overlay(Vector2(0, DEALER_Y)) - layout_to_overlay(Vector2.ZERO)
		tw.tween_property(st, "position", target, T_SWEEP).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
		tw.tween_property(st, "scale", Vector3.ONE * 0.01, T_SWEEP * 0.5).set_delay(T_SWEEP * 0.5)
	for w in result.get("winners", []):
		var id: String = w.get("id", "")
		var spot := layout.get_spot(id) if layout != null else null
		if spot == null:
			continue
		var st: Node3D = by_id.get(id, null)
		if st == null:
			st = _make_stack(ChipBreakdown.break_into_chips(int(w.get("amount", 0))), spot.center, hash(id))
			_anim_root.add_child(st)
		var winnings := int(w.get("winnings", 0))
		var pay_at := spot.center + PAYOUT_OFFSET
		var pay := _make_stack(ChipBreakdown.break_into_chips(winnings), pay_at, hash(id) + 7)
		pay.name = "Payout_" + id
		var rest := pay.position
		pay.position = rest + layout_to_overlay(Vector2(0, DEALER_Y)) - layout_to_overlay(Vector2.ZERO)
		pay.scale = Vector3.ONE * 0.01
		_anim_root.add_child(pay)
		tw.tween_property(pay, "scale", Vector3.ONE, T_PAY * 0.4).set_delay(T_PAY_DELAY)
		tw.tween_property(pay, "position", rest, T_PAY).set_delay(T_PAY_DELAY) \
				.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
		var back := layout_to_overlay(Vector2(0, PLAYER_Y)) - layout_to_overlay(Vector2.ZERO)
		for n in [st, pay]:
			var node: Node3D = n
			var from_pos: Vector3 = rest if node == pay else node.position
			tw.tween_property(node, "position", from_pos + back, T_RETURN).set_delay(T_HOLD_END) \
					.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN)
			tw.tween_property(node, "scale", Vector3.ONE * 0.01, T_RETURN * 0.5).set_delay(T_HOLD_END + T_RETURN * 0.5)
	tw.finished.connect(_finish_animation)


func _finish_animation() -> void:
	_clear_children(_anim_root)
	_animating = false
	var pending := _pending_book
	_pending_book = null
	if pending != null:
		show_stacks(pending)
	resolution_animation_finished.emit()
