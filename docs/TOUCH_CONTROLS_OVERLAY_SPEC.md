# Custom Touch Controls Overlay Specification — Post Void Android

This document outlines the complete architectural design and technical specification for adding a customizable, hardware-accelerated **Touch Controls Overlay** to Post Void Android without breaking or interfering with physical gamepad/controller input.

---

## 1. Executive Summary & Design Philosophy

Post Void is an ultra-fast FPS where twitch reflexes, instantaneous aiming, and rapid slide-canceling are essential to survival. The touch overlay is designed to:
1. **Preserve 1:1 Controller Parity**: Physical gamepads (Xbox, PlayStation, Switch, HID) must work simultaneously or take priority without mode thrashing, ghost IO clearing, or axis contention.
2. **Right-Screen Touch Aiming**: Use the right half of the screen as a seamless virtual trackpad for camera look/aiming.
3. **Dedicated Core Buttons**: Provide high-contrast, customizable touch buttons for **Shoot** and **Slide**.
4. **On-Screen Layout Customizer**: Allow the player to reposition buttons on the fly using a HUD Edit button located in the top-right corner (beside the pause button), with persistent layout storage in `SharedPreferences`.
5. **Zero Frame Drop at 120Hz**: Render the HUD on a lightweight hardware-accelerated Android Canvas layer placed above the `SurfaceView`, avoiding GameMaker CPU/GPU thread contention.

---

## 2. Touch Overlay UI Layout & Virtual Controls

```
┌─────────────────────────────────────────────────────────────────────────┐
│ [FPS: 120]                                           [Edit HUD] [Pause] │
│                                                                         │
│                                           ┌───────────────────────────┐ │
│                                           │                           │ │
│                                           │                           │ │
│                                           │     RIGHT-SCREEN AIM      │ │
│                                           │     TOUCHPAD REGION       │ │
│                                           │                           │ │
│                                           │                           │ │
│                                           │     (Swipe to Look/Turn)  │ │
│                                           │                           │ │
│                                           │   ┌───────┐   ┌───────┐   │ │
│                                           │   │ SLIDE │   │ SHOOT │   │ │
│                                           │   └───────┘   └───────┘   │ │
│                                           └───────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

### A. Right-Screen Aiming Touchpad Region
- **Boundary**: Active on the right $50\%$ of the screen ($X > \text{Width} \times 0.5$).
- **Touch Pointer Tracking**: Tracks the specific `pointerId` that initiated contact in this zone.
- **Delta-Based Movement**: Measures frame-to-frame displacement:
  $$\Delta X = X_{\text{current}} - X_{\text{prev}}$$
  $$\Delta Y = Y_{\text{current}} - Y_{\text{prev}}$$
- **Exclusion Zones**: Touches originating inside designated button hitboxes (Shoot / Slide / Pause / Edit) do NOT generate camera movement.

### B. Core Action Buttons
1. **Shoot Button** (Fire Weapon):
   - **Default Position**: Bottom-right ($X \approx 85\%$, $Y \approx 75\%$).
   - **GML Action**: Maps to `mb_left` / Action 12 / `gp_shoulderrb` (btn 7).
   - **100ms Cooldown Behavior**: Firing on press (`ACTION_DOWN`), held state while touching, with the 100ms debounce enforced so holding does not spam semi-auto weapons.
2. **Slide Button** (Slide / Crouch):
   - **Default Position**: Left of Shoot ($X \approx 70\%$, $Y \approx 80\%$).
   - **GML Action**: Maps to `gp_shoulderlb` (btn 6) / `vk_control` / `vk_shift`.
   - **Behavior**: Engaged on touch down, disengaged on touch release.

### C. HUD Layout Customizer (Top-Right Toggle)
- **Edit Toggle Button**: Positioned at top-right ($X \approx \text{Width} - 160\text{px}$, $Y \approx 40\text{px}$), adjacent to the Pause button.
- **Modes**:
  - **Play Mode (Locked)**: Buttons are translucent ($30\text{–}50\%$ opacity), non-movable, and process gameplay touch events.
  - **Edit Mode (Unlocked)**:
    - Button borders turn glowing cyan/yellow with an increased opacity ($90\%$).
    - A subtle alignment grid overlay appears.
    - Dragging a button updates its $(X, Y)$ coordinate with edge boundary clamping ($20\text{px}$ minimum padding from screen edges).
    - Pressing "Save / Lock" saves normalized coordinates $(X / \text{Width}, Y / \text{Height})$ to `SharedPreferences`.

---

## 3. Non-Breaking Controller Coexistence Architecture

### Why Typical Touch Ports Break Physical Gamepads
In standard GameMaker ports, touch events feed into `display_mouse_set()` or `io_btn_down` directly. When a physical gamepad is connected:
1. Touch coordinates fight with right-stick analog values, causing camera snapping or stuck angles.
2. GameMaker's internal `mouse_check_button()` overrides gamepad button states, resulting in dropped inputs.
3. `gh_clear_ghost_io()` clears all mouse/button buffers if not properly arbitrated.

### The Unified Arbitration Architecture

```
┌────────────────────────┐      ┌────────────────────────┐
│  Physical Controller   │      │   TouchOverlayView     │
│ (Xbox / PS5 / Switch)  │      │ (Aim / Shoot / Slide)  │
└───────────┬────────────┘      └───────────┬────────────┘
            │                               │
            │ MotionEvent / KeyEvent        │ Touch Delta / Button State
            ▼                               ▼
