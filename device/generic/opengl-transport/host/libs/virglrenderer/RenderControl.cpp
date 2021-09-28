/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#undef NDEBUG

#include "Context.h"
#include "EglConfig.h"
#include "EglContext.h"
#include "EglImage.h"
#include "EglSurface.h"
#include "EglSync.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <string>

#include <unistd.h>

#include <GLES/gl.h>
#include <GLES/glext.h>

#include <OpenGLESDispatch/EGLDispatch.h>
#include <OpenGLESDispatch/GLESv1Dispatch.h>
#include <OpenGLESDispatch/GLESv3Dispatch.h>

#include "virgl_hw.h"

#include "RenderControl.h"

#include <hardware/gralloc.h>
#include <hardware/gralloc1.h>
#include <nativebase/nativebase.h>
#include <system/window.h>

static void incRefANWB(android_native_base_t* base) {
    ANativeWindowBuffer* anwb = reinterpret_cast<ANativeWindowBuffer*>(base);
    anwb->layerCount++;
}

static void decRefANWB(android_native_base_t* base) {
    ANativeWindowBuffer* anwb = reinterpret_cast<ANativeWindowBuffer*>(base);
    if (anwb->layerCount > 0) {
        anwb->layerCount--;
        if (anwb->layerCount == 0)
            delete anwb;
    }
}
struct FakeANativeWindowBuffer : public ANativeWindowBuffer {
    FakeANativeWindowBuffer() {
        ANativeWindowBuffer();

        common.incRef = incRefANWB;
        common.decRef = decRefANWB;
        layerCount = 0U;
    }
};

static void incRefANW(android_native_base_t* base) {
    ANativeWindow* anw = reinterpret_cast<ANativeWindow*>(base);
    anw->oem[0]++;
}

static void decRefANW(android_native_base_t* base) {
    ANativeWindow* anw = reinterpret_cast<ANativeWindow*>(base);
    if (anw->oem[0] > 0) {
        anw->oem[0]--;
        if (anw->oem[0] == 0)
            delete anw;
    }
}

static int setSwapInterval(ANativeWindow*, int) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static int dequeueBuffer_DEPRECATED(ANativeWindow* window, ANativeWindowBuffer** buffer) {
    if (!window->oem[1])
        return -EINVAL;
    *buffer = reinterpret_cast<ANativeWindowBuffer*>(window->oem[1]);
    window->oem[1] = 0;
    return 0;
}

static int lockBuffer_DEPRECATED(ANativeWindow*, ANativeWindowBuffer*) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static int queueBuffer_DEPRECATED(ANativeWindow*, ANativeWindowBuffer*) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static int query(const ANativeWindow* window, int what, int* value) {
    switch (what) {
        case NATIVE_WINDOW_WIDTH:
            return static_cast<int>(window->oem[2]);
        case NATIVE_WINDOW_HEIGHT:
            return static_cast<int>(window->oem[3]);
        default:
            return -EINVAL;
    }
}

static int perform(ANativeWindow*, int, ...) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static int cancelBuffer_DEPRECATED(ANativeWindow*, ANativeWindowBuffer*) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static int dequeueBuffer(ANativeWindow* window, ANativeWindowBuffer** buffer, int* fenceFd) {
    *fenceFd = -1;
    return dequeueBuffer_DEPRECATED(window, buffer);
}

static int queueBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    if (fenceFd >= 0)
        close(fenceFd);
    return queueBuffer_DEPRECATED(window, buffer);
}

static int cancelBuffer(ANativeWindow* window, ANativeWindowBuffer* buffer, int fenceFd) {
    if (fenceFd >= 0)
        close(fenceFd);
    return cancelBuffer_DEPRECATED(window, buffer);
}

struct FakeANativeWindow : public ANativeWindow {
    FakeANativeWindow(uint32_t width, uint32_t height) {
        ANativeWindow();

        common.incRef = incRefANW;
        common.decRef = decRefANW;
        oem[0] = 0;
        oem[2] = static_cast<intptr_t>(width);
        oem[3] = static_cast<intptr_t>(height);

        this->setSwapInterval = ::setSwapInterval;
        this->dequeueBuffer_DEPRECATED = ::dequeueBuffer_DEPRECATED;
        this->lockBuffer_DEPRECATED = ::lockBuffer_DEPRECATED;
        this->queueBuffer_DEPRECATED = ::queueBuffer_DEPRECATED;
        this->query = ::query;
        this->perform = ::perform;
        this->cancelBuffer_DEPRECATED = ::cancelBuffer_DEPRECATED;
        this->dequeueBuffer = ::dequeueBuffer;
        this->queueBuffer = ::queueBuffer;
        this->cancelBuffer = ::cancelBuffer;
    }
};

