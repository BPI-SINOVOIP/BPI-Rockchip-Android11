#define LOG_TAG "RkNativeDisplayManager"

#include "android_os_Parcel.h"
#include "android_util_Binder.h"
#include "android/graphics/Bitmap.h"
#include "android/graphics/GraphicsJNI.h"
#include "core_jni_helpers.h"

#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedUtfChars.h>
#include <jni.h>
#include <memory>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <inttypes.h>
#include <sstream>

#include <linux/netlink.h>
#include <sys/socket.h>

#include <rockchip/hardware/outputmanager/1.0/IRkOutputManager.h>
#include <unordered_map>

namespace android{

using namespace rockchip::hardware::outputmanager::V1_0;

using ::rockchip::hardware::outputmanager::V1_0::IRkOutputManager;
using ::rockchip::hardware::outputmanager::V1_0::Result;
using ::rockchip::hardware::outputmanager::V1_0::RkDrmMode;
using android::hardware::hidl_handle;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;
using android::hardware::Return;
using android::hardware::Void;

#define BASE_OFFSET 8*1024
#define DEFAULT_BRIGHTNESS  50
#define DEFAULT_CONTRAST  50
#define DEFAULT_SATURATION  50
#define DEFAULT_HUE  50
#define DEFAULT_OVERSCAN_VALUE 100

static struct {
    jclass clazz;
    jmethodID ctor;
    jfieldID width;
    jfieldID height;
    jfieldID refreshRate;
    jfieldID clock;
    jfieldID flags;
    jfieldID interlaceFlag;
    jfieldID yuvFlag;
    jfieldID connectorId;
    jfieldID mode_type;
    jfieldID idx;
    jfieldID hsync_start;
    jfieldID hsync_end;
    jfieldID htotal;
    jfieldID hskew;
    jfieldID vsync_start;
    jfieldID vsync_end;
    jfieldID vtotal;
    jfieldID vscan;
} gRkPhysicalDisplayInfoClassInfo;

static struct{
    jclass clazz;
    jmethodID ctor;
    jfieldID color_capa;
    jfieldID depth_capa;
}gRkColorModeSupportInfo;

static struct{
    jclass clazz;
    jmethodID ctor;
    jfieldID type;
    jfieldID id;
    jfieldID state;
}gRkConnectorInfo;

sp<IRkOutputManager> mComposer = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////

static void nativeSaveConfig(JNIEnv* env, jobject obj)
{
    if (mComposer != nullptr)
        mComposer->saveConfig();
}

static void nativeSetMode(JNIEnv* env, jobject obj, jint dpy, jint iface_type, jstring mode)
{
    const char* mMode = env->GetStringUTFChars(mode, NULL);

    if (mComposer != nullptr)
        mComposer->setMode(dpy, mMode);
    env->ReleaseStringUTFChars(mode, mMode);
}

static int nativeSetHue(JNIEnv* env, jobject obj, jint dpy, jint degree)
{
    Result ret = Result::UNKNOWN;
    if (mComposer != nullptr)
        ret = mComposer->setHue(dpy, degree);
    return 0;
}

static int nativeSetSaturation(JNIEnv* env, jobject obj, jint dpy, jint saturation)
{
    Result ret = Result::UNKNOWN;
    if (mComposer != nullptr)
        ret = mComposer->setSaturation(dpy, saturation);
    return 0;
}

static int nativeSetContrast(JNIEnv* env, jobject obj, jint dpy, jint contrast)
{
    Result ret = Result::UNKNOWN;
    if (mComposer != nullptr)
        ret = mComposer->setContrast(dpy, contrast);
    return 0;
}

static int nativeSetBrightness(JNIEnv* env, jobject obj, jint dpy, jint brightness)
{
    Result ret = Result::UNKNOWN;
    if (mComposer != nullptr)
        ret = mComposer->setBrightness(dpy, brightness);
    return 0;
}

static int nativeSetScreenScale(JNIEnv* env, jobject obj, jint dpy, jint direction, jint value)
{
    Result ret = Result::UNKNOWN;
    if (mComposer != nullptr)
        ret = mComposer->setScreenScale(dpy, direction, value);
    return 0;
}

static int nativeSetHdrMode(JNIEnv* env, jobject obj, jint dpy, jint hdrMode)
{
    Result ret = Result::UNKNOWN;
    if (mComposer != nullptr)
        ret = mComposer->setHdrMode(dpy, hdrMode);
    return 0;
}

static int nativeSetColorMode(JNIEnv* env, jobject obj, jint dpy, jstring mode)
{
    const char* mMode = env->GetStringUTFChars(mode, NULL);
    Result ret = Result::UNKNOWN;

    if (mComposer != nullptr)
        ret = mComposer->setColorMode(dpy, mMode);
    env->ReleaseStringUTFChars(mode, mMode);
    return 0;
}

static jstring nativeGetCurCorlorMode(JNIEnv* env, jobject obj, jint dpy)
{
    char colorMode[PROPERTY_VALUE_MAX];
    std::string info;

    if (mComposer != nullptr)
    {
        mComposer->getCurCorlorMode(dpy,
                [&](const auto& tmpResult, const auto& tmpMode)
                {
                    if (tmpResult == Result::OK) {
                        info = tmpMode.c_str();
                        sprintf(colorMode, "%s", info.c_str());;
                    }
                });
    }
    return env->NewStringUTF(colorMode);
}

static jstring nativeGetCurMode(JNIEnv* env, jobject obj, jint dpy)
{
    char resolution[128];
    std::string info;

    if (mComposer != nullptr)
    {
        mComposer->getCurMode(dpy,
                [&](const auto& tmpResult, const auto& tmpMode)
                {
                    if (tmpResult == Result::OK) {
                        info = tmpMode.c_str();
                        sprintf(resolution, "%s", info.c_str());;
                    }
                });
    }
    return env->NewStringUTF(resolution);
}

static jint nativeGetNumConnectors(JNIEnv* env, jobject obj)
{
    int numConnectors=0;

    if (mComposer != nullptr)
    {
        mComposer->getNumConnectors(0,
                [&](const auto& tmpResult, const auto& tmpCnt)
                {
                    if (tmpResult == Result::OK) {
                        numConnectors = tmpCnt;
                    }
                });
    }
    ALOGV("%s:%d numConnectors = %d", __FUNCTION__, __LINE__, numConnectors);
    return static_cast<jint>(numConnectors);
}


static jint nativeGetConnectionState(JNIEnv* env, jobject obj, jint dpy)
{
    int cur_state = 0;

    if (mComposer != nullptr)
    {
        mComposer->getConnectState(dpy,
                [&](const auto& tmpResult, const auto& tmpState)
                {
                    if (tmpResult == Result::OK) {
                        cur_state = tmpState;
                    }
                });
    }
    ALOGV("%s:%d state = %d", __FUNCTION__, __LINE__, cur_state);
    return static_cast<jint>(cur_state);
}

static jint nativeGetBuiltIn(JNIEnv* env, jobject obj, jint dpy)
{
    int built_in=0;

    if (mComposer != nullptr)
    {
        mComposer->getBuiltIn(dpy,
                [&](const auto& tmpResult, const auto& tmpBuildin)
                {
                    if (tmpResult == Result::OK) {
                        built_in = tmpBuildin;
                    }
                });
    }
    return static_cast<jint>(built_in);
}

static jobject nativeGetCorlorModeConfigs(JNIEnv* env, jclass clazz,
        jint dpy){
    jobject infoObj = env->NewObject(gRkColorModeSupportInfo.clazz,
            gRkColorModeSupportInfo.ctor);
    hidl_vec<uint32_t> capaities;

    if (mComposer != nullptr)
    {
        mComposer->getCorlorModeConfigs(dpy,
                [&](const auto& tmpResult, const auto& tmpConfigs)
                {
                    if (tmpResult == Result::OK) {
                        capaities = tmpConfigs;
                    }
                });
    }

    for (size_t i = 0; i < capaities.size(); ++i) {
        if (i==0)
            env->SetIntField(infoObj, gRkColorModeSupportInfo.color_capa, (int)capaities[i]);
        else
            env->SetIntField(infoObj, gRkColorModeSupportInfo.depth_capa, (int)capaities[i]);
    }
    return infoObj;
}

static jintArray nativeGetOverscan(JNIEnv* env, jobject obj, jint dpy)
{
    jintArray jOverscanArray = env->NewIntArray(4);
    hidl_vec<uint32_t> overscan;
    jint *mOverscan = new jint[4];

    if (mComposer != nullptr)
    {
        mComposer->getOverscan(dpy,
                [&](const auto& tmpResult, const auto& tmpOverscans)
                {
                    if (tmpResult == Result::OK) {
                        overscan = tmpOverscans;
                    }
                });
    }

    if (overscan.size() != 4)
    {
        mOverscan[0] = DEFAULT_OVERSCAN_VALUE;
        mOverscan[1] = DEFAULT_OVERSCAN_VALUE;
        mOverscan[2] = DEFAULT_OVERSCAN_VALUE;
        mOverscan[3] = DEFAULT_OVERSCAN_VALUE;
    } else {
        mOverscan[0] = overscan[0];
        mOverscan[1] = overscan[1];
        mOverscan[2] = overscan[2];
        mOverscan[3] = overscan[3];
    }

    ALOGV("overscan: %d %d %d %d", overscan[0], overscan[1], overscan[2], overscan[3]);
    env->SetIntArrayRegion(jOverscanArray, 0, 4, mOverscan);
    return jOverscanArray;
}

static jintArray nativeGetBcsh(JNIEnv* env, jobject obj, jint dpy)
{
    jintArray jBcshArray = env->NewIntArray(4);
    hidl_vec<uint32_t> hidlBcsh;
    jint *mBcsh = new jint[4];

    if (mComposer != nullptr)
    {
        mComposer->getBcsh(dpy,
                [&](const auto& tmpResult, const auto& tmpBcshs)
                {
                    if (tmpResult == Result::OK) {
                        hidlBcsh = tmpBcshs;
                    }
                });
    }

    if (hidlBcsh.size() != 4)
    {
        mBcsh[0] = DEFAULT_BRIGHTNESS;
        mBcsh[1] = DEFAULT_CONTRAST;
        mBcsh[2] = DEFAULT_SATURATION;
        mBcsh[3] = DEFAULT_HUE;
    } else {
        mBcsh[0] = hidlBcsh[0];
        mBcsh[1] = hidlBcsh[1];
        mBcsh[2] = hidlBcsh[2];
        mBcsh[3] = hidlBcsh[3];
    }
    ALOGV("bcsh %d %d %d %d", hidlBcsh[0], hidlBcsh[1], hidlBcsh[2], hidlBcsh[3]);
    env->SetIntArrayRegion(jBcshArray, 0, 4, mBcsh);
    return jBcshArray;
}

static jint nativeSetGamma(JNIEnv* env, jobject obj,
        jint dpy, jint size, jintArray r, jintArray g, jintArray b){

    std::vector<uint16_t> hidlRed;
    std::vector<uint16_t> hidlGreen;
    std::vector<uint16_t> hidlBlue;
    jsize jrsize = env->GetArrayLength(r);
    jsize jgsize = env->GetArrayLength(g);
    jsize jbsize = env->GetArrayLength(b);

    jint* jr_data = env->GetIntArrayElements(r, /* isCopy */ NULL);
    jint* jg_data = env->GetIntArrayElements(g, /* isCopy */ NULL);
    jint* jb_data = env->GetIntArrayElements(b, /* isCopy */ NULL);

    for (int i=0;i<jrsize;i++) {
        hidlRed.push_back((uint16_t)jr_data[i]);
    }
    for (int i=0;i<jgsize;i++) {
        hidlGreen.push_back((uint16_t)jg_data[i]);
    }
    for (int i=0;i<jbsize;i++) {
        hidlBlue.push_back((uint16_t)jb_data[i]);
    }
    if (mComposer != nullptr)
    {
        mComposer->setGamma(dpy, size, hidlRed, hidlGreen, hidlBlue);
    }

    env->ReleaseIntArrayElements(r, jr_data, 0);
    env->ReleaseIntArrayElements(g, jg_data, 0);
    env->ReleaseIntArrayElements(b, jb_data, 0);

    return 0;
}

static jint nativeSet3DLut(JNIEnv* env, jobject obj,
        jint dpy, jint size, jintArray r, jintArray g, jintArray b){

    std::vector<uint16_t> hidlRed;
    std::vector<uint16_t> hidlGreen;
    std::vector<uint16_t> hidlBlue;
    jsize jrsize = env->GetArrayLength(r);
    jsize jgsize = env->GetArrayLength(g);
    jsize jbsize = env->GetArrayLength(b);

    jint* jr_data = env->GetIntArrayElements(r, /* isCopy */ NULL);
    jint* jg_data = env->GetIntArrayElements(g, /* isCopy */ NULL);
    jint* jb_data = env->GetIntArrayElements(b, /* isCopy */ NULL);

    for (int i=0;i<jrsize;i++) {
        hidlRed.push_back((uint16_t)jr_data[i]);
    }
    for (int i=0;i<jgsize;i++) {
        hidlGreen.push_back((uint16_t)jg_data[i]);
    }
    for (int i=0;i<jbsize;i++) {
        hidlBlue.push_back((uint16_t)jb_data[i]);
    }

    if (mComposer != nullptr)
    {
        mComposer->set3DLut(dpy, size, hidlRed, hidlGreen, hidlBlue);
    }

    env->ReleaseIntArrayElements(r, jr_data, 0);
    env->ReleaseIntArrayElements(g, jg_data, 0);
    env->ReleaseIntArrayElements(b, jb_data, 0);

    return 0;
}

static jobjectArray nativeGetDisplayConfigs(JNIEnv* env, jclass clazz,
        jint dpy) {
    hidl_vec<RkDrmMode> mModes;
    if (mComposer != nullptr)
    {
        mComposer->getDisplayModes(dpy,
                [&](const auto& tmpResult, const auto& tmpModes)
                {
                    if (tmpResult == Result::OK) {
                        mModes = tmpModes;
                    }
                });
    }

    jobjectArray configArray = env->NewObjectArray(mModes.size(),
            gRkPhysicalDisplayInfoClassInfo.clazz, NULL);

    for (size_t c=0;c<mModes.size();c++) {
        RkDrmMode tmpMode = mModes[c];
        jobject infoObj = env->NewObject(gRkPhysicalDisplayInfoClassInfo.clazz,
                gRkPhysicalDisplayInfoClassInfo.ctor);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.width, tmpMode.width);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.height, tmpMode.height);
        env->SetFloatField(infoObj, gRkPhysicalDisplayInfoClassInfo.refreshRate, tmpMode.refreshRate);//1000 * 1000 * 1000 /
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.clock, tmpMode.clock);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.flags, tmpMode.flags);
        env->SetBooleanField(infoObj, gRkPhysicalDisplayInfoClassInfo.interlaceFlag,
                             tmpMode.interlaceFlag>0?1:0);
        env->SetBooleanField(infoObj, gRkPhysicalDisplayInfoClassInfo.yuvFlag, tmpMode.yuvFlag>0?1:0);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.connectorId, tmpMode.connectorId);//mode_type
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.mode_type, tmpMode.mode_type);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.idx, tmpMode.idx);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.hsync_start, tmpMode.hsync_start);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.hsync_end, tmpMode.hsync_end);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.htotal, tmpMode.htotal);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.hskew, tmpMode.hskew);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.vsync_start, tmpMode.vsync_start);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.vsync_end, tmpMode.vsync_end);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.vtotal, tmpMode.vtotal);
        env->SetIntField(infoObj, gRkPhysicalDisplayInfoClassInfo.vscan, tmpMode.vscan);
        ALOGV("dpy%d %dx%d info.fps %f clock %d hsync_start %d hsync_enc %d htotal %d hskew %d",
                dpy, tmpMode.width, tmpMode.height, tmpMode.refreshRate,
                tmpMode.clock, tmpMode.flags, tmpMode.hsync_start,
                tmpMode.htotal, tmpMode.hskew);
        ALOGV("vsync_start %d vsync_end %d vtotal %d vscan %d flags 0x%x",
                tmpMode.vsync_start, tmpMode.vsync_end,
                tmpMode.vtotal, tmpMode.vscan, tmpMode.flags);

        env->SetObjectArrayElement(configArray, static_cast<jsize>(c), infoObj);
        env->DeleteLocalRef(infoObj);
    }
    return configArray;
}

