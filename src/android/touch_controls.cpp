// On-screen touch controls (Android): an N64 controller on the screen, on top of descore's touch
// module (src/android/descore/touch), as snprecomp's and BattleTanx's touch_controls.cpp are.
// Unlike theirs it maps straight to the pad: the analog stick, A, B, Z, L, R, START and the four C
// buttons, plus MENU (the game's menu). Worms-specific controls may come later (android/PLAN.md).
//
// Java (TouchControlsView) runs everything here on the UI thread: it forwards touches, the screen
// size and whether a gamepad is connected, and draws the buttons from descore_touch_get_shapes().
// While the game takes input every finger comes here; one on a button or the stick goes to
// descore, others are ignored (and kept from the game, where SDL would make them mouse clicks).
// The settings come from the Touch tab (src/main/game_config.cpp), and the game thread reads the
// pad (add_touch_input), both through atomics.
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

extern "C" {
#include "descore/touch/descore_touch.h"
}

#include "wa_android.h"
#include "recompinput/input_state.h"
#include "ultramodern/ultramodern.hpp"

namespace {
    // Buttons, in descore's button order; a held button sends its own index as its one key.
    enum Button {
        BUTTON_A,
        BUTTON_B,
        BUTTON_Z,
        BUTTON_L,
        BUTTON_R,
        BUTTON_START,
        BUTTON_C_UP,
        BUTTON_C_DOWN,
        BUTTON_C_LEFT,
        BUTTON_C_RIGHT,
        BUTTON_MENU, // a tap button: opens the game's menu (Java sends Escape)
        BUTTON_COUNT
    };

    // libultra's controller button bits, in Button order.
    constexpr uint16_t kN64Bits[] = {
        0x8000, // A
        0x4000, // B
        0x2000, // Z
        0x0020, // L
        0x0010, // R
        0x1000, // START
        0x0008, // C up
        0x0004, // C down
        0x0002, // C left
        0x0001, // C right
    };

    const char* const kLabels[BUTTON_COUNT] = { "A", "B", "Z", "L", "R", "START", "C▲", "C▼", "C◀", "C▶", "MENU" };

    std::atomic<uint32_t> held_buttons{ 0 };
    // The stick, x right and y up, -1 to 1; both 0 when no finger holds it.
    std::atomic<float> stick_x{ 0.0f };
    std::atomic<float> stick_y{ 0.0f };
    std::atomic<bool> stick_held{ false };

    // Settings, from the Touch tab; Java lays the buttons out again when they change.
    std::atomic<wa::android::TouchButtons> setting_buttons{ wa::android::TouchButtons::Auto };
    std::atomic<double> setting_size{ 100.0 };
    std::atomic<double> setting_opacity{ 30.0 };
    std::atomic<bool> settings_changed{ false };

    bool gamepad_connected = false;
    bool menu_tapped = false;

    // Which fingers descore has (a button or the stick).
    bool owned[DESCORE_TOUCH_MAX_POINTERS] = {};

    // The descore Size step nearest to a size in percent.
    int scale_step_for(double percent) {
        int best = DESCORE_TOUCH_SCALE_DEFAULT_STEP;
        for (int i = 0; i < DESCORE_TOUCH_SCALE_NUM_STEPS; i++) {
            if (std::abs(kDescoreTouchScaleValues[i] * 100.0 - percent) <
                std::abs(kDescoreTouchScaleValues[best] * 100.0 - percent)) {
                best = i;
            }
        }
        return best;
    }

    void on_key(unsigned char key, int down) {
        if (key >= BUTTON_MENU) {
            return;
        }
        if (down) {
            held_buttons.fetch_or(kN64Bits[key]);
        }
        else {
            held_buttons.fetch_and(~(uint32_t)kN64Bits[key]);
        }
    }

    void on_tap(int button) {
        if (button == BUTTON_MENU) {
            menu_tapped = true;
        }
    }

    // descore's y grows downwards; the N64's stick y grows upwards.
    void on_stick(int, float x, float y) {
        stick_x.store(std::clamp(x, -1.0f, 1.0f));
        stick_y.store(std::clamp(-y, -1.0f, 1.0f));
        stick_held.store(x != 0.0f || y != 0.0f);
    }

