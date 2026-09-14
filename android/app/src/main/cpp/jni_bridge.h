#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeInit(
    JNIEnv* env, jobject thiz, jstring dataPath, jstring savePath, jint width, jint height);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeStep(
    JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeRender(
    JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeResize(
    JNIEnv* env, jobject thiz, jint width, jint height);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeTouchEvent(
    JNIEnv* env, jobject thiz, jint pointerId, jint action, jfloat x, jfloat y);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeKeyEvent(
    JNIEnv* env, jobject thiz, jint keyCode, jboolean isDown);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeGamepadAxis(
    JNIEnv* env, jobject thiz, jint axisId, jfloat value);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativePause(
    JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeResume(
    JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_com_postvoid_port_PostVoidNativeBridge_nativeShutdown(
    JNIEnv* env, jobject thiz);

#ifdef __cplusplus
}
#endif
