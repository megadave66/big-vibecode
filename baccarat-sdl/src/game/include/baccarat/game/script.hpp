#pragma once
// Parser for the --script debug flag. Pure; the UI executes the commands.
// Grammar (comma separated, whitespace tolerant, lowercase):
//   chip:<1|5|25|100>  bet:<spot>  remove:<spot>  clear  rebet  deal
//   wait:<frames>  shot:<path>  assert-bankroll:<cents>
//   assert-phase:<betting|dealing|resolution|payout>  next  quit
// <spot> = player|banker|tie. Bad input throws std::invalid_argument naming the token.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "baccarat/game/session.hpp"

namespace bac::game {

enum class ScriptOp {
    Chip, Bet, Remove, Clear, Rebet, Deal, Wait, Shot, AssertBankroll, AssertPhase, Next, Quit
};

struct ScriptCmd {
    ScriptOp op = ScriptOp::Quit;
    std::int64_t value = 0;  // Chip: cents; Wait: frames; AssertBankroll: cents
    bac::rules::BetSpot spot = bac::rules::BetSpot::Player;  // Bet, Remove
    Phase phase = Phase::Betting;                            // AssertPhase
    std::string text;                                        // Shot: path
};

std::vector<ScriptCmd> parseScript(std::string_view script);

}  // namespace bac::game
