class_name ChipVisual
extends RefCounted
## Builds 3D casino chips from primitives. Meshes and materials are shared (built once).
## Sizes in metres. Table cells are 0.08 m (TableLayoutMapper.CELL_SIZE).

const RADIUS := 0.021
const HEIGHT := 0.0042
const STRIPES := 6

## Body colour, stripe/inlay colour per denomination.
const COLORS := {
	1: [Color(0.93, 0.92, 0.88), Color(0.15, 0.35, 0.75)],
	5: [Color(0.78, 0.08, 0.09), Color(0.97, 0.95, 0.9)],
	25: [Color(0.07, 0.48, 0.2), Color(0.97, 0.95, 0.9)],
	100: [Color(0.06, 0.06, 0.07), Color(0.92, 0.75, 0.3)],
}

static var _body_mesh: CylinderMesh
static var _inlay_mesh: CylinderMesh
static var _stripe_mesh: BoxMesh
static var _mats: Dictionary = {}


static func body_color(value: int) -> Color:
	return COLORS.get(value, COLORS[1])[0]


static func accent_color(value: int) -> Color:
	return COLORS.get(value, COLORS[1])[1]


static func _mat(c: Color, rough: float = 0.55) -> StandardMaterial3D:
	var key := c.to_html()
	if not _mats.has(key):
		var m := StandardMaterial3D.new()
		m.albedo_color = c
		m.roughness = rough
		m.metallic_specular = 0.4
		_mats[key] = m
	return _mats[key]


static func _ensure_meshes() -> void:
	if _body_mesh != null:
		return
	_body_mesh = CylinderMesh.new()
	_body_mesh.top_radius = RADIUS
	_body_mesh.bottom_radius = RADIUS
	_body_mesh.height = HEIGHT
	_body_mesh.radial_segments = 28
	_body_mesh.rings = 1
	_inlay_mesh = CylinderMesh.new()
	_inlay_mesh.top_radius = RADIUS * 0.62
	_inlay_mesh.bottom_radius = RADIUS * 0.62
	_inlay_mesh.height = HEIGHT + 0.0004
	_inlay_mesh.radial_segments = 24
	_inlay_mesh.rings = 1
	_stripe_mesh = BoxMesh.new()
	_stripe_mesh.size = Vector3(RADIUS * 0.42, HEIGHT + 0.0002, RADIUS * 0.22)


## One chip. Origin at the chip's bottom face centre.
static func make_chip(value: int) -> Node3D:
	_ensure_meshes()
	var root := Node3D.new()
	root.name = "Chip"
	root.set_meta("value", value)
	var body := MeshInstance3D.new()
	body.mesh = _body_mesh
	body.material_override = _mat(body_color(value))
	body.position.y = HEIGHT * 0.5
	root.add_child(body)
	var inlay := MeshInstance3D.new()
	inlay.mesh = _inlay_mesh
	inlay.material_override = _mat(accent_color(value).lerp(body_color(value), 0.35), 0.4)
	inlay.position.y = HEIGHT * 0.5
	inlay.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(inlay)
	var stripe_mat := _mat(accent_color(value), 0.45)
	for i in STRIPES:
		var a := TAU * float(i) / STRIPES
		var s := MeshInstance3D.new()
		s.mesh = _stripe_mesh
		s.material_override = stripe_mat
		s.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		var r := RADIUS - _stripe_mesh.size.z * 0.5 + 0.0003
		s.position = Vector3(sin(a) * r, HEIGHT * 0.5, cos(a) * r)
		s.rotation.y = a
		root.add_child(s)
	return root