static jobjectArray nativeGetConnectorInfo(JNIEnv* env, jclass clazz) {
    hidl_vec<RkConnectorInfo> minfo;
    if (mComposer != nullptr)
    {
        mComposer->getConnectorInfo([&](const auto& tmpResult, const auto& tmpInfo)
        {
             if (tmpResult == Result::OK) {
                 minfo = tmpInfo;
             }
        });
    }

    jobjectArray infoArray = env->NewObjectArray(minfo.size(),
            gRkConnectorInfo.clazz, NULL);

    for (size_t c=0;c<minfo.size();c++) {
        RkConnectorInfo tmpConnectorInfo = minfo[c];
        jobject infoObj = env->NewObject(gRkConnectorInfo.clazz,
                gRkConnectorInfo.ctor);
        env->SetIntField(infoObj, gRkConnectorInfo.type, tmpConnectorInfo.type);
        env->SetIntField(infoObj, gRkConnectorInfo.id, tmpConnectorInfo.id);
        env->SetIntField(infoObj, gRkConnectorInfo.state, tmpConnectorInfo.state);

        env->SetObjectArrayElement(infoArray, static_cast<jsize>(c), infoObj);
        env->DeleteLocalRef(infoObj);
    }
    return infoArray;
}

static void nativeUpdateConnectors(JNIEnv* env, jobject obj) {
    ALOGD("nativeInit failed to get IRkOutputManager");
    if (mComposer != nullptr)
        mComposer->hotPlug();
}