// Helpers

static ANativeWindowBuffer* resourceToANWB(Resource* res) {
    ANativeWindowBuffer* buffer = new (std::nothrow) FakeANativeWindowBuffer();
    if (!buffer)
        return nullptr;

    buffer->width = res->args.width;
    buffer->height = res->args.height;
    buffer->stride = res->args.width;
    buffer->handle = reinterpret_cast<const native_handle_t*>(res->args.handle);
    buffer->usage_deprecated = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                               GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER;
    buffer->usage =
        GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN | GRALLOC1_CONSUMER_USAGE_CPU_WRITE_OFTEN |
        GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE | GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN |
        GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN | GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET;

    switch (res->args.format) {
        case VIRGL_FORMAT_B8G8R8A8_UNORM:
            buffer->format = HAL_PIXEL_FORMAT_BGRA_8888;
            break;
        case VIRGL_FORMAT_B5G6R5_UNORM:
            buffer->format = HAL_PIXEL_FORMAT_RGB_565;
            break;
        case VIRGL_FORMAT_R8G8B8A8_UNORM:
            buffer->format = HAL_PIXEL_FORMAT_RGBA_8888;
            break;
        case VIRGL_FORMAT_R8G8B8X8_UNORM:
            buffer->format = HAL_PIXEL_FORMAT_RGBX_8888;
            break;
        default:
            delete buffer;
            return nullptr;
    }

    return buffer;
}

// RenderControl

static GLint rcGetRendererVersion() {
    return 1;  // seems to be hard-coded
}

static EGLint rcGetEGLVersion(void* ctx_, EGLint* major, EGLint* minor) {
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    return s_egl.eglInitialize(rc->dpy, major, minor);
}

static EGLint rcQueryEGLString(void* ctx_, EGLenum name, void* buffer, EGLint bufferSize) {
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    const char* str = s_egl.eglQueryString(rc->dpy, name);
    if (!str)
        str = "";

    if (strlen(str) > (size_t)bufferSize) {
        memset(buffer, 0, bufferSize);
        return -strlen(str);
    }

    char* strOut = static_cast<char*>(buffer);
    strncpy(strOut, str, bufferSize - 1);
    strOut[bufferSize - 1] = 0;

    return strlen(strOut) + 1U;
}

static std::string replaceESVersionString(const std::string& prev, const char* const newver) {
    // Do not touch ES 1.x contexts (they will all be 1.1 anyway)
    if (prev.find("ES-CM") != std::string::npos)
        return prev;

    size_t esStart = prev.find("ES ");
    size_t esEnd = prev.find(" ", esStart + 3);

    // Do not change out-of-spec version strings.
    if (esStart == std::string::npos || esEnd == std::string::npos)
        return prev;

    std::string res = prev.substr(0, esStart + 3);
    res += newver;
    res += prev.substr(esEnd);
    return res;
}

static EGLint rcGetGLString(void* ctx_, EGLenum name, void* buffer, EGLint bufferSize) {
    std::string glStr;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    if (rc->ctx->ctx) {
        const char* str = nullptr;
        switch (rc->ctx->ctx->api) {
            case EglContext::GLESApi::GLESApi_CM:
                str = reinterpret_cast<const char*>(s_gles1.glGetString(name));
                break;
            default:
                str = reinterpret_cast<const char*>(s_gles3.glGetString(name));
                break;
        }
        if (str)
            glStr += str;
    }

    // FIXME: Should probably filter the extensions list like the emulator
    //        does. We need to handle ES2 on ES3 compatibility for older
    //        Android versions, as well as filter out unsupported features.

    if (name == GL_EXTENSIONS) {
        glStr += ChecksumCalculator::getMaxVersionStr();
        glStr += " ";

        // FIXME: Hard-coded to 3.0 for now. We should attempt to detect 3.1.
        glStr += "ANDROID_EMU_gles_max_version_3_0";
        glStr += " ";
    }

    // FIXME: Add support for async swap and the fence_sync extensions

    // We don't support GLDMA; use VIRTGPU_RESOURCE_CREATE and a combination of
    // VIRTGPU_TRANSFER_TO_HOST and VIRTGPU_TRANSFER_FROM_HOST.

    // FIXME: Add support for 'no host error'

    if (name == GL_VERSION)
        glStr = replaceESVersionString(glStr, "3.0");

    int nextBufferSize = glStr.size() + 1;

    if (!buffer || nextBufferSize > bufferSize)
        return -nextBufferSize;

    snprintf((char*)buffer, nextBufferSize, "%s", glStr.c_str());
    return nextBufferSize;
}

