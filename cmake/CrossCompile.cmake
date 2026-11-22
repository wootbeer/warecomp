# Cross-compiling (the Android build, see android/PLAN.md).
#
# Some tools run during the build: file_to_c (shaders, icon, mods), N64Recomp
# and RSPRecomp (game code and audio microcode from the ROM), and the MIPS
# toolchain for patches/. Built by this project they would be built for the
# target, which the build machine can't run. So when cross-compiling:
#
# - file_to_c comes from a desktop build of this project, through
#   WA_HOST_TOOLS_DIR (its build folder, where file_to_c ends up), and is
#   given to rt64 and the rules below as an imported target.
# - The ROM-derived sources aren't regenerated: RecompiledFuncs/,
#   and rsp/n_aspMain.cpp must already exist, made by a
#   desktop build with the ROM in place.

if (NOT CMAKE_CROSSCOMPILING)
    return()
endif()

set(WA_HOST_TOOLS_DIR "" CACHE PATH "Build folder of a desktop build of this project, for its file_to_c (needed when cross-compiling)")

if (CMAKE_HOST_WIN32)
    set(wa_host_exe_suffix ".exe")
else()
    set(wa_host_exe_suffix "")
endif()

set(wa_host_file_to_c "${WA_HOST_TOOLS_DIR}/file_to_c${wa_host_exe_suffix}")
if (NOT WA_HOST_TOOLS_DIR OR NOT EXISTS "${wa_host_file_to_c}")
    message(FATAL_ERROR
        "Cross-compiling needs a file_to_c built for this machine. Build the desktop version "
        "first (see BUILDING.md), then pass its build folder as -DWA_HOST_TOOLS_DIR=<folder> "
        "(Android Studio: wa.hostToolsDir=<folder> in android/local.properties). "
        "Looked for: ${wa_host_file_to_c}")
endif()
add_executable(file_to_c IMPORTED GLOBAL)
set_target_properties(file_to_c PROPERTIES IMPORTED_LOCATION "${wa_host_file_to_c}")
message(STATUS "Cross-compiling: using host file_to_c at ${wa_host_file_to_c}")

set(wa_missing_inputs "")
file(GLOB wa_recompiled_funcs "${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c")
if (NOT wa_recompiled_funcs)
    list(APPEND wa_missing_inputs "RecompiledFuncs/*.c")
endif()
foreach (input IN ITEMS
        RecompiledFuncs/recomp_overlays.inl
        rsp/n_aspMain.cpp)
    if (NOT EXISTS "${CMAKE_SOURCE_DIR}/${input}")
        list(APPEND wa_missing_inputs "${input}")
    endif()
endforeach()
if (wa_missing_inputs)
    list(JOIN wa_missing_inputs "\n  " wa_missing_list)
    message(FATAL_ERROR
        "Cross-compiling uses the sources generated from the ROM by a desktop build, and these "
        "are missing:\n  ${wa_missing_list}\n"
        "Build the desktop version once with the ROM in place (see BUILDING.md); it writes them "
        "into this checkout.")
endif()
