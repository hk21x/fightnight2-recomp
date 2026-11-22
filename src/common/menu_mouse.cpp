// The mouse in the menus (out of a fight): a left click is A, a right click B, the wheel the d-pad up and down. In a
// fight the mouse is the right stick (punches) and its right button blocks (default_keys), so this stays out of it.
// The game's menus are APT movies with no pointer of their own: the selection moves by the wheel.
#include <cstdint>

#include "core/input.h"

namespace fn2 {
bool InFight();  // src/game/ultrawide.cpp

void MenuMouse(input::Pad& pad) {
    static uint32_t held = 0;  // polls a press is still held for
    static int press = -1;     // the control being pressed
    if (InFight()) {
        press = -1, held = 0;
        return;
    }
    if (held) {
        held--;
    } else {
        press = -1;
        const uint32_t buttons = input::MouseButtons();
        static uint32_t last = 0;
        const uint32_t down = buttons & ~last;
        last = buttons;
        const int wheel = input::TakeWheel();
        if (down & 1) press = input::kA;
        else if (down & 2) press = input::kB;
        else if (wheel > 0) press = input::kUp;
        else if (wheel < 0) press = input::kDown;
        if (press >= 0) held = 4;  // a press the game sees: about 4 polls
    }
    if (press < 0) return;
    if (press < input::kA) pad.buttons |= uint16_t(1u << press);
    else pad.analog[press - input::kA] = 255;
}
}  // namespace fn2
