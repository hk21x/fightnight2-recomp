#include "app/app.h"
#include "core/input.h"

namespace fn2 {
void TestPad(int port, input::Pad& pad);  // test_pad.cpp: FN2_PAD_FILE
}

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Fight Night Round 2",      // window and splash
    .short_title = L"Fight Night Round 2",  // message boxes
    .name = L"FightNight2",               // window classes, crash dumps, handover event
    .log = "fightnight2",                 // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "FightNight2"}},  // each recompiled xbe and its exe, for title launches
    .options = {},                        // game settings kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = fn2::TestPad,                  // FN2_PAD_FILE (tests): controller 1 held from a file
    .rumble = nullptr,                    // changes a rumble before it is set
    .languages = {1, 3, 4},               // English, German, French: locale\*.loc and the movies' .nfx, .ger, .fre
    .pal_console = true,                  // a PAL-only disc (region 4): a PAL console with PAL 60 Hz on
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
