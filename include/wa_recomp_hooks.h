#ifndef WA_RECOMP_HOOKS_H
#define WA_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt),
// for whatever the [[patches.hook]] text in wa.us.toml needs beyond what
// N64Recomp's generated recomp.h already provides. Force-included before
// recomp.h, so it includes it itself (it has an include guard).

#include <stdint.h>
#include <stdbool.h>
#include "recomp.h"

// Widescreen (src/game/widescreen.cpp): how far past each side of the 320
// pixel screen the drawing library now draws. The right-edge constants in
// wa.us.toml's instruction patches are 0x141 + this and 0x140 + this.
#define WA_WS_EXTRA 272

#ifdef __cplusplus
extern "C" {
#endif

// recompui (lib/RecompFrontend/recompui/src/api/ui_api_events.cpp): runs the
// launcher/menu UI's queued callbacks. Hooked once per presented frame.
void recomp_run_ui_callbacks(uint8_t* rdram, recomp_context* ctx);

// src/main/scheduler_workaround.cpp: called from the game's busy-wait loops
// so the thread that ends the wait gets to run.
bool wa_audio_fifo_full(void);
void wa_yield_via_priority_drop(uint8_t* rdram);

// src/game/overlays.cpp: registers the code overlay just DMA'd to 0x80048FE0
// (ROM offset and size) with the runtime, in place of the other one.
void wa_load_overlay(uint32_t rom, uint32_t size);
// The same for the frontend's modules at 0x800B20F0 (cartridge address
// 0xB0000000 + ROM offset, and size).
void wa_load_module(uint32_t rom_k1, uint32_t size);

// src/game/coroutines.cpp: the match's setjmp/longjmp coroutines on fibers.
void wa_coro_start(uint8_t* rdram, recomp_context* ctx);
void wa_coro_yield(uint8_t* rdram, recomp_context* ctx);
void wa_coro_resume(uint8_t* rdram, recomp_context* ctx);

// src/game/widescreen.cpp: screen-edge clipping at -WA_WS_EXTRA and
// 320 + WA_WS_EXTRA.
void wa_hud_entry(uint8_t* rdram, recomp_context* ctx);
void wa_hud_end(uint8_t* rdram, recomp_context* ctx);
void wa_ws_clip_7c74(uint8_t* rdram, recomp_context* ctx);
void wa_ws_clip_8010(uint8_t* rdram, recomp_context* ctx);
void wa_ws_clip_9320(uint8_t* rdram, recomp_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // WA_RECOMP_HOOKS_H
