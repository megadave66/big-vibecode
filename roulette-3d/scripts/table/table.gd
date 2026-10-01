class_name RouletteTable
extends Node3D
## Procedural casino table: felt top, padded rail, wooden skirt, legs, and the painted bet layout.
## 1 unit = 1 metre. The felt surface is at y = TABLE_HEIGHT (world, when this node is at the origin).

const TABLE_HEIGHT: float = 0.76
const FELT_SIZE: Vector2 = Vector2(2.3, 1.0)       # x (long), z (deep)
const FELT_CENTER: Vector2 = Vector2(0.0, -0.04)   # x, z
const RAIL_RADIUS: float = 0.05

const FILL_Y: float = 0.0012
const LINE_Y: float = 0.0024
const LINE_W: float = 0.0035   # metres
const MARK_Y: float = 0.003

const COL_FELT := Color(0.04, 0.30, 0.15)
const COL_RED := Color(0.72, 0.06, 0.07)
const COL_BLACK := Color(0.05, 0.05, 0.06)
const COL_ZERO := Color(0.05, 0.42, 0.20)
const COL_LINE := Color(0.92, 0.92, 0.88)

@onready var mapper: TableLayoutMapper = $TableLayoutMapper


func _ready() -> void:
	mapper.position = Vector3(0.0, TABLE_HEIGHT, 0.0)
	_build_structure()
	_build_layout()


# ---------------------------------------------------------------- structure

