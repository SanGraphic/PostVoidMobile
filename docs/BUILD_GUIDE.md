# Build & Compilation Guide — Post Void Android Port

This guide provides detailed instructions on compiling, patching, packaging, and deploying the Post Void Android port.

---

## 1. Environment & Toolchain Requirements

| Component | Required Version | Purpose |
| :--- | :--- | :--- |
| **JDK** | OpenJDK 17 or 21 | Android Gradle Plugin compilation |
| **Android SDK** | API Level 34 (Android 14) | Platform build targets |
| **Android NDK** | NDK r25c (25.2.9519653) or higher | C++ Clang compilation for ARM64-v8a / ARMv7 |
| **CMake** | 3.22.1+ | Native library build management |
| **.NET SDK** | .NET 8.0, 9.0, or 10.0 | Running `PostVoidCLI` UndertaleModLib patcher |
| **Gradle** | 8.5+ | Packaging and assembling APKs |
| **ADB** | Platform Tools 34+ | Device installation and log extraction |

---

## 2. Step-by-Step Asset Patching Pipeline

GameMaker PC bytecode expects Steamworks and specific platform libraries. Before assembling the APK, `Post Void Steam/data.win` must be processed by `PostVoidCLI`:

```powershell
# From the project root:
dotnet run --project tools\PostVoidCLI
```

### What `PostVoidCLI` Does Internally:
1. **Reads `Post Void Steam/data.win`** via `UndertaleIO.Read()`.
2. **Neutralizes Steam Inventory & Initialization in `obj_platform_steam_Create_0`**:
   - Replaces `steam_inventory_get_all_items()` with `PushI -1`.
   - Replaces `steam_initialised()` with `PushI 0` (forcing the safe offline branch).
   - Initializes user data and sets `game_profile_ready = true` so the preloader proceeds immediately.
3. **Neutralizes `steam_update` in `obj_platform_steam_Step_0`**:
   - Replaces instruction 0 with `Exit` opcode.
4. **Bypasses Steam Stats & Achievements in `gml_GlobalScript_Achievements`**:
   - In `achievement_unlock`: Replaces `Bf 7` with `B 7`.
   - In `scr_stat_set`: Replaces `Bf 27` with `B 27`.
   - In `scr_stat_inc`: Replaces `Bf 41` with `B 41` (bypasses `steam_get_stat_int` and `steam_set_stat_int`).
5. **Neutralizes Steam Leaderboards**:
   - In `gml_GlobalScript_scr_new_highscore` and `gml_GlobalScript_scr_menu_update_leaderboard`, replaces all `PushI 115` (`obj_platform_steam`) with `PushI -1`.
6. **Saves Patched Bytecode**:
   - Outputs the patched bytecode directly to `android/app/src/main/assets/game.droid`.

---

## 3. Compiling Native C++ Code (`libgamepad_hook.so`)

The native C++ hook layer (`android/app/src/main/cpp/gamepad_hook/gamepad_hook.cpp`) is built via CMake and NDK Clang.

### CMake Build Definition (`android/app/src/main/cpp/CMakeLists.txt`):
```cmake
cmake_minimum_required(VERSION 3.22.1)
project("postvoid_gamepad_hook")

add_library(gamepad_hook SHARED
    gamepad_hook/gamepad_hook.cpp
)

target_include_directories(gamepad_hook PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gamepad_hook
    log
    android
)
```

To build directly via Gradle:
```powershell
cd android
./gradlew compileDebugSources
```

---

## 4. Assembling Standalone vs BYO Runner APKs

### A. Standalone APK (Recommended for Personal Use)
Includes `game.droid` and all audio group assets embedded inside the APK.
```powershell
cd android
./gradlew assembleDebug
```
- **Output**: `android/app/build/outputs/apk/debug/app-debug.apk` (~665 MB)

### B. BYO (Bring-Your-Own-Data) Runner APK
Excludes `game.droid` and assets from `assets/` so the APK remains lightweight (~15 MB). Users supply their own `data.win` placed in `/Android/data/com.postvoid.port/files/`.

---

## 5. Deployment & Testing

```powershell
# 1. Push APK to temporary device storage
adb push android/app/build/outputs/apk/debug/app-debug.apk /data/local/tmp/app.apk

# 2. Install APK with replace flag
adb shell pm install -r /data/local/tmp/app.apk

# 3. Launch MainActivity
adb shell am start -n com.postvoid.port/.MainActivity

# 4. View live logs during execution
adb logcat -s yoyo RunnerJNILib GamepadHook MainActivity
```
