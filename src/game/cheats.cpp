// What the Cheats and Hacks tabs do in the game (include/wa_cheats.h). From
// btgarecomp's src/game/cheats.cpp, without its netplay and its hooks.
//
// Stock codes are the game's own: the N64 version has no cheat menu, but a new
// team's name is checked against 34 secret names (front_8005EC64), and some
// of them switch things on (STATUS.md round 16-17). The toggles do what the
// name does: most set a flag in the settings' flag array (only the front end
// reads it), kept set while on and cleared once when turned off, like the
// game's own flag (typing the name in the game still works).
//
// GameShark codes are the community's codes for this ROM. They write to the
// game's memory, applied once a frame here as the cheat device did (codes 80,
// 81, D0-D3 and 50).
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include "wa_cheats.h"
#include "recomp.h"
#include "wa_recomp_hooks.h" // wa_perf_frame

namespace {
    using wa::cheats::Cheat;
    using wa::cheats::Choice;
    using wa::cheats::Section;

    struct Entry {
        Cheat cheat;
        // Memory codes, one list per choice after "off" (one list for a toggle).
        std::vector<std::vector<std::string>> codes = {};
        // Put the original bytes back when turned off.
        bool restore = false;
        // Run once when turned off (the stock flags: cleared).
        std::vector<std::string> off_codes = {};
    };