func _mat(c: Color, rough: float = 0.8, metal: float = 0.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = c
	m.roughness = rough
	m.metallic = metal
	return m


func _box(parent: Node3D, size: Vector3, pos: Vector3, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var b := BoxMesh.new()
	b.size = size
	mi.mesh = b
	mi.material_override = mat
	mi.position = pos
	parent.add_child(mi)
	return mi


func _build_structure() -> void:
	var root := Node3D.new()
	root.name = "Structure"
	add_child(root)
	var felt_mat := _mat(COL_FELT, 1.0)
	var wood := _mat(Color(0.30, 0.15, 0.07), 0.45)
	var leather := _mat(Color(0.35, 0.05, 0.06), 0.55)
	var cx: float = FELT_CENTER.x
	var cz: float = FELT_CENTER.y
	var hx: float = FELT_SIZE.x * 0.5
	var hz: float = FELT_SIZE.y * 0.5
	# Felt slab (top face at TABLE_HEIGHT).
	_box(root, Vector3(FELT_SIZE.x, 0.04, FELT_SIZE.y), Vector3(cx, TABLE_HEIGHT - 0.02, cz), felt_mat).name = "Felt"
	# Wooden skirt under the felt, wider than the felt so the rail sits on it.
	var sk_w: float = FELT_SIZE.x + 2.0 * RAIL_RADIUS * 2.0
	var sk_d: float = FELT_SIZE.y + 2.0 * RAIL_RADIUS * 2.0
	_box(root, Vector3(sk_w, 0.14, sk_d), Vector3(cx, TABLE_HEIGHT - 0.11, cz), wood).name = "Skirt"
	# Padded rail: capsules lying along each edge.
	var rail_y: float = TABLE_HEIGHT - 0.04 + RAIL_RADIUS * 0.6
	var long_len: float = sk_w
	var short_len: float = sk_d
	for sz in [-1.0, 1.0]:
		var c := _capsule(root, RAIL_RADIUS, long_len, leather)
		c.rotation_degrees = Vector3(0, 0, 90)
		c.position = Vector3(cx, rail_y, cz + sz * (hz + RAIL_RADIUS))
		c.name = "RailLong"
	for sx in [-1.0, 1.0]:
		var c2 := _capsule(root, RAIL_RADIUS, short_len, leather)
		c2.rotation_degrees = Vector3(90, 0, 0)
		c2.position = Vector3(cx + sx * (hx + RAIL_RADIUS), rail_y, cz)
		c2.name = "RailShort"
	# Wooden lip under the rail.
	_box(root, Vector3(sk_w, 0.05, 0.02), Vector3(cx, TABLE_HEIGHT - 0.13, cz + sk_d * 0.5), wood)
	# Legs.
	for sx in [-1.0, 1.0]:
		for sz in [-1.0, 1.0]:
			_box(root, Vector3(0.09, TABLE_HEIGHT - 0.18, 0.09),
				Vector3(cx + sx * (hx - 0.05), (TABLE_HEIGHT - 0.18) * 0.5, cz + sz * (hz - 0.05)), wood).name = "Leg"


func _capsule(parent: Node3D, radius: float, length: float, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var cap := CapsuleMesh.new()
	cap.radius = radius
	cap.height = length
	cap.radial_segments = 24
	cap.rings = 6
	mi.mesh = cap
	mi.material_override = mat
	parent.add_child(mi)
	return mi


# ---------------------------------------------------------------- painted layout

func _p(lp: Vector2, y: float) -> Vector3:
	return mapper.layout_to_local(lp) + Vector3(0, y, 0)


func _quad(st: SurfaceTool, a: Vector2, b: Vector2, c: Vector2, d: Vector2, y: float, col: Color) -> void:
	# a,b,c,d in layout units, counter-clockwise in layout space. Face up in world.
	st.set_color(col)
	st.set_normal(Vector3.UP)
	for lp in [a, c, b, a, d, c]:
		st.add_vertex(_p(lp, y))


func _rect(st: SurfaceTool, r: Rect2, y: float, col: Color) -> void:
	var p0: Vector2 = r.position
	var p1: Vector2 = r.end
	_quad(st, p0, Vector2(p1.x, p0.y), p1, Vector2(p0.x, p1.y), y, col)


func _line(st: SurfaceTool, a: Vector2, b: Vector2, col: Color) -> void:
	# Thin axis-aligned line; width LINE_W metres.
	var hw: float = LINE_W * 0.5 / TableLayoutMapper.CELL_SIZE
	var d: Vector2 = b - a
	if absf(d.x) >= absf(d.y):
		_rect(st, Rect2(minf(a.x, b.x) - hw, a.y - hw, absf(d.x) + 2.0 * hw, 2.0 * hw), LINE_Y, col)
	else:
		_rect(st, Rect2(a.x - hw, minf(a.y, b.y) - hw, 2.0 * hw, absf(d.y) + 2.0 * hw), LINE_Y, col)


func _box_outline(st: SurfaceTool, r: Rect2) -> void:
	var p0: Vector2 = r.position
	var p1: Vector2 = r.end
	_line(st, p0, Vector2(p1.x, p0.y), COL_LINE)
	_line(st, Vector2(p1.x, p0.y), p1, COL_LINE)
	_line(st, p1, Vector2(p0.x, p1.y), COL_LINE)
	_line(st, Vector2(p0.x, p1.y), p0, COL_LINE)


func _diamond(st: SurfaceTool, center: Vector2, col: Color) -> void:
	var w: float = 0.62
	var h: float = 0.36
	var n := center + Vector2(0, h)
	var e := center + Vector2(w, 0)
	var s := center + Vector2(0, -h)
	var wv := center + Vector2(-w, 0)
	_quad(st, wv, s, e, n, MARK_Y, col)


func _build_layout() -> void:
	var fills := SurfaceTool.new()
	fills.begin(Mesh.PRIMITIVE_TRIANGLES)
	var lines := SurfaceTool.new()
	lines.begin(Mesh.PRIMITIVE_TRIANGLES)
	var marks := SurfaceTool.new()
	marks.begin(Mesh.PRIMITIVE_TRIANGLES)

	var reds: Array[int] = WheelLayout.RED_NUMBERS
	# Number cells.
	for n in range(1, 37):
		var c: int = (n - 1) / 3
		var r: int = (n - 1) % 3
		var col: Color = COL_RED if reds.has(n) else COL_BLACK
		var cell := Rect2(c + 0.08, r + 0.08, 0.84, 0.84)
		_rect(fills, cell, FILL_Y, col)
		_add_label(str(n), Vector2(c + 0.5, r + 0.5), 56, Color.WHITE, 0.0)
	# Zero cells.
	_rect(fills, Rect2(-0.92, 0.08, 0.84, 1.34), FILL_Y, COL_ZERO)
	_rect(fills, Rect2(-0.92, 1.58, 0.84, 1.34), FILL_Y, COL_ZERO)
	_add_label("0", Vector2(-0.5, 0.75), 64, Color.WHITE, 0.0)
	_add_label("00", Vector2(-0.5, 2.25), 64, Color.WHITE, 0.0)
	# Colour marks for even-money strip.
	_diamond(marks, Vector2(5, -1.5), COL_RED)
	_diamond(marks, Vector2(7, -1.5), COL_BLACK)

	# Grid lines.
	for c in range(0, 13):
		_line(lines, Vector2(c, 0), Vector2(c, 3), COL_LINE)
	for r in range(0, 4):
		_line(lines, Vector2(0, r), Vector2(12, r), COL_LINE)
	# Zero block outline and divider.
	_box_outline(lines, Rect2(-1, 0, 1, 3))
	_line(lines, Vector2(-1, 1.5), Vector2(0, 1.5), COL_LINE)
	# Column boxes.
	for r in range(3):
		_box_outline(lines, Rect2(12, r, 1, 1))
		_add_label("2 to 1", Vector2(12.5, r + 0.5), 46, Color.WHITE, 90.0)
	# Dozens.
	var dz := ["1st 12", "2nd 12", "3rd 12"]
	for i in range(3):
		var rct := Rect2(i * 4, -1, 4, 1)
		_box_outline(lines, rct)
		_add_label(dz[i], rct.get_center(), 52, Color.WHITE, 0.0)
	# Even-money strip.
	var outs := ["1-18", "EVEN", "", "", "ODD", "19-36"]
	for i in range(6):
		var rct2 := Rect2(i * 2, -2, 2, 1)
		_box_outline(lines, rct2)
		if outs[i] != "":
			_add_label(outs[i], rct2.get_center(), 52, Color.WHITE, 0.0)

	var fill_mat := _vc_material()
	_add_mesh(fills, fill_mat, "LayoutFills")
	_add_mesh(lines, _vc_material(), "LayoutLines")
	_add_mesh(marks, _vc_material(), "LayoutMarks")


func _vc_material() -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.vertex_color_use_as_albedo = true
	m.vertex_color_is_srgb = true
	m.roughness = 0.95
	m.cull_mode = BaseMaterial3D.CULL_DISABLED
	return m


func _add_mesh(st: SurfaceTool, mat: Material, node_name: String) -> void:
	var mi := MeshInstance3D.new()
	mi.name = node_name
	mi.mesh = st.commit()
	mi.material_override = mat
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mapper.add_child(mi)


func _add_label(text: String, at: Vector2, font_size: int, col: Color, rot_deg: float) -> void:
	var l := Label3D.new()
	l.text = text
	l.font_size = font_size
	l.pixel_size = 0.0005
	l.modulate = col
	l.outline_size = 0
	l.double_sided = false
	l.shaded = false
	l.alpha_cut = Label3D.ALPHA_CUT_OPAQUE_PREPASS
	l.rotation_degrees = Vector3(-90.0, 0.0, 0.0)
	l.position = _p(at, MARK_Y + 0.0006)
	if rot_deg != 0.0:
		l.rotate(Vector3.UP, deg_to_rad(rot_deg))
	l.name = "Label_" + text.replace(" ", "_")
	mapper.add_child(l)
