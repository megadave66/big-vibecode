#include "baccarat/game/script.hpp"

#include <cctype>
#include <charconv>
#include <stdexcept>

namespace bac::game {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

[[noreturn]] void bad(std::string_view tok, const char* why) {
    throw std::invalid_argument("bad script token '" + std::string(tok) + "': " + why);
}

std::int64_t parseInt(std::string_view tok, std::string_view arg) {
    arg = trim(arg);
    std::int64_t v = 0;
    const char* end = arg.data() + arg.size();
    auto [p, ec] = std::from_chars(arg.data(), end, v);
    if (arg.empty() || ec != std::errc{} || p != end) bad(tok, "expected an integer");
    return v;
}

bac::rules::BetSpot parseSpot(std::string_view tok, std::string_view arg) {
    arg = trim(arg);
    if (arg == "player") return bac::rules::BetSpot::Player;
    if (arg == "banker") return bac::rules::BetSpot::Banker;
    if (arg == "tie") return bac::rules::BetSpot::Tie;
    bad(tok, "expected player, banker or tie");
}

ScriptCmd parseOne(std::string_view tok) {
    ScriptCmd c;
    const auto colon = tok.find(':');
    const std::string_view name = trim(tok.substr(0, colon));
    const bool hasArg = colon != std::string_view::npos;
    const std::string_view arg = hasArg ? tok.substr(colon + 1) : std::string_view{};

    auto noArg = [&](ScriptOp op) {
        if (hasArg) bad(tok, "command takes no argument");
        c.op = op;
    };
    auto needArg = [&] {
        if (!hasArg || trim(arg).empty()) bad(tok, "missing argument");
    };

    if (name == "chip") {
        needArg();
        c.op = ScriptOp::Chip;
        const std::int64_t d = parseInt(tok, arg);
        if (d != 1 && d != 5 && d != 25 && d != 100) bad(tok, "chip must be 1, 5, 25 or 100");
        c.value = d * 100;
    } else if (name == "bet") {
        needArg();
        c.op = ScriptOp::Bet;
        c.spot = parseSpot(tok, arg);
    } else if (name == "remove") {
        needArg();
        c.op = ScriptOp::Remove;
        c.spot = parseSpot(tok, arg);
    } else if (name == "clear") {
        noArg(ScriptOp::Clear);
    } else if (name == "rebet") {
        noArg(ScriptOp::Rebet);
    } else if (name == "deal") {
        noArg(ScriptOp::Deal);
    } else if (name == "next") {
        noArg(ScriptOp::Next);
    } else if (name == "quit") {
        noArg(ScriptOp::Quit);
    } else if (name == "wait") {
        needArg();
        c.op = ScriptOp::Wait;
        c.value = parseInt(tok, arg);
        if (c.value < 0) bad(tok, "frames must be >= 0");
    } else if (name == "shot") {
        needArg();
        c.op = ScriptOp::Shot;
        c.text = std::string(trim(arg));
    } else if (name == "assert-bankroll") {
        needArg();
        c.op = ScriptOp::AssertBankroll;
        c.value = parseInt(tok, arg);
    } else if (name == "assert-phase") {
        needArg();
        c.op = ScriptOp::AssertPhase;
        const std::string_view a = trim(arg);
        if (a == "betting") c.phase = Phase::Betting;
        else if (a == "dealing") c.phase = Phase::Dealing;
        else if (a == "resolution") c.phase = Phase::Resolution;
        else if (a == "payout") c.phase = Phase::Payout;
        else bad(tok, "expected betting, dealing, resolution or payout");
    } else {
        bad(tok, "unknown command");
    }
    return c;
}

}  // namespace

std::vector<ScriptCmd> parseScript(std::string_view script) {
    std::vector<ScriptCmd> out;
    if (trim(script).empty()) return out;
    while (true) {
        const auto comma = script.find(',');
        const std::string_view tok = trim(script.substr(0, comma));
        if (tok.empty()) bad(tok, "empty command");
        out.push_back(parseOne(tok));
        if (comma == std::string_view::npos) break;
        script.remove_prefix(comma + 1);
    }
    return out;
}

}  // namespace bac::game
