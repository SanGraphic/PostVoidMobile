#include "jni_bridge.h"
#include "gms_runtime.h"
#include "renderer_gles.h"
#include "oboe_audio_engine.h"
#include "touch_controller.h"
#include "game_controller.h"

JNIEXPORT jboolean JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeInit(
    JNIEnv* env, jobject thiz, jstring dataPath, jstring savePath, jint width, jint height) {

    const char* nativeDataPath = env->GetStringUTFChars(dataPath, nullptr);
    const char* nativeSavePath = env->GetStringUTFChars(savePath, nullptr);

    LOGI("[JNI] Initializing Native Engine (Data: %s, Save: %s, Res: %dx%d)",
         nativeDataPath, nativeSavePath, width, height);

    // Initialize Renderer
    RendererGLES::get().initialize(width, height);

    // Initialize Audio Engine
    OboeAudioEngine::get().initialize();

    // Initialize GMS Runtime (Boots into room_preload -> room_splash_screen -> room_epilepsy -> room_menu)
    GMS_Runtime::get().initialize(nativeDataPath, nativeSavePath);

    env->ReleaseStringUTFChars(dataPath, nativeDataPath);
    env->ReleaseStringUTFChars(savePath, nativeSavePath);

    return JNI_TRUE;
}


JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeStep(
    JNIEnv* env, jobject thiz) {
    GMS_Runtime::get().step();
    GameController::get().update();
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeRender(
    JNIEnv* env, jobject thiz) {
    RendererGLES::get().beginFrame();

    GameController::get().render();
    GMS_Runtime::get().render();

    RendererGLES::get().endFrame();
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeResize(
    JNIEnv* env, jobject thiz, jint width, jint height) {
    RendererGLES::get().resize(width, height);
    GMS_Runtime::get().resize(width, height);
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeTouchEvent(
    JNIEnv* env, jobject thiz, jint pointerId, jint action, jfloat x, jfloat y) {
    TouchController::get().handleTouchEvent(pointerId, action, x, y);
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeKeyEvent(
    JNIEnv* env, jobject thiz, jint keyCode, jboolean isDown) {
    // Key codes mapped to GMS keyboard system
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeGamepadAxis(
    JNIEnv* env, jobject thiz, jint axisId, jfloat value) {
    // Axis values mapped to gamepad controller
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativePause(
    JNIEnv* env, jobject thiz) {
    LOGI("[JNI] App Paused");
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeResume(
    JNIEnv* env, jobject thiz) {
    LOGI("[JNI] App Resumed");
}

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeShutdown(
    JNIEnv* env, jobject thiz) {
    LOGI("[JNI] Shutting down native engine...");
    GMS_Runtime::get().shutdown();
    OboeAudioEngine::get().shutdown();
    RendererGLES::get().shutdown();
}
