// Android frame pacing and frame rate in the log.
//
// The game runs at 60 frames a second (measured on desktop) and its logic advances once a frame, so
// below 60 it plays in slow motion. A frame is a chain of steps on different threads (the game's
// own, the audio microcode, RT64's display list thread), each idle most of the time. Android's
// scheduler keeps such lightly loaded threads on its slow cores at low clocks, and on a mid-range
// phone the chain then missed the 16.7 ms frame often: 36-55 fps on an Adreno 619 Motorola with
// every thread under 25% of a core and RT64 itself idle most of the frame (Sin and Punishment and
// BattleTanx, both 30 fps games, have twice the time and run well there). So the game tells Android
// which threads it relies on and how long its frames take, through the Performance Hint API
// (ADPF, Android 13+); Android then gives them faster cores or clocks when frames run late.
//
// Also, every 10 seconds of play, one log line with the frames per second, the longest gap between
// two frames and how many took over 50 ms, and one with the busiest threads, so players' shared
// logs show how fast a phone runs the game.
//
// Called once per game frame, after its buffer swap (wa_cheats_frame, from the hook in
// func_80033948).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__ANDROID__)
#include <dirent.h>
#include <dlfcn.h>
#include <sched.h>
#include <sys/types.h>
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

    struct ThreadUse {
        int tid;
        std::string name;
        double percent; // of one core
    };

    // Each thread's CPU use since the last call, busiest first.
    std::vector<ThreadUse> thread_use(double window_s) {
        static std::unordered_map<int, ThreadTime> before = read_thread_times();
        std::unordered_map<int, ThreadTime> now = read_thread_times();
        static const double ticks_per_s = (double)sysconf(_SC_CLK_TCK);
        std::vector<ThreadUse> use;
        for (const auto& [tid, t] : now) {
            auto it = before.find(tid);
            const long long ticks = t.ticks - (it != before.end() ? it->second.ticks : 0);
            use.push_back({ tid, t.name, 100.0 * ticks / ticks_per_s / window_s });
        }
        before = std::move(now);
        std::sort(use.begin(), use.end(), [](const ThreadUse& a, const ThreadUse& b) { return a.percent > b.percent; });
        return use;
    }

    std::string busiest_text(const std::vector<ThreadUse>& use) {
        std::string text;
        for (size_t i = 0; i < use.size() && i < 8 && use[i].percent >= 5.0; i++) {
            char item[64];
            std::snprintf(item, sizeof(item), "%s%s %.0f%%", i ? ", " : "", use[i].name.c_str(), use[i].percent);
            text += item;
        }
        return text.empty() ? "none over 5%" : text;
    }

    // --- Performance Hint API (ADPF), looked up at run time: it's Android 13+ (API 33), and
    // setThreads Android 14+ (API 34), while the app runs from Android 9. -------------------------
    struct APerformanceHintManager;
    struct APerformanceHintSession;
    using GetManagerFn = APerformanceHintManager* (*)();
    using CreateSessionFn = APerformanceHintSession* (*)(APerformanceHintManager*, const int32_t*, size_t, int64_t);
    using ReportFn = int (*)(APerformanceHintSession*, int64_t);
    using SetThreadsFn = int (*)(APerformanceHintSession*, const pid_t*, size_t);
    using CloseFn = void (*)(APerformanceHintSession*);
    using PreferredRateFn = int64_t (*)(APerformanceHintManager*);

    constexpr int64_t kTargetFrameNs = 16666667; // 60 fps

    struct Hint {
        bool looked_up = false;
        APerformanceHintManager* manager = nullptr;
        CreateSessionFn create = nullptr;
        ReportFn report = nullptr;
        SetThreadsFn set_threads = nullptr;
        CloseFn close = nullptr;
        APerformanceHintSession* session = nullptr;
        std::vector<int32_t> tids;
        bool failed_once = false;

        void look_up() {
            looked_up = true;
            void* lib = dlopen("libandroid.so", RTLD_NOW);
            if (lib == nullptr) {
                return;
            }
            auto get_manager = (GetManagerFn)dlsym(lib, "APerformanceHint_getManager");
            create = (CreateSessionFn)dlsym(lib, "APerformanceHint_createSession");
            report = (ReportFn)dlsym(lib, "APerformanceHint_reportActualWorkDuration");
            set_threads = (SetThreadsFn)dlsym(lib, "APerformanceHint_setThreads");
            close = (CloseFn)dlsym(lib, "APerformanceHint_closeSession");
            auto preferred_rate = (PreferredRateFn)dlsym(lib, "APerformanceHint_getPreferredUpdateRateNanos");
            if (get_manager != nullptr && create != nullptr && report != nullptr && close != nullptr) {
                manager = get_manager();
            }
            // -1 here means the phone has no performance hint support underneath (its sessions fail).
            if (manager != nullptr && preferred_rate != nullptr) {
                std::printf("[WA] performance hints: preferred update rate %lld ns\n", (long long)preferred_rate(manager));
                std::fflush(stdout);
            }
            if (manager == nullptr) {
                std::printf("[WA] performance hints: not available on this Android version\n");
                std::fflush(stdout);
            }
        }

        // Covers the threads the game's frames depend on: those that used at least 2% of a core
        // over the last window (the game's, the audio's, RT64's), at most 16.
        void update_threads(const std::vector<ThreadUse>& use) {
            if (!looked_up) {
                look_up();
            }
            if (manager == nullptr) {
                return;
            }
            std::vector<int32_t> wanted;
            for (const ThreadUse& t : use) {
                if (t.percent >= 2.0 && wanted.size() < 16) {
                    wanted.push_back(t.tid);
                }
            }
            std::sort(wanted.begin(), wanted.end());
            if (wanted.empty() || wanted == tids) {
                return;
            }
            if (session != nullptr && set_threads != nullptr &&
                set_threads(session, reinterpret_cast<const pid_t*>(wanted.data()), wanted.size()) == 0) {
                tids = std::move(wanted);
                return;
            }
            if (session != nullptr) {
                close(session);
                session = nullptr;
            }
            session = create(manager, wanted.data(), wanted.size(), kTargetFrameNs);
            if (session != nullptr) {
                tids = std::move(wanted);
                std::printf("[WA] performance hints: on, %zu threads, 16.7 ms frames\n", tids.size());
            }
            else {
                tids.clear();
                // Once: the phone doesn't take hint sessions; the fast cores (below) do the job.
                if (!failed_once) {
                    std::printf("[WA] performance hints: the session couldn't be made (not supported by this phone)\n");
                }
                failed_once = true;
                manager = nullptr;
            }
            std::fflush(stdout);
        }

        void report_frame(double frame_ms) {
            if (session != nullptr) {
                report(session, (int64_t)(frame_ms * 1000000.0));
            }
        }
    };

    Hint hint;

    // --- The fast cores: where performance hints aren't supported, the threads the frames depend on
    // are kept on the phone's fastest cores (the ones with the highest top clock), so Android doesn't
    // park them on its slow ones. Only where the cores differ; a thread already moved stays moved. ---
    struct FastCores {
        bool looked_up = false;
        cpu_set_t set;
        int count = 0;
        std::string list;
        std::vector<int> moved;

        void look_up() {
            looked_up = true;
            CPU_ZERO(&set);
            long best = 0;
            std::vector<std::pair<int, long>> cpus;
            for (int cpu = 0; cpu < CPU_SETSIZE && cpu < 64; cpu++) {
                char path[96];
                std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
                FILE* f = std::fopen(path, "r");
                if (f == nullptr) {
                    continue;
                }
                long khz = 0;
                if (std::fscanf(f, "%ld", &khz) == 1 && khz > 0) {
                    cpus.emplace_back(cpu, khz);
                    best = std::max(best, khz);
                }
                std::fclose(f);
            }
            for (const auto& [cpu, khz] : cpus) {
                if (khz == best) {
                    CPU_SET(cpu, &set);
                    count++;
                    list += (list.empty() ? "" : ",") + std::to_string(cpu);
                }
            }
            if (count == 0 || count == (int)cpus.size()) {
                count = 0; // all alike (or unknown): nothing to do
                return;
            }
            std::printf("[WA] fast cores: cpu %s of %zu (%ld MHz)\n", list.c_str(), cpus.size(), best / 1000);
            std::fflush(stdout);
        }

        void move(const std::vector<ThreadUse>& use) {
            if (!looked_up) {
                look_up();
            }
            if (count == 0) {
                return;
            }
            int newly = 0;
            for (const ThreadUse& t : use) {
                if (t.percent < 2.0) {
                    continue;
                }
                if (std::find(moved.begin(), moved.end(), t.tid) != moved.end()) {
                    continue;
                }
                if (sched_setaffinity(t.tid, sizeof(set), &set) == 0) {
                    moved.push_back(t.tid);
                    newly++;
                }
            }
            if (newly > 0) {
                std::printf("[WA] fast cores: %d more threads kept there (%zu in all)\n", newly, moved.size());
                std::fflush(stdout);
            }
        }
    };

    FastCores fast_cores;
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
        thread_use(1.0); // the threads' starting times
        return;
    }
    const double gap_ms = std::chrono::duration<double, std::milli>(now - last).count();
    last = now;
    // A long pause (the settings open, the app in the background) starts a new window.
    if (gap_ms > 2000.0) {
        thread_use(1.0); // starts the thread times over too
        window_start = now;
        frames = slow = 0;
        longest_ms = 0.0;
        return;
    }
    frames++;
    if (gap_ms > 50.0) slow++;
    if (gap_ms > longest_ms) longest_ms = gap_ms;
    hint.report_frame(gap_ms);

    const double window_s = std::chrono::duration<double>(now - window_start).count();
    if (window_s >= 10.0) {
        std::printf("[WA] frame rate: %.1f fps over %.0f s, longest frame %.0f ms, %d over 50 ms\n",
            frames / window_s, window_s, longest_ms, slow);
        const std::vector<ThreadUse> use = thread_use(window_s);
        // Which threads used the CPU (percent of one core): the game, its audio, RT64's.
        std::printf("[WA] busiest threads: %s\n", busiest_text(use).c_str());
        std::fflush(stdout);
        hint.update_threads(use);
        if (hint.session == nullptr) {
            fast_cores.move(use);
        }
        window_start = now;
        frames = slow = 0;
        longest_ms = 0.0;
    }
#endif
}
