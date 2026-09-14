#include "jni_bridge.h"
#include "gms_types.h"

// JNI OnLoad Initialization
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("[PostVoidRunner] JNI_OnLoad successfully linked!");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("[PostVoidRunner] JNI_OnUnload executed.");
}
