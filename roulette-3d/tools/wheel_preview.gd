extends SceneTree
## Visual check of the roulette wheel. Self-quitting, needs a window (not headless).
## Usage (from the project root):
##   godot --path . --fixed-fps 60 -s res://tools/wheel_preview.gd -- --out=<folder> [--seed=7]
## Saves PNGs of the wheel mid-spin, at the drop, at rest with the winning pocket
## highlighted, a top view and a close-up. Then quits.

const WHEEL_SCENE := preload("res://scenes/wheel/wheel.tscn")

var _out := "user://"
var _seed := 7
var _wheel: RouletteWheel
var _cam: Camera3D
var _frame := 0
var _settled_frame := -1
var _number := -1
var _shots := 0


func _initialize() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--out="):
			_out = a.get_slice("=", 1)
		elif a.begins_with("--seed="):
			_seed = int(a.get_slice("=", 1))
	DirAccess.make_dir_recursive_absolute(_out)
	_build.call_deferred()


func _build() -> void:
	root.size = Vector2i(1280, 800)
	var env := WorldEnvironment.new()
	var e := Environment.new()
	var sky := Sky.new()
	var sm := ProceduralSkyMaterial.new()
	sm.sky_top_color = Color(0.12, 0.1, 0.09)
	sm.sky_horizon_color = Color(0.35, 0.28, 0.2)
	sm.ground_bottom_color = Color(0.05, 0.05, 0.05)
	sm.ground_horizon_color = Color(0.3, 0.25, 0.2)
	sky.sky_material = sm
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0.04, 0.05, 0.05)
	e.sky = sky
	e.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	e.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	e.ambient_light_energy = 0.6
	e.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	e.glow_enabled = true
	env.environment = e
	root.add_child(env)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-60, 30, 0)
	sun.light_energy = 1.3
	sun.shadow_enabled = true
	root.add_child(sun)
	var spot := OmniLight3D.new()
	spot.position = Vector3(0.3, 0.9, 0.4)
	spot.omni_range = 3.0
	spot.light_energy = 1.2
	root.add_child(spot)
	# Felt under the wheel.
	var felt := MeshInstance3D.new()
	var pm := PlaneMesh.new()
	pm.size = Vector2(3, 3)
	felt.mesh = pm
	var fm := StandardMaterial3D.new()
	fm.albedo_color = Color(0.03, 0.25, 0.12)
	fm.roughness = 0.95
	felt.material_override = fm
	root.add_child(felt)
	_wheel = WHEEL_SCENE.instantiate()
	root.add_child(_wheel)
	_wheel.ball_settled.connect(_on_settled)
	_cam = Camera3D.new()
	_cam.fov = 40
	root.add_child(_cam)
	_view_oblique()
	var rng := RandomNumberGenerator.new()
	rng.seed = _seed
	_wheel.launch_ball(rng)


func _view_oblique() -> void:
	_cam.look_at_from_position(Vector3(0.0, 0.85, 0.95), Vector3(0, 0.05, 0.02))


func _view_top() -> void:
	_cam.look_at_from_position(Vector3(0.0, 1.45, 0.001), Vector3(0, 0, 0), Vector3(0, 0, -1))


func _view_close(target: Vector3) -> void:
	var dir := Vector3(target.x, 0, target.z).normalized()
	_cam.look_at_from_position(target + dir * 0.28 + Vector3(0, 0.22, 0), target)


func _on_settled(n: int) -> void:
	_number = n
	_settled_frame = _frame
	_wheel.highlight_pocket(n)
	print("settled on ", WheelLayout.label(n), " after ", _wheel.last_spin_time, " s")


func _shot(name: String) -> void:
	var img := root.get_texture().get_image()
	var path := _out.path_join(name)
	img.save_png(path)
	print("saved ", path)
	_shots += 1


func _process(_delta: float) -> bool:
	_frame += 1
	# --fixed-fps 60: one frame = 1/60 s of simulated time.
	match _frame:
		120:
			_shot("wheel_spin_oblique.png")
		125:
			_view_top()
		128:
			_shot("wheel_spin_top.png")
			_view_oblique()
	if _settled_frame > 0:
		var f := _frame - _settled_frame
		if f == 20:
			_shot("wheel_rest_oblique.png")
			_view_top()
		elif f == 24:
			_shot("wheel_rest_top.png")
			_view_close(_wheel.get_ball().global_position)
		elif f == 28:
			_shot("wheel_rest_closeup.png")
			return true
	if _frame > 60 * 70:
		print("no settle within 70 s; quitting")
		return true
	return false
