// Ultrawide: the game's 3D views at the shape the frame is shown at.
//
// Every 3D view's projection is built by 0x213400 (thiscall on the camera: the horizontal field of view in degrees,
// the aspect, near, far), which also builds the camera's culling planes from the same two (+0x158..+0x174). In its
// widescreen mode (the console reporting a widescreen TV) the views' set-up 0x133040 widens the 4:3 field of view by
// 4/3 in its tangent (DAT 0x2EA514) and passes 16:9 (0x3FE38E39); the 4:3 views (0x12B510, 0x12AD10, 0x2138C0,
// 0x132EC0: pictures drawn into textures) pass 4/3. A view asked at 16:9 is given the frame's shape instead, its field
// of view widened the same way (tan(fov/2) times frame / (16:9)), so a frame wider than 16:9 shows more to the sides
// instead of stretching, and the culling widens with it. The 2D (menus, HUD) is laid out for 16:9 and kept so in the
// middle by the renderers.
//
// Cutscenes: the in-engine ones (the walk-ins, "Tonight's Fight", the corners between rounds, replays) are letterboxed
// by four untextured shapes 854 wide (480 x 16:9, from x -107 in the game's 640x480 layout), 33.5 high, sliding in
// from above and below; the shapes go through the 2D shape drawer 0x125280 (fastcall, the shape in ecx: +0x25 shown,
// +0x30 its points, +0x34 the first, 16 bytes each, +0x10/+0x14 its place, +0xE4 its texture or 0). While such a bar is
// on screen the frame is held at 16:9 (app::HoldAspect), its sides black, so a cutscene keeps the shape it was made
// for on a wider screen; the views follow from their next set-up, one a frame. The movies are 2D quads narrower than
// the frame and stay at their own shape in the middle.
#include <algorithm>
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
Clock::time_point g_bar;   // a letterbox bar last drawn
bool g_held = false;       // the frame held at 16:9
float g_free = 0;          // the frame's shape while not held

// a cutscene: a letterbox bar drawn in the last quarter second; held only on a frame wider than 16:9
void Cutscene(bool now) {
    if (!g_held) g_free = app::FrameAspect();
    const bool hold = now && g_free > kWide + 1e-3f;
    if (hold == g_held) return;
    g_held = hold;
    app::HoldAspect(hold ? kWide : 0.0f);
    LOGR("fn2: cutscene %s", hold ? "begins: the frame held at 16:9" : "ends: the frame at the screen's shape");
}
bool BarRecent() { return Clock::now() - g_bar < std::chrono::milliseconds(250); }
}  // namespace

// the 2D shape drawer (ecx the shape)
X86_EXTERN_FUNC(__imp__sub_00125280);
X86_FUNC(sub_00125280) {
    const uint32_t o = ctx.ecx;
    if (kernel::G<uint8_t>(o + 0x25) && kernel::G<uint8_t>(o + 0x30) == 4 && kernel::R32(o + 0xE4) == 0) {
        float x0 = F32(o + 0x34), x1 = x0, y0 = F32(o + 0x38), y1 = y0;
        for (uint32_t i = 1; i < 4; i++) {
            const float x = F32(o + 0x34 + i * 16), y = F32(o + 0x38 + i * 16);
            x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
        }
        const float top = F32(o + 0x14) + y0, bottom = F32(o + 0x14) + y1;
        // wider than the 4:3 layout by far, a strip, at least partly on screen
        if (x1 - x0 > 800.0f && y1 - y0 < 120.0f && bottom > 0.5f && top < 479.5f) {
            g_bar = Clock::now();
            Cutscene(true);
        }
    }
    __imp__sub_00125280(ctx, base);
}

// (camera; fov degrees, aspect, near, far), ret 0x10
X86_EXTERN_FUNC(__imp__sub_00213400);
X86_FUNC(sub_00213400) {
    constexpr float kPi = 3.14159265f;
    Cutscene(BarRecent());
    const float aspect = F32(ctx.esp + 8), frame = app::FrameAspect();
    if (std::fabs(aspect - kWide) < 1e-3f && std::fabs(frame - kWide) > 1e-3f) {
        const float fov = F32(ctx.esp + 4);
        const float wider = 2.0f * std::atan(std::tan(fov * kPi / 360.0f) * frame / kWide) * 180.0f / kPi;
        W(ctx.esp + 4, wider);
        W(ctx.esp + 8, frame);
        static float said = 0;
        if (std::fabs(said - frame) > 1e-3f) {
            said = frame;
            LOGR("fn2: 3D views at %.3f:1 (a 16:9 view of %.1f degrees across shown at %.1f)", frame, fov, wider);
        }
    }
    __imp__sub_00213400(ctx, base);
}
