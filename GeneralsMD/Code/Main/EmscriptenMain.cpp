/*
**  Command & Conquer Generals Zero Hour(tm)
**  Copyright 2025 Electronic Arts Inc.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  GeneralsX Web Port — Emscripten Entry Point
*/

#if defined(__EMSCRIPTEN__)

// SYSTEM INCLUDES
#include <SDL.h>
#include <Utility/gx_trace.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <emscripten.h>
#include <emscripten/html5.h>

// USER INCLUDES
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/Debug.h"
#include "Common/FramePacer.h"
#include "GameLogic/GameLogic.h"
#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"  // declares TheWritableGlobalData; we override
                                // m_userDataDir on Emscripten so it points at
                                // /userdata (mounted as IDBFS).
#include "Common/version.h"
#include "Lib/BaseType.h"

// Web port includes
#include "gles3_big_vfs.h"
#include "gles3_mainloop.h"
#include "gles3_wrapper.h"

// SDL input translation
#include "EmscriptenInput.h"

// Game Client display/view
#include "GameClient/Display.h"
#include "GameClient/View.h"

// WWAudio delayed-release
#include "Threads.h"

// Win32GameEngine
#include "Win32Device/Common/Win32GameEngine.h"

// Win32Mouse
#include "Win32Device/GameClient/Win32Mouse.h"

// BigVFS read function (defined in gles3_big_vfs.cpp). Forward-declared here
// for the diagnostic probe in main() that tests path resolution against the
// .big archives. GeneralsX @feature WebPort 2026-05-05 — black-canvas triage
extern "C" bool Web_VFS_Read_File_Sync(const char *path, void **out_data,
                                       unsigned int *out_size);

// DX8Wrapper diagnostic counters defined in dx8wrapper.cpp. Sampled once a
// second from GameWebFrameCallback to localise where draws are getting lost.
extern "C" int gx_dx8_draw_triangles_calls;
extern "C" int gx_dx8_sorting_path;
extern "C" int gx_dx8_path;
extern "C" int gx_dx8_draw_called;
extern "C" int gx_dx8_draw_polygon_low_skip;
extern "C" int gx_dx8_draw_triangle_disabled;
extern "C" int gx_dx8_draw_dispatched;
extern "C" int gx_dx8_draw_zero_indexbuffer;
extern "C" int gx_dx8_draw_zero_vertexbuffer;
extern "C" int gx_d3d_dip_called;
extern "C" int gx_d3d_dip_no_vb;
extern "C" int gx_d3d_dip_empty_vb;
extern "C" int gx_d3d_dip_no_ib;
extern "C" int gx_d3d_dip_empty_ib;
extern "C" int gx_d3d_dip_drew;
extern "C" int gx_dx8_buftype_dx8;
extern "C" int gx_dx8_buftype_dyn_dx8;
extern "C" int gx_dx8_buftype_sort;
extern "C" int gx_dx8_buftype_dyn_sort;
extern "C" int gx_dx8_buftype_other;
extern "C" int gx_d3d_setstreamsource_calls;
extern "C" int gx_d3d_setstreamsource_null;
extern "C" int gx_d3d_setindices_calls;
extern "C" int gx_d3d_setindices_null;

// WWAudio delayed-release drain helper exists in
// Core/Libraries/Source/WWVegas/WWAudio/Threads.{h,cpp} (see
// Tick_Delayed_Release_Objects). It is NOT included here because the web
// build does not currently link libwwaudio (per build-html5/.../linkLibs.rsp)
// — the audio path is stubbed via milesstub. When real WWAudio is wired up,
// add #include "Threads.h", a `target_link_libraries(z_generals PRIVATE
// z_wwaudio)` in Main/CMakeLists.txt, and a per-frame call to
// WWAudioThreadsClass::Tick_Delayed_Release_Objects() right after
// TheGameEngine->update().

// ============================================================================
// Loading-screen progress hook
//
// The custom shell.html exposes a `window.GeneralsX` object with a
// `setProgress(stage, current, total, label)` function. This C++ helper is a
// thin wrapper that fires off an EM_ASM call to that JS so the page can render
// a progress bar / status string while the engine boots.
//
// Stages the shell understands (matched against an enum of strings, not int):
//   "boot"   — early init (SDL, GL, etc.)         current=0..N total=N
//   "vfs"    — BigVFS archive mounting             current=i..N total=N name=archive
//   "engine" — TheGameEngine->init() bring-up      current=0..1 total=1
//   "ready"  — engine running, hide loading screen current=1    total=1
//
// All calls are no-ops if window.GeneralsX is missing (so the default
// Emscripten shell still works for diagnostics builds).
// ============================================================================
static inline void GX_ReportProgress(const char *stage, int current, int total,
                                     const char *label) {
  EM_ASM({
    if (typeof window !== 'undefined' && window['GeneralsX'] &&
        typeof window['GeneralsX']['setProgress'] === 'function') {
      window['GeneralsX']['setProgress'](
          UTF8ToString($0), $1, $2,
          $3 ? UTF8ToString($3) : '');
    }
  }, stage, current, total, label);
}

