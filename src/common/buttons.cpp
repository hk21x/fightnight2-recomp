// Fight Night Round 2's button pictures as today's Xbox pad shows them (setting modern_buttons), or, while the keyboard
// and mouse play, the keys and mouse bound to each control.
//
// The pictures are separate DXT3 textures (format 0x0E), loaded with the front end: the prompts' A B X Y and START
// (32x32 discs: "A SELECT  B BACK  Y HELP" under every menu, "START DONE" on the name entry), the pad on Select Corner
// (128x64, the pad in its upper left 66x52) and the pad on My Corner > Options > Controls (128x128; the lines to its
// labels are the game's own draws, so each control is drawn where its line ends). When one is bound
// (GameInfo::texture_bound) with the bytes the disc has (the FNV-1a of level 0), the renderers get a picture four times
// larger in its place (gpu::SetReplacement); the guest's bytes stay as they are, so off, the replacement is dropped.
// Each bind compares the look wanted with the one drawn, so a switch between pad and keyboard shows at the next bind.
//
// Diagnostics: FN2_PICS_LOG=1 logs each texture bound (up to 256 on a side) once with its hash, and writes its picture
// beside the exe as pic_<hash>_<w>x<h>_f<format>.bgra; FN2_KEY_PICTURES=1 draws the keys whatever plays (headless);
// FN2_BUTTONS_DUMP=dir writes each picture as drawn, four times larger, as dir/<name>_<look>.pam (the reference
// renderer does not draw replacements).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "core/input.h"
#include "core/log.h"
#include "gpu/texture.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "strokes.h"

namespace fn2 {
void UltrawideTick();  // src/game/ultrawide.cpp
bool modern_buttons = true;  // the setting (src/common/game_info.cpp)

namespace {
std::mutex g_lock;
constexpr int kSharp = 4;  // the renderers' picture, times the texture's size

uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

bool EnvOn(const char* name) {
    const char* e = std::getenv(name);
    return e && *e == '1';
}

// ---- the pictures

constexpr Colour kDark{28, 28, 30, 255}, kEdge{140, 140, 146, 255}, kWhite{255, 255, 255, 255};
constexpr Colour kBody{32, 32, 36, 255}, kBodyEdge{96, 96, 102, 255}, kOutline{236, 236, 236, 255};
constexpr Colour kCapEdge{205, 205, 210, 255};
constexpr Colour kGreen{110, 200, 70, 255}, kRed{235, 70, 60, 255}, kBlue{60, 140, 235, 255}, kYellow{250, 200, 30, 255};

Area Around(float cx, float cy, float w, float h) { return {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2}; }

// a dark disc with a lighter rim
void Disc(Canvas& c, float cx, float cy, float r) {
    float rim = std::max(0.35f, r * 0.11f);
    Area a{cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1};
    Fill(c, a, kEdge, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r); });
    Fill(c, a, kDark, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r - rim); });
}

// A B X Y: the letter in the button's colour on a dark disc
void Face(Canvas& c, float cx, float cy, float r, const char* letter, Colour colour) {
    Disc(c, cx, cy, r);
    Text(c, letter, cx, cy, r * 1.05f, std::max(0.3f, r * 0.21f), colour);
}

// Menu: three lines on a dark disc
void MenuButton(Canvas& c, float cx, float cy, float r) {
    Disc(c, cx, cy, r);
    float k = r / 7;
    std::vector<Segment> lines;
    for (int i = -1; i <= 1; i++) lines.push_back({{cx - 2.8f * k, cy + i * 2.6f * k}, {cx + 2.8f * k, cy + i * 2.6f * k}});
    Fill(c, Area{cx - r, cy - r, cx + r, cy + r}, kWhite,
         [&](float x, float y) { return StrokeDistance(x, y, lines, std::max(0.2f, 0.6f * k)); });
}