static EGLint rcGetNumConfigs(uint32_t* numAttribs) {
    *numAttribs = EglConfig::kNumAttribs;
    return EglConfig::vec.size();
}

static EGLint rcGetConfigs(uint32_t bufSize, GLuint* buffer) {
    size_t configAttribBytes = sizeof(EglConfig::kAttribs);
    size_t nConfigs = EglConfig::vec.size();
    size_t sizeNeeded = configAttribBytes + nConfigs * configAttribBytes;

    if (bufSize < sizeNeeded)
        return -sizeNeeded;

    memcpy(buffer, &EglConfig::kAttribs, configAttribBytes);
    size_t offset = EglConfig::kNumAttribs;
    for (auto const& config : EglConfig::vec) {
        memcpy(&buffer[offset], config->attribs, configAttribBytes);
        offset += EglConfig::kNumAttribs;
    }

    return nConfigs;
}

static EGLint rcChooseConfig(void* ctx_, EGLint* attribs, uint32_t, uint32_t* config_ints,
                             uint32_t configs_size) {
    EGLint num_config;
    EGLConfig configs[configs_size];
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    EGLBoolean ret = s_egl.eglChooseConfig(rc->dpy, attribs, configs, configs_size, &num_config);
    if (!ret)
        num_config = 0;

    if (configs_size) {
        for (EGLint i = 0; i < num_config; i++) {
            config_ints[i] = ~0U;
            EGLint config_id;
            if (s_egl.eglGetConfigAttrib(rc->dpy, configs[i], EGL_CONFIG_ID, &config_id)) {
                for (size_t i = 0; i < EglConfig::vec.size(); i++) {
                    if (EglConfig::vec[i]->attribs[4] == config_id)
                        config_ints[i] = i;
                }
            }
            if (config_ints[i] == ~0U) {
                num_config = 0;
                break;
            }
        }
        if (!num_config)
            memset(config_ints, 0, configs_size * sizeof(uint32_t));
    }

    return num_config;
}

static EGLint rcGetFBParam(EGLint) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static uint32_t rcCreateContext(void* ctx_, uint32_t config_, uint32_t share_, uint32_t glVersion) {
    // clang-format off
    EGLint attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION,    0,
        EGL_CONTEXT_MINOR_VERSION_KHR, 0,
        EGL_NONE
    };
    // clang-format on
    switch (glVersion) {
        case EglContext::GLESApi::GLESApi_CM:
            attrib_list[1] = 1;
            attrib_list[3] = 1;
            break;
        case EglContext::GLESApi::GLESApi_2:
            attrib_list[1] = 2;
            break;
        case EglContext::GLESApi::GLESApi_3_0:
            attrib_list[1] = 3;
            break;
        case EglContext::GLESApi::GLESApi_3_1:
            attrib_list[1] = 3;
            attrib_list[3] = 1;
            break;
    }
    if (!attrib_list[1])
        return 0U;

    if (config_ > EglConfig::vec.size())
        return 0U;
    EglConfig const* config = EglConfig::vec[config_];

    EGLContext share_context = EGL_NO_CONTEXT;
    if (share_ > 0) {
        std::map<uint32_t, EglContext*>::iterator context_it;
        context_it = EglContext::map.find(share_);
        if (context_it == EglContext::map.end())
            return 0U;

        EglContext const* share = context_it->second;
        share_context = share->context;
    }

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    EGLContext context_ =
        s_egl.eglCreateContext(rc->dpy, config->config, share_context, attrib_list);
    if (context_ == EGL_NO_CONTEXT)
        return 0U;

    EglContext* context = new (std::nothrow)
        EglContext(context_, rc->ctx->handle, (enum EglContext::GLESApi)glVersion);
    if (!context) {
        s_egl.eglDestroyContext(rc->dpy, context_);
        return 0U;
    }

    return context->id;
}

