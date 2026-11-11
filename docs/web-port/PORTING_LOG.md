# GeneralsX Web Port — Porting Log

## 2026-03-13 — Session 1: Initial Setup
- Started agent session
- Initialized tracker files (port-todo.md, porting-log.md, architecture-map.md)
- Audited initial build errors from ninja_error.log

## 2026-03-14 — Session 2: Rendering Pipeline
- Fixed 5 rendering bugs:
  1. DUMMY_IUNKNOWN COM refcounting → proper AddRef/Release with m_comRefCount
  2. BGRA texture swizzle → GL_TEXTURE_SWIZZLE_R/B for D3DFMT_A8R8G8B8/X8R8G8B8
  3. DXT mip upload → explicit per-level glCompressedTexImage2D
  4. Cull mode reversal → D3DCULL_CCW maps to GL_BACK (not GL_FRONT)
  5. Disabled lights → upload u_Lights[i].enabled=0 on every draw call
- Identified POOL-DOUBLE-FREE as startup blocker
- Added instrumentation to freeSingleBlock (callstack logging before abort)
- Created RENDERING_STATUS.md and BUILD_INSTRUCTIONS.md
- Created link_mac.sh for link-only builds

## 2026-03-15 — Session 3: Memory System + Porting Audit

### POOL-DOUBLE-FREE Analysis
- Exhaustive static analysis of all candidate double-free paths:
  - StringClass (new[]/delete[] → global operator → DMA pool)
  - AsciiString (DMA allocateBytes/freeBytes directly)
  - HashTemplateClass, TextureLoadTaskClass, DDSFileClass
- Confirmed WASM32 memory layout: MemoryPoolSingleBlock = 8 bytes (m_owningBlob + m_nextBlock)
- Verified sentinel value 0xDEADF00D detection logic is correct
- Root cause requires runtime browser callstack (static analysis exhausted)

### Changes Made
1. **GameMemory.cpp — freeSingleBlock non-fatal on Emscripten**
   - On double-free detection, prints full C++/JS callstack via emscripten_get_callstack()
   - Skips the free (leaks the block) instead of calling abort()
   - Game continues past the crash, allowing startup to proceed
   - The leaked block is a minor memory cost vs. a hard crash

2. **web.cmake — DISABLE_GAMEMEMORY nuclear option**
   - Added commented-out `set(RTS_GAMEMEMORY_ENABLE OFF)` with documentation
   - Enables GameMemoryNull.h path: all pool allocators → system malloc/free
   - Requires full rebuild (ninja) if enabled
   - Fallback if non-fatal workaround isn't sufficient

3. **web.cmake — existing WASM32 pool fixes documented**
   - DISABLE_MEMORYPOOL_MPSB_DLINK: prevents list-consistency bug in DMA free list
   - DISABLE_MEMORYPOOL_BOUNDINGWALL: prevents SI-vptr=0 pattern from post-wall overlap

### Comprehensive Porting Gap Audit
Identified all remaining gaps organized by severity:

**CRITICAL (blocks interactivity):**
- SDL input translation: DirectInput stubs are no-ops. Need SDL2 event → game input bridge for keyboard/mouse. Without this, the game renders but accepts no input.
- GameSpy networking: Must be disabled/stubbed for web. Attempting network calls will hang or crash in WASM.

**HIGH (blocks usability):**
- QueryPerformanceCounter/Frequency: Returns 0, causing timing issues. Need emscripten_get_now() bridge.
- Pointer Lock API: Mouse confinement for RTS camera control.
- Save game persistence: Emscripten FS is ephemeral. Need IDBFS mount for user saves/settings.

**MEDIUM (degraded experience):**
- Audio delayed-release thread: CreateThread is no-op on Emscripten. Sound objects may leak.
- On-demand audio archive loading: Currently deferred, needs trigger after main menu.

**LOW (polish):**
- Canvas resize / fullscreen toggle
- Loading progress UI
- WebGL context loss recovery
- Release build optimization (remove -g, -sSAFE_HEAP, -sASSERTIONS)

### CRITICAL Gaps Fixed (same session)

4. **SDL2 Input Translation Layer (EmscriptenInput.cpp/h)**
   - Created SDL2→DirectInput keyboard translation: 80+ SDL_SCANCODE→DIK_* mappings
   - Modified dinput.h stub: IDirectInputDevice8::GetDeviceData now reads from a ring buffer
   - SDL mouse events routed to Win32Mouse::addWin32Event() via EmscriptenInput_RouteMouseEvent()
   - SDL event pump called once per frame in GameWebFrameCallback before engine update
   - Supports: keyboard up/down, mouse motion, left/right/middle buttons, scroll wheel

5. **GameSpy Networking Disabled**
   - Added `NO_GAMESPY=1` define to web.cmake
   - GameSpy code still compiles (with socket stubs) but is inert at runtime:
     - Threads never start (CreateThread is no-op)
     - TheGameSpyChat stays null (never created during single-player)
     - Overlay system handles null gracefully

6. **QueryPerformanceCounter/Frequency Fixed**
   - Replaced stub (returned 0/1) with emscripten_get_now() bridge
   - Simulates 1MHz counter (microsecond resolution)
   - QPF returns 1000000LL, QPC returns emscripten_get_now() * 1000

7. **DIDEVICEOBJECTDATA Struct Conflict Resolved**
   - windows_base.h and dinput.h both defined the struct differently
   - Added include guard so only one definition is active
   - Unified field type for dwAppData (unsigned long *)

8. **WM_MOUSEWHEEL + MAKELPARAM Added**
   - Added missing WM_MOUSEWHEEL (0x020A) define to windows_base.h
   - Added MAKELPARAM macro for constructing lParam values

## 2026-03-15 — Session 4: Audit & Build Fixes

### Deep audit of all Session 3 changes
Audited every new/modified file for compilation errors, ODR violations,
duplicate symbols, and include path issues.

### Build-breaking fixes

9. **LinuxStubs.cpp — c_dfDIKeyboard duplicate symbol (LINKER ERROR)**
   - EmscriptenInput.cpp defines `const void *c_dfDIKeyboard = nullptr;`
   - LinuxStubs.cpp also defined `void *c_dfDIKeyboard = nullptr;`
   - On Emscripten, both files are compiled → "multiple definition" link error
   - Fix: guarded LinuxStubs.cpp definition with `#ifndef __EMSCRIPTEN__`

10. **LinuxStubs.cpp — TheWin32Mouse type mismatch (ODR violation)**
    - Was: `void *TheWin32Mouse = nullptr;`
    - WinMain.h declares: `extern Win32Mouse *TheWin32Mouse;`
    - W3DGameClient::createMouse() assigns a `Win32Mouse*` to it
    - EmscriptenMain.cpp reads it as `Win32Mouse*` for addWin32Event()
    - Fix: forward-declare `class Win32Mouse;` and define as `Win32Mouse*`

11. **LinuxStubs.cpp — ApplicationHInstance type mismatch (ODR violation)**
    - Was: `void *ApplicationHInstance = nullptr;`
    - WinMain.h declares: `extern HINSTANCE ApplicationHInstance;`
    - Fix: changed to `HINSTANCE ApplicationHInstance = nullptr;`

### Warning fixes

12. **windows_base.h — WSA macro redefinition warnings**
    - 5 WSA* macros (WSAEINVAL, WSAEALREADY, WSAEISCONN, WSAECONNRESET, WSAENOTCONN)
      defined twice: first as numeric constants (for GameSpy), then as POSIX aliases
    - Fix: added `#undef` before each POSIX redefinition

13. **EmscriptenInput.cpp — redundant WM_MOUSEWHEEL_EMSCRIPTEN**
    - Had its own `#define WM_MOUSEWHEEL_EMSCRIPTEN 0x020A`
    - windows_base.h already defines `WM_MOUSEWHEEL 0x020A`
    - Fix: removed redundant define, use `WM_MOUSEWHEEL` directly

14. **DISABLE_GAMEMEMORY Nuclear Option Activated**
    - Uncommented `set(RTS_GAMEMEMORY_ENABLE OFF CACHE BOOL ... FORCE)` in web.cmake
    - Swaps GameMemory.cpp (custom pool) → GameMemoryNull.cpp (pure malloc/free)
    - Eliminates POOL-DOUBLE-FREE entirely (no custom pool = no pool corruption)
    - `FORCE` keyword overrides the cached `ON` value from config-memory.cmake
    - GameMemoryNull.cpp: initMemoryManager() still creates factory/allocator stubs
    - All `NEW` / `delete` → standard `malloc(size) + memset(0)` / `free(p)`
    - Verified: __cdecl handled by Emscripten clang, <malloc.h> available via musl

15. **GameMemoryNull.h — Added Missing Macros and Types**
    - GameMemoryNull.h was incomplete: missing critical macros that hundreds of classes use
    - Added `MEMORY_POOL_GLUE(ARGCLASS, ARGPOOLNAME)` → expands to just virtual destructor declaration
    - Added `MEMORY_POOL_GLUE_WITH_EXPLICIT_CREATE(ARGCLASS, ARGPOOLNAME, ARGINITIAL, ARGOVERFLOW)` → same
    - Added `GCMP_FIND` and `GCMP_CREATE` as empty macros (no pool lookup in null mode)
    - Added `DECLARE_LITERALSTRING_ARG1/ARG2` and `PASS_LITERALSTRING_ARG1/ARG2` as empty macros
    - Added `MemoryPool` forward declaration (some headers reference `MemoryPool*`)
    - Added `MemoryPoolSingleBlock` and `MemoryPoolBlob` forward declarations
    - Added `PoolInitRec` struct (needed by function signature in engine init)
    - Added `STLSpecialAlloc` class with `allocate()`→`::operator new` and `deallocate()`→`::operator delete`
    - In null mode, all MEMORY_POOL_GLUE macros reduce to just `protected: virtual ~ClassName(); public:`
    - No custom operator new/delete per-class; the global operator new/delete (malloc+memset) handles everything

## 2026-03-31 — First Successful Web Link
- `build-html5/GeneralsMD/GeneralsZH.{html,js,wasm}` produced successfully (build.log
  ends with `[100%] Built target z_generals`).
- 145 MB debug .wasm with `-g3 -sASSERTIONS=2`. Distributed copy under
  `GeneralsX/dist/` (committed as untracked) is 44 MB — the RelWithDebInfo build
  the link_mac.sh path produces.
- Project marked publishable: `feat(web): C&C Generals Zero Hour — full
  Emscripten/WebAssembly port` (commit d481e4a, Apr 7 2026) plus follow-up docs.

## 2026-05-04 — Session 5: Triage After Pickup
- Re-opened the project; the `<build-root>/build.log`
  in the parent dir reports undefined `TheMessageTime`, `ApplicationHInstance`,
  `c_dfDIKeyboard`. **That log is stale (Mar 13)** — it predates Session 4's
  stub additions and the Mar 31 successful link. Verified via direct WASM
  object inspection of `LinuxStubs.cpp.o` and `EmscriptenInput.cpp.o`:
  - `LinuxStubs.cpp.o` has `.bss.TheMessageTime`, `.bss.ApplicationHInstance`,
    `.bss.TheWin32Mouse` symbol entries.
  - `EmscriptenInput.cpp.o` has `.bss.c_dfDIKeyboard` plus the keyboard ring
    buffer globals.

### Code-correctness fixes (Session 5)
16. **LinuxStubs.cpp — moved Win32 stub globals inside `#ifndef _WIN32`**
    - `TheMessageTime`, `ApplicationHInstance`, and `c_dfDIKeyboard` were sitting
      *after* the `#endif // !_WIN32` line, so they would have been duplicate-
      symbol errors on a native Windows build (where `WinMain.cpp` defines them).
      The Emscripten link only succeeded because `WinMain.cpp` is excluded from
      that target.
    - Moved all three definitions inside the `#ifndef _WIN32` block. Added a
      header comment that points at the gating in `Main/CMakeLists.txt` so the
      next person who edits this file understands the contract.

