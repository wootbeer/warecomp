#ifndef WA_RECOMP_HOOKS_H
#define WA_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt),
// for whatever the [[patches.hook]] text in wa.us.toml needs beyond what
// N64Recomp's generated recomp.h already provides. Force-included before
// recomp.h, so it includes it itself (it has an include guard).

#include <stdint.h>
#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

// recompui (lib/RecompFrontend/recompui/src/api/ui_api_events.cpp): runs the
// launcher/menu UI's queued callbacks. Hooked once per presented frame.
void recomp_run_ui_callbacks(uint8_t* rdram, recomp_context* ctx);

// src/main/scheduler_workaround.cpp: called from the game's busy-wait loops
// so the thread that ends the wait gets to run.
void wa_yield_via_priority_drop(uint8_t* rdram);

// src/game/overlays.cpp: registers the code overlay just DMA'd to 0x80048FE0
// (ROM offset and size) with the runtime, in place of the other one.
void wa_load_overlay(uint32_t rom, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // WA_RECOMP_HOOKS_H