// CRITICAL SECTIONS
static CriticalSection critSec1;
static CriticalSection critSec2;
static CriticalSection critSec3;
static CriticalSection critSec4;
static CriticalSection critSec5;

// GLOBAL COMMAND LINE ARGUMENTS
int __argc = 0;
char **__argv = nullptr;

// GLOBAL WINDOW HANDLE
HWND ApplicationHWnd = nullptr;

// GLOBAL SDL3 WINDOW
SDL_Window *TheSDL3Window = nullptr;

// GAME TEXT FILE PATHS
const Char *g_csfFile = "data/%s/generals.csf";
const Char *g_strFile = "data/Generals.str";

extern bool GLES3_Init(void *hwnd, bool lite);
extern Win32Mouse *TheWin32Mouse;

GameEngine *CreateGameEngine(void) {
  fprintf(stderr, "INFO: CreateGameEngine() — Win32GameEngine for WebGL 2.0\n");
  return NEW Win32GameEngine();
}

extern "C" void EmscriptenInput_RouteMouseEvent(unsigned int msg,
                                                 unsigned long wParam,
                                                 long lParam,
                                                 unsigned long time) {
  if (TheWin32Mouse) {
    TheWin32Mouse->addWin32Event((UINT)msg, (WPARAM)wParam, (LPARAM)lParam,
                                  (DWORD)time);
  }
}

// Global state tracking for init phase
static bool g_GameEngineInitialized = false;

// ============================================================================
// IDBFS persistence hooks
//
// /userdata is mounted as IDBFS at preRun time (see web_shell.html) and
// FS.syncfs(true) populates MEMFS from IndexedDB before main() runs. After
// that, MEMFS writes only persist if we explicitly FS.syncfs(false). We do
// that on three triggers:
//   - Periodic timer (every ~30s) so a long session doesn't lose recent saves
//     if the tab crashes
//   - Page visibility change to "hidden" (tab in background / minimized)
//   - Page beforeunload (best-effort; some browsers ignore async work here)
//
// All flushes are debounced via the JS side: a flush in flight blocks new
// flushes until completion, so a save burst won't pile up parallel syncs.
// ============================================================================
static void GX_FlushIdbfs(const char *reason) {
  // Note the extra parentheses around the JS body. The C preprocessor splits
  // EM_ASM's first argument at the first top-level comma it sees, and curly
  // braces don't suppress that splitting — so an inline JS object literal
  // like `{ busy: false, pending: false }` would otherwise split the macro
  // arg in two and the body would parse as C++. The parens form a single
  // grouped expression that the preprocessor treats atomically. See the
  // comment in `<emscripten/em_asm.h>` next to `EM_ASM_PREP_ARGS`.
  EM_ASM(({
    if (typeof Module === 'undefined' || !Module.GX_idbfsState) {
      Module.GX_idbfsState = { busy: false, pending: false };
    }
    var st = Module.GX_idbfsState;
    if (st.busy) {
      st.pending = true;
      return;
    }
    st.busy = true;
    var why = UTF8ToString($0);
    FS.syncfs(false, function (err) {
      st.busy = false;
      if (err) {
        console.warn('[GeneralsX] IDBFS flush failed (' + why + '):', err);
      }
      if (st.pending) {
        st.pending = false;
        // Re-enter to handle any writes that happened during the flush.
        FS.syncfs(false, function (e2) {
          if (e2) console.warn('[GeneralsX] IDBFS pending flush failed:', e2);
        });
      }
    });
  }), reason);
}

static void GX_InstallIdbfsVisibilityHooks() {
  EM_ASM({
    if (window.GX_idbfs_visibility_installed) return;
    window.GX_idbfs_visibility_installed = true;
    document.addEventListener('visibilitychange', function () {
      if (document.visibilityState === 'hidden') {
        if (typeof Module !== 'undefined' &&
            typeof Module.ccall === 'function') {
          try {
            Module.ccall('GX_FlushIdbfs_Tab', null, ['string'], ['visibility']);
          } catch (e) {
            // Module may have unloaded.
          }
        }
      }
    });
    window.addEventListener('beforeunload', function () {
      // Best-effort. FS.syncfs is async; the flush kicks off and the browser
      // may or may not let it finish. If the user has UNSAVED changes this
      // is the last chance.
      try { FS.syncfs(false, function () {}); } catch (e) {}
    });
  });
}

extern "C" EMSCRIPTEN_KEEPALIVE
void GX_FlushIdbfs_Tab(const char *reason) {
  GX_FlushIdbfs(reason && *reason ? reason : "tab");
}

