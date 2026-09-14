# Post Void — Android Port: Full Implementation Guide

## Status: ✅ FULLY WORKING (as of 2026-08-28)

### What Works
- Game boots and runs at 60 FPS on Android (OnePlus 12, API 34)
- Full audio via Oboe
- Touch input works (mouse simulation)
- **Physical gamepad/controller — fully working** ✅
  - Buttons map correctly to Xbox layout
  - Analog sticks for movement and look
  - D-pad navigation
  - GML input system recognizes controller as "XInput STANDARD GAMEPAD"

---

## Architecture

The port is a thin Android wrapper around the original `libyoyo.so` (GameMaker Studio 2 runtime).

```
APK
├── libyoyo.so          — Unmodified GMS2 runtime (patched for os_windows spoof)
├── libgamepad_hook.so  — Our PortMaster-style gamepad intercept layer
├── libpostvoid_runner.so — Legacy stub (unused)
├── libc++_shared.so
└── assets/             — Game data from original APK
    ├── gamecontrollerdb.txt
    └── game.apk (embedded)
```

---

## Gamepad System — Key Technical Details

### Problem
`libyoyo.so` on Android expects Java to call `onGPKeyDown(slot, keycode)` via JNI to feed controller input. However, this crashes with SIGSEGV at offset `+0x58` because the internal `g_Gamepads[slot]` C++ struct is never allocated in our minimal Java wrapper (it normally requires SDL2's full init sequence).

### Solution (mirrors PortMaster's gmloader-next approach)
We intercept `Function_Add` inside `libyoyo.so` to replace **all 18 `gamepad_*` GML built-in functions** with our own C++ implementations that read from a Java-driven state array.

### How `libgamepad_hook.so` Works

**Step 1 — Find `libyoyo.so` base address** (bypasses Android linker namespace isolation):
```cpp
dl_iterate_phdr(find_libyoyo_callback, nullptr);
// dlpi_addr = ASLR load bias of libyoyo.so
```

**Step 2 — Resolve `Function_Add` by base + nm offset** (no dlsym needed):
```
nm -D libyoyo.so | grep Function_Add
→ _Z12Function_AddPKcPFvR6RValueP9CInstanceS4_iPS1_Eib  0x003eae5c

fct_add_t fn_add = (fct_add_t)(gh_libyoyo_base + 0x003EAE5C);
```

**Step 3 — Register 18 replacement functions** via `Function_Add`:
- `gamepad_is_connected` → always returns true for slot 0
- `gamepad_get_description` → returns `"XInput STANDARD GAMEPAD"` via `YYCreateString`
- `gamepad_button_check` / `_pressed` / `_released` → reads `gh_gamepads[slot].buttons[]`
- `gamepad_axis_value` → reads `gh_gamepads[slot].axis[]`

**Step 4 — Java pushes input events into C++ state**:
```kotlin
// MainActivity.kt → onKeyDown
GamepadBridge.onButtonDown(0, keyCode)   // → nativeButtonDown JNI

// MainActivity.kt → onGenericMotionEvent  
GamepadBridge.onAxis(0, 0, axisX)       // Left stick X
GamepadBridge.onAxis(0, 1, axisY)       // Left stick Y
```

**Step 5 — Per-frame tick** (called before `RunnerJNILib.Process()`):
```kotlin
GamepadBridge.tick()  // advances: just-pressed(2)→held(1), just-released(-1)→up(0)
```

### Verified libyoyo.so Symbol Offsets

| Symbol | Offset | Purpose |
|--------|--------|---------|
| `_Z12Function_AddPKcPFvR6RValueP9CInstanceS4_iPS1_Eib` | `0x003EAE5C` | Register GML built-in override |
| `_Z14YYCreateStringP6RValuePKc` | `0x0040703C` | Create proper GML RefString |

### Button Mapping

| GML Constant | Index | Android KeyCode | Controller Button |
|---|---|---|---|
| `gp_face1` (32769) | 0 | 96 `BUTTON_A` | A / Cross |
| `gp_face2` (32770) | 1 | 97 `BUTTON_B` | B / Circle |
| `gp_face3` (32771) | 2 | 99 `BUTTON_X` | X / Square |
| `gp_face4` (32772) | 3 | 100 `BUTTON_Y` | Y / Triangle |
| `gp_shoulderl` (32773) | 4 | 102 `BUTTON_L1` | LB |
| `gp_shoulderr` (32774) | 5 | 103 `BUTTON_R1` | RB |
| `gp_shoulderlb` (32775) | 6 | 104 `BUTTON_L2` | LT (digital) |
| `gp_shoulderrb` (32776) | 7 | 105 `BUTTON_R2` | RT (digital) |
| `gp_select` (32777) | 8 | 109 `BUTTON_SELECT` | Back/Select |
| `gp_start` (32778) | 9 | 108 `BUTTON_START` | Start/Menu |
| `gp_stickl` (32779) | 10 | 106 `BUTTON_THUMBL` | L3 |
| `gp_stickr` (32780) | 11 | 107 `BUTTON_THUMBR` | R3 |
| `gp_padu` (32781) | 12 | 19 `DPAD_UP` | D-Pad Up |
| `gp_padd` (32782) | 13 | 20 `DPAD_DOWN` | D-Pad Down |
| `gp_padl` (32783) | 14 | 21 `DPAD_LEFT` | D-Pad Left |
| `gp_padr` (32784) | 15 | 22 `DPAD_RIGHT` | D-Pad Right |

### Axis Mapping

| GML Constant | Index | Android AXIS | Controller |
|---|---|---|---|
| `gp_axislh` (32785) | 0 | `AXIS_X` | Left Stick X |
| `gp_axislv` (32786) | 1 | `AXIS_Y` | Left Stick Y |
| `gp_axisrh` (32787) | 2 | `AXIS_Z` | Right Stick X |
| `gp_axisrv` (32788) | 3 | `AXIS_RZ` | Right Stick Y |
| (trigger) | 4 | `AXIS_LTRIGGER` / `AXIS_BRAKE` | Left Trigger |
| (trigger) | 5 | `AXIS_RTRIGGER` / `AXIS_GAS` | Right Trigger |

---

## libyoyo.so Patches Applied

| Offset | Original | Patched | Purpose |
|--------|----------|---------|---------|
| `0x47E044` | `BL os_type` | `FMOV D0, XZR` | Force `os_type = os_windows (0)` |
| `0x4807C0` | original | `MOV X8, XZR` | Force args[0].val = 0 for os_windows check |

---

## Key Files

| File | Purpose |
|------|---------|
| `android/app/src/main/cpp/gamepad_hook/gamepad_hook.cpp` | Native gamepad hook library |
| `android/app/src/main/java/com/postvoid/port/GamepadBridge.kt` | Kotlin JNI bridge |
| `android/app/src/main/java/com/postvoid/port/MainActivity.kt` | Android activity + input routing |
| `android/app/src/main/java/com/yoyogames/runner/RunnerJNILib.java` | GMS2 JNI stubs |
| `android/app/src/main/jniLibs/arm64-v8a/libyoyo.so` | Patched GMS2 runtime |
| `android/app/src/main/assets/gamecontrollerdb.txt` | SDL gamecontroller DB |

---

## Build & Install

```bash
cd android
gradle assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Debugging

```bash
# Confirm hooks installed:
adb logcat | grep GamepadHook

# Confirm GML recognized controller:
adb logcat | grep "keys_xinput"

# Watch live button presses:
adb logcat -s GamepadHook
```