┌────────────────────────────────────────────────────────┐
│                   GamepadBridge.kt                     │
│  - Merges touch button state with physical button bitmask
│  - Accumulates touch delta into virtual mouse buffer   │
└──────────────────────────┬─────────────────────────────┘
                           │ JNI
┌──────────────────────────▼─────────────────────────────┐
│                 gamepad_hook.cpp                       │
│  - Sets gh_touch_active = true while touching          │
│  - Combined gh_mouse_x += (stick_delta + touch_delta)  │
│  - Retains 100ms trigger cooldown & centering math     │
└──────────────────────────┬─────────────────────────────┘
                           │
┌──────────────────────────▼─────────────────────────────┐
│                 libyoyo.so (GameMaker)                 │
└────────────────────────────────────────────────────────┘
```

### Key Coexistence Rules:
1. **Bitwise OR Button Merging**:
   - `step_held[btn]` is evaluated as:
     $$\text{is\_down} = \text{physical\_key\_down} \lor \text{physical\_trigger\_down} \lor \text{touch\_button\_down}$$
   - Releasing a touch button does not clear a held physical button, and vice-versa.
2. **Camera Delta Accumulation**:
   - Virtual mouse offset from screen center receives both analog stick displacement and touch swipe delta:
     $$X_{\text{offset}} = (\text{Stick}_X \times \text{Sensitivity}) + (\Delta X_{\text{touch}} \times \text{TouchSens})$$
     $$\text{gh\_mouse\_x} = \text{Center}_X + X_{\text{offset}}$$
   - At the end of each GML frame, `gh_mouse_x` and `gh_mouse_y` are reset to exact center ($1188.0, 540.0$).
3. **Automatic Visual Dimming**:
   - When a physical controller button or stick moves, `TouchOverlayView` can automatically fade its button opacity to $10\text{–}15\%$ or hide completely, and instantly fade back in when the screen is touched.

---

## 4. Technical Implementation Blueprint

### A. Kotlin Layer (`TouchOverlayView.kt`)
Placed directly above the `SurfaceView` in `activity_main.xml`:

```xml
<FrameLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent">

    <SurfaceView
        android:id="@+id/surface_view"
        android:layout_width="match_parent"
        android:layout_height="match_parent" />

    <com.postvoid.port.TouchOverlayView
        android:id="@+id/touch_overlay"
        android:layout_width="match_parent"
        android:layout_height="match_parent" />
</FrameLayout>
```

#### State & Pointer Tracking:
```kotlin
class TouchOverlayView(context: Context, attrs: AttributeSet?) : View(context, attrs) {
    private var isEditMode = false
    private var aimPointerId = -1
    private var lastAimX = 0f
    private var lastAimY = 0f

    data class TouchButton(
        var id: String,
        var label: String,
        var normX: Float, // Normalized 0.0 - 1.0
        var normY: Float,
        var radiusDp: Float,
        var gmButton: Int,
        var pointerId: Int = -1
    )

