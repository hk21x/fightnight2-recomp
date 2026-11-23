// Ultrawide: the fights at the shape the aspect setting gives, the front end at 16:9; a change applies at once.
//
// Every 3D view's projection is built by 0x213400 (thiscall on the camera: the horizontal field of view in degrees,
// the aspect, near, far), which also builds the camera's culling planes from the same two (+0x158..+0x174), about once
// a frame. In its widescreen mode (the console reporting a widescreen TV, always: GameInfo::aspect_live) the views'
// set-up 0x133040 widens the 4:3 field of view by 4/3 in its tangent (DAT 0x2EA514) and passes 16:9 (0x3FE38E39); the
// 4:3 views (0x12B510, 0x12AD10, 0x2138C0, 0x132EC0: pictures drawn into textures) pass 4/3. A view asked at 16:9 is
// given the frame's shape instead, its field of view widened the same way (tan(fov/2) times frame / (16:9)), so a
// frame wider than 16:9 shows more to the sides instead of stretching, and the culling widens with it. The fight's 2D
// is 16:9, kept so in the middle by the renderers.
//
// The front end is 2D laid out for 16:9 over full-screen backgrounds, which would stretch; its 3D (the boxers on the
// menus) is seen through a narrow lens, 8 degrees across, where the arena's cameras (the fight's 57.8, the walk-ins'
// 10 to 110) are wide. So the frame is 16:9 there (GameInfo::frame_aspect, a blurred copy at its sides:
// bars_backdrop) and the setting's shape in a fight: from when 16:9 views wider than 20 degrees have been set up for
// 300 ms on end, until a menu's narrow view or neither those views nor the fight's own 2D (shapes through 0x125280,
// which the front end's APT menus never use) for a second (quitting to Select Boxer). In-engine cutscenes (walk-ins,
// replays, knockdowns, round ends) are the arena's 3D and fill the screen too, their letterbox bars (untextured
// shapes 854 wide through 0x125280) reaching its sides. The decision is made each bind's time (at most every 16 ms).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace {
float F32(uint32_t va) {
    uint32_t v = kernel::R32(va);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
void W(uint32_t va, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    kernel::W32(va, v);
}

constexpr float kWide = 16.0f / 9.0f;
using Clock = std::chrono::steady_clock;
Clock::time_point g_bar;    // a letterbox bar last drawn
Clock::time_point g_arena;  // a wide 16:9 view (the arena's) last set up
Clock::time_point g_since;  // since when they have been set up without a break
Clock::time_point g_narrow; // a narrow 16:9 view (a menu's boxer) last set up
Clock::time_point g_shape;  // a 2D shape drawn (the fight's HUD, pause, throw in the towel; not the front end's menus)
std::atomic<bool> g_front{true};  // the front end (or a load between): the frame at 16:9; a fight: the setting's shape
std::atomic<float> g_chosen{0};   // the aspect setting's shape as last asked (GameInfo::frame_aspect)

bool Recent(Clock::time_point t) { return Clock::now() - t < std::chrono::milliseconds(250); }

// in the front end until the arena's views have been set up for 300 ms on end (a menu's one wide view as it opens is
// not a fight); back in it on a menu's narrow boxer view, or neither the arena's views nor the fight's 2D for 200 ms.
// Measured headless (2026-11-23): in a fight the arena's views never stop for more than 60 ms; paused, they stop
// altogether (on Harry's PC they come back about once a second: a rule on them alone flickered the pause menu between
// the shapes), but the pause menu draws its 2D through 0x125280 every frame; after a quit both stop at once (the old
// wait of a second for the 2D showed the next menus stretched). Decided whatever the aspect setting, so a change to
// it in play applies at once
void Decide() {
    const auto now = Clock::now();
    const bool arena = Recent(g_arena) && now - g_since > std::chrono::milliseconds(300);
    const bool left = now - g_arena > std::chrono::milliseconds(200) && now - g_shape > std::chrono::milliseconds(200);
    const bool front = g_front ? !arena : Recent(g_narrow) || left;
    if (front == g_front) return;
    g_front = front;
    if (front) g_since = now;  // back to a fight only after 300 ms of its views from here (a quit's last view is not one)
    LOGR("fn2: %s", front ? "the front end: the frame at 16:9" : "the fight: the frame at the setting's shape");
}
}  // namespace

