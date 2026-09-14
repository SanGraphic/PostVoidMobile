# Post Void Android Port — Gamepad & Input Subsystem Documentation

## 1. Executive Summary

This document details the complete reverse engineering, memory architecture, and implementation of 1:1 physical gamepad support for the Android port of **Post Void** (GameMaker Studio 2.3+ YYC / Bytecode runtime on `arm64-v8a`).

The solution delivers native, instantaneous responsiveness matching the Steam desktop and PortMaster releases, supporting direct USB-C gamepads (e.g. **GameSir X5 Lite**), Bluetooth gamepads (**Nintendo Switch Pro Controller**, **Xbox Wireless Controller**), and standard Android input controllers.

---

## 2. Display, Full-Screen & Resolution Architecture

### 2.1 Cutout-Aware Sticky Immersive Full Screen
Modern Android devices (like OnePlus 12) feature tall aspect ratio displays (19.8:9 / 2.2:1) with camera punch holes.
- Configured `LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES` to render seamlessly edge-to-edge behind the camera cutout without letterboxing.
- Configured sticky immersive mode with `WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE`.

### 2.2 Dynamic Aspect Ratio & 1080p Resolution Capping
To prevent excessive GPU load and battery drain on 1440p/QHD+ panels while supporting every display aspect ratio:
- Native resolution detection using `currentWindowMetrics.bounds` computes the exact aspect ratio.
- Fixed surface buffer size capped at **1080p height** (e.g., `2376 x 1080` on OnePlus 12) via `SurfaceHolder.setFixedSize()`.
- The hardware display scaler upscales the 1080p buffer to the physical panel with zero GPU overhead.

### 2.3 Authentic Post Void Steam Icon
Extracted the genuine Post Void head idol icon directly from `Post Void Steam/Post Void.exe` PE resource group `152` and generated high-resolution mipmaps (`mdpi`..`xxxhdpi`) for standard and adaptive launcher formats.

### 2.4 Custom Port Branding & Credits
- **Options > Credits**: Replaced `"Porting"` category header across all languages in `localization.json` and `localization_ar_test.json` with `"Android Port by SanGraphic"`.
- **Main Menu / Title Screen**: Hooked `draw_text` (`_Z10F_DrawText` and `_Z12GR_Text_Draw`) in `gamepad_hook.cpp` to dynamically replace `"1.4c"` with `"1.4c - Android Port by @SanGraphic"`.

---

## 3. Reverse Engineered Memory Map (`libyoyo.so` arm64-v8a)

The table below lists all **30 ARM64 Inline Hooks** installed into `libyoyo.so`:

