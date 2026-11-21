// Game options: local multiplayer and the mouse stick on the General tab; the
// screen filter, bloom and frame generation on the Graphics tab.
// See include/wa_config.h.
#include "wa_config.h"

#include "recompui/config.h"
#include "rt64_screen_filter.h"

namespace {
    const std::string local_multiplayer_id = "local_multiplayer";

    // Looked up by id on each read: the Config& create_general_tab() returns
    // lives in a container that moves when later tabs are created, so a
    // pointer kept from it dangles (btgarecomp round 96).
    bool options_added = false;
}

void wa::config::add_general_options(recomp::config::Config& general) {
    options_added = true;

    general.add_bool_option(
        local_multiplayer_id,
        "Local Multiplayer",
        "Lets up to 4 players use their own controllers."
        "<br/><br/>"
        "<b>Restart the game after changing this.</b> Then open the Controls tab and use "
        "<b>Assign players</b> to choose each player's controller or keyboard.",
        false
    );

    general.add_enum_option(
        "mouse_stick",
        "Mouse Stick",
        "Moves player 1's analog stick with the mouse. <b>Turn</b> uses left-right motion only; "
        "<b>Full</b> also uses up-down motion. The mouse is held in the window while you play; open the "
        "menu to free it."
        "<br/><br/>"
        "Mouse buttons, the side buttons and the scroll wheel can be bound on the Controls tab.",
        { {0u, "Off", "Off"}, {1u, "Turn", "Turn"}, {2u, "Full", "Full"} },
        0u
    );
    general.add_number_option(
        "mouse_stick_sensitivity",
        "Mouse Stick Sensitivity",
        "How far the stick moves for the mouse's motion.",
        5, 300, 1, 0, true, 100
    );
    general.add_option_disable_dependency("mouse_stick_sensitivity", "mouse_stick", 0u);

    // Also called when the saved settings load.
    static int mouse_mode = 0;
    static double mouse_sensitivity = 100.0;
    general.add_option_change_callback("mouse_stick",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            mouse_mode = (int)std::get<uint32_t>(value);
            wa::set_mouse_stick(mouse_mode, mouse_sensitivity);
        });
    general.add_option_change_callback("mouse_stick_sensitivity",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            mouse_sensitivity = std::get<double>(value);
            wa::set_mouse_stick(mouse_mode, mouse_sensitivity);
        });
}

void wa::config::add_graphics_options(recomp::config::Config& graphics) {
    const std::vector<recomp::config::ConfigOptionEnumOption> screen_filter_options = {
        {RT64::ScreenFilter::None, "None", "None"},
        {RT64::ScreenFilter::CRT, "CRT", "CRT"},
        {RT64::ScreenFilter::Scanlines, "Scanlines", "Scanlines"},
        {RT64::ScreenFilter::LCD, "LCD", "LCD"},
    };
    const std::vector<recomp::config::ConfigOptionEnumOption> bloom_options = {
        {RT64::Bloom::Off, "Off", "Off"},
        {RT64::Bloom::Low, "Low", "Low"},
        {RT64::Bloom::High, "High", "High"},
    };

    graphics.add_enum_option(
        "screen_filter",
        "Screen Filter",
        "Makes the picture look like it does on a screen of the time: "
        "<b>CRT</b> a TV, <b>Scanlines</b> just the TV's lines, <b>LCD</b> a handheld's pixel grid.",
        screen_filter_options,
        RT64::ScreenFilter::None
    );
    graphics.add_enum_option(
        "bloom",
        "Bloom",
        "A glow around bright, strongly coloured parts of the picture, like explosions and fire.",
        bloom_options,
        RT64::Bloom::Off
    );

    graphics.add_bool_option(
        "hide_level_border",
        "Hide Level Border",
        "Hides the striped border the game draws around the level's bounds. The bounds are still there: "
        "worms and weapons stop at them as before.",
        false
    );

    wa::config::add_frame_generation_options(graphics);

    // Also called when the saved settings load.
    graphics.add_option_change_callback("screen_filter",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            RT64::setScreenFilter(static_cast<RT64::ScreenFilter>(std::get<uint32_t>(value)));
        });
    graphics.add_option_change_callback("hide_level_border",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            wa::set_hide_level_border(std::get<bool>(value));
        });
    graphics.add_option_change_callback("bloom",
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            RT64::setBloom(static_cast<RT64::Bloom>(std::get<uint32_t>(value)));
        });
}

bool wa::config::get_local_multiplayer() {
    if (!options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(local_multiplayer_id));
}
