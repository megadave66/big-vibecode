#pragma once
// Draws the gameplay screen with procedural art (no sprite files). Pure drawing: reads GameScreen,
// never changes it. Menus can reuse drawGame() as a frozen backdrop.

#include "Gfx.h"
#include "GameScreen.h"
#include "Text.h"

namespace gd {

struct DrawOptions {
    bool hitboxes = false;      // --hitboxes: outline player boxes and spike hitboxes
    bool hud = true;            // progress bar, percent, tags
};

// World, player, particles and HUD. `alpha` = interpolation between the last two ticks.
void drawGame(Gfx& gfx, TextRenderer& text, const GameScreen& game, double alpha, const DrawOptions& opt);

}  // namespace gd