// View: two overlapping squares on a dark disc
void ViewButton(Canvas& c, float cx, float cy, float r) {
    Disc(c, cx, cy, r);
    float k = r / 7;
    auto square = [&](float x0, float y0, bool front) {
        Area s{x0, y0, x0 + 4.6f * k, y0 + 4.6f * k};
        std::vector<Segment> edges = {{{s.x0, s.y0}, {s.x1, s.y0}}, {{s.x1, s.y0}, {s.x1, s.y1}},
                                      {{s.x1, s.y1}, {s.x0, s.y1}}, {{s.x0, s.y1}, {s.x0, s.y0}}};
        Area around{s.x0 - 1, s.y0 - 1, s.x1 + 1, s.y1 + 1};
        if (front) Fill(c, around, kDark, [&](float x, float y) { return BoxDistance(x, y, s, 0.4f * k, 0.4f * k); });
        Fill(c, around, kWhite, [&](float x, float y) { return StrokeDistance(x, y, edges, std::max(0.2f, 0.5f * k)); });
    };
    square(cx - 3.8f * k, cy - 3.8f * k, false);
    square(cx - 0.8f * k, cy - 0.8f * k, true);
}

// the guide button: a lit ring on a dark disc
void Guide(Canvas& c, float cx, float cy, float r) {
    Disc(c, cx, cy, r);
    Fill(c, Area{cx - r, cy - r, cx + r, cy + r}, Colour{200, 200, 204, 255},
         [&](float x, float y) { return std::abs(CircleDistance(x, y, cx, cy, r * 0.55f)) - std::max(0.2f, r * 0.1f); });
}

// a thumbstick from above: a dark cap, its dished top a ring lighter
void Stick(Canvas& c, float cx, float cy, float r) {
    Area a{cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1};
    float rim = std::max(0.35f, r * 0.1f);
    Fill(c, a, kEdge, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r); });
    Fill(c, a, kDark, [&](float x, float y) { return CircleDistance(x, y, cx, cy, r - rim); });
    Fill(c, a, Colour{78, 78, 82, 255},
         [&](float x, float y) { return std::abs(CircleDistance(x, y, cx, cy, r * 0.62f)) - rim * 0.7f; });
}

// the d-pad: a grey cross on a dark disc
void DPad(Canvas& c, float cx, float cy, float side) {
    float k = side / 20;
    Disc(c, cx, cy, side / 2);
    float w = 2.8f * k, l = side / 2 - 2.6f * k;
    Area arms[2] = {{cx - w, cy - l, cx + w, cy + l}, {cx - l, cy - w, cx + l, cy + w}};
    for (Area a : arms)
        Fill(c, a, Colour{110, 110, 114, 255}, [&](float x, float y) { return BoxDistance(x, y, a, 0.8f * k, 0.8f * k); });
}

// LT RT LB RB: a plate with its name, a trigger rounder at the top than a bumper
void Shoulder(Canvas& c, Area plate, bool trigger, const char* label) {
    float w = plate.x1 - plate.x0, h = plate.y1 - plate.y0;
    float top = trigger ? std::min(w, h) * 0.45f : h * 0.4f, bottom = trigger ? std::min(w, h) * 0.12f : h * 0.4f;
    float rim = std::max(0.3f, h * 0.1f);
    Fill(c, plate, kEdge, [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom); });
    Fill(c, plate, Colour{44, 44, 48, 255}, [&](float x, float y) { return BoxDistance(x, y, plate, top, bottom) + rim; });
    float height = std::min(h * 0.55f, (w - 2 * rim) * 0.8f / TextWidth(label, 1));
    Text(c, label, (plate.x0 + plate.x1) / 2, (plate.y0 + plate.y1) / 2, height, std::max(0.25f, height * 0.18f), kWhite);
}

// ---- the keyboard's pictures (50 Cent's and NightFire's)

constexpr uint16_t kVkLeftButton = 0x01, kVkRightButton = 0x02, kVkMiddleButton = 0x04, kVkLeft = 0x25, kVkUp = 0x26,
                   kVkRight = 0x27, kVkDown = 0x28;
bool MouseButton(uint16_t vk) { return vk == kVkLeftButton || vk == kVkRightButton || vk == kVkMiddleButton; }

