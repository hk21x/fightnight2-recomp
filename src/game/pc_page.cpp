// PC Settings as a page of the game's own menus: X on Options (its footer says X PC SETUP) opens the game's Settings
// screen (screensfe/settings) in a PC mode, its rows the PC settings, in the game's own style; B backs out, START saves.
//
// The screen's rows are option objects, each made by its own init script from constants of settings.const: its label
// ("$621", a string id) and its choices ("4 $622" ... ; the on/off rows each have their own "$2" OFF and "$1" ON). The
// const table keeps file offsets once loaded, read at each use: an item pointed (offset = target - the file's base) at
// a string of ours is shown as written, in the row's own font. In PC mode the rows' labels and choices are pointed at
// ours. The screen's files are unpacked afresh each time it comes up and freed when it goes (Options is unpacked again
// before its FeInitScreen), so only the copy just unpacked is edited and nothing is written back on leaving: the next
// visit's copy is the game's own. (Writing back into the freed copy corrupted the heap: B on Options then crashed.)
//
// Values: the screen's initGUI calls FEA_LoadSettings (0xC60D0), which adds each value to its reply by name: 0xDD0E0
// names the key (the entry's +4), 0xC6050 adds a number and 0x9E850 a bool, thiscall on that entry with a pointer to the
// value. In PC mode, while it runs, the value of each row of ours is ours.
// Saving: START calls FEA_SaveSettings (0xC58B0, cdecl), its argument a string by value (length at +0, the text from
// +8): the 22 values in the screen's order, comma separated. In PC mode they set the PC settings and the game's save
// is skipped. B asks the game's own "undo all changes?" and saves nothing.
// Entry: Options' key handler is callGF2's body (menus.cpp): X (INPUTCODE.CIRCLE) calls callGF("FN2_PCMODE") (seen at
// the function table's lookup: PcPageEnter) and the screen's own goSettings.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
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
    kernel::W32(at + 4, Text(text) - file);
}

// the rows' labels and choices ours, in the copy of settings.const just unpacked
void PointAll(uint32_t file) {
    for (const Row& r : Rows()) {
        Point(file, r.label_item, r.label);
        for (size_t i = 0; i < r.choice_items.size() && i < r.choices.size(); i++) Point(file, r.choice_items[i], r.choices[i]);
    }
    LOGR("fn2: PC Settings: the Settings screen's rows show the PC settings");
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
    g_mode = true;  // the screen's files are unpacked next
    LOGR("fn2: PC Settings opened");
}

// settings.const just unpacked (menus.cpp): ours if the screen is
void PcPageLoaded(uint32_t file) {
    std::lock_guard<std::mutex> hold(g_lock);
    if (g_mode) PointAll(file);
}

