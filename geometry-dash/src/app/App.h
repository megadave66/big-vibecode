#pragma once
// SDL3 front end: window, renderer, fixed-step loop, input mapping, state machine.
//
// Modules (all in src/app/):
//   Gfx          batched shape drawing (SDL_RenderGeometry)
//   Text         cached SDL_ttf text:  app.text().draw(app.gfx(), "Hi", x, y, size, color, align)
//   Audio        SDL_mixer 3 music + SFX: app.audio().playSfx(Sfx::MenuMove)
//   GameScreen   gameplay state (Sim, practice, particles, camera) + hooks
//   GameRenderer draws a GameScreen (also usable as a frozen backdrop behind menus)
//
//   Menus        layout + drawing of main menu, level select, pause, level complete
//   core/Progress attempts / best % / completed per level, saved as JSON (see --save)
//
// State flow: Boot -> MainMenu <-> LevelSelect -> Playing <-> Paused, Playing -> Dead -> Playing
// (Dead lasts the short death burst, then the level restarts by itself), Playing -> Complete ->
// LevelSelect. The game screen keeps stepping in Playing, Dead and Complete.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Audio.h"
#include "Gfx.h"
#include "GameRenderer.h"
#include "GameScreen.h"
#include "Menus.h"
#include "Text.h"
#include "core/FixedTimestep.h"
#include "core/Progress.h"
#include "core/StateMachine.h"

struct SDL_Window;
struct SDL_Renderer;

namespace gd {

struct AppOptions {
    std::string dataDir;          // --data <dir>; empty = $GD_DATA_DIR, then the compiled-in GD_DATA_DIR
    int level = 0;                // --level N (1..10), 0 = not given
    std::string levelFile;        // --level-file <path>: play this file instead of levels/levelNN.json
    bool practice = false;        // --practice
    bool fly = false;             // --fly
    bool hitboxes = false;        // --hitboxes (debug overlay)
    std::string replayFile;       // --replay <file>
    int smokeFrames = -1;         // --smoke N, -1 = off
    std::string screenshotFile;   // --screenshot <out.bmp>
    int screenshotFrame = 0;      // --frame K
    int frameEvery = 0;           // --frame-every K (with --screenshot-dir)
    int maxShots = 20;            // --max-shots N (1..200) cap for --frame-every
    std::string screenshotDir;    // --screenshot-dir DIR
    bool mute = false;            // --mute
    std::string savePath;         // --save <path>: progress file (default: SDL pref path; none in headless runs)
    std::vector<std::string> menuScript;  // --menu-script "down,enter,esc": one key every few frames
    // Scripted input for headless runs (frame numbers):
    std::vector<std::pair<int, int>> holdRanges;   // --hold 60-75,200-215
    std::vector<int> checkpointFrames;             // --checkpoint-at 100,150 (practice)
};

// Returns false (and fills error) on a bad or unknown flag.
bool parseArgs(int argc, char** argv, AppOptions& out, std::string& error);

// One frame of mapped input. "held" is the level (true while pressed); the rest are edges.
struct InputState {
    bool held = false;      // space / up / left mouse
    bool pause = false;     // Esc pressed this frame (pause / back)
    bool confirm = false;   // Enter pressed this frame
    bool up = false, down = false, left = false, right = false;  // arrow presses this frame
    bool checkpoint = false;      // Z or C
    bool removeCheckpoint = false;// X
    bool restart = false;         // R
    bool togglePractice = false;  // P
    bool mouseMoved = false;      // pointer moved this frame (mouseX/mouseY valid)
    bool click = false;           // left button pressed this frame
    float mouseX = 0, mouseY = 0; // logical 1280x720 coordinates
    bool quit = false;      // window closed
};

class App {
public:
    explicit App(AppOptions options);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Runs until quit / smoke done / screenshot saved. Returns the process exit code.
    int run();

    AppState state() const { return state_; }
    Gfx& gfx() { return *gfx_; }
    TextRenderer& text() { return text_; }
    Audio& audio() { return audio_; }
    GameScreen* game() { return game_.get(); }
    const std::string& dataDir() const { return dataDir_; }

    // Builds the game screen for level N (levels/levelNN.json) or an explicit file and goes to
    // AppState::Playing. Returns false with the reason in lastError().
    bool startGame(int levelId);
    bool startGameFile(const std::string& path);
    const std::string& lastError() const { return error_; }

private:
    bool init();
    void shutdown();
    void pollInput(InputState& in);
    void scriptedInput(InputState& in) const;
    void changeState(AppState to);
    void update(const InputState& in);
    void updateMenus(const InputState& in);
    void drawFrame(double alpha);
    void updateLevelSelect(const InputState& in);
    void updatePause(const InputState& in);
    void updateComplete(const InputState& in);
    void updateDead(const InputState& in);
    void activateMain(int index);
    void activateLevel(int index);
    void activatePause(int index);
    void activateComplete(int index);
    void leaveGame();
    void saveProgress();
    void attachProgressHooks();
    bool saveScreenshot(const std::string& path);

    AppOptions opt_;
    std::string dataDir_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Gfx> gfx_;
    TextRenderer text_;
    Audio audio_;
    std::unique_ptr<GameScreen> game_;
    FixedTimestep timestep_;
    AppState state_ = AppState::Boot;
    long frame_ = 0;
    long stateFrames_ = 0;
    int menuIndex_ = 0;            // main menu
    int levelIndex_ = 0;           // level select (0-based)
    int pauseIndex_ = 0;
    int completeIndex_ = 0;
    bool practiceSelected_ = false;  // practice toggle on the level select screen
    bool pendingPause_ = false;      // Esc pressed while dying
    Progress progress_;
    std::vector<menu::LevelInfo> levels_;
    int ticksThisFrame_ = 0;
    bool directStart_ = false;     // started from --level / --level-file
    std::string error_;
    std::string toast_;
};

}  // namespace gd
