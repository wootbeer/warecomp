#ifndef __WA_CONFIG_H__
#define __WA_CONFIG_H__

#include <cstdint>

#include "librecomp/config.hpp"
#include "recomp.h"

// Game options on the General and Graphics tabs (src/main/game_config.cpp).
namespace wa::config {
    // Local Multiplayer and Mouse Stick on the General tab. Call between
    // recompui::config::create_general_tab() and recompui::config::finalize().
    void add_general_options(recomp::config::Config& general);

    // Screen Filter and Bloom at the end of the Graphics tab, passed straight
    // to RT64 (lib-patches/rt64/0018, 0019), then frame generation. Call
    // between recompui::config::create_graphics_tab() and
    // recompui::config::finalize().
    void add_graphics_options(recomp::config::Config& graphics);
    // Frame Generation, its Lossless Scaling options and Load Lossless.dll
    // (src/main/frame_generation_config.cpp); from add_graphics_options().
    void add_frame_generation_options(recomp::config::Config& graphics);

    bool get_local_multiplayer();

#if defined(__ANDROID__)
    // The Touch tab (Android): the on-screen buttons, their size and opacity.
    void add_touch_options(recomp::config::Config& touch);
#endif
}

namespace wa {
    // Graphics tab: Hide Level Border (src/game/level_border.cpp).
    void set_hide_level_border(bool hide);
    // Graphics tab: Crop Bottom Line (src/game/widescreen.cpp).
    void set_crop_bottom_line(bool crop);
    // General tab: Mouse Stick (src/main/mouse_stick.cpp). Mode 0 Off, 1 Turn,
    // 2 Full; sensitivity in percent.
    void set_mouse_stick(int mode, double percent);
    // Lets the mouse be captured while Mouse Stick is on. Call once at startup.
    void init_mouse_stick();
    // The input callback: recompinput's, plus the mouse for player 1.
    bool get_n64_input_with_mouse(int player_index, uint16_t* buttons_out, float* x_out, float* y_out);
}

#endif
