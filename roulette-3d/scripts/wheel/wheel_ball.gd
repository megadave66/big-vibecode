class_name WheelBall
extends RigidBody3D
## The roulette ball: a plain RigidBody3D. All motion comes from the physics engine.
## This script only reports contacts (for audio) and never steers the ball.

## Emitted when the ball starts touching a collider. kind = "fret", "deflector",
## "rim" or "pocket"; strength 0..1 from the change in velocity at the contact.
signal impacted(kind: String, strength: float)

## Velocity change (m/s) that maps to strength 1.0.
@export var full_strength_dv: float = 1.5
## Contacts weaker than this (m/s of velocity change) are ignored.
@export var min_impact_dv: float = 0.04

var _prev_velocity: Vector3 = Vector3.ZERO
var _pending: Array[String] = []


func _ready() -> void:
	contact_monitor = true
	max_contacts_reported = 8
	body_shape_entered.connect(_on_body_shape_entered)


func _physics_process(delta: float) -> void:
	if _pending.is_empty():
		_prev_velocity = linear_velocity
		return
	# A new contact began during the last step: strength = how much the velocity jumped
	# beyond what gravity alone would do.
	var g: Vector3 = ProjectSettings.get_setting("physics/3d/default_gravity_vector", Vector3.DOWN) \
			* float(ProjectSettings.get_setting("physics/3d/default_gravity", 9.8))
	var dv := (linear_velocity - _prev_velocity - g * delta).length()
	for kind in _pending:
		if dv >= min_impact_dv:
			impacted.emit(kind, clampf(dv / full_strength_dv, 0.0, 1.0))
	_pending.clear()
	_prev_velocity = linear_velocity


func _on_body_shape_entered(_rid: RID, body: Node, body_shape_index: int, _local_shape_index: int) -> void:
	if freeze or body == null:
		return
	var kind := "rim"
	if body is CollisionObject3D:
		var owner_id: int = (body as CollisionObject3D).shape_find_owner(body_shape_index)
		var owner_node: Object = (body as CollisionObject3D).shape_owner_get_owner(owner_id)
		if owner_node is Node and (owner_node as Node).has_meta("kind"):
			kind = str((owner_node as Node).get_meta("kind"))
		elif body.has_meta("kind"):
			kind = str(body.get_meta("kind"))
	if not _pending.has(kind):
		_pending.append(kind)
