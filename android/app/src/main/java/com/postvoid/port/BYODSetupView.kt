package com.postvoid.port

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.*
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.documentfile.provider.DocumentFile
import java.io.File
import kotlin.concurrent.thread

class BYODSetupView(
    private val activity: MainActivity,
    private val onSetupCompleted: () -> Unit
) : FrameLayout(activity) {

    private lateinit var statusText: TextView
    private lateinit var autoDetectLayout: LinearLayout
    private lateinit var autoDetectText: TextView
    private lateinit var autoDetectBtn: Button
    private lateinit var selectFileBtn: Button
    private lateinit var selectFolderBtn: Button
    private lateinit var permBtn: Button
    private lateinit var progressBar: ProgressBar

    private var detectedDataWinFile: File? = null

    companion object {
        const val REQ_CODE_FILE = 2001
        const val REQ_CODE_FOLDER = 2002
        const val REQ_CODE_STORAGE_PERM = 2003
        const val REQ_CODE_MANAGE_STORAGE = 2004

        private const val COLOR_BG = 0xFF0A0A0A.toInt()
        private const val COLOR_PANEL = 0xFF141414.toInt()
        private const val COLOR_YELLOW = 0xFFFFE600.toInt()
        private const val COLOR_TEXT_WHITE = 0xFFF0F0F0.toInt()
        private const val COLOR_TEXT_DIM = 0xFF888888.toInt()
        private const val COLOR_SUCCESS = 0xFF00FF66.toInt()
    }

    init {
        setBackgroundColor(COLOR_BG)
        buildUI()
        refreshStorageAndScan()
    }

    fun refreshStorageAndScan() {
        val hasStorage = hasStoragePermission()
        if (hasStorage) {
            permBtn.visibility = View.GONE
            scanForLocalFiles()
        } else {
            permBtn.visibility = View.VISIBLE
            autoDetectLayout.visibility = View.GONE
        }
    }

    private fun hasStoragePermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            ContextCompat.checkSelfPermission(activity, android.Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                    data = Uri.parse("package:${activity.packageName}")
                }
                activity.startActivityForResult(intent, REQ_CODE_MANAGE_STORAGE)
            } catch (e: Exception) {
                val intent = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                activity.startActivityForResult(intent, REQ_CODE_MANAGE_STORAGE)
            }
        } else {
            ActivityCompat.requestPermissions(
                activity,
                arrayOf(android.Manifest.permission.READ_EXTERNAL_STORAGE, android.Manifest.permission.WRITE_EXTERNAL_STORAGE),
                REQ_CODE_STORAGE_PERM
            )
        }
    }

    private fun scanForLocalFiles() {
        thread {
            val candidates = BYODManager.scanCommonLocations()
            post {
                if (candidates.isNotEmpty()) {
                    detectedDataWinFile = candidates.first()
                    autoDetectText.text = "✓ STEAM GAME FILES DETECTED:\n${detectedDataWinFile!!.parentFile?.name ?: ""}/${detectedDataWinFile!!.name}"
                    autoDetectLayout.visibility = View.VISIBLE
                } else {
                    detectedDataWinFile = null
                    autoDetectLayout.visibility = View.GONE
                }
            }
        }
    }

    private fun buildUI() {
        val scrollView = ScrollView(context).apply {
            isFillViewport = true
        }

        val contentLayout = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(24), dp(16), dp(24), dp(16))
        }
        scrollView.addView(contentLayout)
        addView(scrollView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))

        // Title
        val titleText = TextView(context).apply {
            text = "POST VOID"
            setTextColor(COLOR_YELLOW)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 36f)
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
        }
        contentLayout.addView(titleText)

        // Subtitle
        val subTitleText = TextView(context).apply {
            text = "BRING YOUR OWN DATA (BYOD) SETUP"
            setTextColor(COLOR_TEXT_WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            typeface = Typeface.MONOSPACE
            gravity = Gravity.CENTER
            setPadding(0, dp(4), 0, dp(12))
        }
        contentLayout.addView(subTitleText)

        // Description Card
        val card = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            background = createCardDrawable()
            setPadding(dp(20), dp(14), dp(20), dp(14))
            val lp = LinearLayout.LayoutParams(dp(540), LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                bottomMargin = dp(14)
            }
            layoutParams = lp
        }

        val descText = TextView(context).apply {
            text = "Post Void requires original PC game files from Steam.\nSelect your Steam Post Void directory or data.win to patch on-the-fly."
            setTextColor(COLOR_TEXT_WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            setLineSpacing(0f, 1.25f)
            gravity = Gravity.CENTER
        }
        card.addView(descText)
        contentLayout.addView(card)

        // Permission Request Button
        permBtn = createStyledButton("GRANT STORAGE PERMISSION", COLOR_YELLOW, Color.BLACK).apply {
            setOnClickListener { requestStoragePermission() }
            visibility = View.GONE
        }
        contentLayout.addView(permBtn)

        // Auto-detect Panel
        autoDetectLayout = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            background = createCardDrawable(borderColor = COLOR_SUCCESS)
            setPadding(dp(16), dp(12), dp(16), dp(12))
            val lp = LinearLayout.LayoutParams(dp(540), LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                bottomMargin = dp(14)
            }
            layoutParams = lp
            visibility = View.GONE
        }
        autoDetectText = TextView(context).apply {
            setTextColor(COLOR_SUCCESS)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            typeface = Typeface.MONOSPACE
            gravity = Gravity.CENTER
        }
        autoDetectLayout.addView(autoDetectText)

        autoDetectBtn = createStyledButton("⚡ AUTO-PATCH & LAUNCH", COLOR_SUCCESS, Color.BLACK).apply {
            setOnClickListener {
                detectedDataWinFile?.let { startPatchingProcess(it) }
            }
        }
        autoDetectLayout.addView(autoDetectBtn)
        contentLayout.addView(autoDetectLayout)

        // File & Folder Picker Buttons
        val buttonsLayout = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            val lp = LinearLayout.LayoutParams(dp(540), LinearLayout.LayoutParams.WRAP_CONTENT).apply {
                bottomMargin = dp(14)
            }
            layoutParams = lp
        }

        selectFileBtn = createStyledButton("SELECT DATA.WIN", COLOR_YELLOW, Color.BLACK).apply {
            val lp = LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                rightMargin = dp(8)
            }
            layoutParams = lp
            setOnClickListener { openFilePicker() }
        }
        buttonsLayout.addView(selectFileBtn)

        selectFolderBtn = createStyledButton("SELECT GAME FOLDER", COLOR_PANEL, COLOR_TEXT_WHITE, borderColor = COLOR_YELLOW).apply {
            val lp = LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                leftMargin = dp(8)
            }
            layoutParams = lp
            setOnClickListener { openFolderPicker() }
        }
        buttonsLayout.addView(selectFolderBtn)
        contentLayout.addView(buttonsLayout)

        // Progress Bar
        progressBar = ProgressBar(context, null, android.R.attr.progressBarStyleHorizontal).apply {
            isIndeterminate = false
            max = 100
            progress = 0
            val lp = LinearLayout.LayoutParams(dp(540), dp(10)).apply {
                bottomMargin = dp(8)
            }
            layoutParams = lp
            visibility = View.GONE
        }
        contentLayout.addView(progressBar)

        // Status Text
        statusText = TextView(context).apply {
            text = "Ready. Choose game files to begin."
            setTextColor(COLOR_TEXT_DIM)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            typeface = Typeface.MONOSPACE
            gravity = Gravity.CENTER
        }
        contentLayout.addView(statusText)
    }

    private fun openFilePicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        activity.startActivityForResult(intent, REQ_CODE_FILE)
    }

    private fun openFolderPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        activity.startActivityForResult(intent, REQ_CODE_FOLDER)
    }

    fun handleSelectedFileUri(uri: Uri) {
        val directFile = BYODManager.resolveUriToDirectFile(context, uri)
        if (directFile != null && directFile.exists()) {
            startPatchingProcess(directFile)
            return
        }

        setProcessingUI(true, "Caching selected data.win...")
        thread {
            val cacheDataWin = File(context.cacheDir, "picked_data.win")
            val ok = BYODManager.copyUriToFile(context, uri, cacheDataWin) { bytes, total ->
                post {
                    if (total > 0) {
                        val pct = ((bytes * 100) / total).toInt()
                        progressBar.progress = pct
                        statusText.text = "Copying data.win: $pct%"
                    }
                }
            }
            post {
                if (ok && cacheDataWin.length() > 50 * 1024 * 1024) {
                    startPatchingProcess(cacheDataWin)
                } else {
                    setProcessingUI(false, "Failed to read data.win. Please try again.")
                }
            }
        }
    }

    fun handleSelectedFolderUri(treeUri: Uri) {
        setProcessingUI(true, "Scanning selected folder...")
        thread {
            val folderDoc = DocumentFile.fromTreeUri(context, treeUri)
            val dataWinDoc = folderDoc?.findFile("data.win") ?: folderDoc?.findFile("game.droid")
            if (dataWinDoc == null) {
                post {
                    setProcessingUI(false, "data.win not found in selected folder! Please select the Post Void directory.")
                }
                return@thread
            }

            val cacheDataWin = File(context.cacheDir, "picked_data.win")
            val ok = BYODManager.copyUriToFile(context, dataWinDoc.uri, cacheDataWin) { bytes, total ->
                post {
                    if (total > 0) {
                        val pct = ((bytes * 100) / total).toInt()
                        progressBar.progress = pct
                        statusText.text = "Copying data.win: $pct%"
                    }
                }
            }

            if (!ok) {
                post { setProcessingUI(false, "Failed to copy data.win from folder.") }
                return@thread
            }

            val cacheOptions = File(context.cacheDir, "options.ini")
            val cacheLoc = File(context.cacheDir, "localization.json")
            val cacheLocAr = File(context.cacheDir, "localization_ar_test.json")
            folderDoc?.findFile("options.ini")?.let { BYODManager.copyUriToFile(context, it.uri, cacheOptions) }
            folderDoc?.findFile("localization.json")?.let { BYODManager.copyUriToFile(context, it.uri, cacheLoc) }
            folderDoc?.findFile("localization_ar_test.json")?.let { BYODManager.copyUriToFile(context, it.uri, cacheLocAr) }

            val optionsFile = cacheOptions.takeIf { it.exists() }
            val locFile = cacheLoc.takeIf { it.exists() }
            val locArFile = cacheLocAr.takeIf { it.exists() }

            post {
                executePatchAndBuild(cacheDataWin, optionsFile, locFile, locArFile, null)
            }
        }
    }

    private fun startPatchingProcess(dataWinFile: File) {
        setProcessingUI(true, "Inspecting folder structure...")
        thread {
            val sourceInfo = BYODManager.inspectGameDirectory(dataWinFile)
            post {
                executePatchAndBuild(
                    sourceInfo.dataWinFile,
                    sourceInfo.optionsFile,
                    sourceInfo.localizationFile,
                    sourceInfo.localizationArTestFile,
                    sourceInfo.fontsDir
                )
            }
        }
    }

    private fun executePatchAndBuild(
        dataWinFile: File,
        optionsFile: File?,
        locFile: File?,
        locArFile: File?,
        fontsDir: File?
    ) {
        setProcessingUI(true, "Applying on-the-fly xdelta3 patch...")
        thread {
            val patchedGameDroid = File(context.filesDir, BYODManager.GAME_DROID_NAME)
            val patchSuccess = BYODManager.patchDataWin(context, dataWinFile, patchedGameDroid)

            if (!patchSuccess) {
                post {
                    setProcessingUI(false, "Patching failed! Ensure your data.win matches Steam release.")
                }
                return@thread
            }

            post {
                statusText.text = "Patch successful! Assembling container..."
            }

            val buildSuccess = BYODManager.buildContainer(
                context,
                patchedGameDroid,
                optionsFile,
                locFile,
                locArFile,
                fontsDir
            ) { msg, pct ->
                post {
                    statusText.text = msg
                    progressBar.progress = pct
                }
            }

            try { patchedGameDroid.delete() } catch (_: Exception) {}

            post {
                if (buildSuccess) {
                    statusText.text = "Setup Complete! Starting Post Void..."
                    statusText.setTextColor(COLOR_SUCCESS)
                    progressBar.progress = 100
                    postDelayed({ onSetupCompleted() }, 500)
                } else {
                    setProcessingUI(false, "Failed to create game container.")
                }
            }
        }
    }

    private fun setProcessingUI(processing: Boolean, message: String) {
        selectFileBtn.isEnabled = !processing
        selectFolderBtn.isEnabled = !processing
        autoDetectBtn.isEnabled = !processing
        progressBar.visibility = if (processing) View.VISIBLE else View.GONE
        statusText.text = message
        statusText.setTextColor(if (processing) COLOR_YELLOW else COLOR_TEXT_DIM)
    }

    private fun createStyledButton(label: String, bgColor: Int, txtColor: Int, borderColor: Int = 0): Button {
        return Button(context).apply {
            text = label
            setTextColor(txtColor)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = Typeface.DEFAULT_BOLD
            isAllCaps = true
            background = GradientDrawable().apply {
                setColor(bgColor)
                cornerRadius = dp(6).toFloat()
                if (borderColor != 0) {
                    setStroke(dp(2), borderColor)
                }
            }
            val lp = LinearLayout.LayoutParams(dp(540), dp(48)).apply {
                bottomMargin = dp(10)
            }
            layoutParams = lp
        }
    }

    private fun createCardDrawable(borderColor: Int = 0x33FFFFFF): GradientDrawable {
        return GradientDrawable().apply {
            setColor(COLOR_PANEL)
            cornerRadius = dp(8).toFloat()
            setStroke(dp(1), borderColor)
        }
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
}
