// Frame rate in the log (Android): every 10 seconds of play, one line with the game's frames per
// second, the longest gap between two frames, and how many frames took over 50 ms. Players'
// shared logs then show how fast a phone runs the game, and whether it slows down all the time
// or in bursts (explosions, animations). Called once per game frame, after its buffer swap
// (wa_cheats_frame, from the hook in func_80033948). The game runs at 60 frames a second (measured
// on desktop) and its logic advances once a frame, so below 60 it plays in slow motion.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__ANDROID__)
#include <dirent.h>
#include <unistd.h>
#endif

#include "wa_recomp_hooks.h"

#if defined(__ANDROID__)
namespace {
    // CPU time (user + system, in clock ticks) of each of the process's threads, by thread id.
    struct ThreadTime {
        std::string name;
        long long ticks;
    };

    std::unordered_map<int, ThreadTime> read_thread_times() {
        std::unordered_map<int, ThreadTime> out;
        DIR* dir = opendir("/proc/self/task");
        if (dir == nullptr) {
            return out;
        }
        while (dirent* entry = readdir(dir)) {
            const int tid = std::atoi(entry->d_name);
            if (tid <= 0) {
                continue;
            }
            const std::string base = std::string("/proc/self/task/") + entry->d_name;
            char line[1024];
            FILE* f = std::fopen((base + "/stat").c_str(), "r");
            if (f == nullptr) {
                continue;
            }
            size_t n = std::fread(line, 1, sizeof(line) - 1, f);
            std::fclose(f);
            line[n] = '\0';
            // The name is in parentheses and may hold spaces: the fields after it start at ") ".
            const char* p = std::strrchr(line, ')');
            if (p == nullptr) {
                continue;
            }
            // After ") ": state (field 3), ..., utime (field 14), stime (field 15).
            unsigned long long utime = 0, stime = 0;
            if (std::sscanf(p + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &utime, &stime) != 2) {
                continue;
            }
            const char* open = std::strchr(line, '(');
            std::string name = open != nullptr ? std::string(open + 1, p) : std::string(entry->d_name);
            out[tid] = { name, (long long)(utime + stime) };
        }
        closedir(dir);
        return out;
    }

    // The busiest threads since the last call, as "name 85%" (of one core), most first.
    std::string busiest_threads(double window_s) {
        static std::unordered_map<int, ThreadTime> before = read_thread_times();
        std::unordered_map<int, ThreadTime> now = read_thread_times();
        static const double ticks_per_s = (double)sysconf(_SC_CLK_TCK);
        std::vector<std::pair<double, std::string>> use;
        for (const auto& [tid, t] : now) {
            auto it = before.find(tid);
            const long long ticks = t.ticks - (it != before.end() ? it->second.ticks : 0);
            const double percent = 100.0 * ticks / ticks_per_s / window_s;
            if (percent >= 5.0) {
                use.emplace_back(percent, t.name);
            }
        }
        before = std::move(now);
        std::sort(use.begin(), use.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        std::string text;
        for (size_t i = 0; i < use.size() && i < 8; i++) {
            char item[64];
            std::snprintf(item, sizeof(item), "%s%s %.0f%%", i ? ", " : "", use[i].second.c_str(), use[i].first);
            text += item;
        }
        return text.empty() ? "none over 5%" : text;
    }
}
#endif

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
        busiest_threads(1.0); // starts the thread times over too
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
        // Which threads used the CPU (percent of one core): the game, its audio, RT64's.
        std::printf("[WA] busiest threads: %s\n", busiest_threads(window_s).c_str());
        std::fflush(stdout);
        window_start = now;
        frames = slow = 0;
        longest_ms = 0.0;
    }
#endif
}
