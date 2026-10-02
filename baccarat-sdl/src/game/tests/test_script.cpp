#include <doctest/doctest.h>

#include <stdexcept>
#include <string>

#include "baccarat/game/script.hpp"

using namespace bac::game;
using bac::rules::BetSpot;

TEST_CASE("script: empty input") {
    CHECK(parseScript("").empty());
    CHECK(parseScript("   ").empty());
}

TEST_CASE("script: every command") {
    auto v = parseScript(
        "chip:25,bet:player,bet:banker,bet:tie,remove:player,remove:banker,remove:tie,clear,"
        "rebet,deal,wait:30,shot:out/a.bmp,assert-bankroll:99500,"
        "assert-phase:betting,assert-phase:dealing,assert-phase:resolution,assert-phase:payout,"
        "next,quit");
    REQUIRE(v.size() == 19);
    CHECK(v[0].op == ScriptOp::Chip);
    CHECK(v[0].value == 2500);
    CHECK(v[1].op == ScriptOp::Bet);
    CHECK(v[1].spot == BetSpot::Player);
    CHECK(v[2].spot == BetSpot::Banker);
    CHECK(v[3].spot == BetSpot::Tie);
    CHECK(v[4].op == ScriptOp::Remove);
    CHECK(v[4].spot == BetSpot::Player);
    CHECK(v[5].spot == BetSpot::Banker);
    CHECK(v[6].spot == BetSpot::Tie);
    CHECK(v[7].op == ScriptOp::Clear);
    CHECK(v[8].op == ScriptOp::Rebet);
    CHECK(v[9].op == ScriptOp::Deal);
    CHECK(v[10].op == ScriptOp::Wait);
    CHECK(v[10].value == 30);
    CHECK(v[11].op == ScriptOp::Shot);
    CHECK(v[11].text == "out/a.bmp");
    CHECK(v[12].op == ScriptOp::AssertBankroll);
    CHECK(v[12].value == 99500);
    CHECK(v[13].phase == Phase::Betting);
    CHECK(v[14].phase == Phase::Dealing);
    CHECK(v[15].phase == Phase::Resolution);
    CHECK(v[16].phase == Phase::Payout);
    CHECK(v[16].op == ScriptOp::AssertPhase);
    CHECK(v[17].op == ScriptOp::Next);
    CHECK(v[18].op == ScriptOp::Quit);
}

TEST_CASE("script: chips") {
    CHECK(parseScript("chip:1")[0].value == 100);
    CHECK(parseScript("chip:5")[0].value == 500);
    CHECK(parseScript("chip:100")[0].value == 10000);
    CHECK_THROWS_AS(parseScript("chip:10"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("chip:"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("chip"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("chip:abc"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("chip:5x"), std::invalid_argument);
}

TEST_CASE("script: whitespace tolerant") {
    auto v = parseScript("  chip : 5 ,\tbet:player ,\n deal ,wait: 3 ,shot: a b.bmp ");
    REQUIRE(v.size() == 5);
    CHECK(v[0].value == 500);
    CHECK(v[1].spot == BetSpot::Player);
    CHECK(v[2].op == ScriptOp::Deal);
    CHECK(v[3].value == 3);
    CHECK(v[4].text == "a b.bmp");
}

TEST_CASE("script: shot path keeps colons") {
    CHECK(parseScript("shot:C:/x.bmp")[0].text == "C:/x.bmp");
}

TEST_CASE("script: negative bankroll assertion and wait 0") {
    CHECK(parseScript("assert-bankroll:-5")[0].value == -5);
    CHECK(parseScript("wait:0")[0].value == 0);
    CHECK_THROWS_AS(parseScript("wait:-1"), std::invalid_argument);
}

TEST_CASE("script: errors name the bad token") {
    const auto msg = [](const char* s) {
        try {
            parseScript(s);
        } catch (const std::invalid_argument& e) {
            return std::string(e.what());
        }
        return std::string();
    };
    CHECK(msg("deal,bogus").find("bogus") != std::string::npos);
    CHECK(msg("bet:dealer").find("bet:dealer") != std::string::npos);
    CHECK(msg("assert-phase:later").find("assert-phase:later") != std::string::npos);
    CHECK_THROWS_AS(parseScript("bet"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("remove:x"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("remove"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("deal:1"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("clear:x"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("rebet:x"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("next:x"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("quit:x"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("wait"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("wait:x"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("shot"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("shot:"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("assert-bankroll"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("assert-bankroll:1.5"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("assert-phase"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("deal,,deal"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("deal,"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript(",deal"), std::invalid_argument);
    CHECK_THROWS_AS(parseScript("DEAL"), std::invalid_argument);
}