static void rcDestroyContext(void* ctx_, uint32_t ctx) {
    std::map<uint32_t, EglContext*>::iterator it;
    it = EglContext::map.find(ctx);
    if (it == EglContext::map.end())
        return;

    EglContext* context = it->second;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    s_egl.eglDestroyContext(rc->dpy, context->context);
    context->context = EGL_NO_CONTEXT;
    if (context->disposable())
        delete context;
}

static uint32_t rcCreateWindowSurface(void* ctx_, uint32_t config_, uint32_t width,
                                      uint32_t height) {
    if (config_ > EglConfig::vec.size())
        return 0U;

    EglConfig const* config = EglConfig::vec[config_];

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    EglSurface* surface =
        new (std::nothrow) EglSurface(config->config, rc->ctx->handle, width, height);
    if (!surface)
        return 0U;

    return surface->id;
}

static void rcDestroyWindowSurface(void* ctx_, uint32_t surface_) {
    std::map<uint32_t, EglSurface*>::iterator it;
    it = EglSurface::map.find(surface_);
    if (it == EglSurface::map.end())
        return;

    EglSurface* surface = it->second;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    s_egl.eglDestroySurface(rc->dpy, surface->surface);
    surface->surface = EGL_NO_SURFACE;
    if (surface->disposable()) {
        delete surface->window;
        delete surface;
    }
}

static uint32_t rcCreateColorBuffer(uint32_t, uint32_t, GLenum) {
    // NOTE: This CreateColorBuffer implementation is a no-op which returns a
    //       special surface ID to indicate that a pbuffer surface should be
    //       created. This is necessary because the emulator does not create a
    //       true pbuffer, it always creates a fake one. We don't want this.
    return ~1U;
}

static void rcOpenColorBuffer(uint32_t) {
    printf("%s: not implemented\n", __func__);
}

static void rcCloseColorBuffer(uint32_t) {
    printf("%s: not implemented\n", __func__);
}

static void rcSetWindowColorBuffer(void* ctx_, uint32_t windowSurface, uint32_t colorBuffer) {
    std::map<uint32_t, EglSurface*>::iterator surface_it;
    surface_it = EglSurface::map.find(windowSurface);
    if (surface_it == EglSurface::map.end())
        return;

    EglSurface* surface = surface_it->second;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);

    if (colorBuffer == ~1U) {
        EGLint const attrib_list[] = { EGL_WIDTH, (EGLint)surface->width, EGL_HEIGHT,
                                       (EGLint)surface->height, EGL_NONE };
        assert(surface->surface == EGL_NO_SURFACE && "Pbuffer set twice");
        surface->surface = s_egl.eglCreatePbufferSurface(rc->dpy, surface->config, attrib_list);
    } else {
        std::map<uint32_t, Resource*>::iterator resource_it;
        resource_it = Resource::map.find(colorBuffer);
        if (resource_it == Resource::map.end())
            return;

        Resource* res = resource_it->second;
        ANativeWindowBuffer* buffer = resourceToANWB(res);
        if (!buffer)
            return;

        if (surface->surface == EGL_NO_SURFACE) {
            surface->window =
                new (std::nothrow) FakeANativeWindow(res->args.width, res->args.height);
            if (!surface->window)
                return;

            NativeWindowType native_window = reinterpret_cast<NativeWindowType>(surface->window);
            surface->window->oem[1] = (intptr_t)buffer;
            surface->surface =
                s_egl.eglCreateWindowSurface(rc->dpy, surface->config, native_window, nullptr);
        } else {
            surface->window->oem[1] = (intptr_t)buffer;
            s_egl.eglSwapBuffers(rc->dpy, surface->surface);
        }
    }
}

