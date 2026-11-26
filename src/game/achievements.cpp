// Achievements (src/common/game_info.cpp lists them, each "0=1": awarded from here).
//
// The career: the profile is 0x37FFD0 (0x1659B0); its current career 0x37FFD0 + 0xD68 + slot * 0x38EC, the slot the
// short at +0xCBC (0x1626B0 with -1); the career boxer the pointer at the career's +0x38D8 (FEA_CareerBoxer_GetSummary,
// 0xAA780). The boxer's pro record (0x156CE0 on the career's +0x389C, whose +0x3C is the boxer): boxer + 0x360 +
// (byte +0x29 * 0x32 + short +0x46) * 0x2C, wins at +5, losses +6, draws +7, knockouts +8 (bytes; 0xAA300 saves them
// as nRecW/L/D/KO). Title belts: bytes +0x1A..+0x1F, 1 held, 2 held once (nCurrent, nFormer). Turned pro: byte +5.
// A count awards only once it has been seen short of its target (as 50 Cent's): a save that already has it awards
// nothing; another career (another boxer) starts again from what it has.
//
// Events, from the game functions the menus call by name (menus.cpp's look at the function table): a champ created
// (FEA_CreateBoxer_SaveToCareer), a fight store purchase (FEA_FightStorePurchase), a career ended (FEA_EndCareer).
#include <chrono>
#include <cstdint>
#include <string>

#include "core/achievements.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace fn2 {
namespace {
constexpr uint32_t kProfile = 0x37FFD0;

struct Record {
    uint32_t boxer = 0;
    int id = -1, wins = 0, losses = 0, draws = 0, kos = 0, belts = 0, pro = 0;
};

bool Valid(uint32_t va, uint32_t size) { return va >= 0x10000 && mem::IsCommitted(va) && mem::IsCommitted(va + size - 1); }

bool Read(Record& r) {
    const uint32_t slot = kernel::R16(kProfile + 0xCBC);
    if (slot > 3) return false;
    const uint32_t career = kProfile + 0xD68 + slot * 0x38EC;
    const uint32_t boxer = kernel::R32(career + 0x38D8);
    if (!Valid(boxer, 0x400)) return false;
    const int id = int16_t(kernel::R16(boxer + 0x46));
    if (id < 0 || id >= 0x32) return false;
    const uint32_t rec = boxer + 0x360 + (uint32_t(kernel::G<uint8_t>(boxer + 0x29)) * 0x32 + uint32_t(id)) * 0x2C;
    if (!Valid(rec, 0x2C)) return false;
    r.boxer = boxer, r.id = id;
    r.wins = kernel::G<uint8_t>(rec + 5), r.losses = kernel::G<uint8_t>(rec + 6), r.draws = kernel::G<uint8_t>(rec + 7);
    r.kos = kernel::G<uint8_t>(rec + 8), r.pro = kernel::G<uint8_t>(boxer + 5);
    r.belts = 0;
    for (uint32_t i = 0; i < 6; i++) r.belts += kernel::G<uint8_t>(boxer + 0x1A + i) == 1;
    return true;
}

struct Goal {
    const char* id;
    bool (*met)(const Record&);
    bool seen_short = false;
};
Goal g_goals[] = {
    {"first_win", [](const Record& r) { return r.pro && r.wins >= 1; }},
    {"first_ko", [](const Record& r) { return r.pro && r.kos >= 1; }},
    {"ten_wins", [](const Record& r) { return r.pro && r.wins >= 10; }},
    {"ten_kos", [](const Record& r) { return r.pro && r.kos >= 10; }},
    {"unbeaten", [](const Record& r) { return r.pro && r.wins >= 10 && r.losses == 0 && r.draws == 0; }},
    {"champion", [](const Record& r) { return r.belts >= 1; }},
    {"undisputed", [](const Record& r) { return r.belts >= 3; }},
};
}  // namespace

// a few times a second, from the game's thread (texture binds, src/common/buttons.cpp)
void AchievementsTick() {
    static auto last = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::milliseconds(500)) return;
    last = now;
    static Record was;
    Record r;
    if (!Read(r)) return;
    if (r.boxer != was.boxer || r.id != was.id) {
        LOGR("fn2: career boxer %08X (id %d): %d-%d-%d, %d KOs, %d belts held%s", r.boxer, r.id, r.wins, r.losses, r.draws,
             r.kos, r.belts, r.pro ? "" : " (amateur)");
        for (Goal& g : g_goals) g.seen_short = false;  // another career: from what it has
    } else if (r.wins != was.wins || r.losses != was.losses || r.draws != was.draws || r.kos != was.kos || r.belts != was.belts) {
        LOGR("fn2: career record %d-%d-%d, %d KOs, %d belts held%s", r.wins, r.losses, r.draws, r.kos, r.belts, r.pro ? "" : " (amateur)");
    }
    for (Goal& g : g_goals) {
        const bool met = g.met(r);
        if (!met) g.seen_short = true;
        else if (g.seen_short) {
            achievements::Award(g.id);
            g.seen_short = false;
        }
    }
    was = r;
}

// a game function called by name from the menus (callGF)
void AchievementsEvent(const std::string& name) {
    if (name == "FEA_CreateBoxer_SaveToCareer") achievements::Award("create_champ");
    if (name == "FEA_FightStorePurchase") achievements::Award("fight_store");
    if (name == "FEA_EndCareer") achievements::Award("hang_up");
}
}  // namespace fn2
