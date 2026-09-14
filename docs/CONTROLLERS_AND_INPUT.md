# Controllers & Input Subsystem Documentation — Post Void Android

This document details the controller mapping, analog stick math, sensitivity calibration, timing delays, and touch interface architecture in the Post Void Android port.

---

## 1. Controller Mapping Table

All physical gamepads (Bluetooth, USB-OTG, and integrated controller docks like Razer Kishi / Backbone) are handled through Android's standard `InputEvent` API and intercepted at the native layer.

| Game Action | Gamepad Button | Direct Key / GML Action |
| :--- | :--- | :--- |
| **Move Forward** | Left Stick Up / D-Pad Up | `ord("W")` |
| **Move Backward** | Left Stick Down / D-Pad Down | `ord("S")` |
| **Strafe Left** | Left Stick Left / D-Pad Left | `ord("A")` |
| **Strafe Right** | Left Stick Right / D-Pad Right | `ord("D")` |
| **Look / Turn** | Right Analog Stick | Emulated Mouse Delta |
| **Fire Weapon** | Right Trigger (`AXIS_RTRIGGER` / `AXIS_GAS`) | `mb_left` / Action 12 |
| **Knife Melee** | Left Trigger (`AXIS_LTRIGGER` / `AXIS_BRAKE`) | `mb_right` / Knife |
| **Jump** | A / Cross (`BUTTON_A`) | `vk_space` |
| **Slide / Crouch** | B / Circle (`BUTTON_B`) or Left Stick Click (`BUTTON_THUMBL`) | `vk_control` / `vk_shift` |
| **Reload** | X / Square (`BUTTON_X`) | `ord("R")` |
| **Pause / Back** | Start / Options / Menu (`BUTTON_START`) | `vk_escape` |

---

## 2. Analog Aiming Math & Centering Hook

In Post Void's player script (`gml_Object_obj_player_Step_0`), horizontal turning is calculated by measuring the offset of the cursor from the center of the display:

$$\text{\_xdelta} = \text{scr\_input\_display\_mouse\_get\_x}() - (\text{display\_get\_width}() \times 0.5)$$
$$\text{direction} -= (\text{\_xdelta} \times 0.125 \times \text{\_mouse\_sense\_h})$$

### The Android Issue & Fix:
Without hooking, `display_get_width()` on Android defaulted to `480.0` or `1920.0`, while mouse coordinates were centered at the physical surface ($2376 \times 1080$), causing $\text{\_xdelta} \neq 0$ and resulting in violent leftward camera spinning.

### In `gamepad_hook.cpp`:
1. `display_get_width` and `window_get_width` return `gh_surface_width` ($2376.0$).
2. `display_mouse_get_x` and `window_mouse_get_x` return `gh_mouse_x` ($1188.0$).
3. When the right analog stick is deflected, the displacement is added to `gh_mouse_x` / `gh_mouse_y`.
4. At the end of each engine frame, `gh_mouse_x` and `gh_mouse_y` are reset to the exact center coordinates:
   ```cpp
   gh_mouse_x.store(gh_surface_width.load() * 0.5f);
   gh_mouse_y.store(gh_surface_height.load() * 0.5f);
   ```

---

## 3. Right Trigger Press Cooldown & Holding Behavior

### Firing Behavior & Holding Parity
- **Semi-Automatic Weapons (Handgun / Pistol / Shotgun)**:
  - Firing requires an active `pressed` event (`scr_input_shoot_pressed()`).
  - **Holding the Trigger**: Sets `step_held[7] = true` and `step_pressed[7] = false` after frame 1. The gun will **NOT** spam bullets or auto-fire while held.
  - OS-level `KeyEvent` repeats (`repeatCount > 0`) are explicitly suppressed so holding R2 / RT does not generate synthetic button presses.
- **Automatic Weapons (Uzi)**:
  - Continuously fires based on the game's internal weapon timer while `step_held[7]` is `true`.
- **100ms Press Cooldown**:
  - Enforces a minimum interval of **100ms** between distinct trigger pulls to prevent trigger bounce and ensure consistent semi-auto cadence:
  ```cpp
  if (now_ms - gh_gamepads[padIdx].last_press_time_ms[7] < 100ULL) {
      return; // Cooldown: ignore rapid chatter until 100ms has elapsed since previous press
  }
  ```

---

## 4. Supported Controller Architectures

1. **Xbox Controllers**: Xbox Wireless, Xbox Elite Series 2 (via Bluetooth & Type-C).
2. **PlayStation Controllers**: DualShock 4 (CUH-ZCT2), DualSense (CFI-ZCT1W).
3. **Nintendo Switch**: Switch Pro Controller, Joy-Con (L+R pair).
4. **Mobile Docks**: Razer Kishi V1/V2, Backbone One, GameSir G8 Galileo.