static int rcFlushWindowColorBuffer(uint32_t windowSurface) {
    std::map<uint32_t, EglSurface*>::iterator it;
    it = EglSurface::map.find(windowSurface);
    return it == EglSurface::map.end() ? -1 : 0;
}

static EGLint rcMakeCurrent(void* ctx_, uint32_t context_, uint32_t drawSurf, uint32_t readSurf) {
    std::map<uint32_t, EglContext*>::iterator context_it;
    context_it = EglContext::map.find(context_);
    if (context_it == EglContext::map.end())
        return EGL_FALSE;

    EglContext* context = context_it->second;

    std::map<uint32_t, EglSurface*>::iterator surface_it;
    surface_it = EglSurface::map.find(drawSurf);
    if (surface_it == EglSurface::map.end())
        return EGL_FALSE;

    EglSurface* draw_surface = surface_it->second;

    surface_it = EglSurface::map.find(readSurf);
    if (surface_it == EglSurface::map.end())
        return EGL_FALSE;

    EglSurface* read_surface = surface_it->second;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);

    EglSurface* old_draw_surface = draw_surface->bind(rc->ctx->handle, false);
    if (old_draw_surface)
        old_draw_surface->unbind(false);

    EglSurface* old_read_surface = read_surface->bind(rc->ctx->handle, true);
    if (old_read_surface)
        old_read_surface->unbind(true);

    EglContext* old_context = context->bind(rc->ctx->handle);
    if (old_context)
        old_context->unbind();

    EGLBoolean ret = s_egl.eglMakeCurrent(rc->dpy, draw_surface->surface, read_surface->surface,
                                          context->context);
    if (!ret) {
        // If eglMakeCurrent fails, it's specified *not* to have unbound the
        // previous contexts or surfaces, but many implementations do. This bug
        // isn't worked around here, and we just assume the implementations obey
        // the spec.
        context->unbind();
        if (old_context)
            old_context->bind(rc->ctx->handle);
        read_surface->unbind(true);
        if (old_read_surface)
            old_read_surface->bind(rc->ctx->handle, true);
        draw_surface->unbind(false);
        if (old_draw_surface)
            old_draw_surface->bind(rc->ctx->handle, false);
    } else {
        if (old_context && old_context->disposable())
            delete old_context;
        if (old_read_surface && old_read_surface->disposable())
            delete old_read_surface;
        if (old_draw_surface && old_draw_surface->disposable())
            delete old_draw_surface;
        rc->ctx->unbind();
        rc->ctx->bind(context);
    }

    return (EGLint)ret;
}

static void rcFBPost(uint32_t) {
    printf("%s: not implemented\n", __func__);
}

static void rcFBSetSwapInterval(void* ctx_, EGLint interval) {
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    s_egl.eglSwapInterval(rc->dpy, interval);
}

static void rcBindTexture(void* ctx_, uint32_t colorBuffer) {
    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find(colorBuffer);
    if (it == Resource::map.end())
        return;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    Resource* res = it->second;
    if (!res->image) {
        ANativeWindowBuffer* buffer = resourceToANWB(res);
        if (!buffer)
            return;

        EGLClientBuffer client_buffer = static_cast<EGLClientBuffer>(buffer);
        EGLImageKHR image = s_egl.eglCreateImageKHR(
            rc->dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, client_buffer, nullptr);
        if (image == EGL_NO_IMAGE_KHR)
            return;

        EglImage* img = new (std::nothrow) EglImage(rc->dpy, image, s_egl.eglDestroyImageKHR);
        if (!img) {
            s_egl.eglDestroyImageKHR(rc->dpy, image);
            return;
        }

        // FIXME: House keeping, because we won't get asked to delete the image
        //        object otherwise, so we need to keep a reference to it..
        res->image = img;
    }

    if (rc->ctx->ctx->api == EglContext::GLESApi::GLESApi_CM) {
        // FIXME: Unconditional use of GL_TEXTURE_2D here is wrong
        s_gles1.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, res->image->image);
    } else {
        // FIXME: Unconditional use of GL_TEXTURE_2D here is wrong
        s_gles3.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, res->image->image);
    }
}