| Symbol Name | Offset | Type | Purpose |
|---|---|---|---|
| `_Z23YoYo_GetPlatform_DoWorkv` | `0x0047E044` | ARM64 Trampoline | Force `os_type` to `0.0` (`os_windows`) |
| `_Z18F_YoYo_GetPlatformR6RValueP9CInstanceS2_iPS_` | `0x0047E04C` | ARM64 Trampoline | Force GML `os_type` return value to `0.0` |
| `_Z20GET_YoYo_GetPlatformP9CInstanceiP6RValue` | `0x004807C0` | ARM64 Trampoline | Force GML variable getter for `os_type` |
| `_Z13IO_Start_Stepv` | `0x004B4744` | ARM64 Trampoline | Step-synchronous gamepad state advancement & zero dirty buffers |
| `_Z18F_GamepadSupportedR6RValueP9CInstanceS2_iPS_` | `0x0047AAB4` | ARM64 Trampoline | `gamepad_is_supported()` |
| `_Z23F_GamepadGetDeviceCountR6RValueP9CInstanceS2_iPS_` | `0x0047AAEC` | ARM64 Trampoline | `gamepad_get_device_count()` |
| `_Z18F_GamepadConnectedR6RValueP9CInstanceS2_iPS_` | `0x0047AB0C` | ARM64 Trampoline | `gamepad_is_connected(pad)` |
| `_Z23F_GamepadGetDescriptionR6RValueP9CInstanceS2_iPS_` | `0x0047AB7C` | ARM64 Trampoline | Returns `"XInput STANDARD GAMEPAD"` |
| `_Z27F_GamepadGetButtonThresholdR6RValueP9CInstanceS2_iPS_` | `0x0047ABEC` | ARM64 Trampoline | `gamepad_get_button_threshold()` |
| `_Z27F_GamepadSetButtonThresholdR6RValueP9CInstanceS2_iPS_` | `0x0047AC50` | ARM64 Trampoline | `gamepad_set_button_threshold()` |
| `_Z24F_GamepadGetAxisDeadzoneR6RValueP9CInstanceS2_iPS_` | `0x0047ACE8` | ARM64 Trampoline | `gamepad_get_axis_deadzone()` |
| `_Z24F_GamepadSetAxisDeadzoneR6RValueP9CInstanceS2_iPS_` | `0x0047AD4C` | ARM64 Trampoline | `gamepad_set_axis_deadzone()` |
| `_Z20F_GamepadButtonCountR6RValueP9CInstanceS2_iPS_` | `0x0047ADE4` | ARM64 Trampoline | `gamepad_button_count()` |
| `_Z20F_GamepadButtonCheckR6RValueP9CInstanceS2_iPS_` | `0x0047AE48` | ARM64 Trampoline | `gamepad_button_check()` |
| `_Z27F_GamepadButtonCheckPressedR6RValueP9CInstanceS2_iPS_` | `0x0047AEEC` | ARM64 Trampoline | `gamepad_button_check_pressed()` |
| `_Z28F_GamepadButtonCheckReleasedR6RValueP9CInstanceS2_iPS_` | `0x0047AF90` | ARM64 Trampoline | `gamepad_button_check_released()` |
| `_Z20F_GamepadButtonValueR6RValueP9CInstanceS2_iPS_` | `0x0047B034` | ARM64 Trampoline | `gamepad_button_value()` |
| `_Z18F_GamepadAxisCountR6RValueP9CInstanceS2_iPS_` | `0x0047B0D4` | ARM64 Trampoline | `gamepad_axis_count()` |
| `_Z18F_GamepadAxisValueR6RValueP9CInstanceS2_iPS_` | `0x0047B450` | ARM64 Trampoline | `gamepad_axis_value()` |
| `_Z19F_GamepadGetMappingR6RValueP9CInstanceS2_iPS_` | `0x0047B644` | ARM64 Trampoline | `gamepad_get_mapping()` |
| `_Z16F_GamepadGetGuidR6RValueP9CInstanceS2_iPS_` | `0x0047B6D8` | ARM64 Trampoline | `gamepad_get_guid()` |
| `_Z21F_GamepadSetVibrationR6RValueP9CInstanceS2_iPS_` | `0x0047B750` | ARM64 Trampoline | `gamepad_set_vibration()` -> JNI Android Haptics |
| `_Z18F_GamepadSetColourR6RValueP9CInstanceS2_iPS_` | `0x0047B19C` | ARM64 Trampoline | `gamepad_set_colour()` |
| `_Z20YYGML_keyboard_checki` | `0x0044F0F8` | ARM64 Trampoline | GML internal `keyboard_check` |
| `_Z27YYGML_keyboard_check_directi` | `0x0044F3B4` | ARM64 Trampoline | GML internal `keyboard_check_direct` |
| `_Z18F_KeyboardCheckR6RValueP9CInstanceS2_iPS_` | `0x0044F24C` | ARM64 Trampoline | GML built-in `keyboard_check` |
| `_Z18F_CheckMouseButtonR6RValueP9CInstanceS2_iPS_` | `0x00486260` | ARM64 Trampoline | GML built-in `mouse_check_button` |
| `_Z25F_CheckMouseButtonPressedR6RValueP9CInstanceS2_iPS_` | `0x004862AC` | ARM64 Trampoline | GML built-in `mouse_check_button_pressed` |
| `_Z26F_CheckMouseButtonReleasedR6RValueP9CInstanceS2_iPS_` | `0x004862F8` | ARM64 Trampoline | GML built-in `mouse_check_button_released` |
| `_Z25F_CheckMouseButton_CommonR6RValueP9CInstanceS2_ii` | `0x0044F440` | ARM64 Trampoline | Internal mouse check common |
| `_Z32F_CheckMouseButtonPressed_CommonR6RValueP9CInstanceS2_ii` | `0x0044F580` | ARM64 Trampoline | Internal mouse check pressed common |
| `_Z33F_CheckMouseButtonReleased_CommonR6RValueP9CInstanceS2_ii` | `0x0044F6C0` | ARM64 Trampoline | Internal mouse check released common |
| `_Z18F_DisplayMouseGetXR6RValueP9CInstanceS2_iPS_` | `0x0044228C` | ARM64 Trampoline | Center mouse X |
| `_Z18F_DisplayMouseGetYR6RValueP9CInstanceS2_iPS_` | `0x004422B8` | ARM64 Trampoline | Center mouse Y |
| `_Z10F_DrawTextR6RValueP9CInstanceS2_iPS_` | `0x00446CC8` | ARM64 Trampoline | Intercepts "1.4c" -> "1.4c - Android Port by @SanGraphic" |

