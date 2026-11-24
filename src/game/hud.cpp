// hide_hud: the fight's HUD (health and stamina bars, the round card) hidden through the game's own setting (pause menu
// > HUD, the profile's bHud): bit 0 of byte 0x37FFEF, the settings 0x37FFD0 (returned by 0x1659B0) +0x1C bit 24, as
// 0xC60D0 writes it to the save as "bHud". Memory dumps of a fight either side of the pause menu's switch: 0x3F, 0x3E.
// While the setting is on the bit is held clear; turned off, it is set once. F9 in play switches the setting.
#include <cstdint>

#include "app/app.h"
#include "gpu/backend.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace fn2 {
bool hide_hud = false;  // the setting (src/common/game_info.cpp)

// often (each texture bind, at most every 16 ms: src/game/ultrawide.cpp)
void HudTick() {
    constexpr uint32_t kHud = 0x37FFEF;
    static bool forced = false;
    uint8_t& b = kernel::G<uint8_t>(kHud);
    if (hide_hud) {
        b &= uint8_t(~1u);
        forced = true;
    } else if (forced) {
        b |= 1u;
        forced = false;
    }
}

// F9 (not a repeat): the HUD hidden or shown, and saved. F11: the last second and a half of smooth motion's data
// written beside the exe (gpu::RequestInterpDump), to find the skin flicker seen on Harry's PC
bool HudKey(void* /*hwnd*/, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* /*result*/) {
    constexpr uint32_t kKeyDown = 0x100;
    constexpr uintptr_t kF9 = 0x78, kF11 = 0x7A;
    if (msg == kKeyDown && wp == kF11 && !(lp & (1 << 30))) {
        gpu::RequestInterpDump();
        return true;
    }
    if (msg != kKeyDown || wp != kF9 || (lp & (1 << 30))) return false;
    hide_hud = !hide_hud;
    app::Save();
    LOGR("fn2: the HUD %s (F9)", hide_hud ? "hidden" : "shown");
    return true;
}
}  // namespace fn2
