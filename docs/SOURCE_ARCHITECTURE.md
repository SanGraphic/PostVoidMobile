# Source Code Architecture & Technical Internals — Post Void Android

This document details the code architecture across the Kotlin Android app layer, C++ NDK hook layer, GameMaker runtime libraries, and UndertaleModLib bytecode patchers.

---

## 1. High-Level Architecture Diagram

```
┌────────────────────────────────────────────────────────┐
│                   MainActivity.kt                      │
│  - SurfaceView & Render Loop                           │
│  - Display.Mode 120Hz Switching                        │
│  - Key/Motion Event Interception & Forwarding          │
└──────────────────────────┬─────────────────────────────┘
                           │ JNI
┌──────────────────────────▼─────────────────────────────┐
│                   GamepadBridge.kt                     │
│  - Native method declarations                          │
│  - Coordinates surface resizing & input state buffer   │
└──────────────────────────┬─────────────────────────────┘
                           │ Native Calls
┌──────────────────────────▼─────────────────────────────┐
│                 gamepad_hook.cpp                       │
│  - ARM64 Inline Assembly Hooking                       │
│  - GML Function Table Dynamic Redirection              │
│  - Virtual Mouse Centering & Stick Math                │
│  - Steamworks API Dummy Mocking Layer                  │
└──────────────────────────┬─────────────────────────────┘
                           │ In-Memory Hooking
┌──────────────────────────▼─────────────────────────────┐
│                    libyoyo.so                          │
│  - Official GameMaker ARM64 Runner Engine              │
│  - OpenGL ES 3.2 Renderer Pipeline                     │
│  - Bytecode VM Interpreter                             │
└──────────────────────────┬─────────────────────────────┘
                           │ Loads
┌──────────────────────────▼─────────────────────────────┐
│                   game.droid                           │
│  - Bytecode Patched by PostVoidCLI (UndertaleModLib)   │
│  - All Steam API calls neutralized                     │
│  - 3D Meshes, Audio, Shaders, Textures                 │
└────────────────────────────────────────────────────────┘
```

---

## 2. File-by-File Breakdown

### Android / Kotlin Layer
- **`MainActivity.kt`**:
  - Initializes the GameMaker runtime via `RunnerJNILib.Startup()` and `RunnerJNILib.initGLFuncs()`.
  - Configures immersive full screen (`WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`).
  - Sets up the `SurfaceHolder.Callback` to capture dynamic hardware dimensions ($2376 \times 1080$).
  - Queries `display.supportedModes` to automatically lock the display refresh rate to 120Hz.
- **`GamepadBridge.kt`**:
  - Exposes JNI interfaces (`nativeInstallHooks`, `nativeSetSurfaceSize`, `nativeSetMousePos`, `nativeSetGamepadButton`, `nativeSetGamepadAxis`).
  - Provides thread-safe communication between Android input threads and the GameMaker render thread.
- **`GameLogger.kt`**:
  - Spawns a background worker executing `logcat` filtered to PID and redirects output to `/storage/emulated/0/Android/data/com.postvoid.port/files/game.log` for instant crash analysis.

### Native C++ Hook Layer (`gamepad_hook.cpp`)
- **ARM64 Inline Hooking Engine**:
  - Replaces the first 4 instructions of targeted functions in `libyoyo.so` with:
    ```arm64
    ldr x16, [pc, #8]
    br x16
    .quad target_address
    ```
  - Hooks display functions:
    - `display_get_width` (offset `0x00442198`)
    - `display_get_height` (offset `0x004421C4`)
    - `window_get_width` (offset `0x00442838`)
    - `window_get_height` (offset `0x00442864`)
    - `display_mouse_get_x` (offset `0x0044228C`)
    - `display_mouse_get_y` (offset `0x004422B8`)
    - `window_mouse_get_x` (offset `0x004429EC`)
    - `window_mouse_get_y` (offset `0x00442A18`)
- **Steamworks In-Memory Mock Layer**:
  - Implements dummy handlers for `steam_get_stat_int`, `steam_set_stat_int`, `steam_set_achievement`, `steam_initialised`, etc.

### Bytecode Patcher (`tools/PostVoidCLI/Program.cs`)
- Built on `UndertaleModLib` and `Underanalyzer`.
- Operates directly on the compiled GameMaker bytecode chunk (`CODE` and `FUNC` chunks) without needing to recompile the game from scratch.
- Enforces offline fallback profile flags (`game_profile_ready = true`, `global.UserData[0..2] = default/false`).
