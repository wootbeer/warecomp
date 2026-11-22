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
// windows up to about 32:9 (STATUS.md round 15); anything drawn beyond a narrower window's edge
// just isn't visible, and with a 4:3 window RT64 clips to the original area.
//
// Culls are widened in wa.us.toml (right edges by instruction patches, left
// edges by hooks there); the three functions below replace the blocks that
// clip an object or background to the screen's left and right edges.
#include <atomic>
#include <cstdint>
#include <vector>

#include "recomp.h"
#include "wa_recomp_hooks.h"
#include "wa_config.h"

namespace {
    constexpr int32_t kLeft = -WA_WS_EXTRA;
    constexpr int32_t kRight = 320 + WA_WS_EXTRA;

    // HUD anchoring (STATUS.md round 15), used by wa_ws_clip_8010 below.
    void hud_element(uint8_t* rdram, gpr lib, int32_t x, int32_t y, int32_t w, int32_t h);
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

    hud_element(rdram, ctx->r20, (int32_t)ctx->r17, (int32_t)ctx->r18, w, (int32_t)ctx->r21);
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

    // Hide Level Border (src/game/level_border.cpp).
    wa_level_border_begin(rdram, ctx->r19);
}

// --- HUD anchoring (STATUS.md round 15) -----------------------------------
//
// In Expand, RT64 keeps rectangles where the game puts them, in the centred
// 4:3 area, unless the display list anchors them to a window edge (extended
// GBI gEXSetRectAlign); the Graphics tab's HUD Ratio then sets how far out
// an anchored element goes. The match HUD is in the same draw list as the
// level (game_8005FC70 walks it): type-1 entries whose owner (the high half
// of the entry's second word) is 1, each one background drawn by
// func_80008010: the turn timer (left), the teams' names and health bars
// (centred), the wind bar (right) and the weapon panel, which slides in from
// the right. A HUD element that ends left of x 80 is anchored left and one
// that starts at x 200 or right of it is anchored right, keeping the game's
// own margin from the edge; the rest stay centred. An element that touches
// (within 2 pixels) one anchored right in the frame before is anchored right
// too: the weapon panel comes in anchored and stays so as it settles with its
// left part past x 200 (it used to jump back to its 4:3 place there, STATUS.md
// round 18). The team bars end 6 pixels short of the wind bar.
// The command goes in the display list just before the element's own, and
// the next draw-list entry (or the end of the walk) resets it.
namespace {
    constexpr uint16_t kOriginLeft = 0x0;    // G_EX_ORIGIN_LEFT
    constexpr uint16_t kOriginRight = 0x400; // G_EX_ORIGIN_RIGHT
    constexpr uint16_t kOriginNone = 0x800;  // G_EX_ORIGIN_NONE
    // RT64 adds the framebuffer width to right-anchored coordinates, so they
    // are given relative to the right edge (10.2 fixed point).
    constexpr int16_t kRightOriginShift = -320 * 4;
    constexpr uint32_t kDlHead = 0x357C; // the drawing library's display list head

    bool hud_entry = false;
    bool anchored = false;
    gpr anchored_lib = 0;

    struct Rect {
        int32_t x0, y0, x1, y1;
    };
    // HUD elements anchored right in this frame and in the one before.
    std::vector<Rect> right_rects, prev_right_rects;
    constexpr int32_t kTouch = 2;

    bool touches_right_group(const Rect& r) {
        for (const Rect& o : prev_right_rects) {
            if (r.x0 <= o.x1 + kTouch && o.x0 <= r.x1 + kTouch && r.y0 <= o.y1 + kTouch && o.y0 <= r.y1 + kTouch) {
                return true;
            }
        }
        return false;
    }

