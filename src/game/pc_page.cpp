// PC Settings as a page of the game's own menus: X on Options (its footer says X PC SETUP) opens the game's Settings
// screen (screensfe/settings) in a PC mode, its rows the PC settings, in the game's own style; B backs out, START saves.
//
// The screen's rows are option objects, each made by its own init script from constants of settings.const: its label
// ("$621", a string id) and its choices ("4 $622" ... ; the on/off rows each have their own "$2" OFF and "$1" ON). The
// const table keeps file offsets once loaded, read at each use: an item pointed (offset = target - the file's base) at
// a string of ours is shown as written, in the row's own font. In PC mode the rows' labels and choices are pointed at
// ours; out of it, back at the game's.
//
// Values: the screen's initGUI calls FEA_LoadSettings (0xC60D0), which adds each value to its reply by name: 0xDD0E0
// names the key (the entry's +4), 0xC6050 adds a number and 0x9E850 a bool, thiscall on that entry with a pointer to the
// value. In PC mode, while it runs, the value of each row of ours is ours.
// Saving: START calls FEA_SaveSettings (0xC58B0, cdecl), its argument a string by value (length at +0, the text from
// +8): the 22 values in the screen's order, comma separated. In PC mode they set the PC settings and the game's save
// is skipped. B asks the game's own "undo all changes?" and saves nothing.
// Entry: Options' key handler is callGF2's body (menus.cpp): X (INPUTCODE.CIRCLE) calls callGF("FN2_PCMODE") (seen at
// the function table's lookup: PcPageEnter) and the screen's own goSettings.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/hle.h"

