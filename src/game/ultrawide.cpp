// Ultrawide: the fights at the shape the frame is shown at, the front end and the cutscenes at 16:9.
//
// Every 3D view's projection is built by 0x213400 (thiscall on the camera: the horizontal field of view in degrees,
// the aspect, near, far), which also builds the camera's culling planes from the same two (+0x158..+0x174), about once
// a frame. In its widescreen mode (the console reporting a widescreen TV) the views' set-up 0x133040 widens the 4:3
// field of view by 4/3 in its tangent (DAT 0x2EA514) and passes 16:9 (0x3FE38E39); the 4:3 views (0x12B510, 0x12AD10,
// 0x2138C0, 0x132EC0: pictures drawn into textures) pass 4/3. A view asked at 16:9 is given the frame's shape instead,
// its field of view widened the same way (tan(fov/2) times frame / (16:9)), so a frame wider than 16:9 shows more to
// the sides instead of stretching, and the culling widens with it. The HUD is 16:9 2D, kept so in the middle by the
// renderers.
//
// The front end is 2D laid out for 16:9 over full-screen backgrounds, which would stretch; its 3D (the boxers on the
// menus) is seen through a narrow lens, 8 degrees across, where the arena's cameras (the fight's 57.8, the walk-ins'
// 10 to 110) are wide. So the frame is held at 16:9 (app::HoldAspect), its sides black, except while a 16:9 view wider
// than 20 degrees has been set up in the last quarter second, and for 300 ms without a break before that. It stays
// filled while the fight's own 2D is drawn (its HUD, pause, throw in the towel: shapes through 0x125280, which the
// front end's menus, drawn by APT, never use; no view is set up while paused) and is held again by a cutscene's bars, a
// menu's narrow view, or neither the arena nor any shape in the last quarter second (quitting to Select Boxer, whose
// boxers are drawn into 4:3 textures).
//
// Cutscenes: the in-engine ones (the walk-ins, "Tonight's Fight", the corners between rounds, replays) are letterboxed
// by four untextured shapes 854 wide (480 x 16:9, from x -107 in the game's 640x480 layout), 33.5 high, sliding in
// from above and below; the shapes go through the 2D shape drawer 0x125280 (fastcall, the shape in ecx: +0x25 shown,
// +0x30 its points, +0x34 the first, 16 bytes each, +0x10/+0x14 its place, +0xE4 its texture or 0). While such a bar is
// on screen the frame is held at 16:9 too, so a cutscene keeps the shape it was made for. The decision is made each
// bind's time (at most every 16 ms); the views follow from their next set-up.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace fn2 {
extern bool hud_edges;  // the setting (src/common/game_info.cpp)
}

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
bool g_held = false;        // the frame held at 16:9
float g_free = 0;           // the frame's shape while not held

bool Recent(Clock::time_point t) { return Clock::now() - t < std::chrono::milliseconds(250); }

// held unless the arena is in view and no cutscene is; only ever on a frame wider than 16:9
void Decide() {
    static bool first = true;
    if (!g_held) g_free = app::FrameAspect();
    if (first) {  // the game starts in the front end
        first = false;
        if (g_free > kWide + 1e-3f) {
            g_held = true;
            app::HoldAspect(kWide);
            LOGR("fn2: the front end: the frame held at 16:9");
        }
        return;
    }
    // into a fight: the arena for 300 ms on end (a menu's one wide view as it opens is not one); out of it: a cutscene's
    // bars or a menu's boxer, not the arena's views stopping (the pause menu draws the arena still, its camera unchanged)
    const bool cutscene = Recent(g_bar), arena = Recent(g_arena) && Clock::now() - g_since > std::chrono::milliseconds(300);
    // left: neither for a second (a cut between cameras, a load inside the fight, is shorter)
    const auto now = Clock::now();
    const bool left = now - g_arena > std::chrono::seconds(1) && now - g_shape > std::chrono::seconds(1);
    // a cutscene is 3D through the arena's cameras, widened as play is: it fills the screen too, its letterbox bars
    // (854 wide, past both edges of the 640 layout) reaching the screen's sides (GameInfo::edge_fills)
    (void)cutscene;
    const bool hold = g_free > kWide + 1e-3f && (g_held ? !arena : Recent(g_narrow) || left);
    if (hold == g_held) return;
    g_held = hold;
    app::HoldAspect(hold ? kWide : 0.0f);
    LOGR("fn2: %s", hold ? (cutscene ? "a cutscene: the frame held at 16:9" : "the front end: the frame held at 16:9")
                         : "the fight: the frame at the screen's shape");
}
}  // namespace

namespace fn2 {
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
    // the fight's 2D (HUD, round card) is laid out for 16:9 (x -107 to 747 in its 640 wide layout) and kept that shape in
    // the middle of a wider frame (GameInfo::squeeze_screen_programs: its pieces are drawn by flat matrix programs). With
    // hud_edges a piece to one side (its middle over 80 units from the centre, narrower than the layout) is moved out to
    // keep its 16:9 distance from that edge: by 427 (frame / 16:9 - 1) layout units, its place (+0x10) set for the draw
    // and put back. (State::hud_layout cannot: these pieces are drawn depth tested.)
    float moved = 0;
    const float frame = app::FrameAspect();
    if (fn2::hud_edges && frame > kWide + 1e-3f && kernel::G<uint8_t>(o + 0x25) && kernel::G<uint8_t>(o + 0x30)) {
        const uint32_t pts = kernel::R32(o + 0xE4) ? o + 0x74 - 0x40 : o + 0x34;
        float x0 = F32(pts), x1 = x0;
        for (uint32_t i = 1; i < kernel::G<uint8_t>(o + 0x30) && i < 8; i++) {
            const float x = F32(pts + i * 16);
            x0 = std::min(x0, x), x1 = std::max(x1, x);
        }
        const float mid = F32(o + 0x10) + (x0 + x1) / 2;
        if (x1 - x0 < 400.0f && std::fabs(mid - 320.0f) > 80.0f) {
            moved = 427.0f * (frame / kWide - 1.0f) * (mid < 320.0f ? -1.0f : 1.0f);
            W(o + 0x10, F32(o + 0x10) + moved);
        }
    }
    __imp__sub_00125280(ctx, base);
    if (moved != 0) W(o + 0x10, F32(o + 0x10) - moved);
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