    std::string hex(uint32_t value, int digits) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%0*X", digits, value);
        return buf;
    }

    std::string code_text(const char* device, const std::vector<std::string>& codes) {
        std::string ret = std::string(device) + (codes.size() > 2 ? " code:<br/><b>" : " code: <b>");
        for (size_t i = 0; i < codes.size(); i++) {
            ret += (i ? (i % 2 ? " " : "<br/>") : "") + codes[i];
        }
        return ret + "</b>";
    }

    Entry gs(const char* id, const char* name, const char* text, std::vector<std::string> codes, bool restore = true) {
        std::string description = std::string(text) + (text[0] ? "<br/><br/>" : "") + code_text("GameShark", codes);
        return { { id, name, description, Section::GameShark, {} }, { codes }, restore };
    }

    // A code made for each of n teams or worms, `step` bytes apart, as a
    // picker: off, each one, or all of them at once.
    Entry gs_each(const char* id, const char* name, const char* text, const char* what, int n,
                  const std::vector<std::pair<uint32_t, uint16_t>>& lines, uint32_t step, bool restore) {
        Entry e{ { id, name, "", Section::GameShark, { { "Off", "Off" } } }, {}, restore };
        std::vector<std::string> all;
        for (int k = 0; k < n; k++) {
            std::vector<std::string> codes;
            for (const auto& [address, value] : lines) {
                // Conditional lines (D0-D3) test the same address for every one.
                const bool conditional = (address >> 24) >= 0xD0;
                codes.push_back(hex(address + (conditional ? 0 : step * k), 8));
                codes.push_back(hex(value, 4));
            }
            e.cheat.choices.push_back({ std::string(what) + std::to_string(k + 1), std::string(what) + " " + std::to_string(k + 1) });
            all.insert(all.end(), codes.begin(), codes.end());
            e.codes.push_back(codes);
        }
        e.cheat.choices.push_back({ "All", std::string("All ") + what + "s" });
        e.codes.push_back(all);
        e.cheat.description = std::string(text) + "<br/><br/>" + code_text("GameShark", e.codes[0]) +
            "<br/>(" + what + " 1; the others are the same, " + hex(step, 2) + " bytes further on each)";
        return e;
    }

    // The settings' flag array (*0x8008EDAC + 4, + 0x1BE): a static block in
    // the resident segment's data.
    constexpr uint32_t kFlags = 0x80041B76;

    std::string flag_code(int flag, int value) {
        return hex(0x80000000u | ((kFlags + flag) & 0xFFFFFF), 8);
    }

    Entry stock_flag(const char* id, const char* name, const char* team_name, const char* text, int flag) {
        std::string description = std::string(text) + "<br/><br/>The game's code: name a new team <b>" + team_name + "</b>.";
        return { { id, name, description, Section::Stock, {} }, { { flag_code(flag, 1), "0001" } }, false, { flag_code(flag, 0), "0000" } };
    }

    std::vector<Entry> make_entries() {
        std::vector<Entry> e;

        // The game's own codes (front_8005F05C's cases).
        {
            Entry scheme{ { "hidden_scheme", "Hidden Scheme",
                "Every game except the missions is played with one of two schemes the menus don't offer: "
                "<b>Pow Pow</b> or <b>World War 3</b> (their weapons and options).<br/><br/>"
                "The game's code: name a new team <b>3xmr8f56</b> (once for Pow Pow, twice for World War 3, a third time "
                "to turn it off).",
                Section::Stock, { { "Off", "Off" }, { "PowPow", "Pow Pow" }, { "WorldWar3", "World War 3" } } },
                { { flag_code(0, 1), "0001" }, { flag_code(0, 2), "0002" } }, false, { flag_code(0, 0), "0000" } };
            e.push_back(scheme);
        }
        e.push_back(stock_flag("full_arsenal", "Intermediate Scheme Instead of Tournament", "Toupeter",
            "The mode that starts with the Tournament scheme (few weapons; Deathmatch, as far as the code shows) "
            "starts with the Intermediate scheme instead.", 0x17));
        e.push_back(stock_flag("extra_choice", "Extra Team Choice", "23gu5sbr",
            "Adds a sixth choice to a five-choice list on the Create Team screen, and stops teams of one kind (type 7) "
            "losing one from that setting. Found in the code; not yet seen in play.", 5));
        {
            std::vector<std::string> codes;
            for (int k = 0; k < 6; k++) {
                codes.push_back(hex(0x800C5BD7 + 0x100 * k, 8)); codes.push_back("001C");
                codes.push_back(hex(0x800C5BD9 + 0x100 * k, 8)); codes.push_back("000B");
            }
            e.push_back({ { "all_missions_training", "All Missions and Training",
                "Every mission and training level is open for every team (the game's own values). Saved with the teams, "
                "so it stays after turning this off.<br/><br/>"
                "The game's code: name a new team <b>F5x3e964</b> (for that team).",
                Section::Stock, {} }, { codes }, false });
        }

        // GameShark (US ROM). Left out: the button activators (D0044984,
        // D0044985, D1044984), which only make another code wait for a
        // button, and "Have All Training Levels (1st Created Team)", the same
        // as Open All Training for team 1.
        e.push_back(gs_each("gs_all_missions", "Open All Missions",
            "Every mission is open for the team. Saved with the team, so it stays after turning this off.",
            "Team", 6, { { 0x800C5BD7, 0x001B } }, 0x100, false));
        e.push_back(gs_each("gs_all_training", "Open All Training",
            "Every training level is open for the team. Saved with the team, so it stays after turning this off.",
            "Team", 6, { { 0x800C5BD9, 0x000B } }, 0x100, false));
        e.push_back(gs("gs_basic_training", "Enable Basic Training Levels", "", { "810C8D80", "000B" }, false));
        e.push_back(gs_each("gs_infinite_health", "Infinite Health",
            "The worm's health stays at 255. Made for the original GameShark 3.0 and up; worms are counted in "
            "the order the match sets them up.",
            "Worm", 6, { { 0xD125FF9C, 0x8004 }, { 0x81260022, 0x00FF }, { 0xD1260FEC, 0x8004 }, { 0x81261072, 0x00FF } },
            0xA0, false));
        return e;
    }

    const std::vector<Entry>& entries() {
        static const std::vector<Entry> list = make_entries();
        return list;
    }

    // Set by the menu, read by the game thread.
    std::vector<std::atomic<uint32_t>>& wanted() {
        static std::vector<std::atomic<uint32_t>> values(entries().size());
        return values;
    }

    struct Code {
        uint8_t type;
        uint32_t addr;
        uint16_t value;
    };

    std::vector<Code> parse(const std::vector<std::string>& text) {
        std::vector<Code> ret;
        for (size_t i = 0; i + 1 < text.size(); i += 2) {
            uint32_t word = (uint32_t)std::strtoul(text[i].c_str(), nullptr, 16);
            ret.push_back({ (uint8_t)(word >> 24), 0x80000000u | (word & 0xFFFFFF), (uint16_t)std::strtoul(text[i + 1].c_str(), nullptr, 16) });
        }
        return ret;
    }

    // The game thread's own state for each entry.
    struct Applied {
        uint32_t value = 0;
        std::vector<std::vector<Code>> codes;
        std::unordered_map<uint32_t, uint8_t> saved; // original bytes, for restore
    };

    uint8_t read8(uint8_t* rdram, uint32_t addr) {
        return (uint8_t)MEM_B(0, (gpr)(int32_t)addr);
    }

    void write8(uint8_t* rdram, Applied& state, bool restore, uint32_t addr, uint8_t value) {
        if (addr >= 0x80400000u) {
            return;
        }
        if (restore) {
            state.saved.emplace(addr, read8(rdram, addr));
        }
        MEM_B(0, (gpr)(int32_t)addr) = (int8_t)value;
    }

    // 16-bit writes go in as two bytes, which also covers odd addresses.
    void write(uint8_t* rdram, Applied& state, bool restore, uint8_t type, uint32_t addr, uint16_t value) {
        if (type == 0x80) {
            write8(rdram, state, restore, addr, (uint8_t)value);
        }
        else if (type == 0x81) {
            write8(rdram, state, restore, addr, (uint8_t)(value >> 8));
            write8(rdram, state, restore, addr + 1, (uint8_t)value);
        }
    }

    uint16_t read16(uint8_t* rdram, uint32_t addr) {
        return (uint16_t)((read8(rdram, addr) << 8) | read8(rdram, addr + 1));
    }

    void run_codes(uint8_t* rdram, Applied& state, bool restore, const std::vector<Code>& codes) {
        for (size_t i = 0; i < codes.size(); i++) {
            const Code& c = codes[i];
            switch (c.type) {
                case 0x80:
                case 0x81:
                    write(rdram, state, restore, c.type, c.addr, c.value);
                    break;
                // Conditionals: the next line only runs if the test passes.
                case 0xD0: if (read8(rdram, c.addr) != (c.value & 0xFF)) i++; break;
                case 0xD1: if (read16(rdram, c.addr) != c.value) i++; break;
                case 0xD2: if (read8(rdram, c.addr) == (c.value & 0xFF)) i++; break;
                case 0xD3: if (read16(rdram, c.addr) == c.value) i++; break;
                // Repeat: 5000nnss vvvv runs the next line nn times, ss bytes
                // and vvvv apart.
                case 0x50:
                    if (i + 1 < codes.size()) {
                        const Code& next = codes[++i];
                        uint32_t count = (c.addr >> 8) & 0xFF;
                        uint32_t step = c.addr & 0xFF;
                        for (uint32_t k = 0; k < count; k++) {
                            write(rdram, state, restore, next.type, next.addr + k * step, (uint16_t)(next.value + k * c.value));
                        }
                    }
                    break;
            }
        }
    }

    void restore_bytes(uint8_t* rdram, Applied& state) {
        for (auto& [addr, value] : state.saved) {
            MEM_B(0, (gpr)(int32_t)addr) = (int8_t)value;
        }
        state.saved.clear();
    }
}

