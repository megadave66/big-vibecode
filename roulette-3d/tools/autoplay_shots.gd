extends SceneTree
## Plays one real round of the main scene and saves screenshots, then quits.
## Usage (non-headless): godot --path . -s res://tools/autoplay_shots.gd -- --out=<dir>
## Shots: 1_bets.png, 2_spinning.png, 3_settled.png, 4_resolving.png, 5_next_round.png

var out_dir := "user://shots"
var main: Node
var stage := 0
var t := 0.0
var spin_t := 0.0


func _init() -> void:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--out="):
			out_dir = a.substr(6)
	DirAccess.make_dir_recursive_absolute(out_dir)
	main = load("res://scenes/main.tscn").instantiate()
	root.add_child(main)


func _shot(name: String) -> void:
	var img := root.get_viewport().get_texture().get_image()
	img.save_png(out_dir.path_join(name))
	print("saved ", out_dir.path_join(name))


func _process(delta: float) -> bool:
	t += delta
	var flow: RoundFlow = main.flow
	match stage:
		0:
			if t > 1.0:
				for id in ["straight_17", "split_0_37", "corner_1_2_4_5", "five_number", "red", "dozen_2"]:
					flow.place_chip(id, 25)
				flow.place_chip("straight_17", 5)
				var s: BetSpot = main.layout.get_spot("corner_11_12_14_15")
				main.overlay.hover_at(s.center)
				stage = 1
				t = 0.0
		1:
			if t > 0.8:
				_shot("1_bets.png")
				main.overlay.clear_hover()
				main.spin()
				stage = 2
				t = 0.0
		2:
			if t > 2.5:
				_shot("2_spinning.png")
				stage = 3
		3:
			if flow.phase == RoundFlow.Phase.SETTLED:
				spin_t = t
				stage = 4
				t = 0.0
			elif t > 60.0:
				print("FAIL: no settle in 60 s")
				return true
		4:
			if t > 0.9:
				_shot("3_settled.png")
				stage = 5
		5:
			if main._resolving:
				t = 0.0
				stage = 6
		6:
			if t > 0.9:
				_shot("4_resolving.png")
				stage = 7
		7:
			if flow.phase == RoundFlow.Phase.BETTING:
				t = 0.0
				stage = 8
		8:
			if t > 0.8:
				_shot("5_next_round.png")
				print("round done: number=%s bankroll=%d" % [WheelLayout.label(flow.history.back()), flow.bankroll.balance])
				return true
	return false