static void nativeInit(JNIEnv* env, jobject obj) {
        mComposer = IRkOutputManager::getService();
        if (mComposer != nullptr) {
            mComposer->initial();
        } else {
            ALOGD("nativeInit failed to get IRkOutputManager");
        }
}

static int nativeUpdateDispHeader(JNIEnv* env, jobject obj) {
    Result res = Result::UNKNOWN;
    if (mComposer != nullptr)
        res = mComposer->updateDispHeader();
    if(res == Result::OK){
        return 0;
    }else {
        return -1;
    }
}

// ----------------------------------------------------------------------------
//com.android.server.rkdisplay
static const JNINativeMethod sRkDrmModeMethods[] = {
    {"nativeInit", "()V",
        (void*) nativeInit},
    {"nativeUpdateConnectors", "()V",
        (void*) nativeUpdateConnectors},
    {"nativeSaveConfig", "()V",
        (void*) nativeSaveConfig},
    {"nativeGetDisplayConfigs", "(I)[Lcom/android/server/rkdisplay/RkDisplayModes$RkPhysicalDisplayInfo;",
        (void*)nativeGetDisplayConfigs},
    {"nativeGetNumConnectors", "()I",
        (void*)nativeGetNumConnectors},
    {"nativeSetMode", "(IILjava/lang/String;)V",
        (void*)nativeSetMode},
    {"nativeGetCurMode", "(I)Ljava/lang/String;",
        (void*)nativeGetCurMode},
    {"nativeGetCurCorlorMode", "(I)Ljava/lang/String;",
        (void*)nativeGetCurCorlorMode},
    {"nativeGetBuiltIn", "(I)I",
        (void*)nativeGetBuiltIn},
    {"nativeGetConnectionState", "(I)I",
        (void*)nativeGetConnectionState},
    {"nativeGetCorlorModeConfigs", "(I)Lcom/android/server/rkdisplay/RkDisplayModes$RkColorCapacityInfo;",
        (void*)nativeGetCorlorModeConfigs},
    {"nativeGetBcsh", "(I)[I",
        (void*)nativeGetBcsh},
    {"nativeGetOverscan", "(I)[I",
        (void*)nativeGetOverscan},
    {"nativeSetGamma", "(II[I[I[I)I",
        (void*)nativeSetGamma},
    {"nativeSet3DLut", "(II[I[I[I)I",
        (void*)nativeSet3DLut},
    {"nativeSetHue", "(II)I",
        (void*)nativeSetHue},
    {"nativeSetSaturation", "(II)I",
        (void*)nativeSetSaturation},
    {"nativeSetContrast", "(II)I",
        (void*)nativeSetContrast},
    {"nativeSetBrightness", "(II)I",
        (void*)nativeSetBrightness},
    {"nativeSetScreenScale", "(III)I",
        (void*)nativeSetScreenScale},
    {"nativeSetHdrMode", "(II)I",
        (void*)nativeSetHdrMode},
    {"nativeSetColorMode", "(ILjava/lang/String;)I",
        (void*)nativeSetColorMode},
    {"nativeGetConnectorInfo", "()[Lcom/android/server/rkdisplay/RkDisplayModes$RkConnectorInfo;",
        (void*)nativeGetConnectorInfo},
    {"nativeUpdateDispHeader", "()I",
        (void*)nativeUpdateDispHeader},

};