17. **LinuxStubs.cpp — `int TheMessageTime` → `DWORD TheMessageTime`**
    - `Win32GameEngine.cpp` declares `extern DWORD TheMessageTime;`. The previous
      definition used `int`. Same width on WASM32 (32 bits) but different signed-
      ness; now matches the extern verbatim and matches the original WinMain.cpp
      definition.

18. **LinuxStubs.cpp — `void* c_dfDIKeyboard` → `const void *c_dfDIKeyboard`**
    - Matches `dinput.h`'s `extern const void *c_dfDIKeyboard;`. Only the
      non-Emscripten branch was changed; the Emscripten definition in
      `EmscriptenInput.cpp` was already `const void *`.

### Triage notes
- No active build break that I could observe in the working tree. The .wasm
  outputs in both `build-html5/` (Mar 31) and `GeneralsX/dist/` (Apr 7) are
  consistent with a clean build.
- A re-link is needed to pick up the Session 5 LinuxStubs.cpp edits. None of the
  edits should change the link line; the `.o` will simply be regenerated and
  carry the same exported symbols (with the type change being binary-compatible
  on WASM32).

## 2026-05-04 — Session 6: Loading Progress Bar (Polish item #1)

The page sat black for ~10 seconds while BigVFS mounted 15 .big archives over
HTTP, with no UI feedback. Implemented a themed loading screen with a global
progress bar fed by per-stage events from the C++ engine.

### New files
- **`GeneralsMD/Code/Main/web_shell.html`** — custom Emscripten shell. Self-
  contained HTML/CSS/JS; no external assets. C&C-themed dark background with a
  red/gold progress bar. Exposes `window.GeneralsX.setProgress(stage, current,
  total, label)` that the engine calls. Also has:
  - `Module.setStatus` / `Module.monitorRunDependencies` mapped onto a 0-5%
    "WASM download" portion of the bar so the user sees motion immediately.
  - `Module.onAbort` + `window.onerror` populate a hidden `#gx-error` overlay
    that takes over the screen on a fatal exception (instead of a tab-blocking
    `alert()`).
  - `F2` toggles a small on-page log (`#gx-output`) for in-browser triage when
    DevTools is awkward to reach.

### C++ changes
19. **`EmscriptenMain.cpp` — `GX_ReportProgress(stage, current, total, label)`**
    - Static-inline helper at the top of the file. Wraps an `EM_ASM` block that
      calls `window.GeneralsX.setProgress()` with `UTF8ToString` for the two
      string args (stage and label).
    - Guarded with `if (typeof window !== 'undefined' && window.GeneralsX)` so
      builds against the default Emscripten shell still work — the call is a
      no-op there. Useful for `-sSINGLE_FILE` / headless diagnostics builds.
    - Stage taxonomy (matched literally against keys in the JS):
      - `boot`   — 6 substeps for SDL/GL/frame pacer init
      - `vfs`    — one tick per archive (15 archives), label = archive filename
      - `engine` — 0/1 → "Loading INI / textures / W3D", 1/1 → "Engine ready"
      - `ready`  — fades the loading overlay out

20. **Bar weighting** — split as `download:0.05 / boot:0.05 / vfs:0.55 /
    engine:0.35 = 1.00`. The vfs stage gets the lion's share because it's
    network-bound and visibly the slowest. Bar guards against backwards motion
    (`lastPct`) so any out-of-order progress events just hold the bar in place.

### Build wiring
21. **`GeneralsMD/Code/Main/CMakeLists.txt`** — added `--shell-file
    ${GX_WEB_SHELL}` to `target_link_options` for the Emscripten target plus
    a `LINK_DEPENDS` property so changing the shell triggers a relink.
22. **`<build-root>/link_mac.sh`** — added the
    same `--shell-file` to the manual link path so a `link_mac.sh` rebuild
    picks up the new shell without needing a full ninja run.

### To verify
- Run `link_mac.sh` (or `ninja -C build-html5 z_generals`) on the Mac.
- Open `build-html5/GeneralsMD/GeneralsZH.html` (or the `dist/` copy if you
  rebuild that).
- Expected: the loading overlay appears immediately with title "Generals Zero
  Hour / Web Edition", progress bar fills as the WASM downloads (0-5%), then
  the C++ progress events drive it from 5% to 100% with stage strings
  "Booting engine" → "Mounting game data: INI.big (1/15)" → ... → "Initializing
  game engine" → "Ready", then the overlay fades out.

## 2026-05-04 — Session 7: Pointer Lock + Cursor Visibility (Polish item #2)

The browser's default cursor sits on top of the engine's rendered cursor (cursor
double-up) and right-click camera pan hits the screen edge after a short drag.
Implemented two related changes that solve both:

### Cursor visibility — `SetCursor()` routing
23. **`emscripten_compat/windows_base.h`** — `SetCursor(HCURSOR)` was a no-op.
    Replaced its body on Emscripten with a forward to
    `EmscriptenInput_SetCanvasCursor(h ? 1 : 0)`. The engine convention is
    `SetCursor(nullptr)` → "I'll draw the cursor myself, hide the system one"
    and `SetCursor(non-null)` → "show the system cursor", so we just translate
    null vs. non-null into a JS canvas-style toggle.
24. **`EmscriptenInput.cpp`** — added `EmscriptenInput_SetCanvasCursor(int)`.
    Dedup'd against the previous reported state (Win32Mouse calls SetCursor
    every frame for the active cursor) and routed via `EM_ASM` to
    `window.GeneralsX.setCanvasCursor(visible)`. Falls back to a direct
    `canvas.style.cursor =` assignment when the custom shell isn't loaded.
25. **`web_shell.html`** — added `setCanvasCursor` to the `window.GeneralsX`
    surface, which sets `canvas.style.cursor` to `'default'` or `'none'`.

### Pointer Lock — JS-owned because of the user-gesture rule
The hard problem with Pointer Lock on a SDL2-Emscripten app: the browser only
grants the lock if `requestPointerLock()` is called *synchronously* from within
the user-gesture call stack, but SDL2-Emscripten queues mouse events from the
real DOM listeners and drains them via `requestAnimationFrame` — by the time we
see `SDL_MOUSEBUTTONDOWN` in the C++ event loop, the gesture stack is gone.

26. **JS owns the request** — `web_shell.html` now registers a
    `canvas.addEventListener('mousedown')` listener that calls
    `canvas.requestPointerLock()` for `e.button === 2` (right mouse). Mirror
    listener on `mouseup` calls `document.exitPointerLock()`. Both run inside
    the actual DOM gesture stack, so the browser grants the lock cleanly.
27. **`pointerlockchange` → C++ callback** — JS listens for the state change
    and calls `Module.ccall('EmscriptenInput_OnPointerLockChange', ...)` which
    flips an internal `g_relativeMouseActive` flag in the C++ side.
28. **`EmscriptenInput.cpp` SDL_MOUSEMOTION handler** — when `g_relativeMouseActive`
    is true, accumulate `event.motion.xrel` / `yrel` into a virtual cursor
    position (`g_virtualMouseX`, `g_virtualMouseY`) clamped to the canvas
    extents, and feed THAT to Win32Mouse via the existing `WM_MOUSEMOVE` route.
    The engine sees ordinary absolute coords either way — the camera-pan delta
    math is unchanged.
29. **`EmscriptenInput.cpp` SDL_WINDOWEVENT_FOCUS_LOST** — proactively releases
    the lock if the canvas loses focus (e.g., `Alt+Tab`), so our state mirror
    doesn't drift out of sync with the browser's.
30. **Exports plumbing** — added
    `_EmscriptenInput_OnPointerLockChange` to `EXPORTED_FUNCTIONS` in both
    `GeneralsMD/Code/Main/CMakeLists.txt` and `link_mac.sh`. The
    `EMSCRIPTEN_KEEPALIVE` attribute on the C function is also there as
    belt-and-braces.

### Caveats / follow-ups
- We still keep `EmscriptenInput_SetRelativeMouseMode()` as a thin wrapper
  around `SDL_SetRelativeMouseMode()` for source compatibility — but we no
  longer rely on it for the actual lock request. SDL2's call may or may not
  succeed depending on browser timing; we log a warning and continue.
- Cursor visibility for non-canvas areas (e.g., when the engine renders a
  full-screen menu) is currently coupled to whatever the engine last passed
  to `SetCursor`. If the engine forgets to call SetCursor on a state
  transition, the cursor visibility will look stale until the next call.
  Acceptable for now — the existing engine code calls SetCursor reliably.
- A "click-to-start" overlay (so the user makes a first interaction before
  the WASM tries any audio / pointer-lock action) is *not* yet wired up; if
  Chrome's autoplay policy blocks audio in a future session, that's where
  to add it.

### To verify
- Re-link with `link_mac.sh` (or full `ninja`).
- Open the page; the system cursor should be visible during the loading
  overlay.
- After the engine renders the main menu the system cursor should disappear
  (the engine draws its own).
- Right-click and drag on the main map view: the OS cursor should hide, the
  view should scroll continuously past where the screen edge would normally
  stop the drag, and releasing the right button should restore the cursor.
- Console should log `INFO: pointer lock engaged` / `released` on each cycle.

## 2026-05-04 — Session 8: Audio (Polish item #3)

Two related problems blocking sound: the audio archives weren't mounted at
all, and the WWAudio delayed-release worker thread doesn't run on
single-threaded Emscripten so any object the engine asked the audio system
to release-after-N-ms accumulated forever.

### Single-threaded delayed-release drain — implemented but dormant
31. **`Core/Libraries/Source/WWVegas/WWAudio/Threads.h` + `Threads.cpp`** —
    added a public static method `Tick_Delayed_Release_Objects()` that
    mirrors one iteration of `Delayed_Release_Thread_Proc`: walks the
    release list under the existing `m_ListMutex`, frees any entry whose
    `time` deadline has passed, leaves the rest. Same lock as the worker so
    the call is safe even on platforms where the worker IS running (no
    behavioral change there).

32. **Did NOT wire it up in `EmscriptenMain.cpp`.** Inspecting
    `build-html5/.../linkLibs.rsp` showed that `libwwaudio.a` is not in
    `z_generals`'s link line on the web build — the audio path is currently
    stubbed via `milesstub`, so `WWAudioThreadsClass` exists in source and
    compiles into `libwwaudio.a` but is not pulled into the final link.
    Calling the helper from `EmscriptenMain.cpp` would have produced an
    unresolved-symbol link error.

    Left a `NOTE:` comment in `EmscriptenMain.cpp` at the call site and at
    the top of the file explaining what to do when WWAudio is actually
    linked: add `target_link_libraries(z_generals PRIVATE z_wwaudio)`,
    `#include "Threads.h"`, and the per-frame call right after
    `TheGameEngine->update()`.

### Deferred audio archive mount
33. **`EmscriptenMain.cpp` — `DeferredAudio_Step()` state machine.**
    Static-storage state machine driven from the frame callback. States:
    - `0`: idle (default).
    - `1`: armed; counting down `g_deferredMountWaitFrames` (~30 frames /
      ~500ms) so the main menu has a chance to paint *before* we start
      pulling network bytes.
    - `2`: mounting one archive per frame from `kDeferredAudioArchives` so
      the main loop stays responsive between fetches. Each
      `BigVFS::Mount_Archive(name)` does a single ~512KB range request to
      grab the .big header — the actual file bodies stream in later via
      HTTP-range as the audio engine opens individual sounds.
    - `3`: done. Calls `BigVFS::Mount_To_Emscripten_FS()` once at the end
      to refresh the VFS file table so the engine can `fopen()` the new
      paths.
    The state machine arms itself from `main()` immediately after the
    `ready` progress event:
    ```
    g_deferredMountState = 1;
    g_deferredMountWaitFrames = 30;
    ```

