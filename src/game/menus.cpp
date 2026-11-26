// The game's APT menus edited as they load: every front end file comes through the RefPack decompressor 0x2301D0
// (source, destination; returns the size) into a BIGF archive of one screen (NAME.apt, NAME.const, ...). A screen we
// change is found there by its entry's name and size and a run of its bytecode, and patched before it ever runs.
// Bytecode offsets are the file's (.local/apt/aptdis.py disassembles them): Flash AVM1 with EA's opcodes; a branch's
// offset counts from the end of the instruction, its 4 bytes aligned.
//
// Quit Game (the title menu, screensfe/mainmenu.apt): the PAL menu has five rows ($1000 $1001 $1003 $1004 $1005); the
// widget takes six (NTSC's sixth is Play Online). The demo's row "$8" (the string table's QUIT, in each language)
// becomes the sixth row; its input goes to goPlayOnline, as the GameCube's mapping has it (PAL maps none: its
// GC test is made to take the GameCube's rows); goPlayOnline's online hardware test is made to pass, so its own
// popup asks (the popup's title and question in mainmenu.const rewritten); its YES runs goOnline, whose first call is
// callGF("FEA_InitEasoApt"), not in this release's function table: on the title menu that call quits the game.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace fn2 {
std::string Screen();                           // screens.cpp
void AchievementsEvent(const std::string& name);  // achievements.cpp
}