// ============================================================================
// Canvas resize handling
//
// The browser window can resize at any time. We mirror the displayed CSS-pixel
// size of the canvas (× devicePixelRatio, for HiDPI sharpness) into the
// canvas's backing-store width/height attributes. That alone changes the GL
// drawing buffer; we then call SDL_SetWindowSize so SDL emits a
// SDL_WINDOWEVENT_RESIZED that EmscriptenInput.cpp picks up.
//
// The engine itself doesn't currently rescale its UI — C&C Generals was a
// fixed-resolution RTS — but the framebuffer matches the canvas, so CSS
// scaling renders crisply rather than via browser-side bilinear upscaling.
// Future work: wire up an in-engine GUI resize pass so the menu / HUD
// re-layouts to the new backing size.
// ============================================================================
extern "C" EMSCRIPTEN_KEEPALIVE
void GX_OnCanvasResize(int physW, int physH, int logW, int logH) {
  if (physW <= 0 || physH <= 0) return;
  if (logW <= 0 || logH <= 0) {
    logW = EM_ASM_INT({
      var canvas = document.getElementById('canvas');
      return canvas ? canvas.clientWidth : 0;
    });
    logH = EM_ASM_INT({
      var canvas = document.getElementById('canvas');
      return canvas ? canvas.clientHeight : 0;
    });
    if (logW <= 0) logW = physW;
    if (logH <= 0) logH = physH;
  }
  if (TheSDL3Window) {
    SDL_SetWindowSize(TheSDL3Window, logW, logH);
  }
  emscripten_set_canvas_element_size("#canvas", physW, physH);
  EmscriptenInput_UpdateCanvasSize(logW, logH);

  if (TheDisplay != nullptr) {
    TheDisplay->setWidth(logW);
    TheDisplay->setHeight(logH);
  }
  if (TheTacticalView != nullptr) {
    TheTacticalView->setWidth(logW);
    TheTacticalView->setHeight(logH);
    TheTacticalView->setDefaultView(0.0f, 0.0f, 1.0f);
  }

  fprintf(stderr, "INFO: GX_OnCanvasResize → physical: %dx%d, logical: %dx%d\n", physW, physH, logW, logH);
}

// ============================================================================
// Deferred audio archive mount
//
// AudioZH.big / MusicZH.big / SpeechZH.big together can run hundreds of MB
// over HTTP and would dominate the startup wait if we mounted them alongside
// the menu/UI archives. Instead we mount them lazily a few frames after the
// engine reports "ready" — by that point the user is staring at the main menu
// and won't notice a few-second background trickle. The audio engine can
// then pick them up the first time it asks for a sound or music track.
//
// State machine (driven from GameWebFrameCallback):
//   0 = idle, do nothing
//   1 = engine ready, wait N frames so the menu has rendered
//   2 = mount in progress (one archive per call to keep main-loop responsive)
//   3 = done
//
// Archive list intentionally kept inline so it lives next to the startup
// list in main(); easy to compare which is which.
// ============================================================================
namespace {
const char *const kDeferredAudioArchives[] = {
    "Speech.big",
    "SpeechEnglish.big",
    "SpeechZH.big",
    "SpeechEnglishZH.big",
    nullptr,
};
int g_deferredMountState = 0;
int g_deferredMountWaitFrames = 0;
int g_deferredMountIndex = 0;
}  // namespace

static int DeferredAudio_Total() {
  int n = 0;
  while (kDeferredAudioArchives[n] != nullptr) ++n;
  return n;
}

static void DeferredAudio_Step() {
  switch (g_deferredMountState) {
    case 0:  // not yet engaged
      return;

    case 1:  // engine just became ready — let the menu paint a few frames
      if (--g_deferredMountWaitFrames <= 0) {
        g_deferredMountState = 2;
        g_deferredMountIndex = 0;
        EM_ASM({
          if (window['GeneralsX'] &&
              typeof window['GeneralsX']['setSecondaryProgress'] === 'function') {
            window['GeneralsX']['setSecondaryProgress'](
                'Loading audio assets', 0, $0, '');
          }
        }, DeferredAudio_Total());
      }
      return;

    case 2: {  // mount one archive per frame, drain Emscripten's event queue
               // between each fetch so the page stays responsive
      const char *name = kDeferredAudioArchives[g_deferredMountIndex];
      if (name == nullptr) {
        // All done — refresh the VFS file table so the engine sees them.
        BigVFS::Mount_To_Emscripten_FS();
        g_deferredMountState = 3;
        EM_ASM({
          if (window['GeneralsX'] &&
              typeof window['GeneralsX']['setSecondaryProgress'] === 'function') {
            window['GeneralsX']['setSecondaryProgress']('Audio ready', 1, 1, '');
            // Give the user a beat to read it, then hide.
            setTimeout(function () {
              if (window['GeneralsX']['hideSecondaryProgress']) {
                window['GeneralsX']['hideSecondaryProgress']();
              }
            }, 1500);
          }
        });
        fprintf(stderr, "INFO: Deferred audio mount complete\n");
        return;
      }
      EM_ASM({
        if (window['GeneralsX'] &&
            typeof window['GeneralsX']['setSecondaryProgress'] === 'function') {
          window['GeneralsX']['setSecondaryProgress'](
              'Loading audio assets', $0, $1, UTF8ToString($2));
        }
      }, g_deferredMountIndex, DeferredAudio_Total(), name);
      fprintf(stderr, "INFO: Deferred mount: %s\n", name);
      // Mount_Archive is synchronous — single fetch of ~512KB to grab the
      // header. The body of the archive stays on the server and gets pulled
      // by HTTP-range when the audio engine actually opens a sound file.
      BigVFS::Mount_Archive(name);
      ++g_deferredMountIndex;
      return;
    }

    case 3:  // done
    default:
      return;
  }
}