34. **Archive list** — `AudioZH.big`, `MusicZH.big`, `SpeechZH.big`. These
    were called out in `SETUP.md` as "not yet required — audio archive
    mounting is a pending feature"; now they ARE required for sound (but
    still optional for a silent boot since the deferred mount tolerates
    fetch failures).

### Shell / UX
35. **`web_shell.html`** — added a corner "background load" toast
    (`#gx-toast`) plus `window.GeneralsX.setSecondaryProgress(stage,
    current, total, label)` and `hideSecondaryProgress()`. Doesn't block
    input — user can navigate the menu while audio assets are loading. The
    C++ DeferredAudio state machine fires these via `EM_ASM` at the same
    points it does work.

### To verify
- Re-link.
- Open the page; ~500ms after the loading overlay disappears, the toast
  should appear in the bottom-right showing `Loading audio assets — 0/3 —
  AudioZH.big`, then tick to 1/3 (MusicZH.big), 2/3 (SpeechZH.big), then
  `Audio ready` and fade.
- Console should log `INFO: Deferred mount: AudioZH.big` etc. and finally
  `INFO: Deferred audio mount complete`.
- A click on a menu button should produce its hover/click sound (this is
  the actual end-to-end audio test). If silent, suspect the milesstub
  library — see `port-todo.md` follow-up bullet under "Audio".

## 2026-05-04 — Session 9: IndexedDB Save Persistence (Polish item #4)

Save games and `Options.ini` changes evaporated when the tab closed because
the engine wrote to MEMFS (in-memory only). Wired up Emscripten's IDBFS so
the user-data directory is now backed by IndexedDB and persists across
reloads.

### Mount + initial sync (JS, runs before `main()`)
36. **`web_shell.html` — `Module.preRun = [idbfsPreRun]`** — `idbfsPreRun`
    creates `/userdata`, mounts `IDBFS` over it, then calls
    `FS.syncfs(true, …)` to populate MEMFS from IndexedDB. The pre-run hook
    bumps a runtime dependency (`addRunDependency('idbfs-load')`) so the
    Emscripten boot waits for `removeRunDependency` before invoking `main()`.
    Result: by the time the C++ side runs, any previously-saved files are
    already on disk in MEMFS.

### Engine path adjustment (C++)
37. **`GlobalData.cpp`** — added an `#ifdef __EMSCRIPTEN__` block at the end
    of the GlobalData ctor that overrides the dynamically-built path with a
    clean `m_userDataDir = "/userdata/"`. The default Linux path math would
    have produced something like `/home\Command and Conquer Generals Zero
    Hour Data\` (mixing forward and backslashes from the HOME env var and
    the Win32-style trailing separator), which doesn't map to anything
    sensible in MEMFS.

38. **`Core/.../LocalFile.cpp` — backslash-to-forward-slash normalization
    in `LocalFile::open` on Emscripten.** Engine code that *appends* to the
    user-data dir (e.g., `getSaveDirectory()` adds `Save\`) still produces
    backslash-laden paths — and on Emscripten/MEMFS, `\` is a valid filename
    character, not a separator. Without normalization, a save to
    `/userdata/Save\foo.sav` produces a single weirdly-named file at
    `/userdata`. The fix scans the filename once and, if any `\` are
    present, copies into a stack buffer with `/` instead and uses that for
    `fopen`. The Web_VFS_Read_File_Sync read path already handles both
    separators internally, so reads were unaffected.

### Periodic + visibility flush (C++)
39. **`EmscriptenMain.cpp` — `GX_FlushIdbfs(reason)`** wraps an `EM_ASM`
    that calls `FS.syncfs(false, …)` with debouncing. While a flush is in
    flight, additional flush requests just set a `pending` flag; on
    completion, the pending one runs. Prevents a tight save loop from
    piling up parallel syncs.

40. **`EmscriptenMain.cpp` — periodic flush in `GameWebFrameCallback`.**
    Tick counter; once every 30 × 60 = 1800 frames (~30s at 60fps),
    `GX_FlushIdbfs("periodic")`. Cheap on the average frame (single int
    increment), and gives a recoverable point if the tab crashes before
    visibilitychange fires.

41. **`EmscriptenMain.cpp` — `GX_InstallIdbfsVisibilityHooks()`.** Installed
    once early in `main()`, registers two JS listeners:
    - `visibilitychange` → if state is `hidden`, ccall
      `GX_FlushIdbfs_Tab` (an exported C function so we don't allocate a
      new closure per event).
    - `beforeunload` → call `FS.syncfs(false, () => {})` directly. Best
      effort; some browsers cut async work here, but if the user has
      unsaved data this is the last chance.

42. **Exports plumbing** — added `_GX_FlushIdbfs_Tab` to
    `EXPORTED_FUNCTIONS` in both `GeneralsMD/Code/Main/CMakeLists.txt`
    and `link_mac.sh`. EMSCRIPTEN_KEEPALIVE is also on the C function as
    a belt-and-braces measure.

### Caveats / known gaps
- The 30s periodic flush is on a frame counter, not a wall-clock timer.
  If the page is throttled (background tab) the flush rate drops with the
  frame rate, which is fine — nothing's writing while we're throttled.
- This session does NOT address the `chdir`/`SetCurrentDirectory` path that
  some engine subsystems use to navigate into the save directory. Most
  saves go through `LocalFile::open` directly with absolute-ish paths and
  work fine, but anywhere the engine relies on relative paths after a
  `chdir` may need follow-up.
- The leaf name (`Command and Conquer Generals Zero Hour Data`) is
  intentionally NOT preserved on Emscripten — we just use `/userdata/`
  flat. If we ever want to share IDB state with another fork that uses the
  same leaf, that's a forward-compat concern.

### To verify
- Re-link.
- Open the page; the loading overlay should briefly show `Loading saved
  data` (during the IDBFS pre-run sync) before progressing to the boot
  stages.
- Save a game, reload the tab, return to the load-game menu; the save
  should be there.
- DevTools → Application → IndexedDB → `/userdata` should show the entries
  after a save.
- Console should log `[GeneralsX] IDBFS load complete` on first load and
  no errors on subsequent flushes.

## 2026-05-04 — Session 10: Resize + WebGL Context Loss (Polish item #5)

The canvas was a fixed 1024×768 backing store regardless of the actual
window size, so resizing the browser stretched the framebuffer (blurry on
HiDPI, letterboxed on wide windows). And a GPU reset / driver restart would
silently kill the page because the default `webglcontextlost` behavior is
"navigate to error". Both fixed.

### Canvas resize handling
43. **`web_shell.html`** — added a self-invoking IIFE that registers a
    `window.resize` listener and (where supported) a `ResizeObserver` on
    `#canvas`. Both fire a 100ms-debounced `apply()` that:
    - Reads `canvas.clientWidth/Height` (the CSS-pixel display size)
    - Multiplies by `window.devicePixelRatio` for HiDPI sharpness
    - Sets `canvas.width / canvas.height` to that pixel size (this resizes
      the GL drawing buffer; the browser will no longer upscale a smaller
      buffer with bilinear filtering)
    - `Module.ccall('GX_OnCanvasResize', null, ['number', 'number'], [w, h])`
      so the C++ side syncs SDL.
    Initial `apply()` runs synchronously so the very first frame already has
    the right backing-store size.

44. **`EmscriptenMain.cpp` — `GX_OnCanvasResize(int w, int h)`.** Exported
    via EMSCRIPTEN_KEEPALIVE + EXPORTED_FUNCTIONS. Calls
    `SDL_SetWindowSize(TheSDL3Window, w, h)` (which fires
    `SDL_WINDOWEVENT_RESIZED`, picked up by the existing handler in
    `EmscriptenInput.cpp` to update `g_canvasW/H` for pointer-lock clamping)
    and `emscripten_set_canvas_element_size("#canvas", w, h)` to keep the
    WebGL drawing-buffer attribute in sync even before `TheSDL3Window`
    exists (during early boot, the canvas size still matters for the
    initial GLES3_Init call). Logs the new dimensions.

### Engine layout caveat
The engine's GUI / HUD layout is still calculated against the resolution
chosen at `SDL_CreateWindow` time (currently 1024×768 from
`emscripten_get_canvas_element_size`). After resize, the framebuffer is
bigger but the engine's projection matrices and 2D layouts haven't been
re-driven. C&C Generals was a fixed-resolution RTS so there is no plumbed
in-engine resize pass; for now the user gets a crisp framebuffer at any
size with the original 1024×768-anchored UI. A real fluid-resize pass is
out of scope and listed as a follow-up.

### WebGL context-loss overlay
45. **`web_shell.html`** — added two listeners on `#canvas`:
    - `webglcontextlost` calls `e.preventDefault()` (preventing the browser
      from giving up on the page) and `window.GeneralsX.showError(message,
      'Graphics context lost')` to display the `#gx-error` overlay with a
      contextual message about GPU resets / driver restarts.
    - `webglcontextrestored` logs that the context came back, but doesn't
      try to resume — the engine isn't designed to re-upload every texture,
      VBO, shader, and render-state, so we treat loss as terminal.

46. **Reload button.** `#gx-error` now renders a "Reload page" button
    (CSS-styled to match the C&C aesthetic). A click handler calls
    `window.location.reload()`. Used by the context-loss path and any other
    fatal path that puts up the error overlay.

47. **`showError` extended to take an optional title.** Default still
    "Engine fault"; context-loss uses "Graphics context lost". The title is
    rendered into a new `#gx-error-title` element.

### Exports plumbing
48. Added `_GX_OnCanvasResize` to `EXPORTED_FUNCTIONS` in
    `GeneralsMD/Code/Main/CMakeLists.txt` and `link_mac.sh`. Belt and
    braces with the `EMSCRIPTEN_KEEPALIVE` attribute on the C function.

### To verify
- Re-link.
- Open the page in a window noticeably larger or smaller than 1024×768.
  The canvas should fill the viewport and render crisply (no bilinear
  upscaling).
- Drag the window edge to resize. The GL framebuffer should match the new
  size after a ~100ms beat (debounce). Console should log `INFO:
  GX_OnCanvasResize → WxH`.
- HiDPI test: on a Retina display the backing store should be 2× the CSS
  size — check `canvas.width` / `canvas.clientWidth` in DevTools.
- Force context loss: DevTools → "Rendering" panel → "WebGL: Lose context"
  (or the `WEBGL_lose_context` extension via console:
  `gl.getExtension('WEBGL_lose_context').loseContext()`). The `#gx-error`
  overlay should appear with title "Graphics context lost" and a "Reload
  page" button that works.

