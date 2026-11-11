# GeneralsX @feature WebPort 09/03/2026
if(EMSCRIPTEN)
    add_executable(z_generals)
    set_target_properties(z_generals PROPERTIES SUFFIX ".html")
else()
    add_executable(z_generals WIN32)
endif()

# Use a binary name that doesn't conflict with original game.
# GeneralsX @build BenderAI 15/02/2026 Linux branding update
if("${CMAKE_SYSTEM}" MATCHES "Windows")
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME generalszh)
elseif(EMSCRIPTEN)
    # Web: Output GeneralsZH.html + .js + .wasm
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME GeneralsZH)
else()
    # Linux: Use GeneralsX branding
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME GeneralsXZH)
endif()

# GeneralsX @build BenderAI 13/02/2026 Windows-specific libraries
if(WIN32)
    target_link_libraries(z_generals PRIVATE
        binkstub
        comctl32
        core_debug
        d3d8
        dinput8
        dxguid
        imm32
        milesstub
        vfw32
        winmm
    )
endif()

# Cross-platform core libraries
target_link_libraries(z_generals PRIVATE
    core_profile
    d3dx8
    z_gameengine
    z_gameenginedevice
    zi_always
)

# Linux SDL3 windowing/input (Phase 1)
# GeneralsX @build BenderAI 11/02/2026
if(SAGE_USE_SDL3 AND NOT EMSCRIPTEN)
    target_link_libraries(z_generals PRIVATE sdl3lib)
endif()

# TODO Originally referred to build host and user, replace with git info perhaps?
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

# Based on original binary values for these variables.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
"
)

if(MSVC)
    target_link_options(z_generals PRIVATE "/NODEFAULTLIB:libci.lib")
endif()

