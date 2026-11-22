// FN2_PAD_FILE=path (tests, on Windows too: GPU-VM's injected keys do not reach the game): controller 1 holds what the
// file names while it names them, read again every 6 polls. Words: A B X Y BLACK WHITE LT RT UP DOWN LEFT RIGHT START
// BACK LS RS (stick clicks), LSL LSR LSU LSD and RSL RSR RSU RSD (a stick pushed fully; Fight Night punches with the
// right stick). SHOT takes a screenshot (as F12, Windows only) each time it appears in the file.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/app.h"
#include "core/input.h"

namespace fn2 {

void MenuMouse(input::Pad& pad);  // menu_mouse.cpp

void TestPad(int port, input::Pad& pad) {
    if (port == 0) MenuMouse(pad);
    static const char* path = std::getenv("FN2_PAD_FILE");
    if (!path || !*path || port != 0) return;
    static int every = 0;
    static uint16_t buttons = 0;
    static uint8_t analog[8] = {};
    static int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    static bool shot = false;
    if (every++ % 6 == 0) {
        buttons = 0, lx = ly = rx = ry = 0;
        std::fill(std::begin(analog), std::end(analog), uint8_t(0));
        bool shot_now = false;
        if (std::FILE* f = std::fopen(path, "r")) {
            char w[16];
            while (std::fscanf(f, "%15s", w) == 1) {
                const std::string k = w;
                static const std::pair<const char*, int> kNames[] = {
                    {"UP", input::kUp},     {"DOWN", input::kDown}, {"LEFT", input::kLeft}, {"RIGHT", input::kRight},
                    {"START", input::kStart}, {"BACK", input::kBack}, {"LS", input::kLThumb}, {"RS", input::kRThumb},
                    {"A", input::kA},       {"B", input::kB},       {"X", input::kX},       {"Y", input::kY},
                    {"BLACK", input::kBlack}, {"WHITE", input::kWhite}, {"LT", input::kLT}, {"RT", input::kRT}};
                for (const auto& [name, c] : kNames)
                    if (k == name) {
                        if (c < input::kA) buttons |= uint16_t(1u << c);
                        else analog[c - input::kA] = 255;
                    }
                if (k == "LSL") lx = -32767;
                if (k == "LSR") lx = 32767;
                if (k == "LSU") ly = 32767;
                if (k == "LSD") ly = -32767;
                if (k == "RSL") rx = -32767;
                if (k == "RSR") rx = 32767;
                if (k == "RSU") ry = 32767;
                if (k == "RSD") ry = -32767;
                if (k == "SHOT") shot_now = true;
            }
            std::fclose(f);
        }
#ifdef _WIN32  // headless has REF pictures and the snap file instead
        if (shot_now && !shot) app::RequestScreenshot();
#endif
        shot = shot_now;
    }
    pad.connected = true;
    pad.buttons |= buttons;
    for (int i = 0; i < 8; i++) pad.analog[i] = std::max(pad.analog[i], analog[i]);
    if (lx) pad.lx = lx;
    if (ly) pad.ly = ly;
    if (rx) pad.rx = rx;
    if (ry) pad.ry = ry;
}

}  // namespace fn2