// each screen as it comes up (screens.cpp): leaving Settings leaves PC mode
void PcPageScreen(const std::string& screen) {
    std::lock_guard<std::mutex> hold(g_lock);
    if (!g_mode || screen == "SETTINGS") return;
    g_mode = false;  // the copy we edited is freed with the screen
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

// ---- PC Settings in a fight: X on the pause menu opens the pause menu's own Settings panel (native, sub_EE1D0) in a PC
// mode, titled PC SETTINGS, its seven rows the PC settings below, in the game's own style; A keeps, B backs out.
//
// The panel (0x90 bytes, the fight menu manager's state 6, opened by the pause menu's handler 0xECCD0 with
// manager->vtbl[0x98](6), the manager at [[0x365028]+0x30]) builds 7 rows: list rows (0xE0A80, 0xD0 bytes: value +0x90,
// count +0x94, choice string ids from +0x98) and, for the volumes, slider rows (0xE0490, 0xA0 bytes, the same
// arguments). In PC mode the sliders are made list rows (allocation 0xA0 -> 0xD0, 0xE0490 -> 0xE0A80); after the build
// each row gets our choices (string ids 0xF000 up, ours at the lookup 0x231D60) and value (vtbl[0x94], which draws the
// choice) and is enabled (vtbl[0x6C]). Labels and the title: their string ids are ours while the panel builds.
// The game's own work on these rows is kept from it: the live volume preview (0xEE060), the vibration buzz (0xEDCE0)
// and the save on A (0xEDDA0, its settings and volumes); B's 0xEDD50 puts back the volumes it had (unchanged).
namespace fn2 {
namespace {
const char* const kFightRows[7] = {"b3Knockdown", "nNumRounds", "bIllegalBlows", "nAudioType", "nLanguage",
                                   "nRoundLength", "bAutosave"};  // smooth motion, frame cap, vsync, scale, MSAA, AA, fps
const uint16_t kFightLabelIds[7] = {0x25C, 0x277, 0x259, 0x25A, 0x25B, 0x264, 0x25D};  // the rows' own labels
std::atomic<bool> g_fight{false};     // the panel is ours
std::atomic<bool> g_building{false};  // inside the panel's build
std::atomic<int64_t> g_fight_key{0};  // X on the pause menu, ms (steady clock)
std::map<uint32_t, uint32_t> g_text16;  // our string id -> UTF-16 text in guest memory
std::mutex g_text_lock;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
uint32_t Text16(uint32_t id, const char* s) {
    std::lock_guard<std::mutex> hold(g_text_lock);
    auto it = g_text16.find(id);
    if (it != g_text16.end()) return it->second;
    const uint32_t n = uint32_t(std::strlen(s));
    const uint32_t va = mem::AllocSystem(2 * (n + 1));
    for (uint32_t i = 0; i <= n; i++) kernel::W16(va + 2 * i, uint16_t(uint8_t(s[i])));
    return g_text16[id] = va;
}
// our choice ids: row r, choice c
constexpr uint32_t kOurIds = 0xF000;
uint32_t ChoiceId(int r, int c) { return kOurIds + uint32_t(r) * 16 + uint32_t(c); }

uint32_t Call(X86Context& ctx, uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args) {  // thiscall
    const uint32_t ecx = ctx.ecx;
    ctx.ecx = self;
    const uint32_t r = kernel::CallGuest(ctx, fn, args);
    ctx.ecx = ecx;
    return r;
}
const Row* FightRow(int i) { return RowOf(kFightRows[i]); }
}  // namespace

// X on a fight's pause menu (menus.cpp, the pad hook): the pause menu's handler opens our panel at its next call
void PcFightKey() { g_fight_key = NowMs(); }
bool PcFightShown() { return g_fight; }
}  // namespace fn2

// the pause menu's key handler (thiscall: buttons, ?; ret 8)
X86_EXTERN_FUNC(__imp__sub_000ECCD0);
X86_FUNC(sub_000ECCD0) {
    const int64_t key = fn2::g_fight_key.exchange(0);
    if (key && fn2::NowMs() - key < 500) {
        const uint32_t manager = kernel::R32(kernel::R32(0x365028) + 0x30);
        fn2::g_fight = true;
        LOGR("fn2: PC Settings (the fight's pause menu) opened");
        fn2::Call(ctx, kernel::R32(kernel::R32(manager) + 0x98), manager, {6});
        kernel::Return(ctx, 1, 2);
        return;
    }
    __imp__sub_000ECCD0(ctx, base);
}

// the fight's Settings panel: built (thiscall: the manager; ret 4)
X86_EXTERN_FUNC(__imp__sub_000EE1D0);
X86_FUNC(sub_000EE1D0) {
    if (!fn2::g_fight) {
        __imp__sub_000EE1D0(ctx, base);
        return;
    }
    const uint32_t panel = ctx.ecx;
    fn2::g_building = true;
    __imp__sub_000EE1D0(ctx, base);
    fn2::g_building = false;
    const uint32_t result = ctx.eax;
    const uint32_t rows = kernel::R32(panel + 0x60), count = kernel::R32(panel + 0x64);
    for (uint32_t i = 0; i < count && i < 7; i++) {
        const uint32_t row = kernel::R32(rows + 4 * i);
        const fn2::Row* r = fn2::FightRow(int(i));
        if (!row || !r || kernel::R32(row) != 0x2F3370) continue;  // a list row
        const uint32_t n = uint32_t(std::min<size_t>(r->choices.size(), 14));
        for (uint32_t c = 0; c < n; c++) {
            fn2::Text16(fn2::ChoiceId(int(i), int(c)), r->choices[c]);
            kernel::W32(row + 0x98 + 4 * c, fn2::ChoiceId(int(i), int(c)));
        }
        kernel::W32(row + 0x94, n);
        const uint32_t value = uint32_t(std::clamp(r->get(), 0, int(n) - 1));
        const uint32_t vtbl = kernel::R32(row);
        fn2::Call(ctx, kernel::R32(vtbl + 0x6C), row, {1, 0});  // enabled
        fn2::Call(ctx, kernel::R32(vtbl + 0x94), row, {value});
    }
    ctx.eax = result;
    LOGR("fn2: PC Settings: the fight's Settings panel shows the PC settings");
}

// the heap (size, flags; cdecl): while the panel builds ours, a slider row's 0xA0 is a list row's 0xD0
X86_EXTERN_FUNC(__imp__sub_0014ABB0);
X86_FUNC(sub_0014ABB0) {
    if (fn2::g_building && kernel::R32(ctx.esp + 4) == 0xA0) kernel::W32(ctx.esp + 4, 0xD0);
    __imp__sub_0014ABB0(ctx, base);
}
// a slider row (thiscall: the panel, 0; ret 8): a list row while the panel builds ours
X86_EXTERN_FUNC(__imp__sub_000E0490);
X86_EXTERN_FUNC(__imp__sub_000E0A80);
X86_FUNC(sub_000E0490) {
    if (fn2::g_building) __imp__sub_000E0A80(ctx, base);
    else __imp__sub_000E0490(ctx, base);
}

// a string by id (table, id; cdecl): ours from 0xF000; while our panel builds, its labels and title
X86_EXTERN_FUNC(__imp__sub_00231D60);
X86_FUNC(sub_00231D60) {
    const uint32_t id = kernel::R32(ctx.esp + 8) & 0xFFFF;
    uint32_t ours = 0;
    if (id >= fn2::kOurIds) {
        std::lock_guard<std::mutex> hold(fn2::g_text_lock);
        auto it = fn2::g_text16.find(id);
        ours = it != fn2::g_text16.end() ? it->second : 0;
        if (!ours) ours = 0;  // not ours: the game's
    } else if (fn2::g_building) {
        for (int i = 0; i < 7; i++)
            if (id == fn2::kFightLabelIds[i] && fn2::FightRow(i)) ours = fn2::Text16(0xE000 + uint32_t(i), fn2::FightRow(i)->label);
        if (id == 252 || id == 17) ours = fn2::Text16(0xE100, "PC SETTINGS");
    }
    if (ours) {
        ctx.eax = ours;
        ctx.esp += 4;
        return;
    }
    __imp__sub_00231D60(ctx, base);
}

// the panel's live volume preview, its vibration buzz: not for our rows
X86_EXTERN_FUNC(__imp__sub_000EE060);
X86_FUNC(sub_000EE060) {
    if (fn2::g_fight) { ctx.esp += 4; return; }
    __imp__sub_000EE060(ctx, base);
}
X86_EXTERN_FUNC(__imp__sub_000EDCE0);
X86_FUNC(sub_000EDCE0) {
    if (fn2::g_fight) { ctx.esp += 4; return; }
    __imp__sub_000EDCE0(ctx, base);
}
// A: the panel's save; ours sets the PC settings
X86_EXTERN_FUNC(__imp__sub_000EDDA0);
X86_FUNC(sub_000EDDA0) {
    if (!fn2::g_fight) {
        __imp__sub_000EDDA0(ctx, base);
        return;
    }
    const uint32_t panel = ctx.ecx, rows = kernel::R32(panel + 0x60), count = kernel::R32(panel + 0x64);
    std::string said;
    for (uint32_t i = 0; i < count && i < 7; i++) {
        const uint32_t row = kernel::R32(rows + 4 * i);
        const fn2::Row* r = fn2::FightRow(int(i));
        if (!row || !r || kernel::R32(row) != 0x2F3370) continue;
        const int v = int(kernel::R32(row + 0x90));
        r->set(v);
        said += std::string(said.empty() ? "" : ", ") + r->label + " " + r->choices[size_t(std::clamp(v, 0, int(r->choices.size()) - 1))];
    }
    app::Save();
    app::ApplyDisplay();
    fn2::g_fight = false;
    LOGR("fn2: PC Settings (fight) saved: %s", said.c_str());
    ctx.esp += 4;
}
// B: the panel's undo (the volumes it had); ours leaves
X86_EXTERN_FUNC(__imp__sub_000EDD50);
X86_FUNC(sub_000EDD50) {
    if (fn2::g_fight.exchange(false)) LOGR("fn2: PC Settings (fight) closed, nothing changed");
    __imp__sub_000EDD50(ctx, base);
}