target_include_directories(z_generals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# GeneralsX @build BenderAI 09/03/2026 - On Emscripten, CompatLib is skipped so its
# include dir is not propagated transitively. LinuxStubs.cpp needs Windows API type
# declarations (SYSTEMTIME, Sleep, GetTickCount, etc.) from CompatLib/Include.
if(EMSCRIPTEN)
    target_include_directories(z_generals PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../CompatLib/Include
    )
    
    # GeneralsX @build WebPort 2026-07-06 — flags aligned to the last known-good
    # link line (link_mac.sh, 2026-05-24) so a clean CMake build reproduces the
    # .o-surgery era binary: SAFE_HEAP=0 (session 7700f1f), EXIT_RUNTIME=0,
    # 1GB/2GB heap + 4MB stack, idbfs, and the full 12-symbol export list.
    # Two profiles, matching link_mac.sh debug/release:
    #   CMAKE_BUILD_TYPE=Release      -> -O3, closure, emmalloc (distribution)
    #   anything else (RelWithDebInfo)-> -O0 -g3, assertions (development)
    # GeneralsX @build WebPort 2026-07-07
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GX_PROFILE_FLAGS
            "-O3"
            "--closure=1"
            "SHELL:--closure-args=--externs=${CMAKE_CURRENT_SOURCE_DIR}/closure-externs-gx.js"
            "-sMALLOC=emmalloc"
            # Closure's ADVANCED_OPTIMIZATIONS needs the legacy allocator
            # helpers declared (SDL2's SDL_assert.c EM_ASM references them).
            "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','printErr','addFunction','noExitRuntime','UTF8ToString','HEAPU8','allocate','ALLOC_NORMAL','intArrayFromString']"
        )
    else()
        set(GX_PROFILE_FLAGS
            "-O0"
            "-g3"
            "-sASSERTIONS=2"
            "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','printErr','addFunction','noExitRuntime','UTF8ToString','HEAPU8']"
        )
    endif()

    set(EMSCRIPTEN_ASYNC_FLAGS
        ${GX_PROFILE_FLAGS}
        "-sSAFE_HEAP=0"
        "-sEXIT_RUNTIME=0"
        "-sUSE_SDL=2"
        "-sUSE_SDL_IMAGE=2"
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sALLOW_MEMORY_GROWTH=1"
        # Emscripten 6.x defaults memory growth to JS resizable ArrayBuffers;
        # Chrome's WebGL rejects resizable-backed views in texImage2D
        # ("ArrayBufferView value must not be resizable"). Keep the classic
        # replace-the-buffer growth strategy. GeneralsX @build WebPort 2026-07-06
        "-sGROWABLE_ARRAYBUFFERS=0"
        # 1GB initial (grow to 2GB): matches the battle-tested link_mac.sh
        # profile. Growing from a smaller heap forces allocate-and-copy spikes
        # (old+new buffers coexist) which are worse under memory pressure than
        # the upfront commit. GeneralsX @build WebPort 2026-07-07
        "-sINITIAL_MEMORY=1073741824"
        "-sMAXIMUM_MEMORY=2147483648"
        "-sSTACK_SIZE=4194304"
        "-sFETCH=1"
        "-sASYNCIFY=0"
        "-sFORCE_FILESYSTEM=1"
        "-sENVIRONMENT=web"
        "-lidbfs.js"
        # (EXPORTED_RUNTIME_METHODS comes from GX_PROFILE_FLAGS above —
        # repeated -s flags override, so it must appear exactly once.)
        # JS-callable C entry points. EMSCRIPTEN_KEEPALIVE attributes alone
        # *should* be enough, but listing them explicitly is defensive.
        "-sEXPORTED_FUNCTIONS=['_main','_EmscriptenInput_OnPointerLockChange','_EmscriptenInput_GetVirtualMouseX','_EmscriptenInput_GetVirtualMouseY','_GX_FlushIdbfs_Tab','_GX_OnCanvasResize','_AIL_trigger_sample_eos','_GX_Network_EnableVirtual','_GX_Network_PushPacket','_GX_Network_SetVirtualIP','_GX_TickHidden','_malloc','_free']"
        "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$registerPointerlockErrorEventCallback']"
    )

    # Custom HTML shell — provides a themed loading screen + the
    # window.GeneralsX.setProgress() hook that EmscriptenMain.cpp calls.
    # GeneralsX @feature WebPort 2026-05-04 — loading progress bar
    set(GX_WEB_SHELL "${CMAKE_CURRENT_SOURCE_DIR}/web_shell.html")
    target_link_options(z_generals PRIVATE
        "SHELL:--shell-file ${GX_WEB_SHELL}"
    )
    set_property(TARGET z_generals APPEND PROPERTY LINK_DEPENDS "${GX_WEB_SHELL}")

    target_link_options(z_generals PRIVATE ${EMSCRIPTEN_ASYNC_FLAGS})
    target_compile_options(z_generals PRIVATE "-sUSE_SDL=2")

    # NOTE: the May-24 hand-built link (link_mac.sh) included libwwaudio.a from
    # the compile-script pipeline — WWAudio has no CMakeLists and is not in the
    # CMake graph yet (tracked as an open issue). The only WWAudio symbol the
    # web target actually demands is WWAudioThreadsClass::Tick_Delayed_Release_
    # Objects() (single-threaded delayed-release drain, called per frame from
    # EmscriptenMain), so compile that one TU directly into the target — the
    # archive link would have pulled exactly this object anyway.
    # GeneralsX @build WebPort 2026-07-06
    target_sources(z_generals PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Core/Libraries/Source/WWVegas/WWAudio/Threads.cpp
    )
    target_include_directories(z_generals PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Core/Libraries/Source/WWVegas/WWAudio
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../Core/Libraries/Source/WWVegas
    )
endif()

target_sources(z_generals PRIVATE
    $<$<BOOL:${WIN32}>:WinMain.cpp>
    $<$<BOOL:${WIN32}>:WinMain.h>
    # Linux/macOS SDL3 entry point (Phase 1.5 - TheSuperHackers @build 10/02/2026)
    # Not used on Emscripten — EmscriptenMain.cpp replaces it
    $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<NOT:$<BOOL:${EMSCRIPTEN}>>>:SDL3Main.cpp>
    # Web: Emscripten entry point (GeneralsX @feature WebPort 09/03/2026)
    $<$<BOOL:${EMSCRIPTEN}>:EmscriptenMain.cpp>
    # Web: SDL2 → DirectInput/Win32 input translation (GeneralsX @feature WebPort 15/03/2026)
    $<$<BOOL:${EMSCRIPTEN}>:EmscriptenInput.cpp>
    # Linux stubs for P2-P3 platform functions (P2-P3 - TheSuperHackers @build 13/02/2026 BenderAI Session 37)
    $<$<NOT:$<BOOL:${WIN32}>>:LinuxStubs.cpp>
)

# RC files optional for MinGW builds
if(MSVC)
    if(NOT IS_VS6_BUILD)
        target_link_options(z_generals PRIVATE
            "/MANIFEST:NO"
        )
    endif()

    target_sources(z_generals PRIVATE
        RTS.RC
    )
endif()

# Strip debug symbols to separate file for MinGW Release builds
if(MINGW AND COMMAND add_debug_strip_target)
    add_debug_strip_target(z_generals)
endif()
