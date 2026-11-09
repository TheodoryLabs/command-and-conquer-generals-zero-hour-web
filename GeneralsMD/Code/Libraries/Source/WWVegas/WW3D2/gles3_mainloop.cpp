/*
** GeneralsX Web Port — Main Loop Adapter Implementation
**
** Bridges the game's traditional while-loop to Emscripten's
** requestAnimationFrame-based callback system.
**
** LICENSE: GPL-3.0
*/

#if defined(__EMSCRIPTEN__)

#include "gles3_mainloop.h"
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <cstdio>
#include <cmath>

// ============================================================================
// Internal state
// ============================================================================
static struct {
    WebFrameCallback frame_callback = nullptr;
    bool shutdown_requested = false;
    bool paused = false;

    // Timing
    double start_time_ms = 0.0;
    double last_frame_time_ms = 0.0;
    float delta_time_ms = 16.667f;  // ~60fps default
    unsigned int frame_count = 0;

    // FPS calculation (rolling average)
    float fps = 60.0f;
    float fps_accumulator = 0.0f;
    unsigned int fps_frame_count = 0;
    double fps_last_update = 0.0;

    // Loading
    WebMainLoop::LoadingProgressCallback loading_callback = nullptr;

    // Visibility
    WebVisibility::VisibilityCallback visibility_callback = nullptr;
} g_Loop;

// ============================================================================
// WebTiming
// ============================================================================
namespace WebTiming {

double Now_Ms() {
    return emscripten_get_now();
}

void Yield_Ms(int ms) {
    // No-op to avoid Asyncify dependency
}

} // namespace WebTiming

// ============================================================================
// Per-frame callback — called by Emscripten's requestAnimationFrame
// ============================================================================
static void emscripten_main_loop_callback() {
    if (g_Loop.shutdown_requested) {
        emscripten_cancel_main_loop();
        return;
    }

    if (g_Loop.paused) {
        // Still running the loop but skipping game logic
        // This keeps the browser tab responsive
        return;
    }

    // Calculate delta time
    double now = WebTiming::Now_Ms();
    if (g_Loop.last_frame_time_ms > 0.0) {
        g_Loop.delta_time_ms = (float)(now - g_Loop.last_frame_time_ms);
    }
    g_Loop.last_frame_time_ms = now;

    // Clamp delta time to prevent spiral of death after tab switch
    // (browser pauses rAF when tab is hidden; first frame back has huge dt)
    if (g_Loop.delta_time_ms > 100.0f) {
        g_Loop.delta_time_ms = 16.667f;  // pretend it was one frame
    }

    // Update FPS counter (every ~500ms)
    g_Loop.fps_accumulator += g_Loop.delta_time_ms;
    g_Loop.fps_frame_count++;
    if (g_Loop.fps_accumulator >= 500.0f) {
        g_Loop.fps = (float)g_Loop.fps_frame_count / (g_Loop.fps_accumulator / 1000.0f);
        g_Loop.fps_accumulator = 0.0f;
        g_Loop.fps_frame_count = 0;
    }

    // Run game frame
    if (g_Loop.frame_callback) {
        bool keep_running = g_Loop.frame_callback(g_Loop.delta_time_ms);
        if (!keep_running) {
            g_Loop.shutdown_requested = true;
        }
    }

    g_Loop.frame_count++;
}

// GeneralsX @feature 09/11/2026 Browsers stop requestAnimationFrame in a hidden tab, and a lockstep
// LAN game then stalls for every player. The page calls this from a Web Worker timer (not
// throttled when hidden) while a networked game's tab is hidden.
extern "C" EMSCRIPTEN_KEEPALIVE void GX_TickHidden() {
    emscripten_main_loop_callback();
}

// ============================================================================
// WebMainLoop
// ============================================================================
namespace WebMainLoop {

void Start(WebFrameCallback frame_cb, int target_fps,
           bool simulate_infinite_loop) {
    g_Loop.frame_callback = frame_cb;
    g_Loop.shutdown_requested = false;
    g_Loop.start_time_ms = WebTiming::Now_Ms();
    g_Loop.last_frame_time_ms = g_Loop.start_time_ms;
    g_Loop.frame_count = 0;
    g_Loop.fps = (target_fps > 0) ? (float)target_fps : 60.0f;

    fprintf(stderr, "INFO: Starting Emscripten main loop (target FPS: %s)\n",
            target_fps == 0 ? "vsync" : std::to_string(target_fps).c_str());

    // 0 = use requestAnimationFrame (sync to display refresh)
    // simulate_infinite_loop = true means main() won't return
    emscripten_set_main_loop(emscripten_main_loop_callback,
                              target_fps,
                              simulate_infinite_loop ? 1 : 0);
}

void Request_Shutdown() {
    fprintf(stderr, "INFO: Shutdown requested\n");
    g_Loop.shutdown_requested = true;
}

bool Is_Shutdown_Requested() {
    return g_Loop.shutdown_requested;
}

double Get_Elapsed_Time_Ms() {
    return WebTiming::Now_Ms() - g_Loop.start_time_ms;
}

float Get_Delta_Time_Ms() {
    return g_Loop.delta_time_ms;
}

float Get_FPS() {
    return g_Loop.fps;
}

unsigned int Get_Frame_Count() {
    return g_Loop.frame_count;
}

void Pause() {
    g_Loop.paused = true;
    fprintf(stderr, "INFO: Main loop paused\n");
}

void Resume() {
    if (g_Loop.paused) {
        g_Loop.paused = false;
        // Reset timing to avoid huge delta on resume
        g_Loop.last_frame_time_ms = WebTiming::Now_Ms();
        fprintf(stderr, "INFO: Main loop resumed\n");
    }
}

bool Is_Paused() {
    return g_Loop.paused;
}

void Set_Loading_Callback(LoadingProgressCallback cb) {
    g_Loop.loading_callback = cb;
}

void Run_Loading_Phase(LoadStartCallback load_func,
                        LoadReadyCallback ready_func) {
    if (load_func) load_func();

    // Non-yielding poll if needed (Run_Loading_Phase is not actively called in game loop)
    while (ready_func && !ready_func()) {
        if (g_Loop.loading_callback) {
            g_Loop.loading_callback(0.5f, "Loading game assets...");
        }
    }
}

} // namespace WebMainLoop

// ============================================================================
// WebVisibility — pause game when tab is hidden
// ============================================================================
namespace WebVisibility {

static EM_BOOL visibility_change_callback(int eventType,
                                            const EmscriptenVisibilityChangeEvent* event,
                                            void* userData) {
    bool is_visible = !event->hidden;

    if (is_visible) {
        WebMainLoop::Resume();
    } else {
        WebMainLoop::Pause();
    }

    if (g_Loop.visibility_callback) {
        g_Loop.visibility_callback(is_visible);
    }

    return EM_TRUE;
}

void Set_Callback(VisibilityCallback cb) {
    g_Loop.visibility_callback = cb;
}

void Init() {
    emscripten_set_visibilitychange_callback(nullptr, EM_TRUE,
                                              visibility_change_callback);
    fprintf(stderr, "INFO: Tab visibility monitoring active\n");
}

} // namespace WebVisibility

#endif // __EMSCRIPTEN__
