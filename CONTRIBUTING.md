# Contributing to PostVoid Mobile

Thank you for your interest in contributing to **PostVoid Mobile**! This project is an open-source, community-driven mobile runtime for *POST VOID* built on a strict **Bring Your Own Data (BYOD)** legal model.

---

## 1. Golden Rules of Contribution

1. **NO COPYRIGHTED ASSETS**:
   Never submit pull requests containing proprietary game data, audio, music, textures, or compiled bytecode (`data.win`, `game.droid`, `audiogroup*.dat`, Steam DLLs, etc.). All pull requests containing proprietary assets will be immediately closed.
2. **AUTHENTIC STEAM PARITY**:
   Modifications must preserve 1:1 gameplay parity with the latest upstream Steam release (v1.4C). Custom gameplay alterations (cheats, speed hacks, physics tweaks) belong in separate feature branches or forks.
3. **HARDWARE & BATTERY AWARENESS**:
   Android runs on a massive variety of chipsets (from Snapdragon 8 Gen 3 flagships down to low-end Mali and PowerVR GPUs). Avoid heavy render-thread allocations, redundant canvas redraws, or unbounded CPU loops.

---

## 2. How to Contribute

### A. Submitting Device Compatibility Reports
Testing on real physical hardware is essential to this project. If you have tested PostVoid Mobile on your device:
1. Go to the [Issues](https://github.com/SanGraphic/PostVoidMobile/issues) tab.
2. Select **Device Compatibility Report**.
3. Fill in your device model, SoC/GPU, Android version, refresh rate, and gameplay observations (FPS stability, touch latency, controller connectivity).

### B. Reporting Bugs
1. Search existing issues before creating a new report.
2. If the issue has not been reported, open a **Bug Report** using the template.
3. Include device details, steps to reproduce, and Android logcat output (`adb logcat -s "MainActivity" "GamepadBridge" "TouchOverlayView"`) if possible.

### C. Submitting Pull Requests
1. Fork the repository and create a feature branch (`git checkout -b feature/my-enhancement`).
2. Follow standard Kotlin and C++17 formatting standards.
3. Verify your build compiles locally:
   ```bash
   cd android
   ./gradlew assembleByodRelease
   ```
4. Test the generated APK on real hardware or an emulator.
5. Submit your pull request against the `main` branch with a clear summary of your changes.

---

## 3. Local Development Environment

* **JDK**: OpenJDK 17 (Temurin recommended)
* **Android SDK**: Compile SDK 34, Min SDK 24
* **Android NDK**: Version `26.1.10909125`
* **CMake**: Version `3.22.1`
* **Build System**: Gradle 8.7 / AGP 8.3.0

---

## 4. Code Architecture Guidelines

* **Touch Controls & HUD**: Located in `android/app/src/main/java/com/postvoid/port/TouchOverlayView.kt`.
  * Keep canvas drawing optimized: pre-scale bitmaps to avoid runtime downsampling.
  * Only trigger `invalidate()` when visual state genuinely changes.
* **Native C++ Hooks**: Located in `android/app/src/main/cpp/gamepad_hook/gamepad_hook.cpp`.
  * Ensure atomic operations (`std::atomic<double>`) are used for thread-safe lock-free communication between the UI thread and the OpenGL render thread.
* **BYOD & Storage**: Located in `android/app/src/main/java/com/postvoid/port/BYODManager.kt`.
  * Follow Android Scoped Storage best practices.