    void set_button(Button button, float x, float y, float w, float h) {
        const unsigned char key = (unsigned char)button;
        if (button == BUTTON_MENU) {
            descore_touch_set_button(button, x, y, w, h, nullptr, 0, false);
        }
        else {
            descore_touch_set_button(button, x, y, w, h, &key, 1, false);
        }
    }

    // An N64 pad's layout: the stick bottom left with Z above it; L and R in the top corners;
    // A and B bottom right with the C buttons as a diamond above them; START and MENU top centre.
    // Sizes follow the screen height and the Size setting.
    void layout(int width, int height) {
        const float w = (float)width;
        const float h = (float)height;
        const float scale = descore_touch_scale_value(descore_touch_scale_step());
        const float c = std::min(std::max(h * 0.12f * scale, descore_touch_min_cell_px()), h * 0.17f);
        const float m = h * 0.04f;
        const float gap = c * 0.12f;

        descore_touch_set_screen_size(width, height);

        // Stick and Z.
        const float r = c * 1.25f;
        const float stick_cx = m + r * 1.15f;
        const float stick_cy = h - m - r * 1.15f;
        descore_touch_set_stick(0, stick_cx, stick_cy, r, on_stick);
        descore_touch_set_stick_style(0, DESCORE_TOUCH_STICK_ANALOG);
        set_button(BUTTON_Z, m, stick_cy - r * 1.15f - gap - c, c * 1.2f, c);

        // Shoulders.
        set_button(BUTTON_L, m, m, c * 1.6f, c * 0.7f);
        set_button(BUTTON_R, w - m - c * 1.6f, m, c * 1.6f, c * 0.7f);

        // A, B.
        const float a = c * 1.2f;
        const float a_x = w - m - a - c * 0.35f;
        const float a_y = h - m - a;
        set_button(BUTTON_A, a_x, a_y, a, a);
        set_button(BUTTON_B, a_x - gap - c, a_y - c * 0.55f, c, c);

        // C buttons, a diamond above A.
        const float cs = c * 0.8f;
        const float ccx = w - m - cs * 1.5f;
        const float ccy = a_y - gap - cs * 1.5f;
        set_button(BUTTON_C_UP, ccx - cs * 0.5f, ccy - cs * 1.5f, cs, cs);
        set_button(BUTTON_C_DOWN, ccx - cs * 0.5f, ccy + cs * 0.5f, cs, cs);
        set_button(BUTTON_C_LEFT, ccx - cs * 1.5f, ccy - cs * 0.5f, cs, cs);
        set_button(BUTTON_C_RIGHT, ccx + cs * 0.5f, ccy - cs * 0.5f, cs, cs);

        // START and MENU top centre (lower down they'd cover the game's HUD).
        set_button(BUTTON_MENU, w * 0.5f - c * 1.4f - gap * 0.5f, m, c * 1.4f, c * 0.6f);
        set_button(BUTTON_START, w * 0.5f + gap * 0.5f, m, c * 1.4f, c * 0.6f);
    }

    bool buttons_hidden() {
        switch (setting_buttons.load()) {
            case wa::android::TouchButtons::Always: return false;
            case wa::android::TouchButtons::Never: return true;
            default: return gamepad_connected;
        }
    }

    void release_pointer(int id, float x, float y) {
        if (owned[id]) {
            descore_touch_handle(DESCORE_TOUCH_ACTION_POINTER_UP, id, x, y);
        }
        owned[id] = false;
    }

    bool initialized = false;
}

void wa::android::set_touch_buttons(TouchButtons buttons) {
    setting_buttons.store(buttons);
    settings_changed.store(true);
}

void wa::android::set_touch_size(double percent) {
    setting_size.store(percent);
    settings_changed.store(true);
}

void wa::android::set_touch_opacity(double percent) {
    setting_opacity.store(percent);
    settings_changed.store(true);
}

void wa::android::add_touch_input(uint16_t* buttons, float* x, float* y) {
    *buttons |= (uint16_t)held_buttons.load();
    if (stick_held.load()) {
        *x = stick_x.load();
        *y = stick_y.load();
    }
}