// a key's name on its cap, empty for the arrows (drawn as heads) and keys it does not know
std::string KeyLabel(uint16_t vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, char(vk));
    if (vk >= 0x70 && vk <= 0x7B) return "F" + std::to_string(vk - 0x70 + 1);
    switch (vk) {
        case 0x08: return "BKSP";
        case 0x09: return "TAB";
        case 0x0D: return "ENTER";
        case 0x10: case 0xA0: case 0xA1: return "SHIFT";
        case 0x11: case 0xA2: case 0xA3: return "CTRL";
        case 0x12: case 0xA4: case 0xA5: return "ALT";
        case 0x1B: return "ESC";
        case 0x20: return "SPACE";
        case 0x2E: return "DEL";
        default: return "";
    }
}

// a mouse from above, the button named lit (none: just the mouse, for the right stick)
void Mouse(Canvas& c, Area a, uint16_t vk) {
    float h = (a.y1 - a.y0) * 0.92f, w = std::min(h * 0.64f, (a.x1 - a.x0) * 0.92f);
    float cx = (a.x0 + a.x1) / 2, cy = (a.y0 + a.y1) / 2;
    Area body{cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
    float r = w / 2, edge = std::max(0.3f, w * 0.08f), split = body.y0 + h * 0.42f, gap = edge * 0.8f;
    Fill(c, body, kCapEdge, [&](float x, float y) { return BoxDistance(x, y, body, r, r * 0.9f); });
    Fill(c, body, kDark, [&](float x, float y) { return BoxDistance(x, y, body, r, r * 0.9f) + edge; });
    auto inside = [&](float x, float y) { return BoxDistance(x, y, body, r, r * 0.9f) + edge * 1.8f; };
    if (vk == kVkLeftButton)
        Fill(c, body, kWhite, [&](float x, float y) { return std::max({inside(x, y), x - (cx - gap), y - (split - gap)}); });
    if (vk == kVkRightButton)
        Fill(c, body, kWhite, [&](float x, float y) { return std::max({inside(x, y), (cx + gap) - x, y - (split - gap)}); });
    std::vector<Segment> lines = {{{cx, body.y0 + edge}, {cx, split}}, {{body.x0 + edge, split}, {body.x1 - edge, split}}};
    Fill(c, body, kCapEdge,
         [&](float x, float y) { return std::max(StrokeDistance(x, y, lines, edge * 0.5f), -inside(x, y) - edge); });
    Area wheel{cx - w * 0.09f, body.y0 + h * 0.13f, cx + w * 0.09f, body.y0 + h * 0.31f};
    Fill(c, body, vk == kVkMiddleButton ? kWhite : Colour{95, 95, 95, 255},
         [&](float x, float y) { return BoxDistance(x, y, wheel, w * 0.09f, w * 0.09f); });
}

// a key: a dark cap with a light edge and its name, or an arrow's head; a mouse button as the mouse
void KeyCap(Canvas& c, Area cap, uint16_t vk) {
    if (MouseButton(vk)) return Mouse(c, cap, vk);
    float w = cap.x1 - cap.x0, h = cap.y1 - cap.y0, r = std::min(w, h) * 0.2f, edge = std::max(0.3f, std::min(w, h) * 0.08f);
    Fill(c, cap, kCapEdge, [&](float x, float y) { return BoxDistance(x, y, cap, r, r); });
    Fill(c, cap, kDark, [&](float x, float y) { return BoxDistance(x, y, cap, r, r) + edge; });
    float cx = (cap.x0 + cap.x1) / 2, cy = (cap.y0 + cap.y1) / 2;
    if (vk >= kVkLeft && vk <= kVkDown) {
        float k = std::min(w, h) * 0.22f;
        std::vector<Pt> head;
        if (vk == kVkUp) head = {{cx, cy - k}, {cx + k, cy + k * 0.7f}, {cx - k, cy + k * 0.7f}};
        if (vk == kVkDown) head = {{cx, cy + k}, {cx - k, cy - k * 0.7f}, {cx + k, cy - k * 0.7f}};
        if (vk == kVkLeft) head = {{cx - k, cy}, {cx + k * 0.7f, cy - k}, {cx + k * 0.7f, cy + k}};
        if (vk == kVkRight) head = {{cx + k, cy}, {cx - k * 0.7f, cy + k}, {cx - k * 0.7f, cy - k}};
        Fill(c, cap, kWhite, [&](float x, float y) { return PolygonDistance(x, y, head); });
        return;
    }
    std::string label = KeyLabel(vk);
    if (label.empty()) label = "?";
    float height = std::min(h * 0.5f, (w - 2 * edge - std::max(0.6f, w * 0.12f)) / TextWidth(label.c_str(), 1));
    Text(c, label.c_str(), cx, cy, height, std::max(0.25f, height * 0.2f), kWhite);
}

// a key centred on (cx, cy), h tall, as wide as its name needs (a square at least)
void KeyAt(Canvas& c, float cx, float cy, float h, uint16_t vk) {
    float w = h;
    std::string label = KeyLabel(vk);
    if (label.size() > 1) w = std::max(h, TextWidth(label.c_str(), h * 0.5f) + h * 0.45f);
    KeyCap(c, Around(cx, cy, w, h), vk);
}

// ---- what the pictures show

struct Look {
    bool modern = false, keys = false;
    std::array<std::array<uint16_t, 2>, input::kControls> bound{};
    bool operator==(const Look&) const = default;
};

Look Current() {
    static const bool force = EnvOn("FN2_KEY_PICTURES");
    // the test pad (FN2_PAD_FILE) is a controller's: its presses keep the pad's pictures
    static const bool test_pad = [] {
        const char* e = std::getenv("FN2_PAD_FILE");
        return e && *e;
    }();
    Look l;
    l.keys = force || (!test_pad && input::UsingKeyboard());
    if (!l.keys) {
        l.modern = modern_buttons;
        return l;
    }
    for (int c = 0; c < input::kControls; c++) l.bound[size_t(c)] = input::ControlKeys(c);
    return l;
}

// a control's key: its first binding, the second when the first has none. A and B have the menus' keys first (Enter,
// Backspace) and the fight's second: `fight` takes the second
uint16_t KeyOf(const Look& l, int control, bool fight = false) {
    const auto& k = l.bound[size_t(control)];
    if (fight && k[1] && (control == input::kA || control == input::kB)) return k[1];
    return k[0] ? k[0] : k[1];
}

// ---- the textures

enum class Kind { kA, kB, kX, kY, kStart, kSmallPad, kControls };
struct Known {
    uint64_t hash;
    uint32_t width, height;
    Kind kind;
    const char* name;
};
// level 0's FNV-1a as the disc has it (FN2_PICS_LOG), all DXT3
constexpr Known kKnown[] = {
    {0x3FE35FFAD85312F7ull, 32, 32, Kind::kA, "a"},
    {0x8500D44AC7FFAF90ull, 32, 32, Kind::kB, "b"},
    {0x10A5576D6F685C09ull, 32, 32, Kind::kX, "x"},
    {0x390E4184C9DF499Full, 32, 32, Kind::kY, "y"},
    {0xA74476B7D34E7289ull, 32, 32, Kind::kStart, "start"},
    {0x9C49F2EC10204142ull, 128, 64, Kind::kSmallPad, "pad_small"},
    {0x76102222C41AB526ull, 128, 128, Kind::kControls, "pad_controls"},
};

// a prompt's disc: as today's pad has it, or the key (the menus' key for A and B)
void Prompt(Canvas& c, Kind kind, const Look& look) {
    constexpr float kCx = 16, kCy = 16, kR = 12.5f;  // the disc's own disc, measured
    if (look.keys) {
        int control = kind == Kind::kA ? input::kA : kind == Kind::kB ? input::kB : kind == Kind::kX ? input::kX
                    : kind == Kind::kY ? input::kY : input::kStart;
        uint16_t vk = KeyOf(look, control);
        float w = KeyLabel(vk).size() > 1 ? 31 : 24;
        KeyCap(c, Around(kCx, kCy, w, 22), vk);
        return;
    }
    switch (kind) {
        case Kind::kA: Face(c, kCx, kCy, kR, "A", kGreen); break;
        case Kind::kB: Face(c, kCx, kCy, kR, "B", kRed); break;
        case Kind::kX: Face(c, kCx, kCy, kR, "X", kBlue); break;
        case Kind::kY: Face(c, kCx, kCy, kR, "Y", kYellow); break;
        default: MenuButton(c, kCx, kCy, kR); break;
    }
}

// today's pad from the front, filling `box` (Select Corner's small picture), outlined white as the disc's is
void SmallPad(Canvas& c, Area box) {
    float w = box.x1 - box.x0, h = box.y1 - box.y0;
    auto X = [&](float f) { return box.x0 + f * w; };
    auto Y = [&](float f) { return box.y0 + f * h; };
    Area top{X(0.04f), Y(0.10f), X(0.96f), Y(0.62f)};
    const std::vector<Segment> grip_l = {{{X(0.24f), Y(0.50f)}, {X(0.15f), Y(0.80f)}}},
                               grip_r = {{{X(0.76f), Y(0.50f)}, {X(0.85f), Y(0.80f)}}};
    const float gw = h * 0.17f;
    auto body = [&](float x, float y, float grow) {
        auto g = [&](Area a) { return Area{a.x0 - grow, a.y0 - grow, a.x1 + grow, a.y1 + grow}; };
        float d = BoxDistance(x, y, g(top), h * 0.24f, h * 0.14f);
        d = std::min(d, StrokeDistance(x, y, grip_l, gw + grow));
        return std::min(d, StrokeDistance(x, y, grip_r, gw + grow));
    };
    Area all{box.x0 - 2, box.y0 - 2, box.x1 + 2, box.y1 + 2};
    Fill(c, all, kOutline, [&](float x, float y) { return body(x, y, 1.2f); });
    Fill(c, all, kBodyEdge, [&](float x, float y) { return body(x, y, 0); });
    Fill(c, all, kBody, [&](float x, float y) { return body(x, y, -0.8f); });
    float r = h * 0.11f;
    Stick(c, X(0.26f), Y(0.36f), r);
    DPad(c, X(0.38f), Y(0.60f), r * 1.9f);
    Stick(c, X(0.62f), Y(0.60f), r);
    // the face buttons too small for letters: coloured dots on dark discs
    float f = h * 0.065f, fx = X(0.76f), fy = Y(0.36f), d = f * 1.75f;
    for (auto [px, py, col] : {std::tuple{fx, fy - d, kYellow}, {fx - d, fy, kBlue}, {fx + d, fy, kRed}, {fx, fy + d, kGreen}}) {
        Disc(c, px, py, f);
        Fill(c, Area{px - f, py - f, px + f, py + f}, col, [&](float x, float y) { return CircleDistance(x, y, px, py, f * 0.6f); });
    }
    Guide(c, X(0.5f), Y(0.26f), h * 0.075f);
    ViewButton(c, X(0.43f), Y(0.40f), h * 0.05f);
    MenuButton(c, X(0.57f), Y(0.40f), h * 0.05f);
}

// the keyboard and mouse, small (Select Corner while the keyboard plays)
void SmallKeys(Canvas& c, Area box) {
    float w = box.x1 - box.x0, h = box.y1 - box.y0;
    Area board{box.x0 + w * 0.02f, box.y0 + h * 0.22f, box.x0 + w * 0.70f, box.y0 + h * 0.86f};
    Fill(c, Area{board.x0 - 2, board.y0 - 2, board.x1 + 2, board.y1 + 2}, kOutline,
         [&](float x, float y) { return BoxDistance(x, y, board, 3, 3) - 1.2f; });
    Fill(c, board, kBodyEdge, [&](float x, float y) { return BoxDistance(x, y, board, 3, 3); });
    Fill(c, board, kBody, [&](float x, float y) { return BoxDistance(x, y, board, 3, 3) + 0.8f; });
    const float bw = board.x1 - board.x0, bh = board.y1 - board.y0, kx = bw / 11.0f, ky = bh / 4.6f;
    constexpr Colour kKey{170, 170, 176, 255};
    for (int row = 0; row < 3; row++)
        for (int i = 0; i < 10 - row; i++) {
            Area k{board.x0 + kx * (0.5f + i + row * 0.4f), board.y0 + ky * (0.4f + row), 0, 0};
            k.x1 = k.x0 + kx * 0.82f;
            k.y1 = k.y0 + ky * 0.8f;
            Fill(c, k, kKey, [&](float x, float y) { return BoxDistance(x, y, k, 0.6f, 0.6f); });
        }
    Area space{board.x0 + kx * 2.5f, board.y0 + ky * 3.45f, board.x0 + kx * 8.0f, board.y0 + ky * 4.2f};
    Fill(c, space, kKey, [&](float x, float y) { return BoxDistance(x, y, space, 0.6f, 0.6f); });
    Area mouse{box.x0 + w * 0.76f, box.y0 + h * 0.20f, box.x0 + w * 0.98f, box.y0 + h * 0.90f};
    const float mr = (mouse.x1 - mouse.x0) / 2;
    Fill(c, Area{mouse.x0 - 2, mouse.y0 - 2, mouse.x1 + 2, mouse.y1 + 2}, kOutline,
         [&](float x, float y) { return BoxDistance(x, y, mouse, mr, mr * 0.9f) - 1.2f; });
    Mouse(c, mouse, 0);
}

// Options > Controls: today's pad with each control where the console pad's had its line (the labels and lines are the
// game's, drawn over the texture; measured on the 1920x1080 frame, a texel 3.2 pixels across and 3.1 down from
// (758, 370)): LT (16,5), RT (63,0), Y (85,8), X (78,18), B (98,23), A (91,31), White (107,40), Black (100,47), the left
// stick (10,27), the right stick (80,39), START (12,56), the d-pad (40,47)
void ControlsPad(Canvas& c) {
    // a rounded top and two grips angled down and out (capsules)
    Area top{3, 9, 117, 62};
    const std::vector<Segment> grip_l = {{{24, 48}, {14, 94}}}, grip_r = {{{88, 48}, {100, 92}}};
    auto body = [&](float x, float y, float grow) {
        auto g = [&](Area a) { return Area{a.x0 - grow, a.y0 - grow, a.x1 + grow, a.y1 + grow}; };
        float d = BoxDistance(x, y, g(top), 22, 14);
        d = std::min(d, StrokeDistance(x, y, grip_l, 15 + grow));
        return std::min(d, StrokeDistance(x, y, grip_r, 15 + grow));
    };
    Shoulder(c, Area{11, 1, 31, 10}, true, "LT");
    Shoulder(c, Area{54, 0, 74, 9}, true, "RT");
    Area all{0, 0, 128, 128};
    Fill(c, all, kBodyEdge, [&](float x, float y) { return body(x, y, 0); });
    Fill(c, all, kBody, [&](float x, float y) { return body(x, y, -1.4f); });
    Stick(c, 15, 30, 8.5f);
    DPad(c, 43, 49, 16);
    Stick(c, 76, 46, 8.5f);
    Face(c, 87, 13, 5, "Y", kYellow);
    Face(c, 79, 21, 5, "X", kBlue);
    Face(c, 95, 21, 5, "B", kRed);
    Face(c, 87, 29, 5, "A", kGreen);
    Guide(c, 51, 21, 5.5f);
    ViewButton(c, 41, 33, 3.6f);
    MenuButton(c, 15, 54, 4);
    // White and Black are LB and RB on today's pad (the SDK plays them so): small plates where their lines end
    Shoulder(c, Area{98, 36, 112, 44}, false, "LB");
    Shoulder(c, Area{93, 45, 107, 53}, false, "RB");
}

// the same places as keys: what plays each control in a fight (configuration 1's labels)
void ControlsKeys(Canvas& c, const Look& l) {
    const float h = 8;
    KeyAt(c, 21, 5, h, KeyOf(l, input::kLT));
    KeyAt(c, 64, 5, h, KeyOf(l, input::kRT));
    KeyAt(c, 87, 12, h, KeyOf(l, input::kY));
    KeyAt(c, 78, 21, h, KeyOf(l, input::kX));
    KeyAt(c, 96, 21, h, KeyOf(l, input::kB, true));
    KeyAt(c, 87, 30, h, KeyOf(l, input::kA, true));
    KeyAt(c, 105, 40, h, KeyOf(l, input::kWhite));
    KeyAt(c, 100, 50, h, KeyOf(l, input::kBlack));
    KeyAt(c, 15, 57, h, KeyOf(l, input::kStart));
    // movement: the left stick's four keys round where it was
    KeyAt(c, 15, 22, 7, KeyOf(l, input::kLSUp));
    KeyAt(c, 7, 30, 7, KeyOf(l, input::kLSLeft));
    KeyAt(c, 15, 30, 7, KeyOf(l, input::kLSDown));
    KeyAt(c, 23, 30, 7, KeyOf(l, input::kLSRight));
    // the d-pad's arrows
    KeyAt(c, 43, 42, 7, KeyOf(l, input::kUp));
    KeyAt(c, 35, 50, 7, KeyOf(l, input::kLeft));
    KeyAt(c, 43, 50, 7, KeyOf(l, input::kDown));
    KeyAt(c, 51, 50, 7, KeyOf(l, input::kRight));
    // Total Punch Control: the mouse (as the right stick), its keys below
    Mouse(c, Around(72, 46, 12, 17), 0);
    KeyAt(c, 87, 56, 6, KeyOf(l, input::kRSUp));
    KeyAt(c, 81, 62, 6, KeyOf(l, input::kRSLeft));
    KeyAt(c, 87, 62, 6, KeyOf(l, input::kRSDown));
    KeyAt(c, 93, 62, 6, KeyOf(l, input::kRSRight));
    Text(c, "KEYBOARD + MOUSE", 60, 90, 5, 0.9f, Colour{60, 60, 64, 255});
}

std::vector<uint32_t> Draw(Kind kind, const Look& look, uint32_t width, uint32_t height, int scale) {
    Canvas c;
    c.scale = scale;
    c.size = int(std::max(width, height)) * scale;
    c.px.assign(size_t(c.size) * c.size, {0, 0, 0, 0});
    switch (kind) {
        case Kind::kSmallPad:
            if (look.keys) SmallKeys(c, Area{2, 1, 65, 51});
            else SmallPad(c, Area{2, 1, 65, 51});
            break;
        case Kind::kControls:
            if (look.keys) ControlsKeys(c, look);
            else ControlsPad(c);
            break;
        default: Prompt(c, kind, look); break;
    }
    // width x height from the square canvas's top left
    const uint32_t w = width * uint32_t(scale), h = height * uint32_t(scale);
    std::vector<uint32_t> out(size_t(w) * h);
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            const auto& p = c.px[size_t(y) * c.size + x];
            float a = p[3];
            auto ch = [&](float v) { return uint32_t(std::clamp(a > 0 ? v / a : 0.0f, 0.0f, 1.0f) * 255 + 0.5f); };
            out[size_t(y) * w + x] = uint32_t(std::clamp(a, 0.0f, 1.0f) * 255 + 0.5f) << 24 | ch(p[0]) << 16 | ch(p[1]) << 8 | ch(p[2]);
        }
    return out;
}