### Follow-up: CreateDirectory stub now actually mkdirs (Session 9 gap fix)
49. **`emscripten_compat/windows_base.h`** — the previous stub was
    `#define CreateDirectory(a, b) 0`, a pure no-op. The engine calls
    `CreateDirectory(getSaveDirectory().str(), nullptr)` before saving;
    without a real mkdir, `/userdata/Save` never existed and the subsequent
    `fopen` would fail.

    Replaced the macro with a real implementation on Emscripten:
    `GX_CreateDirectory_Web(path)` does `\` → `/` normalization, strips
    trailing `/`, and calls `mkdir(buf, 0755)`. Returns 1 on success and
    also on `EEXIST` (matches the Win32 ERROR_ALREADY_EXISTS path that
    callers expect). On non-Emscripten the macro keeps its old no-op
    behavior (and on real Win32 the actual `CreateDirectory` is shadowed
    by the macro only inside the emscripten_compat header chain).

    With this in place, the IDBFS persistence chain from Session 9 should
    now actually round-trip a save game: engine calls CreateDirectory
    (creates `/userdata/Save`), engine opens save file (creates
    `/userdata/Save/sav0001.sav`), periodic flush pushes to IndexedDB,
    reload re-populates from IndexedDB.

## 2026-05-05 — Session 11: Touch + Click-to-Start + Release Profile

Picked up the project after the May 4 polish push. The build is at parity
with the dist/ output (Apr 7); this session knocks out the remaining
cross-platform polish items so the next person can focus on audio.

### Touch input (Polish stretch goal)

50. **`EmscriptenInput.cpp` — SDL_FINGER* event handlers.** The whole input
    stack downstream of this file is mouse-shaped, so a parallel touch path
    isn't worth the complexity. Instead we synthesise mouse messages from
    finger events. Gestures supported:
    - **1 finger** — pass-through. SDL's built-in
      `SDL_HINT_TOUCH_MOUSE_EVENTS` synthesises `SDL_MOUSEBUTTON*`/
      `SDL_MOUSEMOTION` for the first touch, so the existing mouse pipeline
      handles tap-and-drag without any new code. We just track the active
      finger count.
    - **2 fingers** — explicit RMB-drag camera pan at the centroid +
      pinch-to-wheel zoom. On the second `SDL_FINGERDOWN` we synthesise a
      `WM_LBUTTONUP` (cancelling the in-flight LMB drag from finger 1),
      then a `WM_RBUTTONDOWN` at the centroid. `SDL_FINGERMOTION` ticks
      `WM_MOUSEMOVE` at the new centroid; pinch distance change > 30 px
      emits `WM_MOUSEWHEEL` notches. Lift to 1 finger emits the matching
      `WM_RBUTTONUP`.
    - **Synthetic-mouse suppression** — while in 2-finger mode we set
      `g_suppressSyntheticMouse` so any further `SDL_MOUSEBUTTON*`/
      `SDL_MOUSEMOTION` carrying `which == SDL_TOUCH_MOUSEID` is dropped.
      Otherwise the engine would see both our explicit RMB drag and a
      stray LMB up/down from SDL's compat path.

51. **`EmscriptenInput.cpp` — centroid math against canvas size.** SDL
    finger coords are normalized 0..1; we multiply by `g_canvasW/H` (already
    tracked for pointer-lock clamping) and feed pixel coords through the
    existing `EmscriptenInput_RouteMouseEvent` route. New helper
    `EmscriptenInput_UpdateCanvasSize(int, int)` exposed in
    `EmscriptenInput.h` so `GX_OnCanvasResize` in `EmscriptenMain.cpp` can
    refresh those dimensions on early-boot resizes (before SDL_CreateWindow
    fires `SDL_WINDOWEVENT_RESIZED`).

52. **`web_shell.html` — `touch-action: none` on `canvas#canvas`.** Without
    this, mobile Safari steals two-finger gestures for page pinch-zoom and
    the canvas never sees them. Also added `-webkit-tap-highlight-color:
    transparent` to suppress the iOS tap rectangle, and `user-select: none`
    so long-press doesn't pop a selection magnifier over the game.

### Click-to-start gesture gate

53. **`web_shell.html` — gesture-gating before engine handoff.** Browsers
    block `AudioContext.resume()` and pointer-lock requests before the user
    has interacted with the page. Most users click or press a key while the
    WASM downloads, in which case the existing fade-out path is fine. For
    the corner case where the engine reaches `ready` without any prior
    gesture (e.g., user opens the tab in the background and switches to it
    after everything's loaded), we now reveal a "Begin" button instead of
    silently fading out. Clicking it (or anywhere) hides the overlay and
    calls `Module.SDL2.audioContext.resume()` — best-effort, currently
    no-op since milesstub doesn't create an AudioContext, but lays the
    groundwork for when WWAudio gets wired up.

54. **State**: any of `pointerdown`/`keydown`/`touchstart` listeners (added
    with `{ passive: true, capture: true }` so they don't intercept input
    the engine cares about) flips `gxUserGestureSeen`. The `setProgress('ready')`
    branch checks that flag; if false it sets `gxReadyPending` and shows the
    button. The next gesture (which may be the button click itself) calls
    `gxFinishLoading()`.

### Release-build profile

55. **`link_mac.sh` — `bash link_mac.sh release` switches to optimised
    distribution flags.** Default invocation is unchanged (debug build with
    `-g3 -sSAFE_HEAP=1 -sASSERTIONS=2 -sDEMANGLE_SUPPORT=1`). Release path:
    - `-O3 -DNDEBUG` for both compile and link
    - drops `-g3`, `-sSAFE_HEAP=1`, `-sASSERTIONS=2`, `-sDEMANGLE_SUPPORT=1`
    - adds `--closure 1` for JS minification
    - writes to `GeneralsZH-rel.{html,js,wasm}` so debug + release coexist
    - per-mode log file (`link_mac.${MODE}.log`) so a debug log doesn't get
      stomped by a release run

    Asyncify stays ON in both modes — it's a structural dep (the engine's
    `emscripten_sleep(0)` calls inside `init()` rely on it), not a
    diagnostic option that can be turned off without breaking the boot path.

### Audio path — documented but not implemented

The remaining big rock is real audio. `port-todo.md` "Audio" section now
lays out the two reasonable approaches (SDL_mixer bridge or OpenAL bridge)
plus the WWAudio link wiring needed to enable the delayed-release drain
helper from Session 8. Out-of-scope for this session because either
approach involves replacing the ~100-function `milesstub` with a real
backend that decodes WAV/MP3 from `AudioZH/MusicZH/SpeechZH.big` and
plays it through Web Audio — a multi-session effort.

### To verify

- Re-link with `bash link_mac.sh` (debug) or `bash link_mac.sh release`
  (distribution).
- Open the page on a desktop browser; the existing flow should be
  unchanged (mouse + keyboard work as before).
- On a phone or tablet, single-finger tap should select; one-finger drag
  should drag-select; two-finger drag should pan the camera; pinch
  should zoom. The page itself shouldn't pinch-zoom or scroll.
- Open the page in a backgrounded tab; switch to it after a few seconds.
  The "Begin" button should appear instead of the overlay auto-dismissing.
- Console on first gesture should log no errors (the
  `Module.SDL2.audioContext.resume()` call is wrapped in a try/catch).

### Latent bug caught & fixed during verification

56. **`EmscriptenMain.cpp::GX_FlushIdbfs` — broken `EM_ASM` macro
    expansion** (added in Session 9, never built since). The JS body
    contained an inline object literal `{ busy: false, pending: false }`.
    The C preprocessor splits macro arguments at top-level commas without
    respecting `{}` nesting, so the macro arg would split mid-literal and
    the JS body would parse as C++ — failing with "use of undeclared
    identifier 'pending'" / "unknown type 'st'" / etc.

    Why it never tripped before: every previous successful build (Mar 31
    + Apr 7 dist) was made *before* Session 9 added this code. Sessions
    5–10 were all source-only edits with no rebuild.

    **Fix**: wrap the body in extra parentheses — the documented
    workaround in `<emscripten/em_asm.h>`:
    ```cpp
    EM_ASM(({
      ... { a: 1, b: 2 }; ...
    }), reason);
    ```
    The `({...})` is a single grouped expression that the preprocessor
    treats atomically. Audited the other 5 EM_ASM blocks in the file —
    all use commas inside `()` only (function-call arglists), which the
    preprocessor *does* respect, so they didn't need wrapping.

57. **`link_mac.sh` — missing `EmscriptenInput.cpp.o` from MAIN_OBJS**.
    Compared the existing `MAIN_OBJS=( EmscriptenMain.cpp.o,
    LinuxStubs.cpp.o )` against `build-web/build.ninja`'s actual link line
    for `GeneralsZH.html`, which lists ALL THREE of `EmscriptenMain`,
    `EmscriptenInput`, and `LinuxStubs`. Without `EmscriptenInput.cpp.o`
    in the link line, the link should have failed with unresolved symbols
    (`EmscriptenInput_PumpEvents`, `c_dfDIKeyboard`,
    `g_emscriptenKeyBuffer`, …) — and likely *did* fail every time
    `link_mac.sh` was run since Session 3 first introduced
    `EmscriptenInput.cpp`.

    Added `EmscriptenInput.cpp.o` to MAIN_OBJS with a comment explaining
    that it's not bundled into any `.a` and so must be passed directly.

### Build verification (May 5 2026)

Compiled the two .o files we modify in the Mac workflow with a new
`compile_targeted.sh` helper that lifts the exact em++ flags out of
`build-web/build.ninja`'s rules so we don't trigger ninja's full-tree
re-evaluation (which would also re-run CMake and pull the entire
WWAudio/WW3D2 compile graph). The script:

1. Compiles `EmscriptenInput.cpp.o` and `EmscriptenMain.cpp.o` with the
   same `-g3 -O0 -std=c++20` + PCH config ninja uses.
2. Supports a `--syntax-only` flag for fail-fast iteration (a
   2-line-edit / 5-second-check loop while debugging the EM_ASM
   macro fix above).

Then ran `bash link_mac.sh` (debug profile). It linked cleanly and
produced:

```
GeneralsMD/GeneralsZH.html      30,820 bytes
GeneralsMD/GeneralsZH.js       558,715 bytes
GeneralsMD/GeneralsZH.wasm  91,330,064 bytes  (~87 MB; debug, -g3 -SAFE_HEAP -ASSERTIONS=2)
```

The HTML grew from ~22KB → 30KB (richer shell with touch + start button +
IDBFS preRun). The wasm grew from 44MB → 87MB because:
1. Sessions 6–11 added meaningful new code (IDBFS, deferred audio, resize
   hooks, touch handling, gesture gate).