static bool GameWebFrameCallback(float /*delta_time_ms*/) {
  EmscriptenInput_PumpEvents();

  if (!g_GameEngineInitialized) {
    // Force an initial clear to black so the first frame renders IMMEDIATELY.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // SDL_GL_SwapWindow(TheSDL3Window); // Emscripten does this automatically!
    return true; // Keep looping.
  }

  if (!TheGameEngine) {
    return false;
  }

  if (TheGameEngine->getQuitting()) {
    fprintf(stderr, "INFO: GameEngine requested quit — stopping main loop\n");
    delete TheFramePacer;
    TheFramePacer = nullptr;
    delete TheGameEngine;
    TheGameEngine = nullptr;
    return false; 
  }

  // (Removed per-frame heartbeat log — was drowning every other engine log
  // in the JS console. Re-enable temporarily if you need to confirm the
  // main loop is alive.)

  // Belt-and-braces: clear `m_breakTheMovie` every frame so the
  // W3DDisplay::draw render block isn't accidentally gated off.
  // MainMenuInit (the layout's runInit) normally clears this back to FALSE
  // after GameClient::update sets it TRUE during the post-intro handoff,
  // but if the layout init ever fails to fire we'd be stuck black-screen
  // forever. The clear is a single bool store; safe to do unconditionally.
  // We deliberately do NOT force `m_playIntro` / `m_afterIntro` to FALSE
  // here — the engine's intro state machine in GameClient::update
  // (line 519) is what eventually calls `TheShell->showShell()`, which
  // pushes MainMenu.wnd. Forcing both flags FALSE pre-update bypasses
  // that path and the menu never gets loaded.
  // GeneralsX @feature WebPort 2026-05-05 — black-canvas triage
  if (TheWritableGlobalData) {
    TheWritableGlobalData->m_breakTheMovie = FALSE;
    // Force off any lingering "load screen render" mode — when set, the
    // engine takes a stripped W3DDisplay::draw branch that bypasses
    // drawViews / TheInGameUI->DRAW / TheMouse->DRAW (the real renderers)
    // and only calls the "lite" InGameUI/Mouse draws which produce no
    // geometry for menu UI. If a load screen ever transitioned the engine
    // into this state and didn't transition out, we'd be stuck in it.
    TheWritableGlobalData->m_loadScreenRender = false;
  }

  // Black-canvas triage: dump DX8Wrapper draw counters once a second so we
  // can see where the menu's draw calls are getting lost. Remove once the
  // menu renders. GeneralsX @feature WebPort 2026-05-05.
  static int s_diagFrameTick = 0;
  if (++s_diagFrameTick >= 60) {
    s_diagFrameTick = 0;
    fprintf(stderr,
            "DX8-DIAG: dip:called=%d no_vb=%d empty_vb=%d drew=%d  "
            "SetStreamSource:%d (null=%d)  SetIndices:%d (null=%d)  "
            "buftype dyn_dx8=%d\n",
            gx_d3d_dip_called, gx_d3d_dip_no_vb, gx_d3d_dip_empty_vb,
            gx_d3d_dip_drew,
            gx_d3d_setstreamsource_calls, gx_d3d_setstreamsource_null,
            gx_d3d_setindices_calls, gx_d3d_setindices_null,
            gx_dx8_buftype_dyn_dx8);
    if (TheGameLogic && TheFramePacer) {
      fprintf(stderr,
              "TICK-DIAG: logicFrame=%u inGame=%d isPaused=%d "
              "updateTime=%.4f maxFps=%d frozen=%d halted=%d\n",
              (unsigned)TheGameLogic->getFrame(),
              TheGameLogic->isInGame() ? 1 : 0,
              TheGameLogic->isGamePaused() ? 1 : 0,
              TheFramePacer->getUpdateTime(),
              TheFramePacer->getActualFramesPerSecondLimit(),
              TheFramePacer->isTimeFrozen() ? 1 : 0,
              TheFramePacer->isGameHalted() ? 1 : 0);
    }
  }

  TheGameEngine->update();

  // Tick the WWAudio delayed release objects per-frame to prevent leaks
  WWAudioThreadsClass::Tick_Delayed_Release_Objects();

  // Pump the deferred audio-archive mount state machine. No-op until the
  // engine reports ready; then mounts one archive per frame so the main loop
  // stays responsive during the network fetches.
  DeferredAudio_Step();

  // Periodic IDBFS flush — best-effort persistence in case the tab crashes
  // before visibilitychange / beforeunload fires. Every ~30s at 60fps.
  static int s_idbfsTickCounter = 0;
  if (++s_idbfsTickCounter >= 30 * 60) {
    s_idbfsTickCounter = 0;
    GX_FlushIdbfs("periodic");
  }

  if (TheFramePacer) {
    TheFramePacer->update();
  }

  return true;
}

