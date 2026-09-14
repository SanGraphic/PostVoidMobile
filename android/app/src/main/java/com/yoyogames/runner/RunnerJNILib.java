package com.yoyogames.runner;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;
import java.nio.ByteBuffer;

public class RunnerJNILib {
    private static final String TAG = "RunnerJNILib";

    public static int mGameSpeedControl = 60;
    public static int mSleepMargin = 10;
    public static AssetManager ms_assetManager = null;
    public static Context ms_context = null;

    static {
        try {
            System.loadLibrary("c++_shared");
            System.loadLibrary("yoyo");
            Log.i(TAG, "Successfully loaded native libyoyo.so");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
        }
    }

    // Exact signatures verified byte-for-byte from libyoyo.so Startup disasm
    public static void OpenURL(String url) { Log.i(TAG, "OpenURL: " + url); }
    public static int OsGetInfo() { return 0; }
    public static Object CallExtensionFunction(String ext, String func, int argc, double[] dArgs, Object[] oArgs) { return null; }
    public static Object LoadDynamicAsset(String name) { return null; }
    public static Object GetAssetManager() { return ms_assetManager; }
    public static void LeaveRating(String a, String b, String c, String d) {}

    public static void HttpGet(String url, int id) {}
    public static void HttpPost(String url, String postData, int id) {}
    public static void HttpRequest(String url, String method, String headers, byte[] body, int id) {}

    public static void ClearGamepads() {}
    public static void PlayMP3(String path, int loop) {}
    public static void StopMP3() {}
    public static void PauseMP3() {}
    public static void ResumeMP3() {}
    public static void SetMP3Volume(float vol) {}
    public static boolean PlayingMP3() { return false; }

    public static int sDefaultFrameBuffer = 0;
    public static int GetDefaultFrameBuffer() { return sDefaultFrameBuffer; }
    public static void ShowMessage(String msg) { Log.i(TAG, "ShowMessage: " + msg); }

    public static void setSystemUIVisibilityFlags(int flags) {}

    public static String InputString(String title, String prompt) { return ""; }
    public static int ShowQuestion(String msg) { return 0; }
    public static void ShowMessageAsync(String msg, int id) {}
    public static void InputStringAsync(String a, String b, int id) {}
    public static void ShowQuestionAsync(String msg, int id) {}
    public static void ShowLogin(String a, String b, int id) {}

    public static void RestrictOrientation(boolean a, boolean b, boolean c, boolean d, boolean e) {}
    public static boolean isNetworkConnected() { return true; }
    public static int CheckPermission(String perm) { return 0; }

    public static void VideoOpen(String path) {}
    public static void VideoClose() {}
    public static boolean VideoDraw(ByteBuffer buf) { return false; }
    public static double VideoW() { return 0; }
    public static double VideoH() { return 0; }
    public static double VideoStatus() { return 0; }
    public static void VideoSetVolume(double vol) {}
    public static void VideoPause() {}
    public static void VideoResume() {}
    public static void VideoEnableLoop(double loop) {}
    public static void VideoSeekTo(double time) {}
    public static double VideoGetDuration() { return 0; }
    public static double VideoGetPosition() { return 0; }
    public static double VideoGetStatus() { return 0; }
    public static double VideoGetFormat() { return 0; }
    public static double VideoIsLooping() { return 0; }
    public static double VideoGetVolume() { return 1.0; }

    public static boolean clipboardHasText() { return false; }
    public static void clipboardSetText(String text) {}
    public static String clipboardGetText() { return ""; }
    public static void RequestPermission(String perm) {}
    public static void powersaveEnable(boolean enable) {}
    public static void MoveTaskToBack() {
        Log.e(TAG, "MoveTaskToBack was called by libyoyo.so!");
    }

    public static void analyticsEvent(String name) {}
    public static void analyticsEventExt(String name, String[] params) {}
    public static void cloudStringSave(String key, String val, int id) {}
    public static void cloudSynchronise(int id) {}
    public static int DynamicAssetExists(String name) { return 0; }
    public static void DumpUsedMemory() {}
    public static int UsingGL2() { return 1; }
    public static void PushLocalNotification(float fireTime, String title, String msg, String data) {}
    public static int PushGetLocalNotification(int id, int dsMap) { return 0; }
    public static int PushCancelLocalNotification(int id) { return 0; }

    public static String GetUDID() { return "00000000-0000-0000-0000-000000000000"; }
    public static void WaitForVsync() {}
    public static int HasVsyncHandler() { return 0; }
    public static void VirtualKeyboardToggle(boolean a, int b, int c, int d, boolean e, int[] f) {}
    public static boolean VirtualKeyboardGetStatus() { return false; }
    public static int VirtualKeyboardGetHeight() { return 0; }
    public static void OnKeyboardStringSet(int[] chars) {}
    public static void EnumerateGamepadDevices() {
        Log.i(TAG, "EnumerateGamepadDevices called by GameMaker engine");
        try {
            int[] deviceIds = android.view.InputDevice.getDeviceIds();
            int gpIdx = 0;
            for (int devId : deviceIds) {
                android.view.InputDevice dev = android.view.InputDevice.getDevice(devId);
                if (dev != null) {
                    int sources = dev.getSources();
                    if ((sources & android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD ||
                        (sources & android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK) {
                        Log.i(TAG, "Registering physical gamepad [" + gpIdx + "]: " + dev.getName());
                        registerGamepadConnected(gpIdx, 16, 6);
                        gpIdx++;
                    }
                }
            }
            if (gpIdx == 0) {
                Log.i(TAG, "Registering default gamepad 0");
                registerGamepadConnected(0, 16, 6);
            }
        } catch (Throwable e) {
            Log.e(TAG, "Error in EnumerateGamepadDevices: " + e.getMessage());
        }
    }
    public static ByteBuffer[] EnumerateCertificates() { return new ByteBuffer[0]; }



    // Native methods in libyoyo.so (from gmloader-next & libyoyo.so symbols)
    public static native int Startup(String apkPath, String savePath, String packagePath, int sleepMargin);
    public static native int Process(int width, int height, float accelX, float accelY, float accelZ, int keypadStatus, int orientation, float refreshRate);
    public static native void TouchEvent(int type, int index, float x, float y);
    public static native void RenderSplash(String apkPath, String splashName, int screenWidth, int screenHeight, int texWidth, int texHeight, int pngWidth, int pngHeight);
    public static native void Resume(int param);
    public static native void Pause(int param);
    public static native void KeyEvent(int type, int keycode, int keychar, int eventSource);
    public static native void SetKeyValue(int type, int val, String valString);
    public static native String GetAppID(int param);
    public static native String GetSaveFileName(String fileName);
    public static native int iCadeEventDispatch(int button, byte down);
    public static native void registerGamepadConnected(int deviceIndex, int buttonCount, int axisCount);
    public static native int initGLFuncs(int usingGL2);
    public static native byte canFlip();

    public static native void onGPDeviceAdded(int dev, String name, int numButtons, int numAxes);
    public static native void onGPDeviceRemoved(int dev);
    public static native void onGPNativeAxis(int dev, int axis, float val);
    public static native void onGPKeyDown(int dev, int keycode);
    public static native void onGPKeyUp(int dev, int keycode);
}

