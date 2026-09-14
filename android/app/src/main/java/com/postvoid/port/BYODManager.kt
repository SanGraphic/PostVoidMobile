package com.postvoid.port

import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import android.util.Log
import java.io.*
import java.util.zip.Deflater
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

object BYODManager {
    private const val TAG = "BYODManager"
    const val DATA_WIN_NAME = "data.win"
    const val GAME_DROID_NAME = "game.droid"
    const val PATCH_NAME = "postvoid.xdelta"
    const val CONTAINER_NAME = "game_data.apk"
    private const val PREFS_NAME = "postvoid_byod_prefs"
    private const val KEY_SETUP_COMPLETED = "byod_setup_completed"

    init {
        try {
            System.loadLibrary("gamepad_hook")
            Log.i(TAG, "Native gamepad_hook (with xdelta3) loaded successfully.")
        } catch (e: Throwable) {
            Log.e(TAG, "Failed to load native gamepad_hook library: ${e.message}", e)
        }
    }

    /**
     * Native JNI wrapper around xdelta3 to apply postvoid.xdelta to data.win
     */
    external fun nativeApplyPatch(srcPath: String, patchPath: String, outPath: String): Int

    /**
     * Returns true if this APK was built as the standalone flavor (assets/game.droid embedded)
     */
    fun isStandalone(context: Context): Boolean {
        return try {
            val list = context.assets.list("") ?: emptyArray()
            list.contains(GAME_DROID_NAME)
        } catch (e: Exception) {
            false
        }
    }

    /**
     * Returns true if the game is ready to boot immediately
     */
    fun isGameContainerReady(context: Context): Boolean {
        if (isStandalone(context)) {
            return true
        }
        val containerFile = getGameContainerFile(context)
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val isMarkedComplete = prefs.getBoolean(KEY_SETUP_COMPLETED, false)

        return isMarkedComplete && containerFile.exists() && containerFile.length() > 50 * 1024 * 1024
    }

    fun getGameContainerFile(context: Context): File {
        return File(context.filesDir, CONTAINER_NAME)
    }

    fun getGameContainerPath(context: Context): String {
        return if (isStandalone(context)) {
            context.applicationInfo.publicSourceDir
        } else {
            getGameContainerFile(context).absolutePath
        }
    }

    fun getSaveDirectory(context: Context): File {
        val saveDir = File(context.filesDir, "saves")
        if (!saveDir.exists()) {
            saveDir.mkdirs()
        }
        return saveDir
    }

