// Restart (from snprecomp): a button in the menu's header between Quit and Close,
// and a choice in the quit prompt (lib-patches/RecompFrontend/0027). It quits
// as Quit does, and just before the process ends (main's end, src/main/main.cpp,
// or a fault while the game shuts down, src/main/crash_handler.cpp) starts the game again with the same command
// line and working folder. Handy after changing mods. Not on Android, where
// the app is started by the system.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <shellapi.h>
#elif !defined(__ANDROID__)
#include <spawn.h>
extern char** environ;
#endif

#include "recompui/recompui.h"
#include "ultramodern/ultramodern.hpp"

#include "wa_restart.h"

namespace {
    // The command line to start again with. One-off options are left out, so they don't undo
    // what was changed in the menu since: --set, --fullscreen and --windowed (each saves its
    // setting again), and --rom <file> becomes --game wa (the ROM is stored by then).
    template <typename String>
    std::vector<String> restart_arguments(const std::vector<String>& in) {
        auto is = [](const String& arg, const char* text) {
            size_t n = std::char_traits<char>::length(text);
            if (arg.size() != n) return false;
            for (size_t i = 0; i < n; i++) if (arg[i] != (typename String::value_type)text[i]) return false;
            return true;
        };
        auto starts = [](const String& arg, const char* text) {
            size_t n = std::char_traits<char>::length(text);
            if (arg.size() < n) return false;
            for (size_t i = 0; i < n; i++) if (arg[i] != (typename String::value_type)text[i]) return false;
            return true;
        };
        auto make = [](const char* text) {
            String out;
            for (const char* c = text; *c; c++) out.push_back((typename String::value_type)*c);
            return out;
        };
        std::vector<String> out;
        bool game_given = false;
        for (size_t i = 0; i < in.size(); i++) {
            const String& arg = in[i];
            if (i > 0 && (is(arg, "--set") || is(arg, "--rom"))) {
                if (is(arg, "--rom") && !game_given) {
                    out.push_back(make("--game"));
                    out.push_back(make("wa"));
                    game_given = true;
                }
                i++; // and its value
                continue;
            }
            if (i > 0 && (starts(arg, "--set=") || is(arg, "--fullscreen") || is(arg, "--windowed"))) {
                continue;
            }
            if (i > 0 && is(arg, "--game")) {
                if (game_given) { i++; continue; }
                game_given = true;
            }
            out.push_back(arg);
        }
        return out;
    }

    std::atomic_bool requested = false;
    std::atomic_bool relaunched = false;
#if !defined(_WIN32) && !defined(__ANDROID__)
    std::vector<std::string> arguments;
    // The program's own path. Spawning /proc/self/exe itself works, but the new process is then
    // named "exe" (in top, task managers and pkill), so the link is read while it's still valid.
    std::string program = "/proc/self/exe";
#endif
}

void wa::restart::install(int argc, char** argv) {
#if defined(__ANDROID__)
    (void)argc;
    (void)argv;
#else
#if !defined(_WIN32)
    arguments = restart_arguments(std::vector<std::string>(argv, argv + argc));
    // An AppImage: start the AppImage again, not the program inside its mount (gone once this
    // process ends).
    if (const char* appimage = std::getenv("APPIMAGE")) {
        program = appimage;
        if (!arguments.empty()) arguments[0] = appimage;
    }
    else {
        std::error_code ec;
        std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec && !self.empty()) {
            program = self.string();
        }
    }
#else
    (void)argc;
    (void)argv;
#endif
    recompui::set_restart_callback([]() {
        requested = true;
        ultramodern::quit();
    });
#endif
}

void wa::restart::relaunch_if_requested() {
    if (!requested.load() || relaunched.exchange(true)) {
        return;
    }
#if defined(_WIN32)
    wchar_t exe[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
    if (length == 0 || length >= std::size(exe)) {
        std::fprintf(stderr, "Restart: couldn't find the game's exe\n");
        return;
    }
    // The command line, without the one-off options (restart_arguments), each argument quoted
    // the way the C runtime parses it back. CreateProcessW may write to it, so a copy.
    std::vector<std::wstring> args;
    int count = 0;
    if (LPWSTR* parsed = CommandLineToArgvW(GetCommandLineW(), &count)) {
        args.assign(parsed, parsed + count);
        LocalFree(parsed);
    }
    std::wstring command_line;
    for (const std::wstring& arg : restart_arguments(args)) {
        if (!command_line.empty()) command_line += L' ';
        if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) {
            command_line += arg;
            continue;
        }
        command_line += L'"';
        size_t backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') { backslashes++; continue; }
            if (c == L'"') command_line.append(backslashes * 2 + 1, L'\\');
            else command_line.append(backslashes, L'\\');
            backslashes = 0;
            command_line += c;
        }
        command_line.append(backslashes * 2, L'\\');
        command_line += L'"';
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    else {
        std::fprintf(stderr, "Restart: couldn't start the game again (error %lu)\n", GetLastError());
    }
#elif !defined(__ANDROID__)
    std::vector<char*> argv;
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    pid_t pid;
    int error = posix_spawn(&pid, program.c_str(), nullptr, nullptr, argv.data(), environ);
    if (error != 0) {
        std::fprintf(stderr, "Restart: couldn't start the game again (error %d)\n", error);
    }
#endif
}