void Dump(const Known& k, const Look& look, const std::vector<uint32_t>& bgra) {
    const char* dir = std::getenv("FN2_BUTTONS_DUMP");
    if (!dir || !*dir) return;
    const uint32_t w = k.width * kSharp, h = k.height * kSharp;
    std::string path = std::string(dir) + "/" + k.name + (look.keys ? "_keys" : "_modern") + ".pam";
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fprintf(f, "P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", w, h);
        for (uint32_t q : bgra) {
            const uint8_t px[4] = {uint8_t(q >> 16), uint8_t(q >> 8), uint8_t(q), uint8_t(q >> 24)};
            std::fwrite(px, 1, 4, f);
        }
        std::fclose(f);
    }
}

struct Texture {
    const Known* known = nullptr;
    uint32_t data = 0, bytes = 0;
    uint64_t head = 0;  // the first 64 bytes' FNV-1a: other bytes there now, it is not ours any more
    Look drawn;         // neither modern nor keys: the disc's, no replacement
    struct Drawing {
        Look look;
        std::vector<uint32_t> bgra;
    };
    std::vector<Drawing> cache;  // the newest last, a few
};
std::map<uint32_t, Texture> g_textures;  // by the texels' address
// textures of our formats and sizes found not to be ours, by address: their first bytes' hash then (hashed whole again
// only once those change; the fight binds dozens of 128x128 DXT3 textures each frame)
std::map<uint32_t, uint64_t> g_rejected;

