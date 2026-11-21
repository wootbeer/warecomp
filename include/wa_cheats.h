#ifndef __WA_CHEATS_H__
#define __WA_CHEATS_H__

#include <cstdint>
#include <string>
#include <vector>

// The Cheats and Hacks tabs (src/main/cheats_config.cpp) and what each code
// does in the game (src/game/cheats.cpp). From btgarecomp's.
namespace wa::cheats {
    enum class Section {
        Stock,       // the game's own codes (Cheats tab)
        GameShark,   // Hacks tab
        ActionReplay,
    };

    struct Choice {
        std::string key;  // saved in cheats.json / hacks.json
        std::string name; // shown in the menu
    };

    struct Cheat {
        std::string id;
        std::string name;
        std::string description;
        Section section;
        // Empty: an on/off toggle. Otherwise the choices of a picker, the
        // first one being "off".
        std::vector<Choice> choices;
    };

    // Every code, in menu order.
    const std::vector<Cheat>& list();

    // 0/1 for a toggle, the choice otherwise. Applied on the game's next frame.
    void set_value(size_t index, uint32_t value);
    uint32_t get_value(size_t index);

    // Adds the Cheats tab (when the game has codes for it) and the Hacks tab.
    // Call between the other create_*_tab() calls and
    // recompui::config::finalize().
    void add_cheats_tabs();
}

#endif