2. `EmscriptenInput.cpp.o` is now actually in the link line — `link_mac.sh`
   was missing it from MAIN_OBJS (see #57), so previous link_mac.sh runs
   would either have failed with unresolved symbols, or relied on a stale
   archive somewhere. Either way, this is the first link_mac.sh build we
   know to be self-consistent with the source tree.
3. Debug-only overhead: the build is `-g3 -sSAFE_HEAP=1 -sASSERTIONS=2`,
   which roughly doubles wasm size compared to the release profile.

### Release-profile link (May 5 2026)

58. **Closure compiler regression — legacy allocator export needed.** The
    first `bash link_mac.sh release` run failed at the closure pass with:
    ```
    JSC_UNDEFINED_VARIABLE: variable allocate is undeclared
    JSC_UNDEFINED_VARIABLE: variable ALLOC_NORMAL is undeclared
    ```
    Origin: SDL2's `SDL_assert.c` emits an `EM_ASM` block calling
    `allocate(intArrayFromString(reply), "i8", ALLOC_NORMAL)`. Those names
    are the legacy Emscripten allocator API; without exporting them in
    `EXPORTED_RUNTIME_METHODS` they aren't declared in the JS output that
    closure sees, and ADVANCED_OPTIMIZATIONS errors out.
    
    **Fix**: appended `,allocate,ALLOC_NORMAL,intArrayFromString` to the
    release-mode `EXPORTED_RUNTIME_METHODS`. The debug profile keeps the
    minimal export list (closure isn't run, so no need). Also restructured
    the script so `EXPORTED_RUNTIME_METHODS` is per-mode (was in
    `COMMON_LINK_FLAGS`, where appending another `-sX=…` would have
    overridden rather than merged).

After the fix, the release link succeeded:
```
GeneralsMD/GeneralsZH-rel.html      11,147 bytes  (vs 30,820 debug)
GeneralsMD/GeneralsZH-rel.js       128,568 bytes  (vs 558,715 debug — closure cut 4×)
GeneralsMD/GeneralsZH-rel.wasm  17,139,627 bytes  (vs 91,330,064 debug — 5.3× smaller)
```

Closure brought the JS down from 559KB → 128KB (~77% reduction); dropping
`-g3 -sSAFE_HEAP=1 -sASSERTIONS=2` brought the wasm down from 91MB → 17MB
(~81% reduction). 17 MB wasm is realistic for a streaming web RTS — the
Apr 7 dist was 44 MB at `-g`.

### Audio implementation plan documented

The full audio backend is genuinely a 4-phase, multi-session effort because
of the surface area (~70 AIL_* functions called from `MilesAudioManager`,
3.4k LoC) and the asynchronous-vs-synchronous mismatch between Web Audio's
`decodeAudioData` and `AIL_start_sample`. Rather than rush it in, this
session leaves a detailed phased plan in `port-todo.md` "Wire up real
audio" with:

- The exact init-gate location (`openDevice` line 1429: `AIL_quick_startup`
  returns 0, engine calls `setOn(false, All)`, all audio short-circuits).
- Per-sample flow (`playSample` line 2787) and the 6-function subset that
  unblocks 2D one-shot sounds.
- The streaming + 3D paths and what they each cost.
- The click-to-start gate from Session 11 already harvests a gesture for
  `audioContext.resume()`, so the platform-side autoplay block is already
  handled.

Phase 1 of the plan — replacing `libmilesstub` with a `libwebaudiobridge`
that returns enough non-null handles to keep `m_audioOn = true` — is the
next session's clear starting point. It's a contained scope (rewrite ~10
of the ~100 AIL_* functions, leave the rest as no-ops) that lets the
engine's audio code paths execute without short-circuit, surfacing any
second-order bugs before we add actual sound.

## 2026-05-05 — Session 12: First end-to-end browser test

Wired up Claude Preview against the new `link_mac.sh` build, served
`build-web/GeneralsMD/` over HTTP with a Range-aware `serve.py`, and
drove the page through to the engine's main loop. Found and fixed two
boot-blocking bugs and one cosmetic issue along the way; the deeper
"no-draws" bug needs targeted follow-up.

### Asset symlink + HTTP server (infrastructure)

59. **`build-web/GeneralsMD/assets` was symlinked to `Downloads/GeneralsZH-web/`,
    which only contains four small archives** (PatchINI.big, ShadersZH.big,
    shaders.big, plus an empty `deferred/`). The full asset set actually
    lives in `Downloads/GeneralsZH-web-fresh/assets/` (16 archives total,
    the 15 startup archives + maps.big + gensecZH.big). Re-pointed the
    symlink so BigVFS finds the real files. Audio archives
    (`AudioZH/MusicZH/SpeechZH.big`) are still missing on disk, but the
    deferred mount tolerates 404s.

60. **Custom `build-web/GeneralsMD/serve.py`** with proper HTTP Range
    support — Python 3.14's stock `SimpleHTTPRequestHandler` returns 200
    with full content for Range requests, which breaks BigVFS's
    header-only fetch pattern. The new handler parses `Range: bytes=N-M`
    and responds with 206 + Content-Range. Also serves `.wasm` with the
    correct `application/wasm` MIME and disables caching while iterating.

### Boot-blocking bugs caught in the browser

61. **Engine quit-suicide via `isMusicAlreadyLoaded()`** — first time we
    actually ran the build past engine init in a browser, console showed
    `INFO: GameEngine requested quit — stopping main loop` followed by
    `Aborted(native code called abort())`. Root cause: `GameEngine.cpp`
    line 624 calls `if (!TheAudio->isMusicAlreadyLoaded()) setQuitting(TRUE);`
    The `isMusicAlreadyLoaded` helper iterates `m_allAudioEventInfo` for
    an `AT_Music` entry and then checks `TheFileSystem->doesFileExist()`
    on the resulting path. With milesstub backing the audio path and
    `MusicZH.big` 404'ing on disk, that check fails and the engine kills
    itself before the first frame paints.

    Fix: `EmscriptenMain.cpp` now calls `TheGameEngine->setQuitting(FALSE)`
    immediately after `TheGameEngine->init()` to clear the flag the
    engine just set on itself. Comment block explains the rationale and
    notes the override becomes a no-op when real audio lands.

62. **Error-overlay bleed-through** — when the abort overlay first showed,
    the dimmed loading screen + Begin button were visible *behind* it
    (semi-transparent error background = `rgba(10,0,0,0.92)`). Fix in
    `web_shell.html`: `showError` now hides the loading overlay outright
    (`overlay.style.display = 'none'`) so we never get layered UI.

### What's working: engine boots, main menu pushed

After 61–62 the boot sequence cleanly reaches `INFO: GameEngine::init()
complete` → main loop running → `Begin` button appears → click dismisses
overlay → frames advance with the engine alive (`Shell:push(Menus/MainMenu.wnd)`,
deferred audio mounts running, `INFO: Mounted 16507 files to Emscripten
FS under /assets/`). No abort.

### What's broken: black canvas — root cause not yet pinpointed

After the click-to-start hand-off the canvas stays solid black even
though the engine is running. Instrumented WebGL at the JS layer with
hooks on `glClear / glDrawArrays / glDrawElements / glUseProgram /
glViewport`:

```
Over a 3-second sample (~180 frames):
  clear:        558  (~3 per frame — Begin_Render + others)
  viewport:    2262  (~12 per frame)
  drawArrays:     0  ← never
  drawElements:   0  ← never
  useProgram:     0  ← never
```

The engine is running its render-pass scaffolding (clears + viewport
sets) every frame but **never reaches a single draw call**. With
`useProgram: 0` it's also never binding a shader. This rules out:

- The intro/sizzle Bink-stub trap — fixed by force-clearing
  `m_playIntro / m_afterIntro / m_breakTheMovie` to FALSE every frame
  in `GameWebFrameCallback`. Engine still produces zero draws.
- `m_breakTheMovie == TRUE` gate at `W3DDisplay::draw` line 1851 —
  forced FALSE every frame, no change.
- Resolution mismatch (engine renders at 800×600, canvas was
  464×688). Force-resized the canvas to 800×600 too — still black.
- Shader compile failure — `gles3_wrapper.cpp::Compile_Shader` /
  `Link_Program` log compile failures, none seen.
- Engine quit / not initialized — `g_GameEngineInitialized = TRUE`,
  `TheGameEngine != nullptr`, frames are advancing.

The engine's draw path is:

```
GameClient::update → TheDisplay->DRAW() → W3DDisplay::draw
  → if (!m_breakTheMovie && !m_disableRender && Begin_Render() == OK)
  → drawViews() / TheInGameUI->DRAW() / TheMouse->DRAW() / End_Render()
```

`Begin_Render` ⇒ `Set_Viewport` + `Clear` (matches the 12 viewport / 3
clear per-frame counts we see). But none of the inner `*->DRAW()` calls
ever reach `glDrawElements`. The d3d8 stub's `DrawIndexedPrimitive`
short-circuits with `if (!current_vb || current_vb->data.empty() ||
!current_ib || current_ib->data.empty()) return;` so a possible cause
is that `SetStreamSource` / `SetIndices` aren't reaching the stub for
the Render2D paths the menu uses, OR `Render2DClass::Render` early-outs
on `Indices.Count() == 0`.

**Next-session triage targets**:

a) Add a counter inside `Emscripten_IDirect3DDevice8::DrawIndexedPrimitive`
   that logs how often it gets called and how often it short-circuits
   on empty VB/IB. Counters per frame in the JS console will tell us
   if the engine even reaches the stub or not.

b) Add a counter inside `Render2DClass::Render` (after the
   `Indices.Count()` early-out) so we can tell if the menu has any
   geometry to draw at all. Zero indices means the WindowManager isn't
   pushing button quads — that points at the texture-load failures
   ("Targa: Failed to open file 'trstrtholecvr.tga'") preventing menu
   geometry from being assembled.

c) Inspect `TheDisplay->draw` entry — it might be returning early
   inside its own logic (e.g., `m_loadScreenRender == TRUE` guard at
   line 1854 takes the short InGameUI-only path). The fact that
   `m_loadScreenRender` defaults FALSE makes this less likely but
   worth verifying.

### Other findings worth remembering

63. **BigVFS is making massive duplicate range requests**. Network log
    showed `INIZH.big` fetched ~120 times in a single boot, plus repeated
    fetches of `Textures.big`, `EnglishZH.big`, `WindowZH.big`. This is
    effectively re-downloading the same archive headers per file lookup
    instead of caching them. At the current scale this adds ~10–30
    seconds of network latency to every boot. Filed as a perf followup;
    not blocking, but worth a single hash-map of fetched ranges to
    short-circuit re-fetches.

### Build artefacts after this session

```
build-web/GeneralsMD/GeneralsZH.{html,js,wasm}        — debug build, 31KB / 559KB / 91MB
build-web/GeneralsMD/GeneralsZH-rel.{html,js,wasm}    — release build, 11KB / 128KB / 17MB
                                                        (release does NOT yet have the
                                                        Session 12 boot fixes — re-link
                                                        is needed)
```

### To verify

- `python3 build-web/GeneralsMD/serve.py 8087` and load
  `http://127.0.0.1:8087/GeneralsZH.html`. Engine should boot to the
  "BEGIN" gate within ~15s, click → black canvas + alive frame loop
  (the known remaining issue from this session).
- Console should NOT show `Aborted(native code called abort())`.
- Console should show `INFO: GameEngine::init() complete` →
  `INFO: clearing post-init quit flag (no-music-loaded path)` →
  `Shell:push(Menus/MainMenu.wnd)`.

### Black-canvas root causes — Session 12 follow-up

After more instrumentation we found two related causes that explained
why earlier draw counters were all zero. Both are fixed in the source
tree; the underlying "no draws to the menu" bug is still open and now
we have a much better starting point for the next session.

64. **WebVisibility::Init paused the loop in iframes.** Calling
    `emscripten_set_visibilitychange_callback` made the engine pause
    whenever `document.hidden` flipped true. In Claude Preview's iframe
    (and likely other embed contexts) `document.hidden` is *always* true
    even while the iframe is visibly rendering. Result: the main-loop
    callback's first guard `if (g_Loop.paused) return;` short-circuits
    every frame; `frame_callback` (and therefore the engine's update +
    draw) never runs.

    **Fix**: commented out the `WebVisibility::Init();` call in
    `EmscriptenMain.cpp::main()`. Browsers already throttle background
    tabs at the platform level (rAF stops, setTimeout throttles to 1Hz);
    the explicit pause was a defence-in-depth optimisation that turns
    out to brick the engine in iframe embeds. A real save-state pause
    should be triggered by user-visible UI, not by an event the iframe
    can't authoritatively observe.

65. **`emscripten_set_main_loop` with fps=0 uses requestAnimationFrame**,
    which Chrome aggressively throttles to 0Hz when `document.hidden`
    is true. Even with #64 fixed, the loop never tick'd in the iframe.

    **Fix**: switched `WebMainLoop::Start(GameWebFrameCallback, 0, true)`
    to `WebMainLoop::Start(GameWebFrameCallback, 60, true)`. Non-zero fps
    makes Emscripten pick `setTimeout` instead of rAF, which is throttled
    to ~1Hz in iframes (still slow) but at least keeps the loop ticking.
    On a top-level browser tab the runtime gives `setTimeout` proper
    pacing; the only loss is vsync alignment.

### What we actually see now

After both #64 and #65, the engine runs every frame (slowly in iframe,
60Hz on a top-level tab). WebGL instrumentation captures real activity:

```
Over a 5-second sample (iframe, ~9 frames):
  clear:        9
  viewport:    858  (~95 per frame; engine sets viewport per UI element)
  bindTexture: 143  (~16 per frame; textures ARE being bound)
  texImage2D:    3  (textures uploaded)
  bufferData:    0  ← still 0
  bindBuffer:    0  ← still 0
  useProgram:    0  ← still 0
  drawArrays:    0  ← still 0
  drawElements:  0  ← still 0
```

So the engine is doing per-frame work — clearing, setting viewports,
binding textures, even uploading texture data — but **never** calls
`glDrawArrays` / `glDrawElements` / `glUseProgram` / `glBufferData`.
The render path is making it deep into texture state setup but
short-circuiting before any geometry is sent.

This rules out the throttling theory: a frame ticking at 1Hz still has
all the time it needs to complete a render frame. The black canvas is
a real "no geometry submitted" bug, not a throttling artifact.

### Next-session triage targets (refined)

The pattern (textures bound, buffers untouched, no useProgram) strongly
suggests the d3d8 stub's `Emscripten_IDirect3DDevice8::DrawIndexedPrimitive`
short-circuit:
```cpp
if (!current_vb || current_vb->data.empty()) return 0;
if (!current_ib || current_ib->data.empty()) return 0;
```

Either:

a) `SetStreamSource` is being called with a vertex buffer whose `data`
   vector is empty (length=0 at construction, or the engine never
   wrote into it via Lock/Unlock).

b) `SetStreamSource` is never called at all and `current_vb` stays null.

c) `Render2DClass::Render`'s early-out `if (!Indices.Count() || IsHidden)
   return;` is hit because the menu's window list assembled zero
   geometry — likely because the menu's textures (e.g.,
   `trstrtholecvr.tga` and presumably others) failed to load from the
   .big archives and the windows therefore have nothing to draw.

The `Targa: Failed to open file "trstrtholecvr.tga"` log is the smoking
gun for option (c). Add an instrumentation pass on the texture loader
(or grep INI files for `trstrtholecvr`) to see *which* asset is
expected at that path and whether the menu .wnd files reference it.
`Render2DClass::Render` taking the `Indices.Count() == 0` early-out
also explains why we see zero `bufferData` / `useProgram` calls
despite the engine reaching the menu push.

The textures we DO see (`bindTexture: 143 / texImage2D: 3`) are
probably the engine's MissingTexture fallback being bound to many
windows whose intended texture failed to load — but with no geometry
indices to actually draw them onto, no rasterisation happens.

### Follow-up (perf): BigVFS duplicate range fetches

Network log showed `INIZH.big` fetched ~120 times per boot — once per
file lookup instead of once per archive. A single hash-map of fetched
byte-ranges keyed by archive name would short-circuit re-fetches and
shave ~10-30s off boot. Filed for a follow-up session.

### Build state at end of Session 12

```
build-web/GeneralsMD/GeneralsZH.{html,js,wasm}     — 31KB / 559KB / 91MB
                                                     (debug, includes Sessions 5-12)
```

Source-tree fixes in this session that need re-verification on a real
top-level browser tab (where `document.hidden = false`):
- `EmscriptenMain.cpp` setQuitting(FALSE) post-init (#61)
- `web_shell.html` showError hides loading overlay (#62)
- `EmscriptenMain.cpp` WebVisibility::Init disabled (#64)
- `EmscriptenMain.cpp` main loop fps 0 → 60 (#65)
- `EmscriptenMain.cpp` per-frame `m_breakTheMovie = FALSE` + `m_loadScreenRender = FALSE` (belt-and-braces)

### Session 12 second render fix — `BaseVertexIndex` offset

After the SetStreamSource fix landed pixels on screen, the user reported
"images cut up incorrectly, not in the right menu, no 3D yet". Symptoms:
the C&C Generals logo + various menu textures rendered at full-quad size
with what looked like wrong UV / position data, layered on top of each
other; the 3D scene was missing.

**Root cause**: WebGL2 / OpenGL ES 3.0 has no `glDrawElementsBaseVertex`,
but the W3D engine's dynamic vertex buffer allocator routinely allocates
N vertices INSIDE a much larger shared dynamic VBO and returns a
`VertexBufferOffset` to mark where its allocation starts. The engine
then writes indices `[0, 1, 2, 3]` (relative to ITS allocation) and
calls `SetIndices(BaseVertexIndex = VertexBufferOffset)`. In real D3D8,
BaseVertexIndex is added at fetch time. In our stub it was stored in
`base_vtx_index` but never actually applied — so `glDrawElements` read
indices 0..3 from the FRONT of the VBO, fetching whichever vertices the
first draw of the frame happened to write.

That's why we saw the same logo / atlas page replayed across the canvas
every frame, with wrong UVs (the first allocation's UVs) and wrong
positions (the first allocation's positions). The 3D mesh draws went
through the same path and mis-fetched too — so no scene visible.

**Fix**: shift the per-attribute pointer base forward by
`base_vtx_index * stride` bytes inside `_setup_vertex_attribs`. With the
base pointer pre-shifted, index 0 reads OUR allocation's first vertex,
index 1 reads our second, etc. — emulating BaseVertexIndex correctly.

```cpp
// emscripten_compat/d3d8.h
void _setup_vertex_attribs(UINT stride_override = 0) {
    ...
    UINT stride = stride_override ? stride_override : current_stride;
    if (stride == 0) stride = dec.Get_FVF_Size();
    const UINT base_offset_bytes = (UINT)base_vtx_index * stride;
    dec.Apply_Vertex_Attribs(stride, base_offset_bytes);
}
```

```cpp
// gles3_fvf.cpp — Apply_Vertex_Attribs gains a base_offset_bytes param,
// added to every per-attribute offset:
glVertexAttribPointer(ATTR_POSITION, ..., stride,
    (const void*)(uintptr_t)(base_offset_bytes + location_offset));
