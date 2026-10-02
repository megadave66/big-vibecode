extends SceneTree
## Usage: godot --path <root> -s res://tools/screenshot.gd -- --scene=res://scenes/main.tscn --out=<png> --frames=30 [--view=table|wheel]

func _initialize() -> void:
	var args: Dictionary = {}
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--") and a.contains("="):
			var kv: PackedStringArray = a.substr(2).split("=", true, 1)
			args[kv[0]] = kv[1]
	var scene_path: String = args.get("scene", "res://scenes/main.tscn")
	var out: String = args.get("out", "user://screenshot.png")
	var frames: int = int(args.get("frames", "30"))
	var packed: PackedScene = load(scene_path)
	if packed == null:
		push_error("screenshot: cannot load " + scene_path)
		quit(1)
		return
	var inst: Node = packed.instantiate()
	root.add_child(inst)
	await process_frame
	if args.has("view"):
		var rig: Node = inst.find_child("CameraRig", true, false)
		if rig and rig.has_method("set_view"):
			rig.call("set_view", args["view"], true)
		else:
			push_warning("screenshot: no CameraRig with set_view found")
	_capture(out, frames)


func _capture(out: String, frames: int) -> void:
	for i in frames:
		await process_frame
	var img: Image = root.get_texture().get_image()
	var err: int = img.save_png(out)
	if err != OK:
		push_error("screenshot: save failed %d" % err)
	print("screenshot saved: ", out, " ", img.get_size())
	quit(0 if err == OK else 1)
