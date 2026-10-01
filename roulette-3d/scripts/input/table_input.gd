class_name TableInput
extends Node
## Turns mouse input into layout-space positions using camera ray vs felt plane (pure math).
## Uses _unhandled_input, so clicks eaten by GUI controls never arrive here.

signal table_clicked(layout_pos: Vector2, button_index: int)
signal table_hovered(layout_pos: Vector2, on_table: bool)

@export var mapper_path: NodePath

var _mapper: TableLayoutMapper


func _ready() -> void:
	if not mapper_path.is_empty():
		_mapper = get_node_or_null(mapper_path) as TableLayoutMapper


func set_mapper(m: TableLayoutMapper) -> void:
	_mapper = m


## Ray / plane intersection. Returns Vector3 hit point, or null if parallel or behind the origin.
static func ray_plane_hit(origin: Vector3, dir: Vector3, plane: Plane) -> Variant:
	var hit: Variant = plane.intersects_ray(origin, dir)
	return hit


## Screen position (viewport pixels) to layout position. Returns Vector2, or null when the ray misses the felt plane.
func screen_to_layout(screen_pos: Vector2) -> Variant:
	var cam: Camera3D = get_viewport().get_camera_3d()
	if cam == null or _mapper == null:
		return null
	var origin: Vector3 = cam.project_ray_origin(screen_pos)
	var dir: Vector3 = cam.project_ray_normal(screen_pos)
	var hit: Variant = ray_plane_hit(origin, dir, _mapper.felt_plane())
	if hit == null:
		return null
	return _mapper.world_to_layout(hit)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion:
		var p: Variant = screen_to_layout((event as InputEventMouseMotion).position)
		if p == null:
			table_hovered.emit(Vector2.ZERO, false)
		else:
			table_hovered.emit(p, TableLayoutMapper.LAYOUT_BOUNDS.has_point(p))
	elif event is InputEventMouseButton:
		var mb: InputEventMouseButton = event as InputEventMouseButton
		if not mb.pressed:
			return
		var p2: Variant = screen_to_layout(mb.position)
		if p2 != null:
			table_clicked.emit(p2, mb.button_index)
