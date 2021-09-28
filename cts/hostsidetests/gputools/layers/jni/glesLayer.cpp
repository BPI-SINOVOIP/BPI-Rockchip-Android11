/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/log.h>
#include <cstring>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string>
#include <string.h>
#include <unordered_map>

#define xstr(a) str(a)
#define str(a) #a

#define LOG_TAG "glesLayer" xstr(LAYERNAME)

#define ALOGI(msg, ...) \
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, (msg), __VA_ARGS__)


// Announce if anything loads this layer.  LAYERNAME is defined in Android.mk
class StaticLogMessage {
    public:
        StaticLogMessage(const char* msg) {
            ALOGI("%s", msg);
    }
};
StaticLogMessage g_initMessage("glesLayer" xstr(LAYERNAME) " loaded");

typedef __eglMustCastToProperFunctionPointerType EGLFuncPointer;
typedef void* (*PFNEGLGETNEXTLAYERPROCADDRESSPROC)(void*, const char*);

namespace {

std::unordered_map<std::string, EGLFuncPointer> funcMap;

EGLAPI void EGLAPIENTRY glesLayer_glCompileShaderA (GLuint shader) {
    ALOGI("%s%u", "glesLayer_glCompileShaderA called with parameter ", shader);

    if (funcMap.find("glCompileShader") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for glCompileShader");

    EGLFuncPointer entry = funcMap["glCompileShader"];

    PFNGLCOMPILESHADERPROC next = reinterpret_cast<PFNGLCOMPILESHADERPROC>(entry);

    next(shader);
}

EGLAPI void EGLAPIENTRY glesLayer_glCompileShaderB (GLuint shader) {
    ALOGI("%s%u", "glesLayer_CompileShaderB called with parameter ", shader);

    if (funcMap.find("glCompileShader") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for glCompileShader");

    EGLFuncPointer entry = funcMap["glCompileShader"];

    PFNGLCOMPILESHADERPROC next = reinterpret_cast<PFNGLCOMPILESHADERPROC>(entry);

    next(shader);
}

EGLAPI void EGLAPI glesLayer_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    ALOGI("%s %i, %i, %i", "glesLayer_glDrawArraysInstanced called with parameters (minus GLenum):", first, count, instancecount);

    if (funcMap.find("glDrawArraysInstanced") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for glDrawArraysInstanced");

    EGLFuncPointer entry = funcMap["glDrawArraysInstanced"];

    PFNGLDRAWARRAYSINSTANCEDPROC next = reinterpret_cast<PFNGLDRAWARRAYSINSTANCEDPROC>(entry);

    next(mode, first, count, instancecount);
}

EGLAPI void EGLAPIENTRY glesLayer_glBindBuffer(GLenum target, GLuint buffer) {
    ALOGI("%s %i", "glesLayer_glBindBuffer called with parameters (minus GLenum):", buffer);

    if (funcMap.find("glBindBuffer") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for glBindBuffer");

    EGLFuncPointer entry = funcMap["glBindBuffer"];

    PFNGLBINDBUFFERPROC next = reinterpret_cast<PFNGLBINDBUFFERPROC>(entry);

    next(target, buffer);
}

EGLAPI const GLubyte* EGLAPIENTRY glesLayer_glGetString(GLenum name) {
    ALOGI("%s %lu", "glesLayer_glGetString called with parameters:", (unsigned long)name);

    if (funcMap.find("glGetString") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for glGetString");

    EGLFuncPointer entry = funcMap["glGetString"];

    PFNGLGETSTRINGPROC next = reinterpret_cast<PFNGLGETSTRINGPROC>(entry);

    return next(name);
}

EGLAPI EGLDisplay EGLAPIENTRY glesLayer_eglGetDisplay(EGLNativeDisplayType display_type) {
    ALOGI("%s %lu", "glesLayer_eglGetDisplay called with parameters:", (unsigned long)display_type);

    if (funcMap.find("eglGetDisplay") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for eglGetDisplay");

    EGLFuncPointer entry = funcMap["eglGetDisplay"];

    typedef EGLDisplay (*PFNEGLGETDISPLAYPROC)(EGLNativeDisplayType);
    PFNEGLGETDISPLAYPROC next = reinterpret_cast<PFNEGLGETDISPLAYPROC>(entry);

    return next(display_type);
}

EGLAPI EGLBoolean EGLAPIENTRY glesLayer_eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
    ALOGI("%s %lu %li %li", "glesLayer_eglInitialize called with parameters:", (unsigned long)dpy, major ? (long)*major : 0, minor ? (long)*minor : 0);

    if (funcMap.find("eglInitialize") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for eglInitialize");

    EGLFuncPointer entry = funcMap["eglInitialize"];

    typedef EGLBoolean (*PFNEGLINITIALIZEPROC)(EGLDisplay, EGLint*, EGLint*);
    PFNEGLINITIALIZEPROC next = reinterpret_cast<PFNEGLINITIALIZEPROC>(entry);

    return next(dpy, major, minor);
}

EGLAPI EGLBoolean EGLAPIENTRY glesLayer_eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) {

    const char* msg = "glesLayer_eglChooseConfig called in glesLayer" xstr(LAYERNAME);
    ALOGI("%s", msg);

    if (funcMap.find("eglChooseConfig") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for eglChooseConfig");

    EGLFuncPointer entry = funcMap["eglChooseConfig"];

    typedef EGLBoolean (*PFNEGLCHOOSECONFIGPROC)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
    PFNEGLCHOOSECONFIGPROC next = reinterpret_cast<PFNEGLCHOOSECONFIGPROC>(entry);

    return next(dpy, attrib_list, configs, config_size, num_config);
}

EGLAPI EGLBoolean EGLAPIENTRY glesLayer_eglSwapBuffersWithDamageKHR (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects) {

    const char* msg = "glesLayer_eglSwapBuffersWithDamageKHR called in glesLayer" xstr(LAYERNAME);
    ALOGI("%s", msg);

    if (funcMap.find("eglSwapBuffersWithDamageKHR") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for eglSwapBuffersWithDamageKHR");

    EGLFuncPointer entry = funcMap["eglSwapBuffersWithDamageKHR"];

    typedef EGLBoolean (*PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC)(EGLDisplay, EGLSurface, EGLint*, EGLint);
    PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC next = reinterpret_cast<PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC>(entry);

    return next(dpy, surface, rects, n_rects);
}

EGLAPI void* EGLAPIENTRY glesLayer_eglGetProcAddress (const char* procname) {

    const char* msg = "glesLayer_eglGetProcAddress called in glesLayer" xstr(LAYERNAME) " for:";
    ALOGI("%s%s", msg, procname);

    if (funcMap.find("eglGetProcAddress") == funcMap.end())
        ALOGI("%s", "Unable to find funcMap entry for eglGetProcAddress");

    EGLFuncPointer entry = funcMap["eglGetProcAddress"];

    typedef void* (*PFNEGLGETPROCADDRESSPROC)(const char*);
    PFNEGLGETPROCADDRESSPROC next = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(entry);

    return next(procname);
}

EGLAPI EGLFuncPointer EGLAPIENTRY eglGPA(const char* funcName) {

#define GETPROCADDR(func) if(!strcmp(funcName, #func)) { \
ALOGI("%s%s%s", "Returning glesLayer_" #func " for ", funcName, " in eglGPA"); \
return (EGLFuncPointer)glesLayer_##func; \
}

    if (strcmp("A", xstr(LAYERNAME)) == 0) {

        const char* targetFunc = "glCompileShader";
        if (strcmp(targetFunc, funcName) == 0) {
            ALOGI("%s%s%s", "Returning glesLayer_glCompileShaderA for ", funcName, " in eglGPA");
            return (EGLFuncPointer)glesLayer_glCompileShaderA;
        }

        GETPROCADDR(glDrawArraysInstanced);

    } else if (strcmp("B", xstr(LAYERNAME)) == 0) {

        const char* targetFunc = "glCompileShader";
        if (strcmp(targetFunc, funcName) == 0) {
            ALOGI("%s%s%s", "Returning glesLayer_glCompileShaderB for ", funcName, " in eglGPA");
            return (EGLFuncPointer)glesLayer_glCompileShaderB;
        }

        GETPROCADDR(glBindBuffer);
    }

    GETPROCADDR(glGetString);
    GETPROCADDR(eglGetDisplay);
    GETPROCADDR(eglInitialize);
    GETPROCADDR(eglChooseConfig);
    GETPROCADDR(eglSwapBuffersWithDamageKHR);
    GETPROCADDR(eglGetProcAddress);

    // Don't return anything for unrecognized functions
    return nullptr;
}

EGLAPI void EGLAPIENTRY glesLayer_InitializeLayer(void* layer_id, PFNEGLGETNEXTLAYERPROCADDRESSPROC get_next_layer_proc_address) {
    ALOGI("%s%llu%s%llu", "glesLayer_InitializeLayer called with layer_id (", (unsigned long long) layer_id,
                              ") get_next_layer_proc_address (", (unsigned long long) get_next_layer_proc_address);

    // Pick a real function to look up and test the pointer we've been handed
    const char* func = "eglGetProcAddress";

    ALOGI("%s%s%s%llu%s%llu%s", "Looking up address of ", func,
                                " using get_next_layer_proc_address (", (unsigned long long) get_next_layer_proc_address,
                                ") with layer_id (", (unsigned long long) layer_id,
                                ")");

    void* gpa = get_next_layer_proc_address(layer_id, func);

    // Pick a fake function to look up and test the pointer we've been handed
    func = "eglFoo";

    ALOGI("%s%s%s%llu%s%llu%s", "Looking up address of ", func,
                                " using get_next_layer_proc_address (", (unsigned long long) get_next_layer_proc_address,
                                ") with layer_id (", (unsigned long long) layer_id,
                                ")");

    gpa = get_next_layer_proc_address(layer_id, func);

    ALOGI("%s%llu%s%s", "Got back (", (unsigned long long) gpa, ") for ", func);
}

EGLAPI EGLFuncPointer EGLAPIENTRY glesLayer_GetLayerProcAddress(const char* funcName, EGLFuncPointer next) {

    EGLFuncPointer entry = eglGPA(funcName);

    if (entry != nullptr) {
        ALOGI("%s%s%s%llu%s", "Setting up glesLayer version of ", funcName, " calling down with: next (", (unsigned long long) next, ")");
        funcMap[std::string(funcName)] = next;
        return entry;
    }

    // If the layer does not intercept the function, just return original func pointer
    return next;
}

}  // namespace

extern "C" {

    __attribute((visibility("default"))) EGLAPI void AndroidGLESLayer_Initialize(void* layer_id,
            PFNEGLGETNEXTLAYERPROCADDRESSPROC get_next_layer_proc_address) {
        return (void)glesLayer_InitializeLayer(layer_id, get_next_layer_proc_address);
    }

    __attribute((visibility("default"))) EGLAPI void* AndroidGLESLayer_GetProcAddress(const char *funcName, EGLFuncPointer next) {
        return (void*)glesLayer_GetLayerProcAddress(funcName, next);
    }
}
