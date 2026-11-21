#include <array>
#include <cstdint>
#include <cstdlib>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"

namespace fn2 {
void TestPad(int port, input::Pad& pad);                 // test_pad.cpp: FN2_PAD_FILE
bool ButtonPictures(uint32_t header, uint32_t palette);  // buttons.cpp
extern bool modern_buttons;                              // buttons.cpp

namespace {
constexpr uint16_t kMouseRight = 0x02, kBackspace = 0x08, kTab = 0x09, kEnter = 0x0D, kEscape = 0x1B, kSpace = 0x20,
                   kArrowLeft = 0x25, kArrowUp = 0x26, kArrowRight = 0x27, kArrowDown = 0x28, kLeftShift = 0xA0;

// The game's controls (My Corner > Options > Controls, configuration 1, the default): the left stick and the d-pad
// move; the right stick is Total Punch Control (straights, hooks and uppercuts by its gestures); LT leans and, with
// the right stick, punches to the body; RT blocks; A and White throw the signature punch; B and Black the illegal blow;
// X taunts; Y clinches; START pauses. On today's pad White and Black are LB and RB.
// The keys: WASD moves, the mouse is the right stick (its flicks the punches; IJKL too), Shift leans, Space and the
// right mouse button block; Enter and Backspace choose and go back in the menus, F and G are A and B in a fight.
std::array<uint16_t, 2> Keys(int c) {
    using namespace input;
    switch (c) {
        case kLSUp: return {'W', 0};
        case kLSDown: return {'S', 0};
        case kLSLeft: return {'A', 0};
        case kLSRight: return {'D', 0};
        case kRSUp: return {'I', 0};
        case kRSDown: return {'K', 0};
        case kRSLeft: return {'J', 0};
        case kRSRight: return {'L', 0};
        case kUp: return {kArrowUp, 0};
        case kDown: return {kArrowDown, 0};
        case kLeft: return {kArrowLeft, 0};
        case kRight: return {kArrowRight, 0};
        case kLT: return {kLeftShift, 0};       // lean, body punches
        case kRT: return {kSpace, kMouseRight};  // block
        case kA: return {kEnter, 'F'};           // signature punch; Enter selects in the menus
        case kB: return {kBackspace, 'G'};       // illegal blow; Backspace goes back in the menus
        case kX: return {'T', 0};                // taunt
        case kY: return {'C', 0};                // clinch; help in the menus
        case kWhite: return {'E', 0};            // signature punch (LB)
        case kBlack: return {'R', 0};            // illegal blow (RB)
        case kLThumb: return {'Z', 0};
        case kRThumb: return {'V', 0};
        case kBack: return {kTab, 0};
        case kStart: return {kEscape, 'P'};      // pause
        default: return {0, 0};
    }
}

// after the settings load: FN2_MODERN_BUTTONS (1 or 0) overrides it, for the headless harness, which has no settings file
void Init() {
    if (const char* e = std::getenv("FN2_MODERN_BUTTONS"); e && *e) {
        modern_buttons = *e == '1';
        LOGR("fn2: FN2_MODERN_BUTTONS=%c", *e);
    }
}
}  // namespace
}  // namespace fn2

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
    .options = {                          // game settings kept in the settings file
        {"modern_buttons", &fn2::modern_buttons, nullptr, "Modern button pictures",
         "Today's Xbox pad in the prompts and on the controls page (A B X Y on dark discs, Menu for START, LB and RB for "
         "White and Black); off, the original controller's. With the keyboard the keys show whatever this says."},
    },
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = fn2::Init,                    // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = nullptr,                   // window messages first; true takes one
    .ui = nullptr,                        // ImGui drawing each frame
    .pad = fn2::TestPad,                  // FN2_PAD_FILE (tests): controller 1 held from a file
    .rumble = nullptr,                    // changes a rumble before it is set
    .languages = {1, 3, 4},               // English, German, French: locale\*.loc and the movies' .nfx, .ger, .fre
    .pal_console = true,                  // a PAL-only disc (region 4): a PAL console with PAL 60 Hz on
    .mouse_look = true,                   // the mouse plays the right stick: Total Punch Control's punches
    .default_keys = fn2::Keys,            // the keys above
    .texture_bound = fn2::ButtonPictures,  // buttons.cpp: today's pad and the keyboard's keys in the prompts
    // optional, after these: .achievements (conditions on guest memory), .same_code (other releases' xbe SHA-1s with
    // the same code), .wheel (what a steering wheel's force feedback follows)
};