    void write_align(uint8_t* rdram, gpr lib, uint16_t origin, int16_t offset) {
        gpr head = (gpr)MEM_W(kDlHead, lib);
        auto cmd = [&](uint32_t w0, uint32_t w1) {
            MEM_W(0, head) = (int32_t)w0;
            MEM_W(4, head) = (int32_t)w1;
            head = ADD32(head, 8);
        };
        cmd(0xE0525464, 0x10000064); // gEXEnable (G_SPNOOP with RT64's magic, opcode 0x64)
        cmd(0x64000006, (origin & 0xFFF) | ((uint32_t)(origin & 0xFFF) << 12)); // gEXSetRectAlign
        cmd((uint32_t)(uint16_t)offset << 16, (uint32_t)(uint16_t)offset << 16);
        MEM_W(kDlHead, lib) = (int32_t)head;
    }

    void reset_anchor(uint8_t* rdram) {
        if (anchored) {
            write_align(rdram, anchored_lib, kOriginNone, 0);
            anchored = false;
        }
    }

    void hud_element(uint8_t* rdram, gpr lib, int32_t x, int32_t y, int32_t w, int32_t h) {
        if (!hud_entry) {
            return;
        }
        const Rect r{ x, y, x + w, y + h };
        if (x + w <= 80) {
            write_align(rdram, lib, kOriginLeft, 0);
        }
        else if (x >= 200 || touches_right_group(r)) {
            write_align(rdram, lib, kOriginRight, kRightOriginShift);
            right_rects.push_back(r);
        }
        else {
            return;
        }
        anchored = true;
        anchored_lib = lib;
    }
}

// game_8005FC70, each draw-list entry (0x8005FD1C, the entry in $s1).
extern "C" void wa_hud_entry(uint8_t* rdram, recomp_context* ctx) {
    reset_anchor(rdram);
    const gpr entry = ctx->r17;
    hud_entry = (MEM_W(0x0, entry) == 1) && (((uint32_t)MEM_W(0x4, entry) >> 16) == 1);
}

// Crop Bottom Line (Graphics tab, STATUS.md round 24). A CRT's overscan hid
// the picture's last lines, so the game draws whatever is there: the tips of
// a wave row just below the screen, a sliver of the level. RT64 shows the whole
// picture; with this on, the last line is covered in black after the match's
// draw list, with the game's own fill sequence (as its screen clear: render
// mode, fill cycle, colour, rect, then its 1-cycle mode and render mode back).
namespace {
    std::atomic<bool> crop_bottom_line{ true };

    void draw_bottom_bar(uint8_t* rdram, gpr lib) {
        gpr head = (gpr)MEM_W(kDlHead, lib);
        auto cmd = [&](uint32_t w0, uint32_t w1) {
            MEM_W(0, head) = (int32_t)w0;
            MEM_W(4, head) = (int32_t)w1;
            head = ADD32(head, 8);
        };
        cmd(0xE7000000, 0);          // pipe sync
        cmd(0xE200001C, 0x0F0A4000); // render mode for fills
        cmd(0xE3000A01, 0x00300000); // cycle type: fill
        cmd(0xF7000000, 0x00010001); // fill colour: black
        cmd(0xF65003C0, 0x000003BC); // fill rect 0,239 - 320,240
        cmd(0xE7000000, 0);
        cmd(0xE3000A01, 0x00000000); // cycle type: 1 cycle
        cmd(0xE200001C, 0x00503048); // the drawing library's render mode
        MEM_W(kDlHead, lib) = (int32_t)head;
    }
}

void wa::set_crop_bottom_line(bool crop) {
    crop_bottom_line.store(crop);
}

// game_8005FC70, end of the walk (0x80060504). $s5 is the drawing library.
extern "C" void wa_hud_end(uint8_t* rdram, recomp_context* ctx) {
    reset_anchor(rdram);
    if (crop_bottom_line.load()) {
        draw_bottom_bar(rdram, ctx->r21);
    }
    hud_entry = false;
    prev_right_rects.swap(right_rects);
    right_rects.clear();
}
