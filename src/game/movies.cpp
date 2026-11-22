// skip_intro: the boot's movies left out. The game plays a front end movie with 0xCD9B0 (stdcall: its name, 0, 0; the
// boot through 0xCC390, the scripts' PlayFeMovie through 0x9A2C0) and waits for its end; a missing file is asked for
// again for ever, so the movie is skipped at the player instead: easports and intro, returned from at once.
#include <cctype>
#include <cstdint>
#include <string>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace fn2 {
extern bool skip_intro;
}

X86_EXTERN_FUNC(__imp__sub_000CD9B0);
X86_FUNC(sub_000CD9B0) {
    if (fn2::skip_intro) {
        std::string name;
        for (uint32_t p = kernel::R32(ctx.esp + 4); p && name.size() < 64; p++) {
            const char c = char(kernel::G<uint8_t>(p));
            if (!c) break;
            name += char(std::tolower(static_cast<unsigned char>(c)));
        }
        if (name.find("easports") != std::string::npos || name.find("intro") != std::string::npos) {
            LOGR("fn2: movie %s skipped (skip_intro)", name.c_str());
            ctx.esp += 16;  // ret 12
            return;
        }
    }
    __imp__sub_000CD9B0(ctx, base);
}
