extends PanelContainer
## How to Play screen. All odds and bet names come from Payouts.

signal closed

@onready var rules_text: RichTextLabel = %RulesText


func _ready() -> void:
	%CloseButton.pressed.connect(_on_close_button_pressed)
	rules_text.text = build_text()


## Build the full rules text (BBCode). Public so tests can call it.
static func build_text() -> String:
	var t := ""
	t += "[color=#e6c35c][b]The table[/b][/color]\n"
	t += "This is American roulette. The wheel has 38 pockets: 0, 00 and 1 to 36.\n"
	t += "You start a session with a $1,000 bankroll. There is no table minimum.\n\n"
	t += "[color=#e6c35c][b]A round[/b][/color]\n"
	t += "1. Betting phase: place your chips.\n"
	t += "2. Spin: press Space or the SPIN button. This means no more bets.\n"
	t += "3. The ball runs round the wheel and settles in a pocket.\n"
	t += "4. Winning bets are paid. Lost chips go to the house. Then a new round starts.\n\n"
	t += "[color=#e6c35c][b]Controls[/b][/color]\n"
	t += "- Pick a chip (1, 5, 25 or 100) with the buttons in the bottom bar.\n"
	t += "- Left-click the table to place a chip. Click again to stack more.\n"
	t += "- Right-click to remove the top chip from a spot.\n"
	t += "- Clear bets takes back all your chips.\n"
	t += "- Space or SPIN starts the spin.\n"
	t += "- Tab or C switches between the table view and the wheel view.\n"
	t += "- Esc closes this screen.\n\n"
	t += "[color=#e6c35c][b]Bet types and payouts[/b][/color]\n"
	for bet_type in Payouts.BetType.values():
		t += "- [b]%s[/b]: %s\n" % [Payouts.type_name(bet_type), Payouts.odds_text(bet_type)]
	t += "\n[color=#e6c35c][b]Five-number bet[/b][/color]\n"
	t += "The Five Number bet covers 0, 00, 1, 2 and 3 (0-00-1-2-3). "
	t += "It pays %s.\n" % Payouts.odds_text(Payouts.BetType.FIVE_NUMBER)
	t += "\nGood luck.\n"
	return t


func _on_close_button_pressed() -> void:
	closed.emit()
	queue_free()