// EMSCRIPTEN_ASYNC_API to safely wrap the heavy init
EM_ASYNC_JS(void, ExecuteHeavyInitialization, (), {
    console.log("INFO: JS delegating to GameEngine::init()...");
});

void AsyncEngineInit() {
  fprintf(stderr, "INFO: Initializing GameEngine in async context...\n");
  TheGameEngine = CreateGameEngine();
  TheGameEngine->init(); // This function contains emscripten_sleep(0) calls via Asyncify
  g_GameEngineInitialized = true;
  fprintf(stderr, "INFO: Async GameEngine::init() complete\n");
}

int main(int argc, char *argv[]) {
  __argc = argc;
  __argv = argv;

  fprintf(stderr, "=================================================\n");
  fprintf(stderr, " Command & Conquer Generals: Zero Hour (Web)\n");
  fprintf(stderr, " Emscripten + WebGL 2.0 Build\n");
  fprintf(stderr, "=================================================\n\n");

  try {
    TheAsciiStringCriticalSection = &critSec1;
    TheUnicodeStringCriticalSection = &critSec2;
    TheDmaCriticalSection = &critSec3;
    TheMemoryPoolCriticalSection = &critSec4;
    TheDebugLogCriticalSection = &critSec5;

    initMemoryManager();

    TheVersion = NEW Version;
    CommandLine::parseCommandLineForStartup();

    // Install the IDBFS visibility / unload hooks early — they don't depend on
    // the engine being up, just on Module being available, which it is by the
    // time main() is called.
    GX_InstallIdbfsVisibilityHooks();

    GX_ReportProgress("boot", 1, 6, "Initializing SDL2 video");
    fprintf(stderr, "INFO: Initializing SDL2 (Emscripten) Video ...\n");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
      fprintf(stderr, "FATAL: SDL2 Video init failed: %s\n", SDL_GetError());
      return 1;
    }
    fprintf(stderr, "INFO: Initializing SDL2 (Emscripten) Audio ...\n");
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
      fprintf(stderr, "WARN: SDL2 Audio init failed: %s\n", SDL_GetError());
    }

    GX_ReportProgress("boot", 2, 6, "Creating WebGL 2.0 context");
    fprintf(stderr, "INFO: Creating SDL3 OpenGL ES 3.0 window (→ WebGL 2.0) ...\n");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    // Stencil shadows and the behind-building player colour need a stencil buffer;
    // without one their full-screen quads cover the whole view.
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    int canvas_width = 800;
    int canvas_height = 600;
    int client_w = EM_ASM_INT({
      var canvas = document.getElementById('canvas');
      return canvas ? canvas.clientWidth : 0;
    });
    int client_h = EM_ASM_INT({
      var canvas = document.getElementById('canvas');
      return canvas ? canvas.clientHeight : 0;
    });
    if (client_w > 0) canvas_width = client_w;
    if (client_h > 0) canvas_height = client_h;
    
    const Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    TheSDL3Window = SDL_CreateWindow(
        "Command & Conquer Generals: Zero Hour", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, canvas_width, canvas_height, windowFlags);
    if (!TheSDL3Window) {
      fprintf(stderr, "FATAL: SDL_CreateWindow failed: %s\n", SDL_GetError());
      SDL_Quit();
      return 1;
    }
        
    SDL_GLContext gl_context = SDL_GL_CreateContext(TheSDL3Window);
    if (!gl_context) {
      fprintf(stderr, "FATAL: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
      SDL_DestroyWindow(TheSDL3Window);
      SDL_Quit();
      return 1;
    }
    SDL_GL_MakeCurrent(TheSDL3Window, gl_context);

    double dpr = EM_ASM_DOUBLE({
      return window.devicePixelRatio || 1.0;
    });
    int phys_w = (int)(canvas_width * dpr);
    int phys_h = (int)(canvas_height * dpr);
    emscripten_set_canvas_element_size("#canvas", phys_w, phys_h);
    EmscriptenInput_UpdateCanvasSize(canvas_width, canvas_height);

    ApplicationHWnd = (HWND)(void *)TheSDL3Window;
    fprintf(stderr, "INFO: SDL3 + WebGL 2.0 context ready. logical: %dx%d, physical: %dx%d\n", canvas_width, canvas_height, phys_w, phys_h);
    
    GX_ReportProgress("boot", 3, 6, "Initializing GLES3 wrapper");
    if (!GLES3_Init(nullptr, false)) {
      fprintf(stderr, "FATAL: GLES3_Init failed\n");
      return 1;
    }

    GX_ReportProgress("boot", 4, 6, "Bringing up frame pacer");
    // GeneralsX @feature WebPort 2026-05-05 — black-canvas root cause:
    // WebVisibility::Init() registers a `visibilitychange` callback that
    // pauses the entire main loop (and therefore the engine update + draw
    // path) whenever `document.hidden` becomes true. In an iframe context
    // (AI-tool preview panes, embedded players, etc.) `document.hidden` toggles
    // spuriously across focus shifts, leaving the engine paused and the
    // canvas frozen on a black clear. The pause was meant as a battery /
    // perf optimisation for tabs in the background; it doesn't justify
    // bricking the engine in iframe embeds. We drop the visibility hook
    // entirely. The browser still throttles rAF when the tab is truly
    // hidden, so the original perf concern is mostly handled by the
    // platform — and a real save-state pause should be triggered by
    // user-visible UI, not by an event the iframe can't authoritatively
    // observe.
    // WebVisibility::Init();

    // 1. CLEAR FRAME 0 IMMEDIATELY BEFORE ANY FILE I/O OR INITS
    fprintf(stderr, "INFO: Rendering first frame before VFS init...\n");
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // SDL_GL_SwapWindow(TheSDL3Window); // Emscripten hates sync swap outside main loop

    TheFramePacer = new FramePacer();
    TheFramePacer->enableFramesPerSecondLimit(TRUE);

    // Asset loading: mount .big archives via HTTP into the Emscripten VFS
    // We do this before the game engine starts, but theoretically it could also be async.
    GX_ReportProgress("boot", 5, 6, "Connecting asset stream");
    fprintf(stderr, "INFO: Initializing BigVFS asset system...\n");
    const char *asset_base_url = "assets/";
    if (BigVFS::Init(asset_base_url)) {
      // Note: GameEngine::init() calls
      //   `if (!TheAudio->isMusicAlreadyLoaded()) setQuitting(TRUE);`
      // and `isMusicAlreadyLoaded` walks m_allAudioEventInfo for an
      // AT_Music entry, then asks the filesystem whether the corresponding
      // music file exists. With the current milesstub audio backend that
      // check is unreliable: even if MusicZH.big is mounted in the startup
      // set, doesFileExist will fail unless the specific track filename
      // round-trips through the BigVFS path normalization. Rather than
      // serialise an entire music archive at boot, we clear the quit flag
      // post-init below (see `TheGameEngine->setQuitting(FALSE)` after
      // init). Music loading via the deferred mount continues to work in
      // the background once a real audio bridge replaces milesstub.
      static const char *const startup_archives[] = {
          "INI.big", "Textures.big", "W3D.big", "maps.big", "Window.big",
          "English.big", "shaders.big", "Terrain.big",
          "INIZH.big", "TexturesZH.big", "W3DZH.big", "W3DEnglishZH.big", "MapsZH.big", "WindowZH.big",
          "EnglishZH.big", "ShadersZH.big", "TerrainZH.big",
          "Audio.big", "AudioEnglish.big", "Music.big", "AudioZH.big", "AudioEnglishZH.big", "MusicZH.big",
          nullptr};
      // Count how many archives we will mount so the progress bar has a denominator.
      int archive_total = 0;
      while (startup_archives[archive_total] != nullptr) {
        ++archive_total;
      }
      for (int i = 0; startup_archives[i] != nullptr; ++i) {
        // Report BEFORE the fetch so the UI shows what is currently downloading.
        GX_ReportProgress("vfs", i, archive_total, startup_archives[i]);
        BigVFS::Mount_Archive(startup_archives[i]);
      }
      // Final tick so the bar shows complete before we move to the next stage.
      GX_ReportProgress("vfs", archive_total, archive_total,
                        "Mapping virtual filesystem");
      BigVFS::Mount_To_Emscripten_FS();

      // Diagnostic: probe the BigVFS lookup with the exact path the engine
      // uses for a menu .wnd file. If this prints `size=0` or "not found",
      // the engine's later fopen on the empty MEMFS placeholder will give
      // a 0-byte file and the menu won't render.
      // GeneralsX @feature WebPort 2026-05-05 — black-canvas triage
      {
        const char *probes[] = {
            "Window\\Menus\\MainMenu.wnd",
            "Window\\Menus\\BlankWindow.wnd",
            "data\\generals.csf",
            "Data\\English\\generals.csf",
            nullptr,
        };
        for (int i = 0; probes[i] != nullptr; ++i) {
          void *data = nullptr;
          unsigned int size = 0;
          bool ok = Web_VFS_Read_File_Sync(probes[i], &data, &size);
          fprintf(stderr,
                  "VFS-PROBE: '%s' -> ok=%d size=%u\n",
                  probes[i], (int)ok, size);
          if (ok && size >= 16) {
            const unsigned char *p = (const unsigned char *)data;
            fprintf(stderr, "VFS-PROBE: first 16 bytes: ");
            for (int j = 0; j < 16; ++j) fprintf(stderr, "%02x ", p[j]);
            fprintf(stderr, "\n");
            fprintf(stderr, "VFS-PROBE: as text: '");
            for (int j = 0; j < 64 && j < (int)size; ++j) {
              char c = (char)p[j];
              fputc(c >= 32 && c < 127 ? c : '.', stderr);
            }
            fprintf(stderr, "'\n");
          }
        }

        // Also try going through the engine's actual file open path to
        // confirm the LocalFile::open hook returns real data, not the
        // 0-byte MEMFS placeholder.
        FILE *f = fopen("/window/menus/mainmenu.wnd", "rb");
        if (f) {
          fseek(f, 0, SEEK_END);
          long sz = ftell(f);
          fclose(f);
          fprintf(stderr, "VFS-PROBE: fopen(/window/menus/mainmenu.wnd) -> size=%ld\n", sz);
        } else {
          fprintf(stderr, "VFS-PROBE: fopen(/window/menus/mainmenu.wnd) -> FAILED\n");
        }

        // fmemopen sanity: make sure Emscripten's libc fmemopen wraps a
        // memory buffer correctly. If this prints fseek/ftell = 64 the
        // engine's fmemopen path in LocalFile::open is at least functional.
        {
          static const char buf[64] = {1,2,3,4,5,6,7,8,9,10};
          FILE *mf = fmemopen((void *)buf, 64, "rb");
          if (mf) {
            fseek(mf, 0, SEEK_END);
            long sz = ftell(mf);
            fseek(mf, 0, SEEK_SET);
            char dst[8] = {0};
            size_t n = fread(dst, 1, 8, mf);
            fclose(mf);
            fprintf(stderr, "VFS-PROBE: fmemopen sanity sz=%ld read=%zu first=[%d %d %d %d]\n",
                    sz, n, dst[0], dst[1], dst[2], dst[3]);
          } else {
            fprintf(stderr, "VFS-PROBE: fmemopen returned NULL\n");
          }
        }

        // Mimic exactly what winCreateFromScript -> convertToRAMFile does
        // on the menu file: fmemopen, then fseek(0,END)+ftell to get size,
        // then fseek(0,SET)+fread to copy all bytes. If THIS fails, the
        // engine's identical path in convertToRAMFile is failing too.
        {
          void *data = nullptr;
          unsigned int size = 0;
          if (Web_VFS_Read_File_Sync("Window\\Menus\\MainMenu.wnd", &data, &size)) {
            FILE *mf = fmemopen(data, size, "rb");
            if (mf) {
              long pos1 = ftell(mf);            // expect 0
              int seekret = fseek(mf, 0, SEEK_END);
              long sz = ftell(mf);              // expect 208561
              fseek(mf, 0, SEEK_SET);
              static char *bigbuf = (char *)malloc(size + 1);
              size_t nread = fread(bigbuf, 1, size, mf);
              fclose(mf);
              fprintf(stderr,
                      "VFS-PROBE: fmemopen menu pos1=%ld seekret=%d sz=%ld nread=%zu first10='%c%c%c%c%c%c%c%c%c%c'\n",
                      pos1, seekret, sz, nread,
                      bigbuf[0], bigbuf[1], bigbuf[2], bigbuf[3], bigbuf[4],
                      bigbuf[5], bigbuf[6], bigbuf[7], bigbuf[8], bigbuf[9]);
              free(bigbuf);
            } else {
              fprintf(stderr, "VFS-PROBE: fmemopen on menu FAILED\n");
            }
          }
        }
      }
    } else {
      fprintf(stderr, "WARN: BigVFS::Init failed\n");
    }

    // 2. Initialize the GameEngine synchronously (takes a few seconds)
    GX_ReportProgress("boot", 6, 6, "Engine bring-up");
    GX_ReportProgress("engine", 0, 1, "Loading INI / textures / W3D");
    fprintf(stderr, "INFO: Starting synchronous GameEngine initialization...\n");
    TheGameEngine = CreateGameEngine();
    TheGameEngine->init();
    g_GameEngineInitialized = true;
    // GameEngine::init() calls `setQuitting(TRUE)` when
    // `TheAudio->isMusicAlreadyLoaded()` returns false. That helper iterates
    // m_allAudioEventInfo for an AT_Music entry then asks the file system
    // whether the corresponding music file exists. With the current
    // milesstub backend (no real audio device, no music archive guaranteed
    // to be present) the check is racy at best and an outright `false` at
    // worst — which kills the engine before the main menu paints. Clear
    // the flag here so the main loop boots normally. Real audio is the
    // documented follow-up (`port-todo.md` "Wire up real audio"); when that
    // lands `isMusicAlreadyLoaded` will return true on its own and this
    // override becomes a no-op.
    if (TheGameEngine->getQuitting()) {
      fprintf(stderr,
              "INFO: clearing post-init quit flag (no-music-loaded path)\n");
      TheGameEngine->setQuitting(FALSE);
    }
    GX_ReportProgress("engine", 1, 1, "Engine ready");
    GX_ReportProgress("ready", 1, 1, "");
    fprintf(stderr, "INFO: GameEngine::init() complete\n");

    // GeneralsX @feature WebPort 2026-05-05 — shellmap override.
    //
    // GameLOD::applyStaticLODLevel() disables m_shellMapOn whenever the
    // initial memory probe fails. The wasm heap is fixed-size by design so
    // m_memPassed is always FALSE on web, which leaves the menu without an
    // animated background because no shell map ever loads.
    //
    // Forcing m_shellMapOn=TRUE here makes the engine try to load
    // ShellMap1/ShellMapMD via MSG_NEW_GAME(GAME_SHELL). Path-normalisation
    // gets the .map blob delivered correctly (86 KB), but the EA binary
    // chunk parser inside WorldHeightMap's constructor performs unaligned
    // multi-byte reads that wasm aborts as "alignment fault". Auditing
    // every chunk reader for portable aligned loads is a substantial
    // separate effort.
    //
    // Keep the override OFF by default so the menu still renders as a
    // static background (current shipped behaviour). Set
    // window.GeneralsX.enableShellMap=true in the JS shell to opt-in for
    // local debugging once the WorldHeightMap parsing is fixed.
    if (TheWritableGlobalData) {
      const int opt_in = EM_ASM_INT(({
        if (window['GeneralsX'] && window['GeneralsX']['enableShellMap']) return 1;
        // Also accept ?shellmap=1 / #shellmap query string so the flag
        // survives page reloads without needing a JS hook to fire first.
        try {
          if (location.search && location.search.indexOf('shellmap=1') >= 0) return 1;
          if (location.hash && location.hash.indexOf('shellmap') >= 0) return 1;
        } catch(e) {}
        return 0;
      }));
      GX_TRACE_LOG(
              "GX-TRACE: WebPort post-init: m_shellMapOn was=%d, opt_in=%d, "
              "shellmap='%s'\n",
              (int)TheGlobalData->m_shellMapOn, opt_in,
              TheGlobalData->m_shellMapName.str());
      // Force OFF unless explicitly opted in. The shellmap path correctly
      // fetches the .map (BigVFS now short-circuits empty entries instead
      // of looping on -416), but downstream WorldHeightMap parsing still
      // hangs CPU-bound — likely an infinite loop fed by an unaligned
      // chunk-header read. Keep opt-in until that parser is hardened.
      TheWritableGlobalData->m_shellMapOn = opt_in ? TRUE : FALSE;
    }

    // Engage the deferred audio archive mount. Wait a few frames first so the
    // user sees the main menu paint before the network starts pulling MB.
    g_deferredMountState = 1;
    g_deferredMountWaitFrames = 30;  // ~500ms at 60fps

    // 3. Start the main event loop
    // This provides the JS runtime immediate return of "main"
    fprintf(stderr, "INFO: Starting Emscripten main loop (GameWebFrameCallback) ...\n");
    // GeneralsX @feature WebPort 2026-05-05 — black-canvas root cause #2:
    //
    // `target_fps == 0` makes `emscripten_set_main_loop` use
    // `requestAnimationFrame`. The browser aggressively throttles rAF for
    // any document where `document.hidden` is true — and "hidden" can
    // mean a backgrounded tab, an iframe whose ancestor frame is not
    // currently composited — e.g. an embedding preview iframe
    // (which reports `document.hidden === true` even while visible). With
    // rAF stalled the engine's frame callback never runs, so the canvas
    // stays on whatever the last drawn state was — black.
    //
    // Pass an explicit fps (60) so the runtime uses `setTimeout` instead.
    // The tradeoff is no vsync alignment, but a stable 60Hz tick beats a
    // rAF-throttled 0Hz any day. Native browsers viewing the page
    // top-level will still get good pacing because setTimeout + frame
    // skipping behaves sensibly there too.
    int target_fps = 60;
    const int is_vsync_opt_in = EM_ASM_INT(({
      var vsync_override = 0;
      try {
        if (location.search && location.search.indexOf('vsync=1') >= 0) vsync_override = 1;
        if (location.search && location.search.indexOf('vsync=0') >= 0) vsync_override = -1;
        if (location.hash && location.hash.indexOf('vsync') >= 0) vsync_override = 1;
      } catch(e) {}

      if (vsync_override === 1) return 1;
      if (vsync_override === -1) return 0;

      // Use vsync if we are top-level
      try {
        if (window.self === window.top) {
          return 1;
        }
      } catch(e) {}
      return 0;
    }));

    if (is_vsync_opt_in) {
      target_fps = 0; // 0 maps to requestAnimationFrame (vsync) in Emscripten
      printf("target FPS: vsync\n");
    } else {
      printf("target FPS: 60\n");
    }
    WebMainLoop::Start(GameWebFrameCallback, target_fps, true);

  } catch (const std::exception &e) {
    fprintf(stderr, "FATAL: Exception during init: %s\n", e.what());
    return 1;
  } catch (...) {
    fprintf(stderr, "FATAL: Unknown exception during init\n");
    return 1;
  }

  return 0; // Exits cleanly, transferring control entirely to browser
}

#include "Common/FileSystem.h"
#include "Common/file.h"

extern "C" char* WebAudioBridge_ReadEntireFile(const char* filename, uint32_t* out_size)
{
  if (!TheFileSystem) {
    printf("[WebAudioBridge_ReadEntireFile] TheFileSystem is NULL\n");
    return nullptr;
  }
  File* f = TheFileSystem->openFile(filename, File::READ | File::BINARY);
  if (!f) {
    printf("[WebAudioBridge_ReadEntireFile] Failed to open '%s'\n", filename);
    return nullptr;
  }
  uint32_t size = f->size();
  char* buf = f->readEntireAndClose();
  if (out_size) {
    *out_size = size;
  }
  return buf;
}

#endif // __EMSCRIPTEN__