static void rcBindRenderbuffer(void* ctx_, uint32_t colorBuffer) {
    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find(colorBuffer);
    if (it == Resource::map.end())
        return;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    Resource* res = it->second;
    if (!res->image) {
        ANativeWindowBuffer* buffer = resourceToANWB(res);
        if (!buffer)
            return;

        EGLClientBuffer client_buffer = static_cast<EGLClientBuffer>(buffer);
        EGLImageKHR image = s_egl.eglCreateImageKHR(
            rc->dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, client_buffer, nullptr);
        if (image == EGL_NO_IMAGE_KHR)
            return;

        EglImage* img = new (std::nothrow) EglImage(rc->dpy, image, s_egl.eglDestroyImageKHR);
        if (!img) {
            s_egl.eglDestroyImageKHR(rc->dpy, image);
            return;
        }

        // FIXME: House keeping, because we won't get asked to delete the image
        //        object otherwise, so we need to keep a reference to it..
        res->image = img;
    }

    if (rc->ctx->ctx->api == EglContext::GLESApi::GLESApi_CM) {
        s_gles1.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER_OES, res->image->image);
    } else {
        s_gles3.glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER_OES, res->image->image);
    }
}

static EGLint rcColorBufferCacheFlush(uint32_t, EGLint, int) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static void rcReadColorBuffer(uint32_t, GLint, GLint, GLint, GLint, GLenum, GLenum, void*) {
    printf("%s: not implemented\n", __func__);
}

static int rcUpdateColorBuffer(uint32_t, GLint, GLint, GLint, GLint, GLenum, GLenum, void*) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static int rcOpenColorBuffer2(uint32_t) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static uint32_t rcCreateClientImage(void* ctx_, uint32_t context_, EGLenum target, GLuint buffer_) {
    std::map<uint32_t, EglContext*>::iterator it;
    it = EglContext::map.find(context_);
    if (it == EglContext::map.end())
        return 0U;

    EglContext* context = it->second;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(buffer_);
    EGLImageKHR image = s_egl.eglCreateImageKHR(rc->dpy, context, target, buffer, nullptr);
    EglImage* img = new (std::nothrow) EglImage(rc->dpy, image, s_egl.eglDestroyImageKHR);
    if (!img) {
        s_egl.eglDestroyImageKHR(rc->dpy, image);
        return 0U;
    }

    return img->id;
}

static int rcDestroyClientImage(uint32_t image_) {
    std::map<uint32_t, EglImage*>::iterator it;
    it = EglImage::map.find(image_);
    if (it == EglImage::map.end())
        return EGL_FALSE;

    EglImage* image = it->second;

    delete image;
    return EGL_TRUE;
}

static void rcSelectChecksumHelper(void* ctx_, uint32_t protocol, uint32_t) {
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    rc->ctx->checksum_calc.setVersion(protocol);
}

static void rcCreateSyncKHR(void* ctx_, EGLenum type, EGLint* attribs, uint32_t, int,
                            uint64_t* glsync_out, uint64_t* syncthread_out) {
    *syncthread_out = 0ULL;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    EGLSyncKHR sync = s_egl.eglCreateSyncKHR(rc->dpy, type, attribs);
    if (sync == EGL_NO_SYNC_KHR) {
        *glsync_out = 0ULL;
        return;
    }

    EglSync* syn = new (std::nothrow) EglSync(sync);
    if (!syn) {
        s_egl.eglDestroySyncKHR(rc->dpy, sync);
        *glsync_out = 0ULL;
        return;
    }

    *glsync_out = syn->id;
}

static EGLint rcClientWaitSyncKHR(void* ctx_, uint64_t sync_, EGLint flags, uint64_t timeout) {
    std::map<uint64_t, EglSync*>::iterator it;
    it = EglSync::map.find(sync_);
    if (it == EglSync::map.end())
        return EGL_CONDITION_SATISFIED_KHR;

    EglSync* sync = it->second;
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    return s_egl.eglClientWaitSyncKHR(rc->dpy, sync->sync, flags, timeout);
}

static void rcFlushWindowColorBufferAsync(uint32_t windowSurface) {
    // No-op
}

static int rcDestroySyncKHR(void* ctx_, uint64_t sync_) {
    std::map<uint64_t, EglSync*>::iterator it;
    it = EglSync::map.find(sync_);
    if (it == EglSync::map.end())
        return EGL_FALSE;

    EglSync* sync = it->second;
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    return s_egl.eglDestroySyncKHR(rc->dpy, sync->sync);
}

