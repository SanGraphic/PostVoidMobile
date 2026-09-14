# BYOD (Bring Your Own Data) Architecture & The Way Forward

## 1. Deep Dive: Everything Compiled During `gradle assembleDebug`

When running `gradle assembleDebug`, Android Gradle Plugin (AGP), CMake, and NDK compile and package the following components into `app-debug.apk` (Total Size: **~665 MB**):

```
app-debug.apk (~665 MB)
├── assets/ (~645 MB)  <── [CAN BE STRIPPED FOR BYOD]
│   ├── data.win (567 MB)             # Steam PC GameMaker Bytecode & Textures
│   ├── game.droid (567 MB)           # Android GameMaker Bytecode alias
│   ├── fonts/ (NotoSans fonts)       # CJK & Arabic fallback fonts
│   ├── localization.json             # Multilingual string tables
│   ├── options.ini                   # Game engine configuration
│   └── gamecontrollerdb.txt          # SDL GameController mapping DB
├── lib/arm64-v8a/ (~15 MB)
│   ├── libyoyo.so (14.2 MB)          # GameMaker Studio 2.3+ Android Native Runtime
│   ├── libgamepad_hook.so (350 KB)   # Custom C++ controller & touch hook library
│   ├── libpostvoid_runner.so (200 KB)# Custom GLES/Oboe native runner stubs
│   └── libc++_shared.so (1.2 MB)     # LLVM libc++ runtime
├── classes.dex (~1.5 MB)
│   ├── MainActivity.kt               # Lifecycle, display refresh (120Hz), input routing
│   ├── TouchOverlayView.kt           # Custom 6-button suite, slider, joystick, pause HUD edit
│   ├── GamepadBridge.kt              # JNI wrapper for libgamepad_hook.so
│   ├── BYODManager.kt                # Data verification and file management helper
│   ├── GameLogger.kt                 # In-game logging system
│   └── RunnerJNILib.java             # GameMaker JNI interface definitions
└── res/ (~2.5 MB)
    ├── drawable/                     # High-res SVG/PNG touch button icons
    ├── mipmap/                       # App icons
    └── values/strings.xml            # App metadata ("Post Void")
```

---

## 2. How the Game Engine Loads Data Currently

1. In `MainActivity.kt`:
   ```kotlin
   val apkPath = applicationInfo.publicSourceDir // Path to /data/app/.../base.apk
   val savePath = BYODManager.getSaveDirectory(this).absolutePath
   RunnerJNILib.Startup(apkPath, savePath, packageName, 0)
   ```
2. `libyoyo.so` opens `apkPath` as a ZIP archive and mounts the internal `assets/` virtual filesystem.
3. `libyoyo.so` loads `assets/game.droid` or `assets/data.win` into memory and begins executing GameMaker bytecode.
4. `libgamepad_hook.so` intercepts `Function_Add` inside `libyoyo.so` to inject custom gamepad and touch controllers.

---

## 3. How BYOD Will Work with All Current Changes

With the custom controller/touch isolation and HUD editor in place, BYOD will function seamlessly:

### A. Slim APK (Distribution Build)
- By removing `data.win` and `game.droid` from `src/main/assets/`, the APK size drops from **~665 MB to ~25 MB**.
- The APK contains only the engine runtime, C++ hooks, Kotlin touch controls, shaders, and UI assets.

### B. First Launch Detection (`BYODManager`)
When the app launches:
```
             ┌──────────────────────────────────────────────┐
             │            MainActivity.onCreate()           │
             └──────────────────────┬───────────────────────┘
                                    │
                        isDataWinReady(context)?
                                    │
                    ┌───────────────┴───────────────┐
                   YES                              NO
                    │                               │
       ┌────────────▼─────────────┐    ┌────────────▼─────────────┐
       │ Launch GameMaker Engine  │    │  Show Post Void BYOD UI  │
       │ (RunnerJNILib.Startup)   │    │  - File Picker (SAF)     │
       └──────────────────────────┘    │  - Auto-scan Downloads   │
                                       │  - Extract & Verify      │
                                       └────────────┬─────────────┘
                                                    │ User selects data.win
                                                    ▼
                                       ┌──────────────────────────┐
                                       │ Build Internal Container │
                                       │ (game_data.apk / zip)    │
                                       └────────────┬─────────────┘
                                                    │
                                                    ▼
                                       ┌──────────────────────────┐
                                       │ Launch GameMaker Engine  │
                                       └──────────────────────────┘
```

### C. Constructing the Game Data Container
Because `libyoyo.so`'s `RunnerJNILib.Startup(apkPath, ...)` expects `apkPath` to be a ZIP containing `assets/data.win`, the BYOD process works as follows:
1. When the user provides `data.win` (via Android File Picker or copying to `/sdcard/Android/data/com.postvoid.port/files/`):
2. `BYODManager` creates a lightweight zip container at `${context.filesDir}/game_data.apk` containing:
   - `assets/data.win`
   - `assets/options.ini`
   - `assets/localization.json`
   - `assets/fonts/*` (copied from the app's own assets)
3. `MainActivity.kt` calls:
   ```kotlin
   val dataApkPath = BYODManager.getGameContainerPath(this)
   RunnerJNILib.Startup(dataApkPath, savePath, packageName, 0)
   ```
4. `libyoyo.so` boots the game from `game_data.apk` exactly as if it were embedded in the original APK.

---

## 4. Integration with Touch & Controller Systems

- **No Conflicts**: Touch controls (`TouchOverlayView.kt`), HUD edit mode, button size sliders, reset button, and physical gamepad hooks (`gamepad_hook.cpp`) operate entirely independently of asset loading.
- **Dynamic Mode Switching Remains Active**: Physical gamepads and touch controls will continue to switch dynamically whether the game data is embedded or loaded via BYOD.
- **Saves & Config Preserved**: Game saves (`global.save`), high scores, and custom touch layouts (`postvoid_touch_layout.xml`) remain stored in the app's persistent internal storage across game data updates.

---

## 5. Implementation Roadmap (When Ready)

1. **Dual Build Flavor Support in `build.gradle`**:
   - `debugEmbedded`: Embeds `data.win` for instant local testing/debugging.
   - `releaseByod`: Excludes `data.win`, producing a lightweight ~25 MB installer APK.
2. **Post Void Styled BYOD Setup Screen**:
   - Modern Storage Access Framework (SAF) file picker button ("SELECT STEAM DATA.WIN").
   - Direct path scanner for `/sdcard/Android/data/com.postvoid.port/files/data.win`.
   - Hash/signature verification to confirm a valid Post Void `data.win`.
3. **Fast Container Generator**:
   - Streams `data.win` directly into `game_data.apk` with `Deflater.NO_COMPRESSION` (0% CPU overhead, instant generation in <1 second).