void Apply(Texture& t, const Look& look) {
    if (!look.modern && !look.keys) {
        gpu::ClearReplacement(t.data);
    } else {
        auto d = std::find_if(t.cache.begin(), t.cache.end(), [&](const Texture::Drawing& x) { return x.look == look; });
        if (d == t.cache.end()) {
            t.cache.push_back({look, Draw(t.known->kind, look, t.known->width, t.known->height, kSharp)});
            if (t.cache.size() > 3) t.cache.erase(t.cache.begin());
            d = t.cache.end() - 1;
            Dump(*t.known, look, d->bgra);
        }
        gpu::SetReplacement(t.data, t.bytes, t.known->width * kSharp, t.known->height * kSharp, d->bgra);
    }
    t.drawn = look;
    LOGI("fn2: button picture %s (%08X) drawn as %s", t.known->name, t.data,
         look.keys ? "the keyboard's" : look.modern ? "today's pad" : "the disc has it");
}

void Discover(const gpu::TextureDesc& d, uint64_t hash) {
    static std::set<uint64_t> seen;
    if (!seen.insert(hash).second) return;
    std::vector<uint32_t> bgra;
    const bool decoded = gpu::DecodeBgra(d, 0, static_cast<const uint8_t*>(kernel::HostPtr(d.data)), bgra);
    LOGR("fn2: texture %ux%u format %X at %08X hash %016llX%s", d.width, d.height, d.format, d.data,
         (unsigned long long)hash, decoded ? "" : " (not decoded)");
    if (!decoded) return;
    char name[96];
    std::snprintf(name, sizeof name, "pic_%016llX_%ux%u_f%X.bgra", (unsigned long long)hash, d.width, d.height, d.format);
    if (FILE* f = std::fopen(name, "wb")) {
        std::fwrite(bgra.data(), 4, bgra.size(), f);
        std::fclose(f);
    }
}
}  // namespace

