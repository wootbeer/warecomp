// The game's two code overlays (STATUS.md round 1): the frontend (menus,
// ROM 0x38D40) and the match (ROM 0x7F0B0) both load at 0x80048FE0, in turn,
// from the main thread's loop at 0x800043A0. Indirect calls into them (C++
// virtual calls, and the loader's own calls) are looked up in the runtime's
// function map, which has to know which one is loaded.
//
// This runtime doesn't register overlays when the game DMAs them, and at
// startup it registers every code section in the ROM's first megabyte at the
// address a straight copy of that megabyte would put it (its IPL3 load,
// librecomp's init()). Both overlays are in that megabyte, so they start out
// registered at the wrong addresses (frontend at 0x80038140, the match at
// 0x8007E4B0).
//
// wa_clear_initial_overlays (the game entry's on_init_callback) removes those,
// and wa_load_overlay, hooked right after each of the loader's two DMAs
// (wa.us.toml), registers the overlay just loaded in place of the other.
//
// The frontend also loads one of seven small modules (data and getter
// functions) to 0x800B20F0, just past the match overlay's BSS (its loader is
// front_8004F574; STATUS.md round 4). wa_load_module, hooked after that DMA,
// does the same for them. The first two are in the first megabyte too.
#include <cstdint>

#include "librecomp/overlays.hpp"
#include "recomp.h"

namespace {
    constexpr int32_t kOverlayRam = (int32_t)0x80048FE0;
    // The larger of the two sections (the match: 0x67FE0 of code and data;
    // the loader DMAs 0x69110 with its BSS). Not the DMA size: that ends
    // exactly where the modules start (0x800B20F0), and the runtime's overlap
    // test counts touching ranges as overlapping, so it would try to unload
    // a loaded module and stop ("Cannot partially unload section", round 5).
    constexpr uint32_t kOverlaySpan = 0x67FE0;

    constexpr int32_t kModuleRam = (int32_t)0x800B20F0;
    // The largest module (ROM 0x129760).
    constexpr uint32_t kModuleSpan = 0xB910;

    // Where init()'s first-megabyte load registered them: ROM 0x1000 at
    // 0x80000400, so the frontend (ROM 0x38D40) at 0x80038140, then the
    // match, then the first two modules, all within the megabyte (up to
    // 0x80100400). The resident segment ends at 0x80038140 and isn't touched.
    constexpr int32_t kInitialStart = (int32_t)0x80038140;
    constexpr uint32_t kInitialSpan = 0x80100400u - 0x80038140u;
}

void wa_clear_initial_overlays(uint8_t*, recomp_context*) {
    unload_overlays(kInitialStart, kInitialSpan);
}

extern "C" void wa_load_overlay(uint32_t rom, uint32_t size) {
    unload_overlays(kOverlayRam, kOverlaySpan);
    load_overlays(rom, kOverlayRam, size);
}

// rom_k1 is the cartridge address the loader DMAs from (0xB0000000 + offset).
extern "C" void wa_load_module(uint32_t rom_k1, uint32_t size) {
    unload_overlays(kModuleRam, kModuleSpan);
    load_overlays(rom_k1 - 0xB0000000u, kModuleRam, size);
}
