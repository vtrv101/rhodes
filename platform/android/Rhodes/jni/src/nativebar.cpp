#include "JNIRhodes.h"

#include <common/rhoparams.h>

#undef DEFAULT_LOGCATEGORY
#define DEFAULT_LOGCATEGORY "NativeBar"

RHO_GLOBAL void create_nativebar(int bar_type, rho_param *p)
{
    JNIEnv *env = jnienv();
    jclass clsNativeBar = getJNIClass(RHODES_JAVA_CLASS_NATIVEBAR);
    if (!clsNativeBar) return;
    jmethodID midCreate = getJNIClassStaticMethod(env, clsNativeBar, "create", "(ILjava/util/Vector;)V");
    if (!midCreate) return;

    if (p->type != RHO_PARAM_ARRAY) {
        RAWLOG_ERROR("Unexpected parameter type, should be Array");
        return;
    }

    jobject paramsObj = RhoValueConverter(env).createObject(p);
    env->CallStaticVoidMethod(clsNativeBar, midCreate, bar_type, paramsObj);
    env->DeleteLocalRef(paramsObj);
}

RHO_GLOBAL void remove_nativebar()
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_NATIVEBAR);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "remove", "()V");
    if (!mid) return;

    env->CallStaticVoidMethod(cls, mid);
}

RHO_GLOBAL void nativebar_switch_tab(int index)
{
    JNIEnv *env = jnienv();
    jclass cls = getJNIClass(RHODES_JAVA_CLASS_NATIVEBAR);
    if (!cls) return;
    jmethodID mid = getJNIClassStaticMethod(env, cls, "switchTab", "(I)V");
    if (!mid) return;

    env->CallStaticVoidMethod(cls, mid, index);
}