#define FIND_CLASS(var, className) \
    var = env->FindClass(className); \
    LOG_FATAL_IF(! var, "Unable to find class " className);

#define GET_METHOD_ID(var, clazz, methodName, methodDescriptor) \
    var = env->GetMethodID(clazz, methodName, methodDescriptor); \
    LOG_FATAL_IF(! var, "Unable to find method " methodName);

#define GET_FIELD_ID(var, clazz, fieldName, fieldDescriptor) \
    var = env->GetFieldID(clazz, fieldName, fieldDescriptor); \
    LOG_FATAL_IF(! var, "Unable to find field " fieldName);

int register_com_android_server_rkdisplay_RkDisplayModes(JNIEnv* env)
{
    int res = jniRegisterNativeMethods(env, "com/android/server/rkdisplay/RkDisplayModes",
            sRkDrmModeMethods, NELEM(sRkDrmModeMethods));
    LOG_FATAL_IF(res < 0, "Unable to register native methods register_com_android_server_rkdisplay_RkDisplayModes");
    (void)res; // Don't complain about unused variable in the LOG_NDEBUG case

    jclass clazz;
    FIND_CLASS(clazz, "com/android/server/rkdisplay/RkDisplayModes");

    FIND_CLASS(gRkPhysicalDisplayInfoClassInfo.clazz, "com/android/server/rkdisplay/RkDisplayModes$RkPhysicalDisplayInfo");
    gRkPhysicalDisplayInfoClassInfo.clazz = jclass(env->NewGlobalRef(gRkPhysicalDisplayInfoClassInfo.clazz));
    GET_METHOD_ID(gRkPhysicalDisplayInfoClassInfo.ctor,
            gRkPhysicalDisplayInfoClassInfo.clazz, "<init>", "()V");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.width, gRkPhysicalDisplayInfoClassInfo.clazz, "width", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.height, gRkPhysicalDisplayInfoClassInfo.clazz, "height", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.refreshRate, gRkPhysicalDisplayInfoClassInfo.clazz, "refreshRate", "F");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.clock, gRkPhysicalDisplayInfoClassInfo.clazz, "clock", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.flags, gRkPhysicalDisplayInfoClassInfo.clazz, "flags", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.interlaceFlag, gRkPhysicalDisplayInfoClassInfo.clazz, "interlaceFlag", "Z");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.yuvFlag, gRkPhysicalDisplayInfoClassInfo.clazz, "yuvFlag", "Z");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.connectorId, gRkPhysicalDisplayInfoClassInfo.clazz, "connectorId", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.mode_type, gRkPhysicalDisplayInfoClassInfo.clazz, "mode_type", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.idx, gRkPhysicalDisplayInfoClassInfo.clazz, "idx", "I");

    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.hsync_start, gRkPhysicalDisplayInfoClassInfo.clazz, "hsync_start", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.hsync_end, gRkPhysicalDisplayInfoClassInfo.clazz, "hsync_end", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.htotal, gRkPhysicalDisplayInfoClassInfo.clazz, "htotal", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.hskew, gRkPhysicalDisplayInfoClassInfo.clazz, "hskew", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.vsync_start, gRkPhysicalDisplayInfoClassInfo.clazz, "vsync_start", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.vsync_end, gRkPhysicalDisplayInfoClassInfo.clazz, "vsync_end", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.vtotal, gRkPhysicalDisplayInfoClassInfo.clazz, "vtotal", "I");
    GET_FIELD_ID(gRkPhysicalDisplayInfoClassInfo.vscan, gRkPhysicalDisplayInfoClassInfo.clazz, "vscan", "I");

    FIND_CLASS(gRkColorModeSupportInfo.clazz, "com/android/server/rkdisplay/RkDisplayModes$RkColorCapacityInfo");
    gRkColorModeSupportInfo.clazz = jclass(env->NewGlobalRef(gRkColorModeSupportInfo.clazz));
    GET_METHOD_ID(gRkColorModeSupportInfo.ctor,
            gRkColorModeSupportInfo.clazz, "<init>", "()V");
    GET_FIELD_ID(gRkColorModeSupportInfo.color_capa, gRkColorModeSupportInfo.clazz, "color_capa", "I");
    GET_FIELD_ID(gRkColorModeSupportInfo.depth_capa, gRkColorModeSupportInfo.clazz, "depth_capa", "I");

    FIND_CLASS(gRkConnectorInfo.clazz, "com/android/server/rkdisplay/RkDisplayModes$RkConnectorInfo");
    gRkConnectorInfo.clazz = jclass(env->NewGlobalRef(gRkConnectorInfo.clazz));
    GET_METHOD_ID(gRkConnectorInfo.ctor,
            gRkConnectorInfo.clazz, "<init>", "()V");
    GET_FIELD_ID(gRkConnectorInfo.type, gRkConnectorInfo.clazz, "type", "I");
    GET_FIELD_ID(gRkConnectorInfo.id, gRkConnectorInfo.clazz, "id", "I");
    GET_FIELD_ID(gRkConnectorInfo.state, gRkConnectorInfo.clazz, "state", "I");

    return 0;
}
};