extern "C" {

// Screen size; lays the buttons out again with the current settings.
JNIEXPORT void JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativeLayout(
    JNIEnv*, jclass, jint width, jint height, jfloat density) {
    if (!initialized) {
        descore_touch_init(BUTTON_COUNT, on_key, on_tap);
        initialized = true;
    }
    settings_changed.store(false);
    descore_touch_set_display_density(density);
    descore_touch_set_scale_step(scale_step_for(setting_size.load()));
    descore_touch_set_opacity((float)(setting_opacity.load() / 100.0));
    layout(width, height);
    descore_touch_set_gamepad_connected(buttons_hidden());
}

// One pointer of a touch event (Android's action codes, which descore shares). Bit 0: taken;
// bit 1: MENU was tapped; bit 2: a button or the stick had it, so the buttons need drawing again.
JNIEXPORT jint JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativeTouch(
    JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    menu_tapped = false;
    if (!initialized || pointer_id < 0 || pointer_id >= DESCORE_TOUCH_MAX_POINTERS) {
        return 0;
    }
    bool buttons = owned[pointer_id];
    switch (action) {
        case DESCORE_TOUCH_ACTION_DOWN:
        case DESCORE_TOUCH_ACTION_POINTER_DOWN:
            release_pointer(pointer_id, x, y);
            if (!buttons_hidden() && descore_touch_hit_test(x, y)) {
                owned[pointer_id] = true;
                descore_touch_handle(action, pointer_id, x, y);
                buttons = true;
            }
            break;
        case DESCORE_TOUCH_ACTION_MOVE:
            if (owned[pointer_id]) {
                descore_touch_handle(action, pointer_id, x, y);
            }
            break;
        case DESCORE_TOUCH_ACTION_UP:
        case DESCORE_TOUCH_ACTION_POINTER_UP:
            release_pointer(pointer_id, x, y);
            break;
        default:
            break;
    }
    return 1 | (menu_tapped ? 2 : 0) | (buttons ? 4 : 0);
}

// Lets go of every finger (the game's menu took the input, or the gesture was cancelled), so
// nothing stays held.
JNIEXPORT void JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativeReleaseAll(
    JNIEnv*, jclass) {
    for (int id = 0; id < DESCORE_TOUCH_MAX_POINTERS; id++) {
        release_pointer(id, 0.0f, 0.0f);
    }
    held_buttons.store(0);
    stick_held.store(false);
}

// Whether a gamepad is connected (a handheld's built-in one too): with Buttons on Auto they hide.
JNIEXPORT void JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativeSetGamepadConnected(
    JNIEnv*, jclass, jboolean connected) {
    gamepad_connected = connected;
    if (initialized) {
        descore_touch_set_gamepad_connected(buttons_hidden());
    }
}

// Shapes to draw, 7 floats each (kind, button, x, y, w, h, alpha); returns how many.
JNIEXPORT jint JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativeGetShapes(
    JNIEnv* env, jclass, jfloatArray out) {
    if (!initialized) {
        return 0;
    }
    DescoreTouchShape shapes[DESCORE_TOUCH_MAX_BUTTONS + 16];
    const jsize capacity = env->GetArrayLength(out) / 7;
    const int count = descore_touch_get_shapes(shapes, std::min<int>(capacity, (int)(sizeof(shapes) / sizeof(shapes[0]))));
    jfloat* values = env->GetFloatArrayElements(out, nullptr);
    for (int i = 0; i < count; i++) {
        jfloat* v = values + i * 7;
        v[0] = (jfloat)shapes[i].kind;
        v[1] = (jfloat)shapes[i].button;
        v[2] = shapes[i].x;
        v[3] = shapes[i].y;
        v[4] = shapes[i].w;
        v[5] = shapes[i].h;
        v[6] = shapes[i].alpha;
    }
    env->ReleaseFloatArrayElements(out, values, 0);
    return count;
}

JNIEXPORT jstring JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativeButtonLabel(
    JNIEnv* env, jclass, jint button) {
    return env->NewStringUTF((button >= 0 && button < BUTTON_COUNT) ? kLabels[button] : "");
}

// Bit 0: touch is for the game now -- it's running and takes input (not on the launcher, nor
// while one of the game's menus has it). Bit 1: the settings changed, lay out again.
JNIEXPORT jint JNICALL Java_io_github_wootbeer_warecomp_TouchControlsView_nativePoll(
    JNIEnv*, jclass) {
    const bool started = ultramodern::is_game_started();
    const bool active = started && !recompinput::game_input_disabled();
    return (active ? 1 : 0) | (settings_changed.load() ? 2 : 0);
}

}
