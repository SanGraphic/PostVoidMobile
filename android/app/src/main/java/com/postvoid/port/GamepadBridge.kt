package com.postvoid.port

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.Log

/**
 * GamepadBridge — JNI bridge to libgamepad_hook.so.
 *
 * Mirrors PortMaster's gamepad state management:
 *  - libgamepad_hook.so intercepts Function_Add in libyoyo.so and replaces
 *    all gamepad_* GML built-ins with our own implementations.
 *  - Java calls nativeButtonDown/Up and nativeAxis each frame to push
 *    Android controller input into the C++ state arrays.
 *  - nativeTick() must be called once per frame to advance edge-trigger states
 *    (just-pressed → held, just-released → up).
 *  - nativeInstallHooks() must be called after libyoyo.so is loaded and
 *    RunnerJNILib.Startup() has returned, so Function_Add is available.
 */
object GamepadBridge {

    private const val TAG = "GamepadBridge"
    private var hooksInstalled = false
    private var vibrator: Vibrator? = null

    init {
        try {
            System.loadLibrary("gamepad_hook")
            Log.i(TAG, "libgamepad_hook.so loaded")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load libgamepad_hook.so: ${e.message}")
        }
    }

    fun init(context: Context) {
        try {
            vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vm = context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                vm?.defaultVibrator ?: (context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator)
            } else {
                @Suppress("DEPRECATION")
                context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
            }
        } catch (e: Throwable) {
            Log.w(TAG, "Vibrator init failed: ${e.message}")
        }
    }

    @JvmStatic
    fun onVibration(padIdx: Int, leftMotor: Double, rightMotor: Double) {
        val intensity = maxOf(leftMotor, rightMotor)
        if (intensity <= 0.0) {
            vibrator?.cancel()
            return
        }
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val amp = (intensity.coerceIn(0.0, 1.0) * 255).toInt().coerceIn(1, 255)
                val effect = VibrationEffect.createOneShot(40, amp)
                vibrator?.vibrate(effect)
            } else {
                @Suppress("DEPRECATION")
                vibrator?.vibrate(40)
            }
        } catch (_: Throwable) {}
    }

    /** Call after RunnerJNILib.Startup() returns — registers our gamepad_* replacements */
    fun installHooks() {
        try {
            nativeInstallHooks()
            hooksInstalled = nativeIsReady()
            Log.i(TAG, "Gamepad hooks installed: $hooksInstalled")
            // Mark slot 0 as connected so gamepad_is_connected(0) returns true
            if (hooksInstalled) {
                nativeSetConnected(0, true)
            }
        } catch (e: Throwable) {
            Log.e(TAG, "installHooks error: ${e.message}")
        }
    }

    /** Call once per frame (before/after Process()) to advance button edge states */
    fun tick() {
        if (hooksInstalled) {
            try { nativeTick() } catch (_: Throwable) {}
        }
    }

    // ── Android key → button index mapping (matches gamepad_hook.cpp) ──────────
    fun onButtonDown(padIdx: Int, androidKeycode: Int, scanCode: Int = 0) {
        if (!hooksInstalled) return
        try { nativeButtonDown(padIdx, androidKeycode, scanCode) } catch (_: Throwable) {}
    }

    fun onButtonUp(padIdx: Int, androidKeycode: Int, scanCode: Int = 0) {
        if (!hooksInstalled) return
        try { nativeButtonUp(padIdx, androidKeycode, scanCode) } catch (_: Throwable) {}
    }

    /**
     * axis indices:
     *  0 = Left Stick X (AXIS_X)
     *  1 = Left Stick Y (AXIS_Y)
     *  2 = Right Stick X (AXIS_Z)
     *  3 = Right Stick Y (AXIS_RZ)
     *  4 = Left Trigger (AXIS_LTRIGGER / AXIS_BRAKE)
     *  5 = Right Trigger (AXIS_RTRIGGER / AXIS_GAS)
     */
    fun onAxis(padIdx: Int, axisIdx: Int, value: Float) {
        if (!hooksInstalled) return
        try { nativeAxis(padIdx, axisIdx, value) } catch (_: Throwable) {}
    }

    fun setTouchActive(active: Boolean) {
        if (!hooksInstalled) return
        try { nativeSetTouchActive(active) } catch (_: Throwable) {}
    }

    fun setSurfaceSize(width: Int, height: Int) {
        if (!hooksInstalled) return
        try { nativeSetSurfaceSize(width, height) } catch (_: Throwable) {}
    }

    fun onRelativeLook(dx: Float, dy: Float) {
        if (!hooksInstalled) return
        try { nativeRelativeLook(dx, dy) } catch (_: Throwable) {}
    }

    fun isInGame(): Boolean {
        if (!hooksInstalled) return false
        return try { nativeIsInGame() } catch (_: Throwable) { false }
    }

    fun setControllerConnected(connected: Boolean) {
        if (!hooksInstalled) return
        try { nativeSetControllerConnected(connected) } catch (_: Throwable) {}
    }

    // ── Native methods in libgamepad_hook.so ──────────────────────────────────
    @JvmStatic external fun nativeInstallHooks()
    @JvmStatic external fun nativeIsReady(): Boolean
    @JvmStatic external fun nativeTick()
    @JvmStatic external fun nativeSetConnected(padIdx: Int, connected: Boolean)
    @JvmStatic external fun nativeSetControllerConnected(connected: Boolean)
    @JvmStatic external fun nativeSetTouchActive(active: Boolean)
    @JvmStatic external fun nativeIsInGame(): Boolean
    @JvmStatic external fun nativeSetSurfaceSize(width: Int, height: Int)
    @JvmStatic external fun nativeButtonDown(padIdx: Int, androidKeycode: Int, scanCode: Int)
    @JvmStatic external fun nativeButtonUp(padIdx: Int, androidKeycode: Int, scanCode: Int)
    @JvmStatic external fun nativeAxis(padIdx: Int, axisIdx: Int, value: Float)
    @JvmStatic external fun nativeRelativeLook(dx: Float, dy: Float)
}