namespace fn2 {
// the frame held at 16:9 (the front end, a cutscene): GameInfo::bars_backdrop, a blurred copy of it at the sides
bool FrameHeld() { return g_front && g_chosen > kWide + 1e-3f; }

// GameInfo::frame_aspect (aspect_live: every aspect choice applies at once, the console a widescreen TV): the front
// end at 16:9 (its 2D is 16:9 art), a fight at the setting's shape; never under 16:9, the game's 2D being laid out for
// 16:9 whatever is chosen (a 4:3 choice shows 16:9)
float FrameShape(float chosen) {
    g_chosen = chosen;
    return g_front ? kWide : std::max(chosen, kWide);
}

// in a fight (src/common/game_info.cpp: in-between frames only there): the arena's views or the fight's own 2D lately
bool InFight() {
    const auto now = Clock::now();
    return now - g_arena < std::chrono::seconds(1) || now - g_shape < std::chrono::seconds(1);
}

// each texture bound (GameInfo::texture_bound, src/common/buttons.cpp): every frame binds some, in the front end too,
// where no 3D view may be set up; at most once in 16 ms
void HudTick();  // src/game/hud.cpp
void UltrawideTick() {
    static Clock::time_point last;
    const auto now = Clock::now();
    if (now - last < std::chrono::milliseconds(16)) return;
    last = now;
    Decide();
    HudTick();
}
}  // namespace fn2

// the 2D shape drawer (ecx the shape): a letterbox bar on screen is a cutscene
X86_EXTERN_FUNC(__imp__sub_00125280);
X86_FUNC(sub_00125280) {
    const uint32_t o = ctx.ecx;
    if (kernel::G<uint8_t>(o + 0x25)) g_shape = Clock::now();
    if (kernel::G<uint8_t>(o + 0x25) && kernel::G<uint8_t>(o + 0x30) == 4 && kernel::R32(o + 0xE4) == 0) {
        float x0 = F32(o + 0x34), x1 = x0, y0 = F32(o + 0x38), y1 = y0;
        for (uint32_t i = 1; i < 4; i++) {
            const float x = F32(o + 0x34 + i * 16), y = F32(o + 0x38 + i * 16);
            x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
        }
        const float top = F32(o + 0x14) + y0, bottom = F32(o + 0x14) + y1;
        // wider than the 4:3 layout by far, a strip, at least partly on screen
        if (x1 - x0 > 800.0f && y1 - y0 < 120.0f && bottom > 0.5f && top < 479.5f) g_bar = Clock::now();
    }
    // the fight's 2D (the HUD, an APT overlay movie drawn through the model drawer 0x217F80 with a 2D camera; the round
    // card through this drawer) is laid out for 16:9 and kept that shape in the middle of a wider frame (GameInfo::
    // squeeze_screen_programs: drawn by flat matrix programs; at 2.4:1 it was stretched 1.33 times across). Its pieces
    // stay where a 16:9 screen has them: placing them at the frame's edges would mean moving the movie's own objects.
    __imp__sub_00125280(ctx, base);
}

// (camera; fov degrees, aspect, near, far), ret 0x10
X86_EXTERN_FUNC(__imp__sub_00213400);
X86_FUNC(sub_00213400) {
    constexpr float kPi = 3.14159265f;
    const float fov = F32(ctx.esp + 4), aspect = F32(ctx.esp + 8);
    if (std::fabs(aspect - kWide) < 1e-3f) {
        const auto now = Clock::now();
        if (fov > 20.0f) {
            if (!Recent(g_arena)) g_since = now;
            g_arena = now;
        } else {
            g_narrow = now;
        }
        const float frame = app::FrameAspect();
        if (std::fabs(frame - kWide) > 1e-3f) {
            const float wider = 2.0f * std::atan(std::tan(fov * kPi / 360.0f) * frame / kWide) * 180.0f / kPi;
            W(ctx.esp + 4, wider);
            W(ctx.esp + 8, frame);
            static float said = 0;
            if (std::fabs(said - frame) > 1e-3f) {
                said = frame;
                LOGR("fn2: 3D views at %.3f:1 (a 16:9 view of %.1f degrees across shown at %.1f)", frame, fov, wider);
            }
        }
    }
    __imp__sub_00213400(ctx, base);
}

