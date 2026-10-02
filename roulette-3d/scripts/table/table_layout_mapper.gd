class_name TableLayoutMapper
extends Node3D
## Maps 2D bet-layout units (1 unit = one number cell) to 3D points on the felt.
## Layout x runs along the table (to the player's right). Layout y runs away from
## the dealer toward the table centre, so y < 0 (dozens, outside bets) is nearest
## the player (+Z in world space) and y > 0 is farther away (-Z).
## The node's own transform is the layout origin (corner of the 1 cell, at felt height).

const CELL_SIZE: float = 0.08
## Whole painted area in layout units: 0/00 column to the "2 to 1" column, outside bets to row 3.
const LAYOUT_BOUNDS: Rect2 = Rect2(-1.0, -2.0, 14.0, 5.0)


func layout_to_local(p: Vector2) -> Vector3:
	return Vector3(p.x * CELL_SIZE, 0.0, -p.y * CELL_SIZE)


func local_to_layout(v: Vector3) -> Vector2:
	return Vector2(v.x / CELL_SIZE, -v.z / CELL_SIZE)


func layout_to_world(p: Vector2) -> Vector3:
	var local: Vector3 = layout_to_local(p)
	return to_global(local) if is_inside_tree() else transform * local


func world_to_layout(w: Vector3) -> Vector2:
	var local: Vector3 = to_local(w) if is_inside_tree() else transform.affine_inverse() * w
	return local_to_layout(local)


## The felt plane in world space (normal up through the layout origin).
func felt_plane() -> Plane:
	var t: Transform3D = global_transform if is_inside_tree() else transform
	return Plane(t.basis.y.normalized(), t.origin)
