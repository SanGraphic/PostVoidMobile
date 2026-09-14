package com.postvoid.port

import android.content.Context
import android.os.Build
import android.os.Process
import android.util.Log
import android.view.KeyEvent
import java.io.BufferedReader
import java.io.File
import java.io.FileWriter
import java.io.InputStreamReader
import java.io.PrintWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.atomic.AtomicBoolean

/**
 * GameLogger — Active session log file generator.
 *
 * For debug builds:
 *  - Automatically rotates existing `game.log` to `game_previous.log` on startup.
 *  - Continuously writes all Logcat output (native libyoyo.so, libgamepad_hook.so, AndroidRuntime)
 *    into `/sdcard/Android/data/com.postvoid.port/files/game.log`.
 *  - Logs comprehensive input device enumerations and every gamepad/key/motion event.
 *  - Hooks uncaught exceptions and flushes them to disk immediately.
 */
object GameLogger {

    private const val TAG = "GameLogger"
    private const val LOG_FILE_NAME = "game.log"
    private const val PREV_LOG_FILE_NAME = "game_previous.log"

    private var logWriter: PrintWriter? = null
    private val isRunning = AtomicBoolean(false)
    private var logcatProcess: java.lang.Process? = null
    private var logcatThread: Thread? = null
    private val dateFormat = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)

    fun init(context: Context) {
        if (!BuildConfig.DEBUG) {
            Log.i(TAG, "Release build: GameLogger disabled.")
            return
        }

        try {
            val dir = context.getExternalFilesDir(null) ?: context.filesDir
            if (!dir.exists()) dir.mkdirs()

            val logFile = File(dir, LOG_FILE_NAME)
            val prevFile = File(dir, PREV_LOG_FILE_NAME)

            // Rotate previous session log
            if (logFile.exists() && logFile.length() > 0) {
                if (prevFile.exists()) prevFile.delete()
                logFile.renameTo(prevFile)
            }

            logWriter = PrintWriter(FileWriter(logFile, true), true)
            isRunning.set(true)

            // Log session header
            val header = buildString {
                appendLine("================================================================")
                appendLine(" POST VOID ANDROID PORT — SESSION LOG")
                appendLine(" Timestamp:   ${dateFormat.format(Date())}")
                appendLine(" App Version: ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
                appendLine(" Device:      ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE})")
                appendLine(" Android OS:  API ${Build.VERSION.SDK_INT} (Android ${Build.VERSION.RELEASE})")
                appendLine(" ABI:         ${Build.SUPPORTED_ABIS.joinToString(", ")}")
                appendLine(" PID:         ${Process.myPid()}")
                appendLine(" Log File:    ${logFile.absolutePath}")
                appendLine("================================================================")
            }
            writeRaw(header)
            Log.i(TAG, "GameLogger initialized. Output -> ${logFile.absolutePath}")

            // Hook Uncaught Exceptions
            val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
            Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
                log(TAG, "FATAL CRASH on thread ${thread.name}: ${throwable.message}")
                throwable.printStackTrace(logWriter)
                flush()
                defaultHandler?.uncaughtException(thread, throwable)
            }

            // Start async Logcat reader thread for this PID
            startLogcatCapture()

        } catch (e: Throwable) {
            Log.e(TAG, "Failed to initialize GameLogger: ${e.message}", e)
        }
    }

    private fun startLogcatCapture() {
        logcatThread = Thread({
            try {
                val pid = Process.myPid()
                val cmd = arrayOf("logcat", "-v", "time", "--pid=$pid")
                logcatProcess = Runtime.getRuntime().exec(cmd)
                val reader = BufferedReader(InputStreamReader(logcatProcess?.inputStream))

                var line: String? = null
                while (isRunning.get()) {
                    val l = reader.readLine() ?: break
                    writeRaw(l)
                }
            } catch (e: Throwable) {
                if (isRunning.get()) {
                    writeRaw("[GameLogger] Logcat capture ended: ${e.message}")
                }
            }
        }, "GameLogger-LogcatThread").apply {
            isDaemon = true
            start()
        }
    }

    @Synchronized
    fun log(tag: String, message: String) {
        val entry = "[${dateFormat.format(Date())}] [$tag] $message"
        Log.i(tag, message)
        writeRaw(entry)
    }

    @Synchronized
    fun logError(tag: String, message: String, throwable: Throwable? = null) {
        val entry = "[${dateFormat.format(Date())}] [ERROR] [$tag] $message"
        Log.e(tag, message, throwable)
        writeRaw(entry)
        if (throwable != null) {
            throwable.printStackTrace(logWriter)
            logWriter?.flush()
        }
    }

    @Synchronized
    private fun writeRaw(text: String) {
        try {
            logWriter?.println(text)
            logWriter?.flush()
        } catch (_: Throwable) {}
    }

    fun logInputDevices(context: Context) {
        try {
            val inputManager = context.getSystemService(Context.INPUT_SERVICE) as? android.hardware.input.InputManager
                ?: return
            val deviceIds = inputManager.inputDeviceIds
            val sb = StringBuilder()
            sb.appendLine("--- CONNECTED INPUT DEVICES (${deviceIds.size}) ---")
            for (id in deviceIds) {
                val dev = inputManager.getInputDevice(id) ?: continue
                sb.appendLine("Device #$id: '${dev.name}' (vendorId=0x${Integer.toHexString(dev.vendorId)}, productId=0x${Integer.toHexString(dev.productId)}, isVirtual=${dev.isVirtual})")
                sb.appendLine("   Sources:       0x${Integer.toHexString(dev.sources)}")
                sb.appendLine("   Keyboard Type: ${dev.keyboardType}")
            }
            sb.appendLine("----------------------------------------")
            log(TAG, sb.toString())
        } catch (e: Throwable) {
            logError(TAG, "Error enumerating input devices", e)
        }
    }

    fun logKeyEvent(event: KeyEvent, mappedToGamepad: Boolean) {
        if (!BuildConfig.DEBUG) return
        val actionStr = when (event.action) {
            KeyEvent.ACTION_DOWN -> "DOWN"
            KeyEvent.ACTION_UP   -> "UP"
            KeyEvent.ACTION_MULTIPLE -> "MULTIPLE"
            else -> "ACTION(${event.action})"
        }
        val dev = event.device?.name ?: "Unknown"
        log("InputEvent", "KEY [$actionStr] keyCode=${event.keyCode} scanCode=${event.scanCode} mapped=$mappedToGamepad dev='$dev'")
    }

    fun logMotionEvent(axisX: Float, axisY: Float, axisZ: Float, axisRZ: Float, lT: Float, rT: Float, hatX: Float, hatY: Float) {
        if (!BuildConfig.DEBUG) return
        log("MotionEvent", "AXES: LX=%.2f LY=%.2f RX=%.2f RY=%.2f LT=%.2f RT=%.2f HAT=(%.1f, %.1f)".format(
            axisX, axisY, axisZ, axisRZ, lT, rT, hatX, hatY
        ))
    }

    fun flush() {
        try { logWriter?.flush() } catch (_: Throwable) {}
    }

    fun close() {
        isRunning.set(false)
        try {
            logcatProcess?.destroy()
            logWriter?.flush()
            logWriter?.close()
        } catch (_: Throwable) {}
    }
}