// ... same for normal, diffuse, texcoord0..3 ...
```

After both this fix and the SetStreamSource fix:
- The C&C Generals Zero Hour logo renders at the right position with
  correct UVs (full logo, no atlas-tiling).
- The Options/setup dialog (visible at boot) shows proper widgets:
  dropdowns, sliders, checkboxes — each cropped to its correct UV
  sub-rectangle.
- The 3D scene background renders behind the dialog (terrain, structures,
  visible vehicle on the right edge).

### Session 12 first render fix — black-canvas FIXED (`SetStreamSource` clobber)

**Root cause** found via in-engine diagnostic counters wired through
`dx8wrapper.cpp` and `emscripten_compat/d3d8.h`. The W3D engine's
`Apply_Render_State_Changes` walks `MAX_VERTEX_STREAMS` once per draw and
calls `SetStreamSource(i, vb, ...)` for every stream — stream 0 with the
real VB, streams 1..N-1 with `nullptr` (since only stream 0 is used). The
Emscripten d3d8 stub at
`GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/emscripten_compat/d3d8.h`
only models a SINGLE bound stream (`current_vb`), so the null calls for
streams 1+ kept clobbering the just-set stream 0:

```
SetStreamSource:7952 (null=3976)  ← exactly 50% null
dip:called=3976 no_vb=3976 drew=0  ← every draw short-circuited
```

**Fix**: filter `Emscripten_IDirect3DDevice8::SetStreamSource` to stream 0
only; `return 0` for any other stream number. The W3D path doesn't actually
use multi-stream geometry, so dropping streams 1..N is correct.

```cpp
virtual HRESULT SetStreamSource(UINT StreamNumber, ...) override {
    if (StreamNumber != 0) return 0;   // ← the one-line fix
    current_vb = static_cast<...>(pStreamData);
    ...
}
```

After the fix:

```
dip:called=3976 no_vb=0 empty_vb=0 drew=3976  ← every draw lands on GL
```

**Result**: the C&C Generals logo, "GENERALS / ZERO HOUR" title text, and
the menu's game-asset thumbnails all render to the canvas. The menu
button column is on the right side of the engine's 800×600 viewport and
gets CSS-clipped at the iframe's 464×688 backing buffer, but the
fundamental draw pipeline is now alive end-to-end.

### Session 12 follow-up — narrowed black-canvas down to d3d8 stub draw path

After more rounds of in-browser instrumentation, the black-canvas root
cause is now isolated to the d3d8 stub's draw short-circuit. Five
diagnostics were added in `EmscriptenMain.cpp` to probe the asset and
file-IO chain end-to-end:

```
VFS-PROBE: 'Window\Menus\MainMenu.wnd' -> ok=1 size=208561
VFS-PROBE: first 16 bytes: 46 49 4c 45 5f 56 45 52 53 49 4f 4e 20 3d 20 32
VFS-PROBE: as text: 'FILE_VERSION = 2;..STARTLAYOUTBLOCK..  LAYOUTINIT = W3DMainMenuI'
VFS-PROBE: fmemopen menu pos1=0 seekret=0 sz=208561 nread=208561 first10='FILE_VERSI'
```

→ `Web_VFS_Read_File_Sync` correctly returns the 208KB menu file.
→ `fmemopen` over the BigVFS-fetched buffer correctly handles
   `fseek(0,END)+ftell` and `fread(208561)`.
→ Therefore `LocalFile::open`'s BigVFS hook is producing a valid `m_file`
   that can read the entire menu — `WindowLayout::load` and
   `winCreateFromScript` should be parsing the .wnd correctly.

WebGL state-change instrumentation:

```
clear:          ~1 / frame
viewport:       ~12 / frame
bindTexture:    ~12 / frame   ← textures ARE bound
texImage2D:     occasionally  ← textures uploaded
bufferData:     0             ← vertex/index data NEVER uploaded
bindBuffer:     0
useProgram:     0
drawElements:   0
drawArrays:     0
enableVertexAttribArray: 0
```

The engine reaches `DX8Wrapper::Set_Texture` (which bindTextures) but never
reaches `DX8Wrapper::Draw → D3DDevice::DrawIndexedPrimitive →
GLES3_Draw_Triangles → glUseProgram + glDrawElements`. The most likely
short-circuit:

```cpp
// emscripten_compat/d3d8.h Emscripten_IDirect3DDevice8::DrawIndexedPrimitive
if (!current_vb || current_vb->data.empty()) return 0;
if (!current_ib || current_ib->data.empty()) return 0;
```

`current_vb`/`current_ib` are stored by `SetStreamSource`/`SetIndices` in
`Apply_Render_State_Changes`. The engine's `DX8VertexBufferClass` holds an
`Emscripten_IDirect3DVertexBuffer8*` whose `data` (a `std::vector<uint8_t>`)
must be non-empty. If `data.empty()`, the draw is silently skipped.

#### Concrete next-session targets (with file paths)

a) **Add a counter inside the d3d8 stub's DrawIndexedPrimitive**:
   `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/emscripten_compat/d3d8.h`
   line ~929. Make it bump three counters: `total_calls`, `skipped_no_vb`,
   `skipped_no_ib`. Expose via an `extern int` set + an `extern "C"`
   getter. Print from `EmscriptenMain.cpp::GameWebFrameCallback` once a
   second. This will tell us:
   - whether `DrawIndexedPrimitive` is ever called for the menu
   - if so, which short-circuit is taken

b) **Trace `DX8VertexBufferClass::Get_DX8_Vertex_Buffer`** in
   `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/dx8vertexbuffer.cpp`.
   Confirm it returns a non-null `Emscripten_IDirect3DVertexBuffer8*` with
   non-empty `data` when the engine creates a dynamic VB for menu UI quads.

c) **Trace `Emscripten_IDirect3DVertexBuffer8::Lock`/`Unlock`** in
   `emscripten_compat/d3d8.h` line ~636. The engine writes vertex data
   between Lock and Unlock. If `data` stays empty after Unlock, the
   resize-to-length in the constructor isn't happening, OR the engine's
   Lock returned a null pointer that didn't actually write data.

#### Probe code already in EmscriptenMain.cpp

Lines ~575–650 of `EmscriptenMain.cpp` now run a self-test on boot:
- 4 path probes against `Web_VFS_Read_File_Sync`
- 1 fopen of the MEMFS placeholder (confirms it's 0 bytes)
- 1 fmemopen sanity test on a small static buffer
- 1 end-to-end test that mimics `RAMFile::open(LocalFile)` on the menu

Keep these in place during black-canvas triage; remove once the menu
renders.


## 2026-05-20 → 05-24 — Session 13: Audio bridge, virtual network, skirmish hardening
*(Reconstructed summary — this stretch was originally logged only in commit messages and compile scripts.)*

- **Web Audio bridge**: `Core/Libraries/Source/WebAudioBridge/web_audio_bridge.cpp` (~1,400 lines)
  reimplements the Miles `AIL_*` surface on the Web Audio API — `AudioContext` graph with
  per-sample gain/panner nodes (HRTF for 3D), `decodeAudioData`, an IMA ADPCM decoder written
  from scratch, and detection of MP3 payloads inside nominal WAV headers. Compiled into the
  `milesstub` target via `cmake/miles.cmake`.
- **Virtual UDP transport**: `udp.cpp` gained `GX_Network_EnableVirtual()` — outbound packets
  hand off to `window.GeneralsX.onSendPacket`, inbound arrive via exported
  `GX_Network_PushPacket()`. A ready seam for WebRTC multiplayer; no JS side yet.
- **Skirmish fixes**: function-signature mismatch in Skirmish mode fixed; ODR memory-layout
  definitions aligned globally; mouse drift, viewport clipping, and terrain z-fighting decals fixed.
- **Build profile**: ASYNCIFY disabled (`-sASYNCIFY=0`) in favor of synchronous XHR in BigVFS;
  release profile gained `emmalloc` + Closure tuning; `MapsZH.big` slimmed for web serving.

## 2026-07-06 — Session 14: Resurrection — reproducible CMake build on Emscripten 6
The project moved machines with no build artifacts. Recovered the asset set and the May-24
binaries from backups, then made the from-source build reproducible for the first time since Mar 31:

- `cmake/web.cmake`: include-guard added — `CMAKE_PROJECT_INCLUDE` re-includes the file at every
  subproject `project()` call (gamespy/Generals/GeneralsMD), which duplicated targets. The
  `-DCMAKE_PROJECT_INCLUDE` path must also be **absolute**, and `-G Ninja` must be explicit.
- Emscripten 6.0.2 drift: `-sDEMANGLE_SUPPORT` removed (flag deleted upstream);
  `-sGROWABLE_ARRAYBUFFERS=0` required — Emscripten 6 defaults memory growth to JS resizable
  ArrayBuffers, and Chrome's WebGL rejects resizable-backed views in `texImage2D`.
- The fetched miles stub headers drifted (unpinned FetchContent): `mss.h` gained a `WAVEFORMAT`
  definition colliding with `emscripten_compat/windows_base.h`; guarded. Pin the dependency.
- Link parity with the last hand-built binary: full `EXPORTED_FUNCTIONS` list, `-lidbfs.js`,
  1 GB/2 GB heap, 4 MB stack moved from `link_mac.sh` into `Main/CMakeLists.txt`;
  `WWAudio/Threads.cpp` compiled directly into the web target (WWAudio has no CMake target yet).
- **Verified**: clean `emcmake cmake -G Ninja` + `ninja z_generals` build boots to the animated
  main menu in a headless browser. CI now runs this exact build on every push.

## 2026-11-09 — Session 15: Serverless WebRTC multiplayer on the virtual LAN
- Set out to carry the virtual-UDP seam between machines, not just tabs, with no server.
- Found the seam had one packet queue shared by every UDP socket, so the lobby socket (8086)
  and the game socket (8088) could read each other's packets. `udp.cpp` now tags packets with
  their destination port and each socket reads only its own; the queue is capped at 1024.
- The JS side hard-coded `srcPort: 8087` and compared the broadcast address against
  `0xFFFFFFFF` while `EM_ASM` hands it over as signed `-1`. The sender's real port now flows
  through `onSendPacket`, and addresses are coerced to unsigned. Packets travel as one binary
  frame (src/dest IP and port, then payload) on both transports.
- Added `?p2p`, a WebRTC mesh. Each pair has a `lan` channel (unordered, no retransmits:
  UDP-like) for game frames and a reliable `ctl` channel for control. Control messages reach
  their target over a direct `ctl` or through a neighbour linked to it (ttl 3).
  - Bootstrap: invite/reply codes (deflated JSON holding the SDP, about 1 KB) swapped by hand.
  - Discovery: peers gossip their direct links every 5 s; a peer that learns of someone it
    isn't linked to connects through that neighbour, so one pairing joins the whole group.
  - Offers: the smaller id offers (manual invites excepted); offers carry a nonce, so a stale
    answer can't land on a newer connection. ICE is not trickled; gathering gives up after 5 s.
  - Resilience: a peer without a direct link still gets every frame, forwarded by a
    neighbour; links the browser reports `failed` are dropped and retried through the mesh
    (`disconnected` is left to WebRTC, which recovers it); peers silent for 60 s are forgotten.
- Dead end: a WebSocket relay (discovery + fallback route) was built and then dropped to keep
  the port serverless.
- Dead end that taught the most: slow code swaps. The joiner's ICE agent starts checking the
  host's candidates as soon as it makes its reply, and nothing answers until the host pastes
  it: Chromium gives up after about 30 s. Stripping the host's candidates from the invite
  fixed Chromium (with no pairs it waits, and learns the host from its incoming checks), but
  Firefox fails an agent with no pairs after a ~5 s grace period. The fix that works in both:
  keep one placeholder candidate on TEST-NET-1 (192.0.2.x, never routed) pending, adding a
  fresh one every 3 s, until the link opens. Codes now stay good for 10 minutes.
- Two adversarial reviews caught: a newcomer the larger-id peer didn't record (one-way until
  the offer arrived); tearing links down after 10 s `disconnected`, which in a two-player game
  left no route to re-signal; unanswered invites never closed; a stalled manual handshake
  blocking the mesh for 10 minutes; a bad or stale pasted code breaking a live link; the
  panel claiming "copied" when the clipboard refused. All fixed; codes are validated and
  size-capped, and a virtual-IP clash logs a warning.
- Built with Emscripten 6.0.2 (the CI recipe, in the `emscripten/emsdk:6.0.2` image): links,
  no warnings from `udp.cpp`; `GX_Network_PushPacket` exports with 5 arguments. Boots to engine
  init and stops at the first INI read without game data, as expected.
- Verified with the shell's network block, game side stubbed:
  - Firefox 159, Edge 157 and Chromium 152 as one serverless group (Firefox invited Edge,
    Edge invited Chromium, Firefox–Chromium linked through the mesh); broadcasts and unicasts
    correct; a 3-minute code swap works with either Firefox or Edge as the joiner.
  - Chromium, three tabs: with A–C cut and blocked, frames went through B both ways;
    unblocked, A–C relinked in 26 s; B closed, A–C kept going and B was forgotten.
  - Node: a `disconnected` link survives the ticks and a `failed` one ends (fails on the
    previous code).
- A real LAN match (Steam Zero Hour data, two headless Edge players paired with `?p2p`) found
  four existing bugs that stopped the browser LAN before any packet left, all fixed:
  - `WSAStartup` shim returned 0 without filling `wVersion`, so `Transport::init` failed its
    2.2 check and the lobby showed "Failed trying to communicate with the network".
  - `wchar_t` is 4 bytes in wasm, so `LANMessage` is 536 bytes; `Transport::queueSend` dropped
    everything over the retail 476 (the `static_assert` is compiled out for Emscripten). Web
    builds now accept up to what `doRecv` reads back.
  - `HEAPU8` was not exported, so the first `UDP::Write` aborted; now in
    `EXPORTED_RUNTIME_METHODS`, and the `EM_ASM` reads `Module['HEAPU8']` (Closure-safe).
  - The default name `VirtualMachine_a_b_c_d` is cut to 12 characters, so every player was
    "VirtualMachi" and joins failed as duplicate names; it is now `Player<last octet>`.
- Verified: both players listed in the LAN lobby, host created a game, the other joined and
  accepted, the match started on Alpine Assault, each built a dozer, and both clocks passed
  1:14 in step with no desync.
- Saved LAN names came back as their first letter ("Player12" -> "P"): the quoted-printable
  name codec walked bytes and stopped at the first `00 00`, which with a 4-byte `wchar_t` comes
  right after the first char. It now walks characters and writes each as two UTF-16LE bytes,
  the same format the Windows build writes, so existing prefs still load.
  Names already saved by the old encoder load as one letter, so on 4-byte `wchar_t` builds a
  one-letter saved LAN name is treated as that damage and replaced by the default (checked in the
  browser: a planted `V_00` loads as `Player11`, a planted `Amir` still loads). The base
  Generals copy of the codec and the same guard are fixed too.
- Review nits fixed: auto-assigned virtual IPs are now random `10.0.x.y` (253 addresses gave an
  8-player group about a 10% chance of a clash; now about 0.04%), and the default name is
  unique per IP (`Player<low 16 bits>` on 10.0.x.y, `P<hex IP>` otherwise). Names with emoji
  are saved as UTF-16 surrogate pairs, as on Windows, and joined back on 4-byte `wchar_t`.
- Long matches: game frames are dropped when a channel's send buffer passes 256 KB, as UDP
  would, instead of piling up seconds of stale frames behind a slow link (the game resends
  what it needs). Node check: a backed-up channel drops, a drained one sends again.
- 30-minute match (2 GPU-rendered headless Edge players, auto IPs 10.0.45.179 / 10.0.91.253,
  default names Player11699 / Player23549): both ordered a unit every 2 minutes for 9 rounds and
  every order took effect; the link stayed direct, no disconnection screen appeared, and each
  browser's memory held at 0.7–1.0 GB with no growth.
- Hidden tabs: browsers stop `requestAnimationFrame` in a background tab, so one player switching
  tabs froze that engine and the lockstep match with it. In a networked game the page now ticks
  the engine (`GX_TickHidden`) from a Web Worker timer while hidden, one tick in flight at a
  time so slow frames can't pile up. Single player still pauses. Tested on a fresh 2-player
  match, counting packets: player 2 sent 401 per 30 s visible and about 204 per 30 s while hidden
  for 2 minutes, and both players carried on normally afterwards. With the ticker switched off
  (the old behaviour) it sent almost nothing, and after 1 minute it came back to the
  Disconnection Menu.
- More players (GPU-rendered headless Edge on one 15 GB machine):
  - 8 players: 7 code swaps formed a full mesh (all 28 links direct), all 8 shared one LAN
    lobby and an 8-player game was set up; the match itself did not fit in memory (about
    1.6 GB per copy) and the host's joins timed out while it was swapped out.
  - 4 players: mesh, lobby, join, accept and start on Dark Night (4). Two copies then failed
    from machine limits (one lost its WebGL context, one stalled in swap); the other two kept
    playing in step. A leave-and-return to the lobby kept the full name.

## 2026-11-10 — Session 16: In-game rendering — shroud, terrain, minimap, colours

Reported in Edge (ANGLE on OpenGL, AMD RX 580, Mesa 25.2) and reproduced in headless Edge with
`--use-angle=gl` at 1873x1035 on Alpine Assault (one-player LAN game): blocky stair-step shroud,
black dashes across the terrain, dark stretched patches on cliffs, and a black, streaked minimap.
Five emulation bugs, all in the DX8 → WebGL2 layer:

- **No texture coordinate generation.** The shroud (and cloud shadows, the terrain noise pass,
  projected decals) asks D3D to build texcoords from the camera-space position
  (`D3DTSS_TCI_CAMERASPACEPOSITION`) through `D3DTS_TEXTUREn` (`D3DTTFF_COUNTn`). The shader
  ignored `D3DTSS_TEXCOORDINDEX` and the texture matrices, so the shroud pass multiplied the
  terrain by shroud texels picked with the terrain tiles' UVs: that was the black dashes, the dark
  cliff patches and the shroud's shape. The vertex shader now does D3D8's per-stage texcoords
  (UV set selection, camera-space normal / position / reflection, texture matrix, `PROJECTED`
  divided per fragment). Stage defaults are D3D's (`TEXCOORDINDEX` = stage).
- **Wrong `D3DTSS_*` numbers** in `emscripten_compat/d3d8types.h`: MIN/MAG filter swapped,
  bump-env, border colour, ADDRESSW, COLORARG0/ALPHAARG0 and `TEXTURETRANSFORMFLAGS` (124
  instead of 24) off. `DX8Wrapper::TextureStageStates[stage][32]` was indexed out of bounds with
  124/125, and the GLES3 layer, which reads the real numbers, never saw transform flags while
  the bump matrix landed on slot 24. All now match D3D8.
- **No linear filtering in the caps.** `TextureFilterCaps` was 0, so `TextureFilterClass` mapped
  `FILTER_TYPE_BEST`/`DEFAULT` to POINT: the shroud's stair steps (and point-sampled fonts).
  The caps now report MIN/MAG linear and MIP linear/point.
- **Minimap texture format.** The radar picks `D3DFMT_R8G8B8` first; the upload had no case for
  it and sent the 3-byte buffer as 4-byte RGBA (`Unknown DX8 texture format 20`), hence black
  with streaks.
- **WebGL2 has no texture swizzle.** `GL_TEXTURE_SWIZZLE_*` fails with INVALID_ENUM (257 errors
  per run), so every uncompressed A8R8G8B8 / X8R8G8B8 texture was sampled with red and blue
  swapped (the menu's blue night scene and orange UI were really the sunny desert shellmap and
  Zero Hour's blue UI), and L8 / A8 / A8L8 read the wrong channels. `To_GL_Pixels` now converts
  channel order on the CPU for both create and update, and replaces the two copies of that code.
- Also: `LockRect` ignored `pRect` (D3D points `pBits` at the rect's first pixel); fixed for
  textures and surfaces, used by `SurfaceClass::Lock(min,max)` and the font column tests.

Verified (same map, USA, same start, headless Edge on ANGLE-GL 1873x1035, before = deployed
build): shroud edges and the map edge fade softly, no black dashes, cliffs show rock instead of
dark smears, the minimap shows the terrain with its shroud, and the menu and control bar
have their real colours. Not fixed: some terrain blend tiles (snow patches) still have hard
cell edges; that was the same before, and it's the 2-stage terrain blend pass, a separate issue.

## 2026-11-10 — Session 17: Black infantry, resolution lock, detail level

Three reports from the live server: infantry render as black silhouettes, the resolution is
stuck at the smallest mode, and higher detail levels don't take. Reproduced and verified in
headless Edge (`--use-angle=gl`) with a LAN game on Alpine Assault (USA, Rangers from a barracks).

- **Black infantry: recoloured textures were empty.** Team-coloured textures (infantry) are copies
  made by `W3DAssetManager::Recolor_Texture_One_Time`, which builds a texture from a surface
  through `D3DXLoadSurfaceFromSurface`. That was a `return 0` stub, so the copies stayed all
  zero (read back from GL: RGBA 0). It now copies, point-sampling between the rects and
  converting between the 16/24/32-bit RGB formats. `SurfaceClass::Copy` across formats and the
  missing-texture surface used the same stub.
- **Material colour source.** The vertex shader took ambient and diffuse from the vertex colour
  whenever `D3DRS_COLORVERTEX` was on. D3D only does that per `D3DRS_{AMBIENT,DIFFUSE,EMISSIVE}MATERIALSOURCE`
  = `D3DMCS_COLOR1`, and only when the vertex has a diffuse colour. Skinned meshes always carry
  one (0 when the mesh has none) with the material as source, so they came out black even once
  textured. The GLES3 layer now passes the three sources as a bitmask. Its render-state cache
  must start at 0 like `DX8Wrapper::RenderStates`: the wrapper drops a "change" to a value it
  already holds, so seeding D3D's own defaults there made them stick.
- **Resolution.** The shim offered one mode and `web_shell.html` pinned the canvas to 800x600,
  CSS-scaled. The device now lists the usual modes from 800x600 to 3840x2160 plus the screen's
  size; `CreateDevice`/`Reset` size the canvas backing store to the back buffer and set its CSS
  aspect ratio, and the page's lock follows `Module.gxCanvasW/H`. The engine re-lays out its UI
  for the mode, mouse mapping already scales to `TheDisplay`, and the choice persists in
  `Options.ini`.
- **Detail level.** `CPUDetectClass::Init_Memory` only ran when CPUID exists, never on wasm, so
  RAM read 0 MB: `GameLODManager` failed its 256 MB check, which forces low-res textures, turns
  trees and the shell map off, and recommends Low. It now runs unconditionally (the web build
  reports 1 GB).

Verified: Rangers are textured with their team colour; vehicles keep theirs; Options lists the
modes, 1280x720 applies at once (16:9 canvas, UI re-laid out, clicks land) and survives a
reload; Very High sticks; the shell map now plays behind the main menu.

## 2026-11-11 — Session 18: Fixes from an adversarial rendering review

Three independent reviewers read the GLES3/D3D8 shim against D3D8 semantics; these are the
must-fix findings, each checked in the code first.

- **Texture stage arguments were ignored.** The fragment shader always combined arg1 = texture and
  arg2 = current. It now reads D3DTSS_COLORARG0/1/2 and ALPHAARG0/1/2 for stages 0-1, with the
  COMPLEMENT / ALPHAREPLICATE modifiers; a stage with no texture reads TEXTURE as white (D3D), not
  WebGL's black; COLOROP DISABLE ends the chain. MULTIPLYADD is arg0 + arg1·arg2 and DOT3 is
  4·dot(arg1-½, arg2-½), which keeps render2d's grayscale path identical. Stage args default to
  D3D's (TEXTURE, CURRENT).
- **Fog never showed.** FOGSTART/FOGEND arrive as float bits and were dropped, so start = end = 0.
  Stored now; the fog distance is eye-space z, as D3D vertex fog without RANGEFOGENABLE.
- **Texture uploads stole another stage's binding.** Create/Update_Texture bound to whichever unit
  was active; they now restore that unit's texture.
- **Non-indexed and UP draws applied BaseVertexIndex.** A leftover SetIndices base shifted the
  shadow-darkening and occluded-player-colour quads past their 80-byte VBO. Only
  DrawIndexedPrimitive uses the base now.
- **XYZRHW vertices went through World·View·Projection.** They are screen pixels with 1/w; the
  vertex shader maps them straight to clip space (D3D pixel centres at +0.5) and skips lighting and fog.
- **Stencil.** Only STENCILENABLE reached GL. Func/ref/mask/ops/write mask are forwarded, and the
  canvas now asks for an 8-bit stencil buffer (EmscriptenMain.cpp): it had none, so once the quads
  above drew, the player-colour quad tinted the whole view red. Clear ignores the write masks, as
  D3D's Clear does.
- **Point lights** now stop at Range and add their Ambient (D3D: global + Σ attenuated light ambient).
- **Render-state cache.** Session 17 said this cache "must start at 0 like DX8Wrapper::RenderStates";
  that was wrong: the engine's cache starts at 0x12345678 (Invalidate_Cached_Render_States). The
  redundant-state skip here dropped the engine's first "set to 0" (TEXTUREFACTOR stayed white,
  AMBIENT stayed 0.2). The skip is gone; the engine already filters repeats.

Verified: same LAN game on Alpine Assault in headless Edge (ANGLE-GL) renders the same as the
deployed build (only the flashing general's button differs), 8 stencil bits, no WebGL errors.
Not exercised by that scene: fog, stencil shadows, point lights. Still open (should-fix): stages
2-3 always modulate, missing ADDSMOOTH/SUBTRACT/specular, SetRenderTarget no-op, authored mips
replaced by glGenerateMipmap, small DXT mips under-allocated, no S3TC fallback.

## 2026-11-11 — Session 19: Pixel shaders, four texture stages, specular, small DXT levels

The should-fix items from the Session 18 review, starting with the terrain blend edges.

- **Terrain blends (hard snow edges) — root cause: no pixel shader ever ran.** The web build reports
  PS 1.1, so the game picks TerrainShaderPixelShader (terrain.pso: `lrp r0, v0.a, t1, t0` then
  `mul r0, r0, v0`) and the flat/road variants. Two things stopped it: LoadAndCreateD3DShader sized
  its buffer with getFileInfo, which returns 0 for these backslash archive paths here, so the
  shader tokens were never read; and the shim's CreatePixelShader/SetPixelShader were stubs. The
  terrain then drew with whatever stage states were left over: dark, unlit by the vertex colour,
  and with no blend between the base and blend tile. Fixed: the loader asks the open file for its
  size; CreatePixelShader parses ps.1.1 tokens (tex, mov, add, sub, mul, mad, dp3, dp4, lrp; write
  masks, _x2/_x4/_d2, _sat; .a/.b replicate; neg/bias/sign/comp/x2 source modifiers) into a small
  program the fragment shader interprets, with c0-c7 from SetPixelShaderConstant. A program with
  anything else (texbem: wave, motion blur) gets no handle, so its caller keeps the fixed-function
  path as before. Trees still use the fixed-function path (their vertex shader is not emulated, so
  the game never selects the pixel shader for them). Water's runtime-assembled shaders are untouched
  (D3DXAssembleShader still fails).
- **Texture stages 0-3** now all run D3D's op chain with their own COLORARG0/1/2 and ALPHAARG0/1/2,
  stop at the first DISABLE, and saturate every stage (MODULATE2X no longer feeds >1 onward). Added
  SUBTRACT, ADDSMOOTH, BLENDDIFFUSEALPHA, BLENDFACTORALPHA, BLENDTEXTUREALPHAPM,
  MODULATECOLOR_ADDALPHA, MODULATEINV*, LERP, and the same ops on the alpha side. BLENDTEXTUREALPHA
  and BLENDCURRENTALPHA now use the texture's and current alpha (they used arg1.a/arg2.a).
  Stage ops are seeded with D3D's defaults (stage 0 MODULATE/SELECTARG1, others DISABLE).
- **Specular.** Vertex specular colour (FVF SPECULAR, attribute 11) is passed through when lighting
  is off; with lighting on, Blinn-Phong per light (local viewer, material power and specular, light
  specular). D3DRS_SPECULARENABLE adds it after the stages; D3DTA_SPECULAR reads it.
- **DXT levels below 4x4** were allocated w*h*BPP bytes, less than one 8/16-byte block; they get
  whole blocks now (texture levels and standalone surfaces).

Verified in headless Edge (same LAN game, Alpine Assault): snow and grass patches blend softly, the
terrain is lit by the vertex colour (it was much darker), no WebGL errors. Not exercised: specular
(nothing in that scene enables it), the monochrome filter, fog, stencil shadows, point lights.
Still open: SetRenderTarget no-op, authored mips replaced by glGenerateMipmap, no S3TC fallback,
a texture changed while bound is not re-uploaded, COLORWRITEENABLE ignored.