// GameInfo::texture_bound
bool ButtonPictures(uint32_t header, uint32_t /*palette*/) {
    UltrawideTick();
    static const bool log = EnvOn("FN2_PICS_LOG");
    gpu::TextureDesc d;
    if (!gpu::Describe(header, d) || d.cube || d.depth > 1 || d.width > 256 || d.height > 256) return false;
    const bool ours = d.format == 0x0E && ((d.width == 32 && d.height == 32) || (d.width == 128 && (d.height == 64 || d.height == 128)));
    if (!ours && !log) return false;
    const uint32_t bytes = gpu::LevelSize(d, 0);
    if (!bytes || !mem::IsCommitted(d.data) || !mem::IsCommitted(d.data + bytes - 1)) return false;
    const auto* texels = static_cast<const uint8_t*>(kernel::HostPtr(d.data));
    std::lock_guard<std::mutex> hold(g_lock);
    if (log) Discover(d, Fnv(texels, bytes));
    if (!ours) return false;
    const Look look = Current();
    const uint64_t head = Fnv(texels, std::min<uint32_t>(bytes, 64));
    auto it = g_textures.find(d.data);
    if (it != g_textures.end() && (it->second.head != head || it->second.bytes != bytes)) {
        gpu::ClearReplacement(d.data);  // the memory holds another texture now
        g_textures.erase(it);
        it = g_textures.end();
    }
    if (it == g_textures.end()) {
        if (auto r = g_rejected.find(d.data); r != g_rejected.end() && r->second == head) return false;
        const uint64_t hash = Fnv(texels, bytes);
        const Known* known = nullptr;
        for (const Known& k : kKnown)
            if (k.hash == hash && k.width == d.width && k.height == d.height) known = &k;
        if (!known) {
            if (g_rejected.size() > 4096) g_rejected.clear();
            g_rejected[d.data] = head;
            return false;
        }
        g_rejected.erase(d.data);
        Texture t;
        t.known = known;
        t.data = d.data;
        t.bytes = bytes;
        t.head = head;
        it = g_textures.emplace(d.data, std::move(t)).first;
    }
    if (look == it->second.drawn) return false;
    Apply(it->second, look);
    return true;
}

}  // namespace fn2
