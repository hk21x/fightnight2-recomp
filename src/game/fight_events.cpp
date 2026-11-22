// TEMPORARY probe: the fight's messages to the crowd (0x1732D0, thiscall: the crowd, the message; its type at +4)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"
X86_EXTERN_FUNC(__imp__sub_001732D0);
X86_FUNC(sub_001732D0) {
    const uint32_t m = kernel::R32(ctx.esp + 4);
    char b[200]; int n = 0;
    for (uint32_t i = 0; i < 12; i++) n += std::snprintf(b + n, sizeof b - n, " %08X", kernel::R32(m + i * 4));
    LOGR("probe fightmsg: type %u msg %08X:%s", kernel::R32(m + 4), m, b);
    if (kernel::R32(m + 4) == 3 || kernel::R32(m + 4) == 9) if (FILE* f = std::fopen("memdump", "w")) std::fclose(f);
    __imp__sub_001732D0(ctx, base);
}

namespace fn2 {
void ShortFightTick() {
    static const bool on = [] { const char* e = std::getenv("FN2_SHORT_FIGHT"); return e && *e == '1'; }();
    if (!on) return;
    uint32_t v = kernel::R32(0x37FFD0 + 0x1C);
    v &= ~((7u << 18) | (3u << 22));
    kernel::W32(0x37FFD0 + 0x1C, v);
}
}
