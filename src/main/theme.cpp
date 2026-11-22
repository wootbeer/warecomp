// The launcher's and menus' colours: Worms Armageddon's reds, oranges and yellows (the rusty
// radiation symbol of its logo and app icon) on dark rust-brown backgrounds.
//
// recompui's default palette (lib/RecompFrontend/recompui/src/elements/ui_theme.cpp) is blue;
// every blue colour is set here instead. The backgrounds stay as dark as the defaults (about the
// same brightness, a warm hue), so text keeps its contrast. Primary (selections, titles) is a
// burnt orange-red and Secondary a warm yellow. Greys and whites, the semantic colours (warning,
// danger, success) and the controller button colours (A is the N64's blue) are left as they are.
// Must run before the UI builds its stylesheet, i.e. before recomp::start.
#include "elements/ui_theme.h"

namespace {
    using recompui::theme::color;

    void set(color c, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        recompui::theme::set_theme_color(c, recompui::Color{ r, g, b, a });
    }

    // A colour and its translucent variants (5%, 20%, 30%, 50% and 80%, as the defaults).
    void set_family(color base, color a5, color a20, color a30, color a50, color a80, uint8_t r, uint8_t g, uint8_t b) {
        set(base, r, g, b);
        set(a5, r, g, b, 13);
        set(a20, r, g, b, 51);
        set(a30, r, g, b, 77);
        set(a50, r, g, b, 128);
        set(a80, r, g, b, 204);
    }
}

namespace wa {
    void apply_theme() {
        // Backgrounds: deep rust-brown, darkest first.
        set(color::Background1, 18, 7, 3);
        set(color::Background2, 32, 13, 6);
        set(color::Background3, 44, 22, 12);
        set(color::BGOverlay, 232, 180, 140, 26);
        set(color::ModalOverlay, 18, 7, 3, 229);
        set(color::BGShadow2, 18, 7, 3, 184);

        // Primary: burnt orange-red.
        set_family(color::Primary, color::PrimaryA5, color::PrimaryA20, color::PrimaryA30, color::PrimaryA50, color::PrimaryA80,
            214, 84, 28);
        set(color::PrimaryL, 245, 192, 150);
        set(color::PrimaryD, 110, 34, 8);

        // Secondary: warm yellow.
        set_family(color::Secondary, color::SecondaryA5, color::SecondaryA20, color::SecondaryA30, color::SecondaryA50, color::SecondaryA80,
            236, 180, 40);
        set(color::SecondaryL, 252, 226, 150);
        set(color::SecondaryD, 196, 138, 12);

        // Raised panels: a warm tint instead of the blue-grey one.
        set(color::Elevated, 232, 180, 140, 26);
        set(color::ElevatedSoft, 232, 180, 140, 13);
        set(color::ElevatedBorder, 232, 180, 140, 204);
        set(color::ElevatedBorderHard, 232, 180, 140, 255);
    }
}
