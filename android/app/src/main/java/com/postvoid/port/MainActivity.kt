package com.postvoid.port

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.hardware.input.InputManager
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.util.TypedValue
import android.view.Gravity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.util.Locale
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

import com.yoyogames.runner.RunnerJNILib

class MainActivity : AppCompatActivity(), GLSurfaceView.Renderer {

    private lateinit var rootLayout: FrameLayout
    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var fpsTextView: TextView
    private var touchOverlayView: TouchOverlayView? = null
    private var inputManager: InputManager? = null

    private val inputDeviceListener = object : InputManager.InputDeviceListener {
        override fun onInputDeviceAdded(deviceId: Int) {
            updateTouchOverlayVisibility()
        }
        override fun onInputDeviceRemoved(deviceId: Int) {
            updateTouchOverlayVisibility()
        }
        override fun onInputDeviceChanged(deviceId: Int) {
            updateTouchOverlayVisibility()
        }
    }

    @Volatile
    private var isEngineInitialized = false
    private var surfaceWidth = 1920
    private var surfaceHeight = 1080

    private var frameCount = 0
    private var renderedFrames = 0
    private var lastFpsUpdateTime = System.nanoTime()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        volumeControlStream = android.media.AudioManager.STREAM_MUSIC
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Extend edge-to-edge behind camera cutout/notch on high-aspect ratio displays (e.g. OnePlus 12)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }

        // Enable 120Hz display refresh rate on supported devices (OnePlus 12, etc.)
        enableHighRefreshRate()

        // Initialize active file logger for debug sessions (writes to game.log)
        GameLogger.init(this)
        GameLogger.logInputDevices(this)
        GamepadBridge.init(this)

        hideSystemUI()

        rootLayout = FrameLayout(this).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            setBackgroundColor(Color.BLACK)
        }
        setContentView(rootLayout)

        if (BYODManager.isGameContainerReady(this)) {
            initGameEngine()
        } else {
            showBYODSetupOverlay()
        }
    }

    private var byodSetupView: BYODSetupView? = null

    private fun showBYODSetupOverlay() {
        val setupView = BYODSetupView(this) {
            rootLayout.removeView(byodSetupView)
            byodSetupView = null
            initGameEngine()
        }
        byodSetupView = setupView
        rootLayout.addView(setupView)
    }

    private fun initGameEngine() {
        RunnerJNILib.ms_context = this
        RunnerJNILib.ms_assetManager = assets

        glSurfaceView = GLSurfaceView(this)
        rootLayout.addView(
            glSurfaceView,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        )

        // FPS counter in top-left (hidden by default)
        fpsTextView = TextView(this).apply {
            text = "60 FPS"
            visibility = android.view.View.GONE
            setTextColor(Color.parseColor("#39FF14"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setTypeface(Typeface.MONOSPACE, Typeface.BOLD)
            setShadowLayer(4f, 2f, 2f, Color.BLACK)
            setPadding(16, 8, 16, 8)
            background = GradientDrawable().apply {
                setColor(Color.parseColor("#80000000"))
                cornerRadius = 12f
            }
            val lp = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                gravity = Gravity.TOP or Gravity.START
                setMargins(48, 28, 0, 0)
            }
            layoutParams = lp
        }
        rootLayout.addView(fpsTextView)

        // On-screen touch controls overlay layer
        touchOverlayView = TouchOverlayView(this).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        }
        rootLayout.addView(touchOverlayView)

        inputManager = getSystemService(Context.INPUT_SERVICE) as? InputManager
        inputManager?.registerInputDeviceListener(inputDeviceListener, null)
        updateTouchOverlayVisibility()

        setupGLSurface()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK) return
        when (requestCode) {
            BYODSetupView.REQ_CODE_FILE -> {
                data?.data?.let { byodSetupView?.handleSelectedFileUri(it) }
            }
            BYODSetupView.REQ_CODE_FOLDER -> {
                data?.data?.let { byodSetupView?.handleSelectedFolderUri(it) }
            }
            BYODSetupView.REQ_CODE_MANAGE_STORAGE, BYODSetupView.REQ_CODE_STORAGE_PERM -> {
                byodSetupView?.refreshStorageAndScan()
            }
        }
    }

    private fun enableHighRefreshRate() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val modes = display?.supportedModes ?: emptyArray()
            val highRefreshMode = modes.maxByOrNull { it.refreshRate }
            if (highRefreshMode != null) {
                Log.i("MainActivity", "Requesting 120Hz display mode: ID=${highRefreshMode.modeId}, ${highRefreshMode.refreshRate}Hz")
                window.attributes = window.attributes.apply {
                    preferredDisplayModeId = highRefreshMode.modeId
                }
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            @Suppress("DEPRECATION")
            val modes = windowManager.defaultDisplay.supportedModes
            val maxMode = modes.maxByOrNull { it.refreshRate }
            if (maxMode != null) {
                window.attributes = window.attributes.apply {
                    preferredDisplayModeId = maxMode.modeId
                }
            }
        }
    }

    private fun calculateTargetResolution(): Pair<Int, Int> {
        val (screenW, screenH) = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val bounds = windowManager.currentWindowMetrics.bounds
            Pair(maxOf(bounds.width(), bounds.height()), minOf(bounds.width(), bounds.height()))
        } else {
            val dm = resources.displayMetrics
            Pair(maxOf(dm.widthPixels, dm.heightPixels), minOf(dm.widthPixels, dm.heightPixels))
        }

        val aspect = screenW.toFloat() / screenH.toFloat()

        // Cap height at 1080p equivalent to preserve battery and maintain 60 FPS while supporting any aspect ratio
        val targetH = if (screenH > 1080) 1080 else screenH
        var targetW = (targetH * aspect).toInt()
        // Ensure even width so integer division (w div 2) in GML equals (w * 0.5), preventing camera drift
        if (targetW % 2 != 0) {
            targetW--
        }

        Log.i("MainActivity", "True screen bounds: ${screenW}x${screenH} (aspect: $aspect) -> Target surface: ${targetW}x${targetH}")
        return Pair(targetW, targetH)
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setupGLSurface() {
        val (targetW, targetH) = calculateTargetResolution()
        surfaceWidth = targetW
        surfaceHeight = targetH
        touchOverlayView?.setSurfaceSize(targetW, targetH)

        glSurfaceView.holder.setFixedSize(targetW, targetH)
        glSurfaceView.setEGLConfigChooser(8, 8, 8, 8, 24, 8)
        glSurfaceView.setEGLContextClientVersion(2)
        glSurfaceView.setRenderer(this)
        glSurfaceView.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        glSurfaceView.isFocusable = true
        glSurfaceView.isFocusableInTouchMode = true
        glSurfaceView.requestFocus()

        glSurfaceView.setOnTouchListener { _, event ->
            onTouchEvent(event)
        }

        glSurfaceView.setOnGenericMotionListener { _, event ->
            onGenericMotionEvent(event)
        }
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        val fbo = IntArray(1)
        GLES20.glGetIntegerv(GLES20.GL_FRAMEBUFFER_BINDING, fbo, 0)
        RunnerJNILib.sDefaultFrameBuffer = fbo[0]
        Log.i("MainActivity", "Captured EGL default FrameBuffer: ${fbo[0]}")

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                glSurfaceView.holder.surface.setFrameRate(120.0f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT)
            } catch (_: Throwable) {}
        }

        val apkPath = BYODManager.getGameContainerPath(this)
        val savePath = BYODManager.getSaveDirectory(this).absolutePath
        RunnerJNILib.ms_context = this
        RunnerJNILib.ms_assetManager = assets

        Log.i("MainActivity", "Initializing libyoyo.so: apk=$apkPath, save=$savePath")
        if (!isEngineInitialized) {
            try {
                val res = RunnerJNILib.Startup(apkPath, savePath, packageName, 0)
                Log.i("MainActivity", "RunnerJNILib.Startup returned: $res")
                RunnerJNILib.initGLFuncs(1)
                RunnerJNILib.Resume(0)
                isEngineInitialized = true

                // Install PortMaster-style gamepad function overrides into libyoyo.so
                GamepadBridge.installHooks()
                GamepadBridge.setSurfaceSize(surfaceWidth, surfaceHeight)
            } catch (e: Throwable) {
                Log.e("MainActivity", "Error in RunnerJNILib.Startup: ${e.message}", e)
            }
        }
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        surfaceWidth = width
        surfaceHeight = height
        GamepadBridge.setSurfaceSize(width, height)
        touchOverlayView?.setSurfaceSize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        if (isEngineInitialized) {
            try {
                val ret = RunnerJNILib.Process(surfaceWidth, surfaceHeight, 0.0f, 0.0f, 0.0f, 0, 0, 0.0f)
                frameCount++
                renderedFrames++

                val now = System.nanoTime()
                val elapsedNanos = now - lastFpsUpdateTime
                if (elapsedNanos >= 500_000_000L) {
                    val fps = (renderedFrames * 1_000_000_000.0) / elapsedNanos
                    val fpsText = String.format(Locale.US, "%.0f FPS", fps)
                    fpsTextView.post {
                        fpsTextView.text = fpsText
                    }
                    renderedFrames = 0
                    lastFpsUpdateTime = now
                }

                if (frameCount % 120 == 0 || frameCount < 10) {
                    Log.i("MainActivity", "Frame $frameCount: Process returned $ret, size=${surfaceWidth}x${surfaceHeight}")
                }

                // Sync gamepad connection and refresh touch overlay state every 10 frames
                if (frameCount % 10 == 0) {
                    val hasGamepad = isPhysicalGamepadConnected()
                    GamepadBridge.setControllerConnected(hasGamepad)
                    val targetVis = if (hasGamepad) View.GONE else View.VISIBLE
                    if (touchOverlayView?.visibility != targetVis) {
                        touchOverlayView?.post {
                            touchOverlayView?.visibility = targetVis
                        }
                    }
                    touchOverlayView?.postInvalidate()
                }
            } catch (e: Throwable) {
                Log.e("MainActivity", "Error in RunnerJNILib.Process: ${e.message}", e)
            }
        }
    }

    fun forwardTouchToRunner(event: MotionEvent) {
        if (!isEngineInitialized) return
        val action = event.actionMasked
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        val viewW = glSurfaceView.width.toFloat()
        val viewH = glSurfaceView.height.toFloat()
        val scaleX = if (viewW > 0f) surfaceWidth.toFloat() / viewW else 1.0f
        val scaleY = if (viewH > 0f) surfaceHeight.toFloat() / viewH else 1.0f
        val x = event.getX(pointerIndex) * scaleX
        val y = event.getY(pointerIndex) * scaleY
        try {
            RunnerJNILib.TouchEvent(action, pointerId, x, y)
        } catch (_: Throwable) {}
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!isEngineInitialized) return super.onTouchEvent(event)
        val action = event.actionMasked
        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                GamepadBridge.setTouchActive(true)
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                GamepadBridge.setTouchActive(false)
            }
        }
        forwardTouchToRunner(event)
        return true
    }

    private fun isVolumeKeyEvent(keyCode: Int): Boolean {
        return keyCode == KeyEvent.KEYCODE_VOLUME_UP ||
               keyCode == KeyEvent.KEYCODE_VOLUME_DOWN ||
               keyCode == KeyEvent.KEYCODE_VOLUME_MUTE
    }

    private fun isGamepadKeyEvent(keyCode: Int, event: KeyEvent?): Boolean {
        if (isVolumeKeyEvent(keyCode)) return false
        if (KeyEvent.isGamepadButton(keyCode)) return true
        if (keyCode in 19..22 || keyCode in 96..110) return true
        if (event != null) {
            val sc = event.scanCode
            if (sc in 304..318 || sc in 103..108) return true
            val source = event.source
            if ((source and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                (source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK ||
                (source and InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD) {
                return true
            }
        }
        return false
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (isVolumeKeyEvent(event.keyCode)) {
            return super.dispatchKeyEvent(event)
        }
        if (!isEngineInitialized) return super.dispatchKeyEvent(event)
        val isGamepad = isGamepadKeyEvent(event.keyCode, event)
        if (isGamepad) {
            when (event.action) {
                KeyEvent.ACTION_DOWN -> {
                    if (event.repeatCount == 0) {
                        GamepadBridge.onButtonDown(0, event.keyCode, event.scanCode)
                    }
                }
                KeyEvent.ACTION_UP -> GamepadBridge.onButtonUp(0, event.keyCode, event.scanCode)
            }
            return true
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (isVolumeKeyEvent(keyCode)) {
            return super.onKeyDown(keyCode, event)
        }
        if (!isEngineInitialized) return super.onKeyDown(keyCode, event)
        if (isGamepadKeyEvent(keyCode, event)) {
            if (event?.repeatCount == 0) {
                GamepadBridge.onButtonDown(0, keyCode, event.scanCode)
            }
            return true
        }
        try {
            RunnerJNILib.KeyEvent(0, keyCode, event?.unicodeChar ?: 0, 0)
        } catch (_: Throwable) {}
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (isVolumeKeyEvent(keyCode)) {
            return super.onKeyUp(keyCode, event)
        }
        if (!isEngineInitialized) return super.onKeyUp(keyCode, event)
        if (isGamepadKeyEvent(keyCode, event)) {
            GamepadBridge.onButtonUp(0, keyCode, event?.scanCode ?: 0)
            return true
        }
        try {
            RunnerJNILib.KeyEvent(1, keyCode, event?.unicodeChar ?: 0, 0)
        } catch (_: Throwable) {}
        return super.onKeyUp(keyCode, event)
    }


    private var lastHatX = 0f
    private var lastHatY = 0f

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (!isEngineInitialized) return super.onGenericMotionEvent(event)
        val source = event.source
        val isJoystickOrGamepad = (source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK ||
                                  (source and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
        if (isJoystickOrGamepad) {
            val axisX  = event.getAxisValue(MotionEvent.AXIS_X)
            val axisY  = event.getAxisValue(MotionEvent.AXIS_Y)

            var axisZ  = event.getAxisValue(MotionEvent.AXIS_Z)
            if (axisZ == 0f) axisZ = event.getAxisValue(MotionEvent.AXIS_RX)

            var axisRZ = event.getAxisValue(MotionEvent.AXIS_RZ)
            if (axisRZ == 0f) axisRZ = event.getAxisValue(MotionEvent.AXIS_RY)

            var lT = event.getAxisValue(MotionEvent.AXIS_LTRIGGER)
            if (lT == 0f) lT = event.getAxisValue(MotionEvent.AXIS_BRAKE)

            var rT = event.getAxisValue(MotionEvent.AXIS_RTRIGGER)
            if (rT == 0f) rT = event.getAxisValue(MotionEvent.AXIS_GAS)

            val hatX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            val hatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)

            GamepadBridge.onAxis(0, 0, axisX)
            GamepadBridge.onAxis(0, 1, axisY)
            GamepadBridge.onAxis(0, 2, axisZ)
            GamepadBridge.onAxis(0, 3, axisRZ)
            GamepadBridge.onAxis(0, 4, lT)
            GamepadBridge.onAxis(0, 5, rT)

            if (hatX != lastHatX) {
                if (hatX < -0.5f) GamepadBridge.onButtonDown(0, KeyEvent.KEYCODE_DPAD_LEFT, 105)
                else if (lastHatX < -0.5f) GamepadBridge.onButtonUp(0, KeyEvent.KEYCODE_DPAD_LEFT, 105)

                if (hatX > 0.5f) GamepadBridge.onButtonDown(0, KeyEvent.KEYCODE_DPAD_RIGHT, 106)
                else if (lastHatX > 0.5f) GamepadBridge.onButtonUp(0, KeyEvent.KEYCODE_DPAD_RIGHT, 106)

                lastHatX = hatX
            }

            if (hatY != lastHatY) {
                if (hatY < -0.5f) GamepadBridge.onButtonDown(0, KeyEvent.KEYCODE_DPAD_UP, 103)
                else if (lastHatY < -0.5f) GamepadBridge.onButtonUp(0, KeyEvent.KEYCODE_DPAD_UP, 103)

                if (hatY > 0.5f) GamepadBridge.onButtonDown(0, KeyEvent.KEYCODE_DPAD_DOWN, 108)
                else if (lastHatY > 0.5f) GamepadBridge.onButtonUp(0, KeyEvent.KEYCODE_DPAD_DOWN, 108)

                lastHatY = hatY
            }

            return true
        }
        return super.onGenericMotionEvent(event)
    }


    override fun onResume() {
        super.onResume()
        enableHighRefreshRate()
        hideSystemUI()
        updateTouchOverlayVisibility()
        byodSetupView?.refreshStorageAndScan()
        if (::glSurfaceView.isInitialized) glSurfaceView.onResume()
        if (isEngineInitialized) try { RunnerJNILib.Resume(0) } catch (_: Throwable) {}
    }

    override fun onPause() {
        super.onPause()
        if (::glSurfaceView.isInitialized) glSurfaceView.onPause()
        if (isEngineInitialized) try { RunnerJNILib.Pause(0) } catch (_: Throwable) {}
    }

    override fun onDestroy() {
        super.onDestroy()
        inputManager?.unregisterInputDeviceListener(inputDeviceListener)
        GameLogger.close()
    }

    private fun isPhysicalGamepadConnected(): Boolean {
        try {
            val deviceIds = InputDevice.getDeviceIds()
            for (id in deviceIds) {
                val dev = InputDevice.getDevice(id) ?: continue
                if (dev.isVirtual) continue

                // Exclude sensors, touchscreens, and styluses that falsely report joystick/gamepad axes
                val name = dev.name?.lowercase(Locale.ROOT) ?: ""
                if (name.contains("sensor") || name.contains("touch") || name.contains("pen") || name.contains("stylus")) {
                    continue
                }

                val sources = dev.sources
                val isGamepadSource = (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                val isJoystickSource = (sources and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                if (!isGamepadSource && !isJoystickSource) continue

                // Strictly verify the device actually has physical gamepad action buttons
                val hasKeys = dev.hasKeys(
                    KeyEvent.KEYCODE_BUTTON_A,
                    KeyEvent.KEYCODE_BUTTON_B,
                    KeyEvent.KEYCODE_BUTTON_X,
                    KeyEvent.KEYCODE_BUTTON_Y,
                    KeyEvent.KEYCODE_BUTTON_START,
                    KeyEvent.KEYCODE_BUTTON_SELECT,
                    KeyEvent.KEYCODE_BUTTON_L1,
                    KeyEvent.KEYCODE_BUTTON_R1
                )
                if (hasKeys.any { it }) {
                    return true
                }
            }
        } catch (_: Throwable) {}
        return false
    }

    private fun updateTouchOverlayVisibility() {
        runOnUiThread {
            val hasGamepad = isPhysicalGamepadConnected()
            GamepadBridge.setControllerConnected(hasGamepad)
            touchOverlayView?.visibility = if (hasGamepad) View.GONE else View.VISIBLE
            touchOverlayView?.postInvalidate()
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            enableHighRefreshRate()
            hideSystemUI()
            updateTouchOverlayVisibility()
        }
    }

    override fun onConfigurationChanged(newConfig: android.content.res.Configuration) {
        super.onConfigurationChanged(newConfig)
        hideSystemUI()
        updateTouchOverlayVisibility()
    }

    private fun hideSystemUI() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
}

