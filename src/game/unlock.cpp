// unlock_all: every venue, boxer and Create Champ and fight store item available, without touching the save. The game
// asks the profile (0x1659B0) through eleven checkers, thiscall (the profile, a short id), each a bool: an item's
// category (IsFSItemUnlocked 0xB5720 switches over ten of them) tests a bit of the profile's unlock words, 0x162E30 the
// venues (and the venue cheat byte +0xC36). On, each answers true; the original still runs first, so its log shows what
// was locked. Off, they answer as the game does: the player's own progress, never written by this.
#include <cstdint>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace fn2 {
bool unlock_all = false;  // the setting (src/common/game_info.cpp)
}

namespace {
void Unlocked(X86Context& ctx, uint32_t fn) {
    if (!fn2::unlock_all || (ctx.eax & 0xFF)) return;
    LOGR_FIRST(16, "fn2: unlock_all: item %u of checker %06X unlocked", kernel::R32(ctx.esp - 4) & 0xFFFF, fn);
    ctx.eax = (ctx.eax & ~0xFFu) | 1;
}
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00162E30);
X86_FUNC(sub_00162E30) {
    __imp__sub_00162E30(ctx, base);
    Unlocked(ctx, 0x162E30);
}
X86_EXTERN_FUNC(__imp__sub_00162F10);
X86_FUNC(sub_00162F10) {
    __imp__sub_00162F10(ctx, base);
    Unlocked(ctx, 0x162F10);
}
X86_EXTERN_FUNC(__imp__sub_00162FB0);
X86_FUNC(sub_00162FB0) {
    __imp__sub_00162FB0(ctx, base);
    Unlocked(ctx, 0x162FB0);
}
X86_EXTERN_FUNC(__imp__sub_00163050);
X86_FUNC(sub_00163050) {
    __imp__sub_00163050(ctx, base);
    Unlocked(ctx, 0x163050);
}
X86_EXTERN_FUNC(__imp__sub_001630F0);
X86_FUNC(sub_001630F0) {
    __imp__sub_001630F0(ctx, base);
    Unlocked(ctx, 0x1630F0);
}
X86_EXTERN_FUNC(__imp__sub_00163190);
X86_FUNC(sub_00163190) {
    __imp__sub_00163190(ctx, base);
    Unlocked(ctx, 0x163190);
}
X86_EXTERN_FUNC(__imp__sub_00163230);
X86_FUNC(sub_00163230) {
    __imp__sub_00163230(ctx, base);
    Unlocked(ctx, 0x163230);
}
X86_EXTERN_FUNC(__imp__sub_00163350);
X86_FUNC(sub_00163350) {
    __imp__sub_00163350(ctx, base);
    Unlocked(ctx, 0x163350);
}
X86_EXTERN_FUNC(__imp__sub_001633F0);
X86_FUNC(sub_001633F0) {
    __imp__sub_001633F0(ctx, base);
    Unlocked(ctx, 0x1633F0);
}
X86_EXTERN_FUNC(__imp__sub_00163490);
X86_FUNC(sub_00163490) {
    __imp__sub_00163490(ctx, base);
    Unlocked(ctx, 0x163490);
}
X86_EXTERN_FUNC(__imp__sub_00163530);
X86_FUNC(sub_00163530) {
    __imp__sub_00163530(ctx, base);
    Unlocked(ctx, 0x163530);
}
