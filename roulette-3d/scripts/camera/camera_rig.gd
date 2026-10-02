class_name CameraRig
extends Node3D
## Camera rig with named views and smooth blending. Child Camera3D sits at the rig origin.

signal view_changed(view_name: String)

const BLEND_TIME: float = 0.7
## view name -> [eye position, look-at target]  (world space, metres)
const VIEWS: Dictionary = {
	"table": [Vector3(0.42, 1.56, 0.67), Vector3(0.42, 0.76, -0.12)],
	"wheel": [Vector3(-0.58, 1.5, 0.8), Vector3(-0.58, 0.78, -0.04)],
}
const ORDER: Array[String] = ["table", "wheel"]

@export var start_view: String = "table"

var current_view: String = ""
var _tween: Tween


func _ready() -> void:
	set_view(start_view, true)


func view_names() -> Array[String]:
	return ORDER.duplicate()


func view_transform(view_name: String) -> Transform3D:
	var v: Array = VIEWS[view_name]
	return Transform3D(Basis.IDENTITY, v[0]).looking_at(v[1], Vector3.UP)


func set_view(view_name: String, instant: bool = false) -> void:
	if not VIEWS.has(view_name):
		push_warning("CameraRig: unknown view '%s'" % view_name)
		return
	current_view = view_name
	var target: Transform3D = view_transform(view_name)
	if _tween:
		_tween.kill()
		_tween = null
	if instant or not is_inside_tree():
		transform = target
	else:
		var from: Transform3D = transform
		_tween = create_tween()
		_tween.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
		_tween.tween_method(func(t: float) -> void: transform = from.interpolate_with(target, t), 0.0, 1.0, BLEND_TIME)
	view_changed.emit(view_name)


func toggle() -> void:
	var i: int = ORDER.find(current_view)
	set_view(ORDER[(i + 1) % ORDER.size()])


func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("toggle_view"):
		toggle()