namespace fn2 {
extern bool smooth_motion, skip_intro, modern_buttons, unlock_all, hide_hud;
uint32_t SettingsConst();  // menus.cpp: the loaded settings.const, 0 before the screen's first use

namespace {
enum Kind { kBool, kList };
struct Row {
    const char* key;        // FEA_LoadSettings' name for it
    int save_index;         // its place in FEA_SaveSettings' list
    uint32_t label_item;    // settings.const items
    std::vector<uint32_t> choice_items;
    std::vector<int> values;  // the row's own values, by choice (m_arrValues)
    const char* label;
    std::vector<const char*> choices;
    int (*get)();
    void (*set)(int);
};

app::Settings& S() { return app::Get(); }
int Pick(int v, std::initializer_list<int> list) {
    int i = 0;
    for (int x : list) {
        if (x == v) return i;
        i++;
    }
    return 0;
}
template <size_t N>
int At(const int (&list)[N], int i) { return list[size_t(std::clamp(i, 0, int(N) - 1))]; }

const int kCaps[] = {30, 60, 120, 144, 240, 0};
const int kAspect[] = {0, 2, 3};
const int kScale[] = {100, 150, 200};
const int kAniso[] = {0, 2, 4, 8, 16};

// the game's rows (the screen's order), each with a PC setting of the same shape. On/off rows: OFF first, value 0
const std::vector<Row>& Rows() {
    static const std::vector<Row> rows = {
        {"nDifficulty", 0, 69, {78, 79, 80}, {0, 1, 2}, "DISPLAY MODE", {"FULL SCREEN", "BORDERLESS", "WINDOW"},
         [] { return int(S().display_mode); }, [](int v) { S().display_mode = app::DisplayMode(std::clamp(v, 0, 2)); }},
        {"nNumRounds", 1, 1, {10, 11, 12, 13, 14, 15}, {0, 1, 2, 3, 4, 5}, "FRAME RATE CAP",
         {"30", "60", "120", "144", "240", "UNLIMITED"}, [] { return Pick(int(S().fps_cap), {30, 60, 120, 144, 240, 0}); },
         [](int v) { S().fps_cap = uint32_t(At(kCaps, v)); }},
        {"nRoundLength", 2, 91, {100, 101, 102}, {0, 1, 2}, "EDGE SMOOTHING", {"OFF", "FXAA", "SMAA"},
         [] { return S().smaa ? 2 : S().fxaa ? 1 : 0; }, [](int v) { S().fxaa = v == 1, S().smaa = v == 2; }},
        {"bIllegalBlows", 3, 113, {122, 123}, {0, 1}, "VERTICAL SYNC", {"OFF", "ON"}, [] { return int(S().vsync); },
         [](int v) { S().vsync = v != 0; }},
        {"b3Knockdown", 4, 134, {143, 144}, {0, 1}, "SMOOTH MOTION", {"OFF", "ON"}, [] { return int(smooth_motion); },
         [](int v) { smooth_motion = v != 0; }},
        {"nSavedBTB", 5, 155, {164, 165, 166}, {0, 1, 2}, "ASPECT RATIO", {"FROM THE SCREEN", "16:9", "FILL THE SCREEN"},
         [] { return Pick(int(S().aspect), {0, 2, 3}); }, [](int v) { S().aspect = uint32_t(At(kAspect, v)); }},
        {"bAutoRecovery", 6, 177, {186, 187}, {0, 1}, "ENHANCED TEXTURES", {"OFF", "ON"},
         [] { return int(S().world_textures); }, [](int v) { S().world_textures = v != 0; }},
        {"bTCC", 7, 306, {315, 316}, {0, 1}, "SHARP TEXT AND HUD", {"OFF", "ON"}, [] { return int(S().sharp_2d); },
         [](int v) { S().sharp_2d = v != 0; }},
        {"bKOMoment", 8, 349, {358, 359}, {0, 1}, "SKIP INTRO MOVIES", {"OFF", "ON"}, [] { return int(skip_intro); },
         [](int v) { skip_intro = v != 0; }},
        {"bFightStoppage", 9, 370, {379, 380}, {0, 1}, "MODERN BUTTONS", {"OFF", "ON"}, [] { return int(modern_buttons); },
         [](int v) { modern_buttons = v != 0; }},
        {"nAudioType", 10, 327, {336, 337, 338}, {0, 1, 3}, "RENDER SCALE", {"100%", "150%", "200%"},
         [] { return Pick(int(S().render_percent), {100, 150, 200}); },
         [](int v) { S().render_percent = uint32_t(At(kScale, v)); }},
        {"nCamera", 15, 198, {207, 208, 209, 210, 211}, {0, 1, 2, 3, 4}, "TEXTURE FILTERING",
         {"AS THE GAME ASKS", "2X", "4X", "8X", "16X"}, [] { return Pick(int(S().aniso), {0, 2, 4, 8, 16}); },
         [](int v) { S().aniso = uint32_t(At(kAniso, v)); }},
        {"bWidescreen", 16, 285, {294, 295}, {0, 1}, "UNLOCK EVERYTHING", {"OFF", "ON"}, [] { return int(unlock_all); },
         [](int v) { unlock_all = v != 0; }},
        {"bCutscenes", 17, 222, {231, 232}, {0, 1}, "SMOOTH ALPHA EDGES", {"OFF", "ON"},
         [] { return int(S().alpha_coverage); }, [](int v) { S().alpha_coverage = v != 0; }},
        {"bHud", 18, 391, {400, 401}, {0, 1}, "HUD", {"OFF", "ON"}, [] { return int(!hide_hud); },
         [](int v) { hide_hud = v == 0; }},
        {"bAutosave", 19, 243, {252, 253}, {0, 1}, "SHOW FRAMES A SECOND", {"OFF", "ON"}, [] { return int(S().show_fps); },
         [](int v) { S().show_fps = v != 0; }},
        {"bVibration", 20, 264, {273, 274}, {0, 1}, "PAUSE WHEN NOT IN FRONT", {"OFF", "ON"},
         [] { return int(S().pause_unfocused); }, [](int v) { S().pause_unfocused = v != 0; }},
        {"nLanguage", 21, 26, {35, 36, 37}, {0, 1, 2}, "MSAA", {"OFF", "2X", "4X"},
         [] { return Pick(int(S().msaa), {0, 1, 2}); }, [](int v) { S().msaa = uint32_t(std::clamp(v, 0, 2)); }},
    };
    return rows;
}

std::mutex g_lock;
bool g_mode = false;          // the screen is ours
uint32_t g_pointed = 0;       // the const file our items point into, 0 when the game's
std::vector<std::pair<uint32_t, uint32_t>> g_saved;  // item address, the game's offset
bool g_loading = false;       // inside FEA_LoadSettings

uint32_t Text(const char* s) {  // a string of ours in guest memory, kept for the game's life
    const uint32_t n = uint32_t(std::strlen(s)) + 1;
    const uint32_t va = mem::AllocSystem(n);
    std::memcpy(kernel::HostPtr(va), s, n);
    return va;
}

void Point(uint32_t file, uint32_t item, const char* text) {
    const uint32_t at = file + 0x20 + item * 8;
    if (kernel::R32(at) != 1) return;  // not a string item
    g_saved.emplace_back(at + 4, kernel::R32(at + 4));
    kernel::W32(at + 4, Text(text) - file);
}

// the rows' labels and choices ours (once the screen's file is loaded)
void PointAll() {
    const uint32_t file = SettingsConst();
    if (!file || g_pointed == file) return;
    g_saved.clear();
    for (const Row& r : Rows()) {
        Point(file, r.label_item, r.label);
        for (size_t i = 0; i < r.choice_items.size() && i < r.choices.size(); i++) Point(file, r.choice_items[i], r.choices[i]);
    }
    g_pointed = file;
    LOGR("fn2: PC Settings: the Settings screen's rows show the PC settings");
}

void Restore() {
    for (auto [at, offset] : g_saved) kernel::W32(at, offset);
    g_saved.clear();
    g_pointed = 0;
}

const Row* RowOf(const std::string& key) {
    for (const Row& r : Rows())
        if (key == r.key) return &r;
    return nullptr;
}

// a value being added to FEA_LoadSettings' reply: ours for our rows
void Substitute(X86Context& ctx) {
    std::lock_guard<std::mutex> hold(g_lock);
    if (!g_mode || !g_loading) return;
    const std::string key = kernel::ReadCString(kernel::R32(ctx.ecx + 4), 32);
    const Row* r = RowOf(key);
    if (!r) return;
    const int choice = std::clamp(r->get(), 0, int(r->values.size()) - 1);
    kernel::W32(kernel::R32(ctx.esp + 4), uint32_t(r->values[size_t(choice)]));
}
}  // namespace

// the PC page is up: its Y (the game's help for the rows it lends) is kept from the game
bool PcPageShown() {
    std::lock_guard<std::mutex> hold(g_lock);
    return g_mode;
}

// callGF("FN2_PCMODE"), X on Options, just before its goSettings
void PcPageEnter() {
    std::lock_guard<std::mutex> hold(g_lock);
    g_mode = true;
    PointAll();
    LOGR("fn2: PC Settings opened");
}

// the screen's file just loaded (menus.cpp): ours if the screen is
void PcPageLoaded() {
    std::lock_guard<std::mutex> hold(g_lock);
    if (g_mode) PointAll();
}

// each screen as it comes up (screens.cpp): leaving Settings leaves PC mode
void PcPageScreen(const std::string& screen) {
    std::lock_guard<std::mutex> hold(g_lock);
    if (!g_mode || screen == "SETTINGS") return;
    g_mode = false;
    Restore();
    LOGR("fn2: PC Settings closed, the Settings screen the game's again");
}
}  // namespace fn2

