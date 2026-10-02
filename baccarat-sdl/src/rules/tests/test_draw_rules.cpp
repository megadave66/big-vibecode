#include <doctest/doctest.h>

#include <optional>
#include <stdexcept>

#include "baccarat/rules/rules.hpp"

using namespace bac::rules;

// Independent expected banker table, written by hand from the Punto Banco
// rules card. Rows: banker two-card total 0..9.
// Columns: player stood ('-'), then player third card points 0..9.
//                            -0123456789
static const char* const kBankerTable[10] = {
    /* B0 */ "DDDDDDDDDDD",
    /* B1 */ "DDDDDDDDDDD",
    /* B2 */ "DDDDDDDDDDD",
    /* B3 */ "DDDDDDDDDSD",
    /* B4 */ "DSSDDDDDDSS",
    /* B5 */ "DSSSSDDDDSS",
    /* B6 */ "SSSSSSSDDSS",
    /* B7 */ "SSSSSSSSSSS",
    /* B8 */ "SSSSSSSSSSS",  // natural
    /* B9 */ "SSSSSSSSSSS",  // natural
};

TEST_CASE("bankerShouldDraw: exhaustive 10x11 table") {
    int checked = 0;
    for (int b = 0; b <= 9; ++b) {
        for (int col = 0; col <= 10; ++col) {
            const bool expectDraw = kBankerTable[b][col] == 'D';
            CAPTURE(b);
            CAPTURE(col);
            if (col == 0) {
                // Player stood: a non-natural player total of 6 or 7.
                for (int p : {6, 7}) {
                    CAPTURE(p);
                    CHECK(bankerShouldDraw(p, b, std::nullopt) == expectDraw);
                    ++checked;
                }
            } else {
                const int third = col - 1;
                // Player drew: player two-card total 0..5.
                for (int p = 0; p <= 5; ++p) {
                    CAPTURE(p);
                    CHECK(bankerShouldDraw(p, b, third) == expectDraw);
                    ++checked;
                }
            }
        }
    }
    CHECK(checked == 10 * (2 + 10 * 6));
}

TEST_CASE("bankerShouldDraw: player natural means banker never draws") {
    for (int p : {8, 9})
        for (int b = 0; b <= 9; ++b) {
            CHECK_FALSE(bankerShouldDraw(p, b, std::nullopt));
            for (int t = 0; t <= 9; ++t) CHECK_FALSE(bankerShouldDraw(p, b, t));
        }
}

// Independent expected player table. Rows: player 0..9, columns banker 0..9.
static const char* const kPlayerTable[10] = {
    //        banker 0123456789
    /* P0 */ "DDDDDDDDSS",
    /* P1 */ "DDDDDDDDSS",
    /* P2 */ "DDDDDDDDSS",
    /* P3 */ "DDDDDDDDSS",
    /* P4 */ "DDDDDDDDSS",
    /* P5 */ "DDDDDDDDSS",
    /* P6 */ "SSSSSSSSSS",
    /* P7 */ "SSSSSSSSSS",
    /* P8 */ "SSSSSSSSSS",
    /* P9 */ "SSSSSSSSSS",
};

TEST_CASE("playerShouldDraw: exhaustive 10x10 including naturals") {
    for (int p = 0; p <= 9; ++p)
        for (int b = 0; b <= 9; ++b) {
            CAPTURE(p);
            CAPTURE(b);
            CHECK(playerShouldDraw(p, b) == (kPlayerTable[p][b] == 'D'));
        }
}

TEST_CASE("draw rules reject out-of-range totals") {
    CHECK_THROWS_AS(playerShouldDraw(-1, 0), std::invalid_argument);
    CHECK_THROWS_AS(playerShouldDraw(0, 10), std::invalid_argument);
    CHECK_THROWS_AS(bankerShouldDraw(10, 0, std::nullopt), std::invalid_argument);
    CHECK_THROWS_AS(bankerShouldDraw(0, -1, std::nullopt), std::invalid_argument);
    CHECK_THROWS_AS(bankerShouldDraw(3, 3, 10), std::invalid_argument);
    CHECK_THROWS_AS(bankerShouldDraw(3, 3, -1), std::invalid_argument);
}
