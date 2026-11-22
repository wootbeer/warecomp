// Hide Level Border (Graphics tab, STATUS.md round 16).
//
// The striped border around a level's bounds is part of the landscape image
// (the 960 x 348 CI8 background func_80009320 draws): an 8-pixel band along
// the top and both sides, in palette entries 1 and 4, which the landscape
// uses for nothing else. The image itself is left alone (the game may well
// use it for collisions); instead the landscape is drawn with its own copy of
// the palette in which those two entries are transparent. The game loads one
// palette (at 0x8012CCB4) for everything in the frame and the HUD uses those
// entries too (the health bars, the timer's frame), so the copy is loaded
// just before the landscape's background command and the game's palette
// again right after it.
//
// The copy lives at 0x80700000, in the upper 4 MB of RDRAM, which the game
// doesn't use (it is a 4 MB game; nothing is ever written there).
#include <atomic>
#include <cstdint>

#include "recomp.h"
#include "wa_config.h"
#include "wa_recomp_hooks.h"

namespace {
    std::atomic<bool> hide_border{ false };
    bool palette_swapped = false;

    constexpr uint32_t kGamePalette = 0x8012CCB4;
    constexpr uint32_t kBorderlessPalette = 0x80700000;
    constexpr uint32_t kDlHead = 0x357C; // the drawing library's display list head

    void cmd(uint8_t* rdram, gpr& head, uint32_t w0, uint32_t w1) {
        MEM_W(0, head) = (int32_t)w0;
        MEM_W(4, head) = (int32_t)w1;
        head = ADD32(head, 8);
    }

    // The game's own palette load (G_SETTIMG, tile and load syncs,
    // G_SETTILE for the palette's TMEM, G_LOADTLUT of 256 entries,
    // pipe sync), with the palette's address.
    void load_palette(uint8_t* rdram, gpr lib, uint32_t address) {
        gpr head = (gpr)MEM_W(kDlHead, lib);
        cmd(rdram, head, 0xFD100000, address);
        cmd(rdram, head, 0xE8000000, 0);
        cmd(rdram, head, 0xF5000100, 0x07000000);
        cmd(rdram, head, 0xE6000000, 0);
        cmd(rdram, head, 0xF0000000, 0x073FC000);
        cmd(rdram, head, 0xE7000000, 0);
        MEM_W(kDlHead, lib) = (int32_t)head;
    }
}

void wa::set_hide_level_border(bool hide) {
    hide_border.store(hide);
}

// From wa_ws_clip_9320 (src/game/widescreen.cpp), with the landscape about to
// be drawn: lib is the drawing library (its display list head at +0x357C).
extern "C" void wa_level_border_begin(uint8_t* rdram, gpr lib) {
    palette_swapped = false;
    if (!hide_border.load()) {
        return;
    }
    for (int i = 0; i < 256; i++) {
        MEM_H(i * 2, (gpr)(int32_t)kBorderlessPalette) = MEM_H(i * 2, (gpr)(int32_t)kGamePalette);
    }
    MEM_H(1 * 2, (gpr)(int32_t)kBorderlessPalette) = 0;
    MEM_H(4 * 2, (gpr)(int32_t)kBorderlessPalette) = 0;
    load_palette(rdram, lib, kBorderlessPalette);
    palette_swapped = true;
}

// func_80009320's return (0x80009588), after the landscape's background
// command: the game's palette again.
extern "C" void wa_level_border_end(uint8_t* rdram, recomp_context* ctx) {
    if (palette_swapped) {
        load_palette(rdram, ctx->r19, kGamePalette);
        palette_swapped = false;
    }
}
