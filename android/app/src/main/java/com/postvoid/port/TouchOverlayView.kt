package com.postvoid.port

import android.content.Context
import android.graphics.*
import android.os.Build
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min

class TouchOverlayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val density = context.resources.displayMetrics.density
    private var vibrator: Vibrator? = null

    init {
        try {
            vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vm = context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                vm?.defaultVibrator ?: (context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator)
            } else {
                @Suppress("DEPRECATION")
                context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
            }
        } catch (_: Throwable) {}
    }

    data class OverlayButton(
        val id: String,
        val label: String,
        var normX: Float,
        var normY: Float,
        val defaultNormX: Float,
        val defaultNormY: Float,
        val baseRadiusDp: Float,
        var sizeScale: Float = 1.0f,
        val drawableRes: Int,
        val gmBtn: Int,
        val keyCode: Int,
        val scanCode: Int,
        var pointerId: Int = -1,
        var origBitmap: Bitmap? = null,
        var scaledBitmap: Bitmap? = null
    ) {
        fun getPixelX(width: Int): Float = normX * width
        fun getPixelY(height: Int): Float = normY * height
        fun getPixelRadius(density: Float): Float = baseRadiusDp * sizeScale * density

        fun updateScaledBitmap(density: Float) {
            val bmp = origBitmap ?: return
            val r = getPixelRadius(density)
            val targetSize = max(32, (r * 2f).toInt())
            if (scaledBitmap == null || scaledBitmap?.width != targetSize || scaledBitmap?.height != targetSize) {
                try {
                    scaledBitmap = Bitmap.createScaledBitmap(bmp, targetSize, targetSize, true)
                } catch (_: Throwable) {
                    scaledBitmap = bmp
                }
            }
        }

        fun contains(x: Float, y: Float, width: Int, height: Int, density: Float, extraSlop: Float = 0f): Boolean {
            val px = getPixelX(width)
            val py = getPixelY(height)
            val r = getPixelRadius(density) + extraSlop
            return hypot(x - px, y - py) <= r
        }

        fun resetToDefaults() {
            normX = defaultNormX
            normY = defaultNormY
            sizeScale = 1.0f
        }
    }

    private val buttons = listOf(
        OverlayButton("shoot", "FIRE", 0.86f, 0.72f, 0.86f, 0.72f, 44f, 1.0f, R.drawable.ic_touch_shoot, 7, 105, 313),
        OverlayButton("slide", "SLIDE", 0.72f, 0.82f, 0.72f, 0.82f, 38f, 1.0f, R.drawable.ic_touch_slide, 6, 104, 312),
        OverlayButton("jump", "JUMP", 0.86f, 0.48f, 0.86f, 0.48f, 38f, 1.0f, R.drawable.ic_touch_jump, 0, 96, 304),
        OverlayButton("reload", "RELOAD", 0.72f, 0.60f, 0.72f, 0.60f, 36f, 1.0f, R.drawable.ic_touch_reload, 2, 99, 307),
        OverlayButton("pause", "PAUSE", 0.95f, 0.08f, 0.95f, 0.08f, 26f, 1.0f, R.drawable.ic_touch_pause, 9, 108, 315),
        OverlayButton("edit_hud", "EDIT", 0.87f, 0.08f, 0.87f, 0.08f, 26f, 1.0f, R.drawable.ic_touch_edit_hud, -1, 0, 0)
    )

    private var isEditMode = false
    private var selectedButton: OverlayButton? = null
    private var draggedButton: OverlayButton? = null
    private var dragPointerId = -1
    private var isDraggingSlider = false
    private var sliderPointerId = -1

    private var aimPointerId = -1
    private var lastAimX = 0f
    private var lastAimY = 0f
    private val lookSensitivity = 1.35f
    private var surfaceWidth = 2376
    private var surfaceHeight = 1080

    fun setSurfaceSize(w: Int, h: Int) {
        if (w > 0 && h > 0) {
            surfaceWidth = w
            surfaceHeight = h
        }
    }

    private var movePointerId = -1
    private var joyOriginX = 0f
    private var joyOriginY = 0f
    private var joyCurrentX = 0f
    private var joyCurrentY = 0f
    private val joyRadiusDp = 58f
    private val joyDeadzoneDp = 6f

    private val dstRect = RectF()
    private val buttonNormalPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        alpha = (255 * 0.52f).toInt()
        isFilterBitmap = true
    }
    private val buttonPressedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        alpha = (255 * 0.95f).toInt()
        isFilterBitmap = true
    }
    private val editGlowPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.5f * density
        color = Color.argb(200, 0, 255, 255)
    }
    private val editSelectedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3.5f * density
        color = Color.YELLOW
    }
    private val editGridPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 1f * density
        color = Color.argb(35, 255, 255, 255)
    }
    private val bannerBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(220, 10, 10, 10)
    }
    private val bannerBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
        color = Color.WHITE
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.YELLOW
        textSize = 13f * density
        typeface = Typeface.DEFAULT_BOLD
        textAlign = Paint.Align.CENTER
    }
    private val resetBtnBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.BLACK
    }
    private val resetBtnBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.5f * density
        color = Color.rgb(255, 50, 80)
    }
    private val resetBtnTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 14f * density
        typeface = Typeface.DEFAULT_BOLD
        textAlign = Paint.Align.CENTER
    }
    private val sliderCardBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(235, 15, 15, 15)
    }
    private val sliderCardBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
        color = Color.WHITE
    }
    private val sliderTrackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f * density
        strokeCap = Paint.Cap.ROUND
        color = Color.argb(180, 255, 255, 255)
    }
    private val sliderThumbPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.YELLOW
    }
    private val sliderThumbBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
        color = Color.BLACK
    }
    private val sliderBtnBgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(220, 30, 30, 30)
    }
    private val sliderBtnBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 1.5f * density
        color = Color.WHITE
    }
    private val sliderBtnTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 18f * density
        typeface = Typeface.DEFAULT_BOLD
        textAlign = Paint.Align.CENTER
    }
    private val joyBasePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 2.5f * density
        color = Color.argb(90, 255, 255, 255)
    }
    private val joyKnobPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
        color = Color.argb(140, 255, 255, 255)
    }

    init {
        loadLayout()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        loadBitmaps()
    }

    private fun loadBitmaps() {
        for (btn in buttons) {
            try {
                btn.origBitmap = BitmapFactory.decodeResource(resources, btn.drawableRes)
                btn.updateScaledBitmap(density)
            } catch (_: Throwable) {}
        }
    }

    private fun saveLayout() {
        val prefs = context.getSharedPreferences("postvoid_touch_layout", Context.MODE_PRIVATE)
        prefs.edit().apply {
            for (btn in buttons) {
                putFloat("${btn.id}_x", btn.normX)
                putFloat("${btn.id}_y", btn.normY)
                putFloat("${btn.id}_scale", btn.sizeScale)
            }
            apply()
        }
    }

    private fun loadLayout() {
        val prefs = context.getSharedPreferences("postvoid_touch_layout", Context.MODE_PRIVATE)
        for (btn in buttons) {
            btn.normX = prefs.getFloat("${btn.id}_x", btn.defaultNormX)
            btn.normY = prefs.getFloat("${btn.id}_y", btn.defaultNormY)
            btn.sizeScale = prefs.getFloat("${btn.id}_scale", 1.0f).coerceIn(0.5f, 2.0f)
            btn.updateScaledBitmap(density)
        }
    }

    private fun resetToDefaults() {
        for (btn in buttons) {
            btn.resetToDefaults()
            btn.updateScaledBitmap(density)
        }
        selectedButton = null
        saveLayout()
        triggerHaptic()
        invalidate()
    }

    private fun triggerHaptic() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                vibrator?.vibrate(VibrationEffect.createOneShot(15, 180))
            } else {
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
            }
        } catch (_: Throwable) {}
    }

    fun resetAllTouches() {
        if (movePointerId != -1) {
            movePointerId = -1
            GamepadBridge.onAxis(0, 0, 0f)
            GamepadBridge.onAxis(0, 1, 0f)
        }
        aimPointerId = -1
        dragPointerId = -1
        draggedButton = null
        isDraggingSlider = false
        sliderPointerId = -1
        for (btn in buttons) {
            if (btn.pointerId != -1) {
                btn.pointerId = -1
                GamepadBridge.onButtonUp(0, btn.keyCode, btn.scanCode)
            }
        }
        GamepadBridge.setTouchActive(false)
        postInvalidate()
    }

    override fun onVisibilityChanged(changedView: View, visibility: Int) {
        super.onVisibilityChanged(changedView, visibility)
        if (visibility != VISIBLE) {
            resetAllTouches()
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        resetAllTouches()
    }

    private fun getResetButtonRect(): RectF {
        val btnWidth = 100f * density
        val btnHeight = 38f * density
        val r = width - 16f * density
        val b = height - 16f * density
        return RectF(r - btnWidth, b - btnHeight, r, b)
    }

    private fun getSliderBarRect(): RectF {
        val barWidth = min(width * 0.48f, 320f * density)
        val barHeight = 58f * density
        val left = (width - barWidth) / 2f
        val b = height - 16f * density
        return RectF(left, b - barHeight, left + barWidth, b)
    }

    private fun getSliderMinusRect(barRect: RectF): RectF {
        val size = 30f * density
        val left = barRect.left + 12f * density
        val top = barRect.centerY() - size / 2f + 8f * density
        return RectF(left, top, left + size, top + size)
    }

    private fun getSliderPlusRect(barRect: RectF): RectF {
        val size = 30f * density
        val right = barRect.right - 12f * density
        val top = barRect.centerY() - size / 2f + 8f * density
        return RectF(right - size, top, right, top + size)
    }

    private fun getSliderTrackRect(barRect: RectF): RectF {
        val minus = getSliderMinusRect(barRect)
        val plus = getSliderPlusRect(barRect)
        val trackLeft = minus.right + 12f * density
        val trackRight = plus.left - 12f * density
        val trackY = minus.centerY()
        return RectF(trackLeft, trackY - 2f * density, trackRight, trackY + 2f * density)
    }

    private fun updateSliderFromTouchX(touchX: Float) {
        val btn = selectedButton ?: return
        val track = getSliderTrackRect(getSliderBarRect())
        val frac = ((touchX - track.left) / track.width()).coerceIn(0f, 1f)
        btn.sizeScale = (0.50f + frac * 1.50f).coerceIn(0.50f, 2.00f)
        btn.updateScaledBitmap(density)
        invalidate()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (visibility != VISIBLE) return false

        val actionIndex = event.actionIndex
        val pointerId = event.getPointerId(actionIndex)
        val x = event.getX(actionIndex)
        val y = event.getY(actionIndex)
        val inGame = GamepadBridge.isInGame()

        if (isEditMode) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    handleEditTouchDown(pointerId, x, y)
                }
                MotionEvent.ACTION_MOVE -> {
                    handleEditTouchMove(event)
                }
                MotionEvent.ACTION_POINTER_UP -> {
                    handleEditTouchUp(pointerId)
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    draggedButton = null
                    dragPointerId = -1
                    isDraggingSlider = false
                    sliderPointerId = -1
                    saveLayout()
                }
            }
            invalidate()
            return true
        }

        if (inGame) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    resetAllTouches()
                    handleGameTouchDown(pointerId, x, y)
                    invalidate()
                }
                MotionEvent.ACTION_POINTER_DOWN -> {
                    handleGameTouchDown(pointerId, x, y)
                    invalidate()
                }
                MotionEvent.ACTION_MOVE -> {
                    if (handleGameTouchMove(event)) {
                        invalidate()
                    }
                }
                MotionEvent.ACTION_POINTER_UP -> {
                    handleGameTouchUp(pointerId)
                    invalidate()
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    resetAllTouches()
                    invalidate()
                }
            }
            return true
        }

        val editBtn = buttons.first { it.id == "edit_hud" }
        val isInsideEdit = editBtn.contains(x, y, width, height, density, 16f * density)

        if (isInsideEdit) {
            if (event.actionMasked == MotionEvent.ACTION_DOWN) {
                isEditMode = true
                selectedButton = editBtn
                triggerHaptic()
                invalidate()
            }
            return true
        }

        return false
    }

    private fun handleEditTouchDown(pointerId: Int, x: Float, y: Float) {
        val editBtn = buttons.first { it.id == "edit_hud" }
        if (editBtn.contains(x, y, width, height, density, 14f * density)) {
            isEditMode = false
            selectedButton = null
            saveLayout()
            triggerHaptic()
            return
        }

        val resetRect = getResetButtonRect()
        if (resetRect.contains(x, y)) {
            resetToDefaults()
            return
        }

        if (selectedButton != null) {
            val barRect = getSliderBarRect()
            if (barRect.contains(x, y)) {
                val minusRect = getSliderMinusRect(barRect)
                val plusRect = getSliderPlusRect(barRect)
                val trackRect = getSliderTrackRect(barRect)

                if (minusRect.contains(x, y)) {
                    selectedButton?.let {
                        it.sizeScale = max(0.50f, it.sizeScale - 0.05f)
                        it.updateScaledBitmap(density)
                        triggerHaptic()
                    }
                    return
                }
                if (plusRect.contains(x, y)) {
                    selectedButton?.let {
                        it.sizeScale = min(2.00f, it.sizeScale + 0.05f)
                        it.updateScaledBitmap(density)
                        triggerHaptic()
                    }
                    return
                }
                if (x >= trackRect.left - 10f * density && x <= trackRect.right + 10f * density) {
                    isDraggingSlider = true
                    sliderPointerId = pointerId
                    updateSliderFromTouchX(x)
                    triggerHaptic()
                    return
                }
                return
            }
        }

        for (btn in buttons) {
            if (btn.contains(x, y, width, height, density, 16f * density)) {
                selectedButton = btn
                draggedButton = btn
                dragPointerId = pointerId
                triggerHaptic()
                return
            }
        }

        selectedButton = null
    }

    private fun handleEditTouchMove(event: MotionEvent) {
        if (isDraggingSlider && sliderPointerId != -1) {
            val pIdx = event.findPointerIndex(sliderPointerId)
            if (pIdx >= 0) {
                updateSliderFromTouchX(event.getX(pIdx))
            }
        }

        if (draggedButton != null && dragPointerId != -1) {
            val dragIdx = event.findPointerIndex(dragPointerId)
            if (dragIdx >= 0) {
                val cx = event.getX(dragIdx).coerceIn(30f * density, width - 30f * density)
                val cy = event.getY(dragIdx).coerceIn(30f * density, height - 30f * density)
                draggedButton?.normX = cx / width
                draggedButton?.normY = cy / height
            }
        }
    }

    private fun handleEditTouchUp(pointerId: Int) {
        if (pointerId == dragPointerId) {
            draggedButton = null
            dragPointerId = -1
            saveLayout()
        }
        if (pointerId == sliderPointerId) {
            isDraggingSlider = false
            sliderPointerId = -1
            saveLayout()
        }
    }

    private fun handleGameTouchDown(pointerId: Int, x: Float, y: Float) {
        GamepadBridge.setTouchActive(true)

        val editBtn = buttons.first { it.id == "edit_hud" }
        if (editBtn.contains(x, y, width, height, density, 12f * density)) {
            isEditMode = true
            selectedButton = editBtn
            triggerHaptic()
            return
        }

        for (btn in buttons) {
            if (btn.id != "edit_hud" && btn.contains(x, y, width, height, density, 8f * density) && btn.pointerId == -1) {
                btn.pointerId = pointerId
                triggerHaptic()
                GamepadBridge.onButtonDown(0, btn.keyCode, btn.scanCode)
                return
            }
        }

        if (x < width * 0.45f && movePointerId == -1) {
            movePointerId = pointerId
            joyOriginX = x
            joyOriginY = y
            joyCurrentX = x
            joyCurrentY = y
            return
        }

        if (x >= width * 0.45f && aimPointerId == -1) {
            aimPointerId = pointerId
            lastAimX = x
            lastAimY = y
            return
        }
    }

    private fun handleGameTouchMove(event: MotionEvent): Boolean {
        var visualChanged = false

        if (movePointerId != -1) {
            val moveIdx = event.findPointerIndex(movePointerId)
            if (moveIdx >= 0) {
                val newX = event.getX(moveIdx)
                val newY = event.getY(moveIdx)
                if (hypot(newX - joyCurrentX, newY - joyCurrentY) >= 1.0f * density) {
                    joyCurrentX = newX
                    joyCurrentY = newY
                    visualChanged = true
                }

                val dx = joyCurrentX - joyOriginX
                val dy = joyCurrentY - joyOriginY
                val dist = hypot(dx, dy)
                val maxDist = joyRadiusDp * density
                val deadzone = joyDeadzoneDp * density

                if (dist > deadzone) {
                    val clampedDist = min(dist, maxDist)
                    val normX = (dx / dist) * (clampedDist / maxDist)
                    val normY = (dy / dist) * (clampedDist / maxDist)
                    GamepadBridge.onAxis(0, 0, normX)
                    GamepadBridge.onAxis(0, 1, normY)
                } else {
                    GamepadBridge.onAxis(0, 0, 0f)
                    GamepadBridge.onAxis(0, 1, 0f)
                }
            } else {
                movePointerId = -1
                GamepadBridge.onAxis(0, 0, 0f)
                GamepadBridge.onAxis(0, 1, 0f)
                visualChanged = true
            }
        }

        if (aimPointerId != -1) {
            val aimIdx = event.findPointerIndex(aimPointerId)
            if (aimIdx >= 0) {
                val curX = event.getX(aimIdx)
                val curY = event.getY(aimIdx)
                val scaleX = if (width > 0) surfaceWidth.toFloat() / width.toFloat() else 1.0f
                val scaleY = if (height > 0) surfaceHeight.toFloat() / height.toFloat() else 1.0f
                val dx = (curX - lastAimX) * lookSensitivity * scaleX
                val dy = (curY - lastAimY) * lookSensitivity * scaleY
                lastAimX = curX
                lastAimY = curY
                GamepadBridge.onRelativeLook(dx, dy)
            } else {
                aimPointerId = -1
            }
        }

        for (btn in buttons) {
            if (btn.pointerId != -1) {
                val pIdx = event.findPointerIndex(btn.pointerId)
                if (pIdx < 0) {
                    btn.pointerId = -1
                    GamepadBridge.onButtonUp(0, btn.keyCode, btn.scanCode)
                    visualChanged = true
                }
            }
        }

        return visualChanged
    }

    private fun handleGameTouchUp(pointerId: Int) {
        if (pointerId == movePointerId) {
            movePointerId = -1
            GamepadBridge.onAxis(0, 0, 0f)
            GamepadBridge.onAxis(0, 1, 0f)
        }

        if (pointerId == aimPointerId) {
            aimPointerId = -1
        }

        for (btn in buttons) {
            if (btn.pointerId == pointerId) {
                btn.pointerId = -1
                GamepadBridge.onButtonUp(0, btn.keyCode, btn.scanCode)
            }
        }

        val anyActive = movePointerId != -1 || aimPointerId != -1 || buttons.any { it.pointerId != -1 }
        if (!anyActive) {
            GamepadBridge.setTouchActive(false)
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (visibility != VISIBLE) return

        val inGame = GamepadBridge.isInGame()

        if (isEditMode) {
            drawEditGrid(canvas)
            drawResetButton(canvas)
            if (selectedButton != null) {
                drawSizeSlider(canvas)
            }
        }

        if (inGame && !isEditMode && movePointerId != -1) {
            drawJoystick(canvas)
        }

        for (btn in buttons) {
            if (!inGame && !isEditMode && btn.id != "edit_hud") {
                continue
            }

            val px = btn.getPixelX(width)
            val py = btn.getPixelY(height)
            val r = btn.getPixelRadius(density)
            val isPressed = btn.pointerId != -1
            val paint = if (isPressed) buttonPressedPaint else buttonNormalPaint

            canvas.save()
            if (isPressed) {
                canvas.scale(0.90f, 0.90f, px, py)
            }

            val bmp = btn.scaledBitmap ?: btn.origBitmap
            if (bmp != null) {
                dstRect.set(px - r, py - r, px + r, py + r)
                canvas.drawBitmap(bmp, null, dstRect, paint)
            } else {
                canvas.drawCircle(px, py, r, paint)
            }

            if (isEditMode) {
                if (selectedButton == btn) {
                    canvas.drawCircle(px, py, r + 5f * density, editSelectedPaint)
                    drawSelectionBrackets(canvas, px, py, r + 8f * density)
                } else {
                    canvas.drawCircle(px, py, r + 3f * density, editGlowPaint)
                }
            }

            canvas.restore()
        }
    }

    private fun drawEditGrid(canvas: Canvas) {
        val step = 75f * density
        var x = 0f
        while (x < width) {
            canvas.drawLine(x, 0f, x, height.toFloat(), editGridPaint)
            x += step
        }
        var y = 0f
        while (y < height) {
            canvas.drawLine(0f, y, width.toFloat(), y, editGridPaint)
            y += step
        }

        val bannerW = 460f * density
        val bannerH = 32f * density
        val bannerL = (width - bannerW) / 2f
        val bannerT = 16f * density
        val bannerRect = RectF(bannerL, bannerT, bannerL + bannerW, bannerT + bannerH)
        canvas.drawRoundRect(bannerRect, 6f * density, 6f * density, bannerBgPaint)
        canvas.drawRoundRect(bannerRect, 6f * density, 6f * density, bannerBorderPaint)
        canvas.drawText("EDIT HUD  -  DRAG TO MOVE  -  TAP TO RESIZE", width * 0.5f, bannerT + 21f * density, textPaint)
    }

    private fun drawResetButton(canvas: Canvas) {
        val rect = getResetButtonRect()
        canvas.drawRoundRect(rect, 8f * density, 8f * density, resetBtnBgPaint)
        canvas.drawRoundRect(rect, 8f * density, 8f * density, resetBtnBorderPaint)
        canvas.drawText("RESET", rect.centerX(), rect.centerY() + 5f * density, resetBtnTextPaint)
    }

    private fun drawSizeSlider(canvas: Canvas) {
        val btn = selectedButton ?: return
        val barRect = getSliderBarRect()

        canvas.drawRoundRect(barRect, 10f * density, 10f * density, sliderCardBgPaint)
        canvas.drawRoundRect(barRect, 10f * density, 10f * density, sliderCardBorderPaint)

        val labelText = "[ ${btn.label} ]  SIZE: ${(btn.sizeScale * 100).toInt()}%"
        canvas.drawText(labelText, barRect.centerX(), barRect.top + 16f * density, textPaint)

        val minusRect = getSliderMinusRect(barRect)
        canvas.drawRoundRect(minusRect, 6f * density, 6f * density, sliderBtnBgPaint)
        canvas.drawRoundRect(minusRect, 6f * density, 6f * density, sliderBtnBorderPaint)
        canvas.drawText("-", minusRect.centerX(), minusRect.centerY() + 6f * density, sliderBtnTextPaint)

        val plusRect = getSliderPlusRect(barRect)
        canvas.drawRoundRect(plusRect, 6f * density, 6f * density, sliderBtnBgPaint)
        canvas.drawRoundRect(plusRect, 6f * density, 6f * density, sliderBtnBorderPaint)
        canvas.drawText("+", plusRect.centerX(), plusRect.centerY() + 6f * density, sliderBtnTextPaint)

        val trackRect = getSliderTrackRect(barRect)
        canvas.drawLine(trackRect.left, trackRect.centerY(), trackRect.right, trackRect.centerY(), sliderTrackPaint)

        val frac = ((btn.sizeScale - 0.50f) / 1.50f).coerceIn(0f, 1f)
        val thumbX = trackRect.left + frac * trackRect.width()
        val thumbY = trackRect.centerY()

        canvas.drawCircle(thumbX, thumbY, 9f * density, sliderThumbPaint)
        canvas.drawCircle(thumbX, thumbY, 9f * density, sliderThumbBorderPaint)
    }

    private fun drawSelectionBrackets(canvas: Canvas, cx: Float, cy: Float, r: Float) {
        val arm = 8f * density
        val paint = editSelectedPaint

        canvas.drawLine(cx - r, cy - r + arm, cx - r, cy - r, paint)
        canvas.drawLine(cx - r, cy - r, cx - r + arm, cy - r, paint)
        canvas.drawLine(cx + r - arm, cy - r, cx + r, cy - r, paint)
        canvas.drawLine(cx + r, cy - r, cx + r, cy - r + arm, paint)
        canvas.drawLine(cx - r, cy + r - arm, cx - r, cy + r, paint)
        canvas.drawLine(cx - r, cy + r, cx - r + arm, cy + r, paint)
        canvas.drawLine(cx + r - arm, cy + r, cx + r, cy + r, paint)
        canvas.drawLine(cx + r, cy + r, cx + r, cy + r - arm, paint)
    }

    private fun drawJoystick(canvas: Canvas) {
        val maxDist = joyRadiusDp * density
        canvas.drawCircle(joyOriginX, joyOriginY, maxDist, joyBasePaint)

        val dx = joyCurrentX - joyOriginX
        val dy = joyCurrentY - joyOriginY
        val dist = hypot(dx, dy)
        val clampedDist = min(dist, maxDist)
        val knobX = if (dist > 0) joyOriginX + (dx / dist) * clampedDist else joyOriginX
        val knobY = if (dist > 0) joyOriginY + (dy / dist) * clampedDist else joyOriginY

        canvas.drawCircle(knobX, knobY, 22f * density, joyKnobPaint)
    }
}
