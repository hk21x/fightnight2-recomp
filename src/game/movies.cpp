// skip_intro: the boot's movies left out. The game plays a front end movie with 0xCD9B0 (stdcall: its name, 0, 0; the
// boot through 0xCC390, the scripts' PlayFeMovie through 0x9A2C0) and waits for its end; a missing file is asked for
// again for ever, so the movie is skipped at the player instead: easports and intro, returned from at once.
// While a movie plays (the call lasts as long as the movie), the sides of a wide screen are black, not the menus' blurred
// copy of the frame (MoviePlaying, src/game/ultrawide.cpp's FrameHeld): Harry, 2026-11-25, the EA Sports logo with bars.
#include <atomic>
#include <cctype>
#include <cstdint>
#include <string>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace fn2 {
extern bool skip_intro;
namespace {
std::atomic<bool> g_movie{false};
}
bool MoviePlaying() { return g_movie; }
}

X86_EXTERN_FUNC(__imp__sub_000CD9B0);
X86_FUNC(sub_000CD9B0) {
    std::string name;
    for (uint32_t p = kernel::R32(ctx.esp + 4); p && name.size() < 64; p++) {
        const char c = char(kernel::G<uint8_t>(p));
        if (!c) break;
        name += char(std::tolower(static_cast<unsigned char>(c)));
    }
    if (fn2::skip_intro) {
        if (name.find("easports") != std::string::npos || name.find("intro") != std::string::npos) {
            LOGR("fn2: movie %s skipped (skip_intro)", name.c_str());
            ctx.esp += 16;  // ret 12
            return;
        }
    }
    LOGR("fn2: movie %s playing", name.c_str());
    fn2::g_movie = true;
    __imp__sub_000CD9B0(ctx, base);
    fn2::g_movie = false;
    LOGR("fn2: movie %s ended", name.c_str());
}