// FEA_LoadSettings
X86_EXTERN_FUNC(__imp__sub_000C60D0);
X86_FUNC(sub_000C60D0) {
    {
        std::lock_guard<std::mutex> hold(fn2::g_lock);
        fn2::g_loading = true;
    }
    __imp__sub_000C60D0(ctx, base);
    std::lock_guard<std::mutex> hold(fn2::g_lock);
    fn2::g_loading = false;
}

// a number and a bool added to a reply (thiscall: the entry, a pointer to the value)
X86_EXTERN_FUNC(__imp__sub_000C6050);
X86_FUNC(sub_000C6050) {
    fn2::Substitute(ctx);
    __imp__sub_000C6050(ctx, base);
}
X86_EXTERN_FUNC(__imp__sub_0009E850);
X86_FUNC(sub_0009E850) {
    fn2::Substitute(ctx);
    __imp__sub_0009E850(ctx, base);
}

// FEA_SaveSettings (cdecl, a string by value)
X86_EXTERN_FUNC(__imp__sub_000C58B0);
X86_FUNC(sub_000C58B0) {
    {
        std::lock_guard<std::mutex> hold(fn2::g_lock);
        if (!fn2::g_mode) goto game;
        const uint32_t n = std::min<uint32_t>(kernel::R32(ctx.esp + 4), 512);
        std::string text;
        for (uint32_t i = 0; i < n; i++) text += char(kernel::G<uint8_t>(ctx.esp + 12 + i));
        std::vector<int> v;
        for (size_t p = 0; p <= text.size();) {
            const size_t c = text.find(',', p);
            v.push_back(std::atoi(text.substr(p, c == std::string::npos ? std::string::npos : c - p).c_str()));
            if (c == std::string::npos) break;
            p = c + 1;
        }
        for (const fn2::Row& r : fn2::Rows()) {
            if (r.save_index >= int(v.size())) continue;
            int choice = 0;
            for (size_t i = 0; i < r.values.size(); i++)
                if (r.values[i] == v[size_t(r.save_index)]) choice = int(i);
            r.set(choice);
        }
        app::Save();
        app::ApplyDisplay();
        LOGR("fn2: PC Settings saved (%s)", text.c_str());
        ctx.esp += 4;  // ret: the game's save skipped
        return;
    }
game:
    __imp__sub_000C58B0(ctx, base);
}