    fun markSetupCompleted(context: Context, completed: Boolean = true) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putBoolean(KEY_SETUP_COMPLETED, completed).apply()
    }

    /**
     * Extracts an asset from the APK to a local destination file
     */
    fun extractAssetToFile(context: Context, assetName: String, destFile: File): Boolean {
        return try {
            destFile.parentFile?.mkdirs()
            context.assets.open(assetName).use { input ->
                FileOutputStream(destFile).use { output ->
                    input.copyTo(output)
                }
            }
            destFile.exists() && destFile.length() > 0
        } catch (e: Exception) {
            Log.e(TAG, "Failed to extract asset $assetName to ${destFile.absolutePath}", e)
            false
        }
    }

    /**
     * Copies a content URI (e.g. from SAF file picker) to a local file
     */
    fun copyUriToFile(context: Context, uri: Uri, destFile: File, onProgress: ((Long, Long) -> Unit)? = null): Boolean {
        return try {
            destFile.parentFile?.mkdirs()
            val totalSize = getUriFileSize(context, uri)
            context.contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(destFile).use { output ->
                    val buffer = ByteArray(256 * 1024)
                    var bytesCopied = 0L
                    var read: Int
                    while (input.read(buffer).also { read = it } != -1) {
                        output.write(buffer, 0, read)
                        bytesCopied += read
                        onProgress?.invoke(bytesCopied, totalSize)
                    }
                }
            }
            destFile.exists() && destFile.length() > 0
        } catch (e: Exception) {
            Log.e(TAG, "Failed to copy URI $uri to ${destFile.absolutePath}", e)
            false
        }
    }

    private fun getUriFileSize(context: Context, uri: Uri): Long {
        var size = -1L
        try {
            context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
                if (sizeIndex != -1 && cursor.moveToFirst()) {
                    size = cursor.getLong(sizeIndex)
                }
            }
        } catch (_: Exception) {}
        return size
    }

    /**
     * Scans common directories on external storage for existing Post Void game files
     */
    fun scanCommonLocations(): List<File> {
        val candidates = mutableListOf<File>()
        val storageRoot = Environment.getExternalStorageDirectory() ?: return candidates

        val checkPaths = listOf(
            File(storageRoot, "postvoid"),
            File(storageRoot, "PostVoid"),
            File(storageRoot, "Post Void"),
            File(storageRoot, "Download/postvoid"),
            File(storageRoot, "Download/PostVoid"),
            File(storageRoot, "Download/Post Void"),
            File(storageRoot, "Download"),
            File(storageRoot, "Documents/postvoid"),
            File(storageRoot, "Documents/PostVoid"),
            File(storageRoot, "Documents/Post Void"),
            File(storageRoot, "Games/postvoid"),
            File(storageRoot, "Games/PostVoid"),
            File(storageRoot, "Games/Post Void")
        )

        for (dir in checkPaths) {
            if (dir.exists() && dir.isDirectory) {
                val dataWin = File(dir, DATA_WIN_NAME)
                if (dataWin.exists() && dataWin.length() > 50 * 1024 * 1024) {
                    candidates.add(dataWin)
                }
                val gameDroid = File(dir, GAME_DROID_NAME)
                if (gameDroid.exists() && gameDroid.length() > 50 * 1024 * 1024 && !candidates.contains(dataWin)) {
                    candidates.add(gameDroid)
                }
            }
        }

        return candidates
    }

    /**
     * Tries to resolve a SAF Uri to a real filesystem path if possible
     */
    fun resolveUriToDirectFile(context: Context, uri: Uri): File? {
        try {
            if ("file".equals(uri.scheme, ignoreCase = true)) {
                return uri.path?.let { File(it) }
            }

            if (DocumentsContract.isDocumentUri(context, uri)) {
                val docId = DocumentsContract.getDocumentId(uri)
                if (docId.startsWith("primary:")) {
                    val relPath = docId.substringAfter("primary:")
                    val storageRoot = Environment.getExternalStorageDirectory()
                    val file = File(storageRoot, relPath)
                    if (file.exists()) return file
                }
            }
        } catch (_: Exception) {}
        return null
    }

    /**
     * Info about discovered sibling files in a game directory
     */
    data class GameSourceInfo(
        val dataWinFile: File,
        val optionsFile: File? = null,
        val localizationFile: File? = null,
        val localizationArTestFile: File? = null,
        val fontsDir: File? = null
    )

    /**
     * Inspects parent directory of a selected data.win to resolve sibling files
     */
    fun inspectGameDirectory(dataFile: File): GameSourceInfo {
        val parent = dataFile.parentFile
        if (parent == null || !parent.exists() || !parent.isDirectory) {
            return GameSourceInfo(dataWinFile = dataFile)
        }

        val options = File(parent, "options.ini").takeIf { it.exists() && it.isFile }
        val loc = File(parent, "localization.json").takeIf { it.exists() && it.isFile }
        val locArTest = File(parent, "localization_ar_test.json").takeIf { it.exists() && it.isFile }
        val fonts = File(parent, "fonts").takeIf { it.exists() && it.isDirectory }

        return GameSourceInfo(
            dataWinFile = dataFile,
            optionsFile = options,
            localizationFile = loc,
            localizationArTestFile = locArTest,
            fontsDir = fonts
        )
    }

    /**
     * Patches data.win using postvoid.xdelta to produce game.droid
     */
    fun patchDataWin(context: Context, srcDataWin: File, outGameDroid: File): Boolean {
        val patchCacheFile = File(context.cacheDir, PATCH_NAME)
        try {
            if (patchCacheFile.exists()) patchCacheFile.delete()
            if (outGameDroid.exists()) outGameDroid.delete()
            outGameDroid.parentFile?.mkdirs()
            Log.i(TAG, "Extracting $PATCH_NAME from assets to cache...")
            extractAssetToFile(context, PATCH_NAME, patchCacheFile)

            Log.i(TAG, "Applying native patch: src=${srcDataWin.absolutePath} (${srcDataWin.length()} bytes) -> ${outGameDroid.absolutePath}")
            val ret = nativeApplyPatch(srcDataWin.absolutePath, patchCacheFile.absolutePath, outGameDroid.absolutePath)
            Log.i(TAG, "Native patch returned code: $ret")

            return ret == 0 && outGameDroid.exists() && outGameDroid.length() > 50 * 1024 * 1024
        } catch (e: Throwable) {
            Log.e(TAG, "Failed during native patching", e)
            return false
        } finally {
            try { patchCacheFile.delete() } catch (_: Exception) {}
        }
    }

    /**
     * Constructs the game_data.apk container using ZipOutputStream (Deflater.NO_COMPRESSION)
     */
    fun buildContainer(
        context: Context,
        gameDroidFile: File,
        optionsFile: File? = null,
        locFile: File? = null,
        locArTestFile: File? = null,
        fontsDir: File? = null,
        statusListener: ((String, Int) -> Unit)? = null
    ): Boolean {
        val targetApk = getGameContainerFile(context)
        val tempApk = File(context.filesDir, "${CONTAINER_NAME}.tmp")

        try {
            statusListener?.invoke("Creating game container archive...", 10)
            if (tempApk.exists()) tempApk.delete()

            ZipOutputStream(BufferedOutputStream(FileOutputStream(tempApk), 1024 * 1024)).use { zipOut ->
                zipOut.setLevel(Deflater.NO_COMPRESSION)

                // 1. Pack assets/game.droid (The core patched GameMaker bytecode)
                statusListener?.invoke("Packing game bytecode (assets/game.droid)...", 30)
                writeZipEntry(zipOut, "assets/game.droid", FileInputStream(gameDroidFile))

                // 2. Pack assets/options.ini
                statusListener?.invoke("Packing options (assets/options.ini)...", 50)
                if (optionsFile != null && optionsFile.exists()) {
                    writeZipEntry(zipOut, "assets/options.ini", FileInputStream(optionsFile))
                } else {
                    writeAssetZipEntry(context, zipOut, "options.ini", "assets/options.ini")
                }

                // 3. Pack assets/localization.json
                statusListener?.invoke("Packing strings (assets/localization.json)...", 65)
                if (locFile != null && locFile.exists()) {
                    writeZipEntry(zipOut, "assets/localization.json", FileInputStream(locFile))
                } else {
                    writeAssetZipEntry(context, zipOut, "localization.json", "assets/localization.json")
                }

                // 3b. Pack assets/localization_ar_test.json (Required by obj_preload)
                statusListener?.invoke("Packing localization tests...", 75)
                if (locArTestFile != null && locArTestFile.exists()) {
                    writeZipEntry(zipOut, "assets/localization_ar_test.json", FileInputStream(locArTestFile))
                } else {
                    writeAssetZipEntry(context, zipOut, "localization_ar_test.json", "assets/localization_ar_test.json")
                }

                // 4. Pack assets/gamecontrollerdb.txt
                statusListener?.invoke("Packing controller mapping...", 85)
                writeAssetZipEntry(context, zipOut, "gamecontrollerdb.txt", "assets/gamecontrollerdb.txt")

                // 5. Pack assets/fonts/
                statusListener?.invoke("Packing fonts...", 92)
                if (fontsDir != null && fontsDir.exists() && fontsDir.isDirectory) {
                    val fontFiles = fontsDir.listFiles() ?: emptyArray()
                    for (f in fontFiles) {
                        if (f.isFile) {
                            writeZipEntry(zipOut, "assets/fonts/${f.name}", FileInputStream(f))
                        }
                    }
                } else {
                    val fontAssets = context.assets.list("fonts") ?: emptyArray()
                    for (fName in fontAssets) {
                        writeAssetZipEntry(context, zipOut, "fonts/$fName", "assets/fonts/$fName")
                    }
                }
            }

            statusListener?.invoke("Finalizing container...", 98)
            if (targetApk.exists()) targetApk.delete()
            val renamed = tempApk.renameTo(targetApk)
            if (!renamed) {
                tempApk.copyTo(targetApk, overwrite = true)
                tempApk.delete()
            }

            markSetupCompleted(context, true)
            statusListener?.invoke("Ready to Launch!", 100)
            Log.i(TAG, "Game container successfully built: ${targetApk.absolutePath} (${targetApk.length()} bytes)")
            return true
        } catch (e: Throwable) {
            Log.e(TAG, "Error building game container", e)
            try { tempApk.delete() } catch (_: Exception) {}
            return false
        }
    }

    private fun writeZipEntry(zipOut: ZipOutputStream, entryName: String, input: InputStream) {
        val entry = ZipEntry(entryName)
        zipOut.putNextEntry(entry)
        val buffer = ByteArray(256 * 1024)
        input.use { inStream ->
            var read: Int
            while (inStream.read(buffer).also { read = it } != -1) {
                zipOut.write(buffer, 0, read)
            }
        }
        zipOut.closeEntry()
    }

    private fun writeAssetZipEntry(context: Context, zipOut: ZipOutputStream, assetPath: String, entryName: String) {
        try {
            context.assets.open(assetPath).use { input ->
                val entry = ZipEntry(entryName)
                zipOut.putNextEntry(entry)
                val buffer = ByteArray(64 * 1024)
                var read: Int
                while (input.read(buffer).also { read = it } != -1) {
                    zipOut.write(buffer, 0, read)
                }
                zipOut.closeEntry()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Optional asset $assetPath not bundled: ${e.message}")
        }
    }
}
