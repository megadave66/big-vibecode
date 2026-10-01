extends SceneTree
## Visual check for section 3 part B. Loads main.tscn, adds BetOverlay + HUD with sample chips, saves a PNG, quits.
## Usage: godot --path <root> -s res://tools/bet_ui_preview.gd -- [--out=<png>] [--mode=stacks|win|anim] [--frames=20]

const SHOTS := "/tmp/roulette/shots/"


func _initialize() -> void:
	var args: Dictionary = {}
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--") and a.contains("="):
			var kv: PackedStringArray = a.substr(2).split("=", true, 1)
			args[kv[0]] = kv[1]
	var mode: String = args.get("mode", "stacks")
	var out: String = args.get("out", SHOTS + "bet_ui%s.png" % ("" if mode == "stacks" else "_" + mode))
	var frames: int = int(args.get("frames", "20"))

	var main: Node = (load("res://scenes/main.tscn") as PackedScene).instantiate()
	root.add_child(main)
	await process_frame

	var mapper: TableLayoutMapper = main.find_child("TableLayoutMapper", true, false)
	var spot_overlay: Node3D = mapper.get_node("SpotOverlay")
	var layout := BetLayout.new()
	# main.gd already adds a BetOverlay and a Hud. Reuse them: a second copy would draw on top
	# and show the first one's text through it ("ghost" labels).
	var overlay: BetOverlay = main.get("overlay") as BetOverlay
	if overlay == null:
		overlay = BetOverlay.new()
		spot_overlay.add_child(overlay)
	overlay.setup(layout, mapper)

	var hud: Hud = main.get("hud") as Hud
	if hud == null:
		hud = (load("res://scenes/ui/hud.tscn") as PackedScene).instantiate()
		(main.get_node("UI") as CanvasLayer).add_child(hud)

	var book := BetBook.new()
	for c in [25, 5, 5, 1]:
		book.place("straight_17", c)
	book.place("split_0_37", 5)
	for c in [100, 25, 25]:
		book.place("corner_1_2_4_5", c)
	book.place("five_number", 1)
	book.place("five_number", 1)
	for c in [100, 100, 25, 5]:
		book.place("red", c)
	book.place("street_31", 25)
	overlay.show_stacks(book)

	hud.set_bankroll(1000 - book.total())
	hud.set_total_bet(book.total())
	hud.set_last_result(-35)
	for n in [17, 0, 32, 37, 5, 26, 11, 3]:
		hud.push_history(n)
	hud.select_chip(25)

	match mode:
		"win":
			hud.set_phase(RoundFlow.Phase.SETTLED)
			hud.announce_number(17)
			overlay.highlight_winning(17)
		"anim":
			var result := Resolver.resolve(_wagers(book, layout), 17)
			overlay.highlight_winning(17)
			overlay.animate_resolution(result)
			frames = int(args.get("frames", "75"))
		_:
			hud.set_phase(RoundFlow.Phase.BETTING)
			var hover := overlay.hover_at(Vector2(4.0, 2.0), true)  # corner 11-12-14-15
			var cam := root.get_viewport().get_camera_3d()
			var sp := cam.unproject_position(mapper.layout_to_world(hover.center))
			hud.show_spot_tooltip(hover, sp)

	for i in frames:
		await process_frame
	var img: Image = root.get_texture().get_image()
	var err := img.save_png(out)
	print("bet_ui_preview saved: ", out, " ", img.get_size(), " err=", err)
	quit(0 if err == OK else 1)


func _wagers(book: BetBook, layout: BetLayout) -> Array:
	var out: Array = []
	var bets := book.bets()
	for id in bets:
		var s := layout.get_spot(id)
		out.append({"id": id, "type": s.type, "numbers": s.numbers, "amount": bets[id]})
	return out