const std::vector<Cheat>& wa::cheats::list() {
    static const std::vector<Cheat> list = [] {
        std::vector<Cheat> ret;
        for (const Entry& e : entries()) {
            ret.push_back(e.cheat);
        }
        return ret;
    }();
    return list;
}

uint32_t wa::cheats::get_value(size_t index) {
    return index < wanted().size() ? wanted()[index].load() : 0;
}

void wa::cheats::set_value(size_t index, uint32_t value) {
    if (index < wanted().size()) {
        wanted()[index].store(value);
    }
}

// Once a frame, after each swap (the hook in wa.us.toml's func_80033948), on
// the menus and in game.
extern "C" void wa_cheats_frame(uint8_t* rdram) {
    wa_perf_frame();
    static std::vector<Applied> applied = [] {
        std::vector<Applied> ret(entries().size());
        for (size_t i = 0; i < entries().size(); i++) {
            for (const auto& codes : entries()[i].codes) {
                ret[i].codes.push_back(parse(codes));
            }
        }
        return ret;
    }();

    for (size_t i = 0; i < entries().size(); i++) {
        const Entry& e = entries()[i];
        Applied& state = applied[i];
        uint32_t value = wanted()[i].load();
        if (value != state.value) {
            restore_bytes(rdram, state);
            if (value == 0 && !e.off_codes.empty()) {
                run_codes(rdram, state, false, parse(e.off_codes));
            }
            state.value = value;
        }
        if (value == 0) {
            continue;
        }
        if (value - 1 < state.codes.size()) {
            run_codes(rdram, state, e.restore, state.codes[value - 1]);
        }
    }
}
