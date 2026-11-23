// Frame rate in the log (Android): every 10 seconds of play, one line with the game's frames per
// second, the longest gap between two frames, and how many frames took over 50 ms. Players'
// shared logs then show how fast a phone runs the game, and whether it slows down all the time
// or in bursts (explosions, animations). Called once per game frame, after its buffer swap
// (wa_cheats_frame, from the hook in func_80033948).
#include <chrono>
#include <cstdio>

#include "wa_recomp_hooks.h"

extern "C" void wa_perf_frame() {
#if defined(__ANDROID__)
    using clock = std::chrono::steady_clock;
    static clock::time_point window_start{};
    static clock::time_point last{};
    static int frames = 0;
    static int slow = 0;
    static double longest_ms = 0.0;

    const clock::time_point now = clock::now();
    if (window_start == clock::time_point{}) {
        window_start = last = now;
        return;
    }
    const double gap_ms = std::chrono::duration<double, std::milli>(now - last).count();
    last = now;
    // A long pause (the settings open, the app in the background) starts a new window.
    if (gap_ms > 2000.0) {
        window_start = now;
        frames = slow = 0;
        longest_ms = 0.0;
        return;
    }
    frames++;
    if (gap_ms > 50.0) slow++;
    if (gap_ms > longest_ms) longest_ms = gap_ms;

    const double window_s = std::chrono::duration<double>(now - window_start).count();
    if (window_s >= 10.0) {
        std::printf("[WA] frame rate: %.1f fps over %.0f s, longest frame %.0f ms, %d over 50 ms\n",
            frames / window_s, window_s, longest_ms, slow);
        std::fflush(stdout);
        window_start = now;
        frames = slow = 0;
        longest_ms = 0.0;
    }
#endif
}