---

## 4. Input Mapping & State Machine Pipeline

### 4.1 Button Mapping Table
| Physical Control | Android Keycode | Linux ScanCode | GM Constant | Index | GML Action |
|---|---|---|---|---|---|
| **A / South** | `KEYCODE_BUTTON_A` (96) | 304 | `gp_face1` | 0 | Confirm / Jump |
| **B / East** | `KEYCODE_BUTTON_B` (97) | 305 | `gp_face2` | 1 | Back / Cancel |
| **X / West** | `KEYCODE_BUTTON_X` (99) | 307 | `gp_face3` | 2 | Reload Weapon |
| **Y / North** | `KEYCODE_BUTTON_Y` (100) | 308 | `gp_face4` | 3 | Melee / Secondary |
| **LB / L1** | `KEYCODE_BUTTON_L1` (102) | 310 | `gp_shoulderl` | 4 | Left Bumper |
| **RB / R1** | `KEYCODE_BUTTON_R1` (103) | 311 | `gp_shoulderr` | 5 | Right Bumper |
| **LT / L2** | `KEYCODE_BUTTON_L2` (104) / `AXIS_LTRIGGER` | 312 | `gp_shoulderlb` | 6 | Slide (Hysteresis 0.20/0.08) |
| **RT / R2** | `KEYCODE_BUTTON_R2` (105) / `AXIS_RTRIGGER` | 313 | `gp_shoulderrb` | 7 | Shoot (Hysteresis 0.20/0.08 + 100ms Debounce) |
| **Select / Back** | `KEYCODE_BUTTON_SELECT` (109) / `KEYCODE_BACK` (4) | 314 | `gp_select` | 8 | Scoreboard / Map |
| **Start / Menu** | `KEYCODE_BUTTON_START` (108) / `KEYCODE_MENU` (82) | 315 | `gp_start` | 9 | Pause Menu |
| **L3 / LS Click** | `KEYCODE_BUTTON_THUMBL` (106) | 317 | `gp_stickl` | 10 | Sprint |
| **R3 / RS Click** | `KEYCODE_BUTTON_THUMBR` (107) | 318 | `gp_stickr` | 11 | Zoom / Melee |
| **D-Pad Up** | `KEYCODE_DPAD_UP` (19) | 103 | `gp_padu` | 12 | Menu Up |
| **D-Pad Down** | `KEYCODE_DPAD_DOWN` (20) | 108 | `gp_padd` | 13 | Menu Down |
| **D-Pad Left** | `KEYCODE_DPAD_LEFT` (21) | 105 | `gp_padl` | 14 | Menu Left |
| **D-Pad Right** | `KEYCODE_DPAD_RIGHT` (22) | 106 | `gp_padr` | 15 | Menu Right |