namespace {
uint32_t BE(uint32_t va) {
    const uint32_t v = kernel::R32(va);
    return __builtin_bswap32(v);
}

struct Patch {
    uint32_t at;
    std::initializer_list<uint8_t> was, now;
};
// the title menu: file size 205613
const Patch kTitleMenu[] = {
    // menuarray[4] = $8 if DEMO_VERSION else $1005  ->  menuarray[4] = $1005; menuarray[5] = $8; branch on
    {0x312e6,
     {0xae, 0x4c, 0x5a, 0x49, 0x12, 0x9d, 0x0c, 0x00, 0x00, 0x00, 0xae, 0x45, 0xb5, 0x04, 0xa2, 0x4d, 0x4f, 0x99, 0x07,
      0x00, 0x00, 0x00, 0xae, 0x45, 0xb5, 0x04, 0xa2, 0x4e, 0x4f},
     {0xae, 0x45, 0xb5, 0x04, 0xa2, 0x4e, 0x4f, 0xae, 0x45, 0xb5, 0x05, 0xa2, 0x4d, 0x4f, 0x99, 0x00, 0x00, 0x00, 0x07,
      0x00, 0x00, 0x00, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17}},  // the branch skips the last 7 (Pop: the stream walkable)
    // the sixth row's strip: ExtraTapeM._visible = false -> 1, where the PAL and Xbox tests hide it
    {0x310f8, {0xae, 0x21, 0xa2, 0x22, 0x74, 0x4f}, {0xae, 0x21, 0xa2, 0x22, 0x5a, 0x4f}},
    {0x31164, {0xae, 0x21, 0xa2, 0x22, 0x74, 0x4f}, {0xae, 0x21, 0xa2, 0x22, 0x5a, 0x4f}},
    {0x310fe, {0xae, 0x23, 0xa2, 0x22, 0x74, 0x4f}, {0xae, 0x23, 0xa2, 0x22, 0x5a, 0x4f}},
    {0x3116a, {0xae, 0x23, 0xa2, 0x22, 0x74, 0x4f}, {0xae, 0x23, 0xa2, 0x22, 0x5a, 0x4f}},
    // the input mapping: PLATFORM == GC test -> taken (pop it; branch to the next instruction)
    {0x3134a, {0x9d, 0x00, 0x5c, 0x00, 0x00, 0x00}, {0x17, 0x99, 0x00, 0x00, 0x00, 0x00}},
    // goPlayOnline: bHaveHardware == 1 test -> taken
    {0x315e5, {0x9d, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00}, {0x17, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

// Options (screensfe/options.apt, file size 135960): "X PC SETTINGS" in its button footer. The footer's buttons are set
// by its sprite's init script (0x21028: m_crossBtn = "B,10,$19", ...); m_circleBtn (the X button: FN2 shows the PS2
// names' circle as X, triangle as B, square as Y) was ''. Its value is now the constant m_rightAnalogBtn, rewritten to
// the spec "B,40,PC SETTINGS" (the same length; the right stick's button stays unset). The press is ours (PcSettingsKey).
// A sixth menu row was tried first: shown, but Options' menu has no sixth row to move to.
struct PatchV {
    uint32_t at;
    std::vector<uint8_t> was, now;
};
const PatchV kOptions[] = {
    {0x21048, {0xa2, 0x08, 0xa2, 0x01, 0x1d}, {0xa2, 0x08, 0xa2, 0x15, 0x1d}},
};

bool ApplyV(uint32_t file, const PatchV* patches, size_t n, const char* what) {
    for (size_t i = 0; i < n; i++)
        for (size_t k = 0; k < patches[i].was.size(); k++)
            if (kernel::G<uint8_t>(file + patches[i].at + uint32_t(k)) != patches[i].was[k]) {
                LOGR("fn2: %s not patched: the bytes at %X are not as expected", what, patches[i].at);
                return false;
            }
    for (size_t i = 0; i < n; i++)
        for (size_t k = 0; k < patches[i].now.size(); k++) kernel::G<uint8_t>(file + patches[i].at + uint32_t(k)) = patches[i].now[k];
    return true;
}

bool Apply(uint32_t file, const Patch* patches, size_t n, const char* what) {
    for (size_t i = 0; i < n; i++) {
        size_t k = 0;
        for (uint8_t b : patches[i].was)
            if (kernel::G<uint8_t>(file + patches[i].at + uint32_t(k++)) != b) {
                LOGR("fn2: %s not patched: the bytes at %X are not as expected", what, patches[i].at);
                return false;
            }
    }
    for (size_t i = 0; i < n; i++) {
        size_t k = 0;
        for (uint8_t b : patches[i].now) kernel::G<uint8_t>(file + patches[i].at + uint32_t(k++)) = b;
    }
    return true;
}

// a NUL-ended string in a .const file replaced in place by one no longer
void ReplaceText(uint32_t file, uint32_t size, const char* from, const char* to) {
    const size_t n = std::strlen(from) + 1;
    const uint8_t* p = static_cast<const uint8_t*>(kernel::HostPtr(file));
    for (uint32_t i = 0; i + n <= size; i++)
        if (!std::memcmp(p + i, from, n)) {
            std::memset(kernel::HostPtr(file + i), 0, n);
            std::memcpy(kernel::HostPtr(file + i), to, std::strlen(to));
            return;
        }
    LOGR("fn2: \"%s\" not found to replace", from);
}

void Edit(uint32_t big, uint32_t size) {
    if (size < 16 || kernel::R32(big) != 0x46474942) return;  // "BIGF"
    const uint32_t count = BE(big + 8);
    uint32_t e = big + 16;
    uint32_t apt = 0, apt_size = 0, cst = 0, cst_size = 0, opt = 0, opt_size = 0, opt_cst = 0, opt_cst_size = 0;
    for (uint32_t i = 0; i < count && i < 64; i++) {
        const uint32_t off = BE(e), len = BE(e + 4);
        const std::string name = kernel::ReadCString(e + 8, 64);
        e += 8 + uint32_t(name.size()) + 1;
        if (name == "mainmenu.apt") apt = big + off, apt_size = len;
        if (name == "mainmenu.const") cst = big + off, cst_size = len;
        if (name == "options.apt") opt = big + off, opt_size = len;
        if (name == "options.const") opt_cst = big + off, opt_cst_size = len;
    }
    if (apt && apt_size == 205613 && cst) {
        if (Apply(apt, kTitleMenu, std::size(kTitleMenu), "the title menu")) {
            ReplaceText(cst, cst_size, "WARNING!", "QUIT");
            ReplaceText(cst, cst_size, "UNSAVED DATA WILL BE LOST. WOULD YOU LIKE TO CONTINUE?",
                        "UNSAVED PROGRESS WILL BE LOST. QUIT TO WINDOWS?");
            LOGR("fn2: the title menu has Quit Game");
        }
    }
    if (opt && opt_size == 135960 && opt_cst && ApplyV(opt, kOptions, std::size(kOptions), "Options")) {
        ReplaceText(opt_cst, opt_cst_size, "m_rightAnalogBtn", "B,40,PC SETUP");
        LOGR("fn2: Options has X PC SETTINGS");
    }
}
}  // namespace

namespace fn2 {
bool InFight();  // ultrawide.cpp
bool FightMenuShown();  // src/common/buttons.cpp
// X on Options (its footer says PC SETUP) and on a fight's pause menu: the settings window, the press kept from the game
void PcSettingsKey(input::Pad& pad) {
    static bool was = false;
    const int x = input::kX - input::kA;
    const bool now = pad.analog[x] > 128;
    const bool paused = FightMenuShown(), options = !paused && !InFight() && Screen() == "OPTIONS";
    if (now && !was && (options || paused) && !app::OverlayOpen()) {
        LOGR("fn2: PC SETTINGS (X on %s)", options ? "Options" : "the pause menu");
        app::ToggleOverlay();
    }
    was = now;
    if (options || paused) pad.analog[x] = 0;
}
}  // namespace fn2

// RefPack: (source, destination), the size out
X86_EXTERN_FUNC(__imp__sub_002301D0);
X86_FUNC(sub_002301D0) {
    const uint32_t dst = kernel::R32(ctx.esp + 8);
    __imp__sub_002301D0(ctx, base);
    if (ctx.eax) Edit(dst, ctx.eax);
}

// bsearch (key, base, count, width, compare), cdecl: the APT function table's lookup by name (0xCC650's callGF)
X86_EXTERN_FUNC(__imp__sub_00260D54);
X86_FUNC(sub_00260D54) {
    if (kernel::R32(ctx.esp + 8) == 0x38F3C8) {
        const uint32_t key = kernel::R32(ctx.esp + 4);
        const std::string name = kernel::ReadCString(kernel::R32(key), 64);
        fn2::AchievementsEvent(name);
        if (name == "FEA_InitEasoApt" && fn2::Screen() == "MAINMENU") {
            LOGR("fn2: Quit Game (the title menu)");
            app::Quit();
        }
    }
    __imp__sub_00260D54(ctx, base);
}