static void rcSetPuid(void* ctx_, uint64_t proto) {
    union {
        uint64_t proto;
        struct {
            int pid;
            int tid;
        } id;
    } puid;

    puid.proto = proto;

    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    rc->ctx->setPidTid(puid.id.pid, puid.id.tid);
}

static int rcUpdateColorBufferDMA(uint32_t, GLint, GLint, GLint, GLint, GLenum, GLenum, void*,
                                  uint32_t) {
    printf("%s: not implemented\n", __func__);
    return 0;
}

static uint32_t rcCreateColorBufferDMA(uint32_t, uint32_t, GLenum, int) {
    printf("%s: not implemented\n", __func__);
    return 0U;
}

static void rcWaitSyncKHR(void* ctx_, uint64_t sync_, EGLint flags) {
    std::map<uint64_t, EglSync*>::iterator it;
    it = EglSync::map.find(sync_);
    if (it == EglSync::map.end())
        return;

    EglSync* sync = it->second;
    RenderControl* rc = static_cast<RenderControl*>(ctx_);
    // FIXME: No eglWaitSyncKHR support in SwiftShader
    //        This call will BLOCK when it should be asynchronous!
    s_egl.eglClientWaitSyncKHR(rc->dpy, sync->sync, flags, EGL_FOREVER_KHR);
}

RenderControl::RenderControl(Context* ctx_, EGLDisplay dpy_) {
    rcGetRendererVersion = ::rcGetRendererVersion;
    rcGetEGLVersion_dec = ::rcGetEGLVersion;
    rcQueryEGLString_dec = ::rcQueryEGLString;
    rcGetGLString_dec = ::rcGetGLString;
    rcGetNumConfigs = ::rcGetNumConfigs;
    rcGetConfigs = ::rcGetConfigs;
    rcChooseConfig_dec = ::rcChooseConfig;
    rcGetFBParam = ::rcGetFBParam;
    rcCreateContext_dec = ::rcCreateContext;
    rcDestroyContext_dec = ::rcDestroyContext;
    rcCreateWindowSurface_dec = ::rcCreateWindowSurface;
    rcDestroyWindowSurface_dec = ::rcDestroyWindowSurface;
    rcCreateColorBuffer = ::rcCreateColorBuffer;
    rcOpenColorBuffer = ::rcOpenColorBuffer;
    rcCloseColorBuffer = ::rcCloseColorBuffer;
    rcSetWindowColorBuffer_dec = ::rcSetWindowColorBuffer;
    rcFlushWindowColorBuffer = ::rcFlushWindowColorBuffer;
    rcMakeCurrent_dec = ::rcMakeCurrent;
    rcFBPost = ::rcFBPost;
    rcFBSetSwapInterval_dec = ::rcFBSetSwapInterval;
    rcBindTexture_dec = ::rcBindTexture;
    rcBindRenderbuffer_dec = ::rcBindRenderbuffer;
    rcColorBufferCacheFlush = ::rcColorBufferCacheFlush;
    rcReadColorBuffer = ::rcReadColorBuffer;
    rcUpdateColorBuffer = ::rcUpdateColorBuffer;
    rcOpenColorBuffer2 = ::rcOpenColorBuffer2;
    rcCreateClientImage_dec = ::rcCreateClientImage;
    rcDestroyClientImage = ::rcDestroyClientImage;
    rcSelectChecksumHelper_dec = ::rcSelectChecksumHelper;
    rcCreateSyncKHR_dec = ::rcCreateSyncKHR;
    rcClientWaitSyncKHR_dec = ::rcClientWaitSyncKHR;
    rcFlushWindowColorBufferAsync = ::rcFlushWindowColorBufferAsync;
    rcDestroySyncKHR_dec = ::rcDestroySyncKHR;
    rcSetPuid_dec = ::rcSetPuid;
    rcUpdateColorBufferDMA = ::rcUpdateColorBufferDMA;
    rcCreateColorBufferDMA = ::rcCreateColorBufferDMA;
    rcWaitSyncKHR_dec = ::rcWaitSyncKHR;

    dpy = dpy_;
    ctx = ctx_;
}
