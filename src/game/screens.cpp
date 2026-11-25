// The front end's screens: each APT screen calls the game function FeInitScreen (0x9A880, registered by name in the
// table at 0x38F3C8: name, wrapper, function; 12 bytes an entry, the count at 0x390828) as it comes up, its argument a
// string by value (length at [esp+4], the text from [esp+12]). Screen() is the last one named (MAINMENU, OPTIONS, ...).
#include <cstdint>
#include <mutex>
#include <string>

#include "core/log.h"
#include "kernel/kernel.h"
#include "xdk/hle.h"

namespace fn2 {
namespace {
std::mutex g_lock;
std::string g_screen;
}  // namespace
std::string Screen() {
    std::lock_guard<std::mutex> l(g_lock);
    return g_screen;
}
}  // namespace fn2

X86_EXTERN_FUNC(__imp__sub_0009A880);
X86_FUNC(sub_0009A880) {
    const uint32_t n = kernel::R32(ctx.esp + 4);
    std::string screen;
    for (uint32_t i = 0; i < n && i < 64; i++) screen += char(kernel::G<uint8_t>(ctx.esp + 12 + i));
    {
        std::lock_guard<std::mutex> l(fn2::g_lock);
        fn2::g_screen = screen;
    }
    LOGR("fn2: screen %s", screen.c_str());
    __imp__sub_0009A880(ctx, base);
}
