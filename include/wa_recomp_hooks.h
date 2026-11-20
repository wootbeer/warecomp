#ifndef WA_RECOMP_HOOKS_H
#define WA_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt),
// for whatever the [[patches.hook]] text in wa.us.toml needs beyond what
// N64Recomp's generated recomp.h already provides.

#ifdef __cplusplus
extern "C" {
#endif

// recompui (lib/RecompFrontend/recompui/src/api/ui_api_events.cpp): runs the
// launcher/menu UI's queued callbacks. Hooked once per presented frame.
void recomp_run_ui_callbacks(uint8_t* rdram, recomp_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // WA_RECOMP_HOOKS_H