    private val buttons = mutableListOf(
        TouchButton("shoot", "FIRE", 0.86f, 0.75f, 40f, 7), // btn 7 (Shoot)
        TouchButton("slide", "SLIDE", 0.72f, 0.82f, 36f, 6) // btn 6 (Slide)
    )
}
```

#### Multi-Touch Event Processing:
```kotlin
override fun onTouchEvent(event: MotionEvent): Boolean {
    val actionIndex = event.actionIndex
    val pointerId = event.getPointerId(actionIndex)
    val x = event.getX(actionIndex)
    val y = event.getY(actionIndex)

    when (event.actionMasked) {
        MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
            // Check Edit HUD button
            if (isTouchInsideEditButton(x, y)) {
                toggleEditMode()
                return true
            }

            if (isEditMode) {
                // Select button for dragging
                selectButtonToDrag(pointerId, x, y)
                return true
            }

            // Check action buttons
            var hitButton = false
            for (btn in buttons) {
                if (btn.contains(x, y, width, height) && btn.pointerId == -1) {
                    btn.pointerId = pointerId
                    GamepadBridge.onButtonDown(0, getKeyCodeForGmBtn(btn.gmButton), 0)
                    triggerHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                    hitButton = true
                    break
                }
            }

            // If not button and on right side -> Aim Touchpad
            if (!hitButton && x > width * 0.5f && aimPointerId == -1) {
                aimPointerId = pointerId
                lastAimX = x
                lastAimY = y
            }
        }

        MotionEvent.ACTION_MOVE -> {
            if (isEditMode) {
                updateDraggedButtonPosition(event)
                invalidate()
                return true
            }

            // Process camera aiming delta
            val aimIdx = event.findPointerIndex(aimPointerId)
            if (aimIdx >= 0) {
                val curX = event.getX(aimIdx)
                val curY = event.getY(aimIdx)
                val dx = curX - lastAimX
                val dy = curY - lastAimY
                lastAimX = curX
                lastAimY = curY

                GamepadBridge.onRelativeLook(dx, dy)
            }
        }

        MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
            if (pointerId == aimPointerId) {
                aimPointerId = -1
            }

            for (btn in buttons) {
                if (btn.pointerId == pointerId) {
                    btn.pointerId = -1
                    GamepadBridge.onButtonUp(0, getKeyCodeForGmBtn(btn.gmButton), 0)
                }
            }
            if (isEditMode) {
                saveLayoutToPreferences()
            }
        }
    }
    return true
}
```

### B. Layout Persistence Model (`SharedPreferences`)
Layout positions are stored normalized ($0.0 \text{ to } 1.0$) so that changing display orientation, aspect ratio, or switching between devices restores the exact relative positioning:

```kotlin
private fun saveLayoutToPreferences() {
    val prefs = context.getSharedPreferences("postvoid_touch_layout", Context.MODE_PRIVATE)
    prefs.edit().apply {
        for (btn in buttons) {
            putFloat("${btn.id}_x", btn.normX)
            putFloat("${btn.id}_y", btn.normY)
        }
        apply()
    }
}

private fun loadLayoutFromPreferences() {
    val prefs = context.getSharedPreferences("postvoid_touch_layout", Context.MODE_PRIVATE)
    for (btn in buttons) {
        btn.normX = prefs.getFloat("${btn.id}_x", btn.normX)
        btn.normY = prefs.getFloat("${btn.id}_y", btn.normY)
    }
}
```

### C. JNI & C++ Integration (`gamepad_hook.cpp`)

Add a relative look handler in `gamepad_hook.cpp`:
```cpp
static std::atomic<double> gh_touch_dx(0.0);
static std::atomic<double> gh_touch_dy(0.0);

extern "C" JNIEXPORT void JNICALL
Java_com_postvoid_port_GamepadBridge_nativeRelativeLook(JNIEnv*, jclass, jfloat dx, jfloat dy) {
    gh_touch_dx.fetch_add((double)dx * 1.5, std::memory_order_relaxed);
    gh_touch_dy.fetch_add((double)dy * 1.5, std::memory_order_relaxed);
}
```

Inside `gh_display_mouse_get_x`:
```cpp
static void gh_display_mouse_get_x(RValue& ret, CInstance*, CInstance*, int, RValue*) {
    ret.kind = VALUE_REAL;
    double center = gh_surface_width.load(std::memory_order_relaxed) * 0.5;
    double t_dx   = gh_touch_dx.exchange(0.0, std::memory_order_acq_rel);
    ret.rvalue.val = center + t_dx;
}
```

---

## 5. Touch Feel, Latency & Haptic Tuning

| Parameter | Recommended Value | Description |
| :--- | :--- | :--- |
| **Touch Sampling Rate** | **120Hz / 240Hz** | Native OnePlus 12 touch digitizer polling |
| **Aim Sensitivity Multiplier** | `1.2x - 1.8x` | Adjustable via Settings |
| **Haptic Duration (Shoot)** | `15ms` (Strength: High) | Crisp click feedback on trigger |
| **Haptic Duration (Slide)** | `25ms` (Strength: Medium) | Tactile engagement sensation |
| **Button Radius** | `36dp - 44dp` | Ergonomic thumb contact zone |
| **Default Inactive Opacity** | `35%` | Maximum gameplay visibility |
| **Active Press Opacity** | `80%` | Immediate visual feedback |

---

## 6. Verification & Parity Checklist (When Implementing)

- [ ] **Simultaneous Multi-Touch**: Player can aim with right thumb while tapping Shoot or holding Slide without dropped inputs.
- [ ] **Controller Coexistence**: Connecting a Bluetooth Xbox/PS5 controller allows physical playing immediately without touch overlay interference.
- [ ] **HUD Edit Persistence**: Moving buttons, quitting the game, and reopening preserves button positions exactly.
- [ ] **No Auto-Fire on Hold**: Holding the touch Shoot button does not machine-gun semi-auto weapons (respects 100ms cooldown).
- [ ] **120Hz Fluidity**: Touch tracking produces zero frame stutter on Snapdragon 8 Gen 3 Adreno 750 pipeline.
