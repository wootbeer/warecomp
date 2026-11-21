// Widescreen (STATUS.md round 7).
//
// The game is 2D: everything is drawn with S2DEX2 sprites and backgrounds by a
// small drawing library in the resident code. A match puts its whole scene in
// a draw list (game_8005FC70 walks it; game_800614CC projects world to screen
// with the camera at the view's top-left), and only the library's fixed
// screen bounds, x in [0, 320] and y in [0, 240], decide what's drawn and
// where an object is cut off. RT64's Expand mode keeps the 320 x 240 picture
// in the middle of a wider window and shows whatever is drawn outside it, so
// widening those bounds horizontally to [-WA_WS_EXTRA, 320 + WA_WS_EXTRA]
// fills the sides with the level as the game itself would draw it.
//
// The bounds are fixed (WA_WS_EXTRA, include/wa_recomp_hooks.h), covering
// windows up to about 21.6:9; anything drawn beyond a narrower window's edge
// just isn't visible, and with a 4:3 window RT64 clips to the original area.
//
// Culls are widened in wa.us.toml (right edges by instruction patches, left
// edges by hooks there); the three functions below replace the blocks that
// clip an object or background to the screen's left and right edges.
#include <cstdint>

#include "recomp.h"
#include "wa_recomp_hooks.h"

namespace {
    constexpr int32_t kLeft = -WA_WS_EXTRA;
    constexpr int32_t kRight = 320 + WA_WS_EXTRA;
}

// func_80007C74 (background), 0x80007E84-0x80007ED4. x = $s2, image width in
// pixels = $s1, $fp = that width << 2, the uObjBg being filled = $s0. The
// original: left of 0, start the image -x in and draw it from 0; otherwise
// from x, cut at 320. Here the edges are kLeft and kRight. The frame width
// goes in $v0: the delay slot at 0x80007ED8 stores it.
extern "C" void wa_ws_clip_7c74(uint8_t* rdram, recomp_context* ctx) {
    // The delay slot at 0x80007E88.
    MEM_W(0x3598, ctx->r21) = (int32_t)ctx->r2;

    const int32_t x = (int32_t)ctx->r18;
    const int32_t w = (int32_t)ctx->r17;
    const gpr obj = ctx->r16;
    int32_t frameW;
    if (x < kLeft) {
        const int32_t skip = kLeft - x;
        frameW = w - skip;
        MEM_H(0x0, obj) = (int16_t)(skip << 5);
        MEM_H(0x4, obj) = (int16_t)(kLeft << 2);
    }
    else {
        frameW = w;
        if (kRight - w < x) {
            frameW = kRight - x;
        }
        MEM_H(0x0, obj) = 0;
        MEM_H(0x4, obj) = (int16_t)(x << 2);
        ctx->r8 = kRight;
    }
    MEM_H(0x2, obj) = (int16_t)ctx->r30;
    ctx->r3 = frameW;
    ctx->r2 = (gpr)(int32_t)(frameW << 2);
}

// func_80008010 (blitter), 0x80008284-0x800082D8. x = $s1, width = $s3
// (already cut at the right edge), source x offset = $s7, texture width at
// sp+0x2E, the uObjBg = $s0. Left of kLeft: start the image (kLeft - x) in,
// draw from kLeft; otherwise from x.
extern "C" void wa_ws_clip_8010(uint8_t* rdram, recomp_context* ctx) {
    // The delay slot at 0x80008288.
    MEM_W(0x3598, ctx->r20) = (int32_t)ctx->r2;

    const int32_t x = (int32_t)ctx->r17;
    const int32_t w = (int32_t)ctx->r19;
    const gpr obj = ctx->r16;
    const uint16_t texW = (uint16_t)MEM_HU(0x2E, ctx->r29);
    if (x < kLeft) {
        const int32_t skip = kLeft - x;
        ctx->r17 = skip;
        MEM_H(0x0, obj) = (int16_t)(skip << 5);
        MEM_H(0x4, obj) = (int16_t)(kLeft << 2);
        MEM_H(0x6, obj) = (int16_t)((w - skip) << 2);
    }
    else {
        MEM_H(0x0, obj) = (int16_t)((int32_t)ctx->r23 << 5);
        MEM_H(0x4, obj) = (int16_t)(x << 2);
        MEM_H(0x6, obj) = (int16_t)(w << 2);
    }
    MEM_H(0x2, obj) = (int16_t)(texW << 2);
}

// func_80009320 (background), 0x8000941C-0x80009474. x = $s1, image width =
// $s4, the uObjBg = $s0. The original: left of 0, start the image -x in and
// draw from 0 over at most 320; otherwise from x to 320. The frame width goes
// in $v0: the delay slot at 0x80009478 stores it.
extern "C" void wa_ws_clip_9320(uint8_t* rdram, recomp_context* ctx) {
    // The delay slot at 0x80009420.
    MEM_W(0x3598, ctx->r19) = (int32_t)ctx->r2;

    const int32_t x = (int32_t)ctx->r17;
    const int32_t w = (int32_t)ctx->r20;
    const gpr obj = ctx->r16;
    int32_t frameW;
    if (x < kLeft) {
        const int32_t skip = kLeft - x;
        frameW = kRight - kLeft;
        if (!(skip < w - (kRight - kLeft))) {
            frameW = w - skip;
        }
        ctx->r5 = skip;
        MEM_H(0x0, obj) = (int16_t)(skip << 5);
        MEM_H(0x4, obj) = (int16_t)(kLeft << 2);
    }
    else {
        frameW = kRight - x;
        MEM_H(0x0, obj) = 0;
        MEM_H(0x4, obj) = (int16_t)(x << 2);
    }
    MEM_H(0x2, obj) = (int16_t)(w << 2);
    ctx->r3 = frameW;
    ctx->r2 = (gpr)(int32_t)(frameW << 2);
}
