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

extern "C" {
#include <linux/virtio_gpu.h>
#include <virglrenderer.h>
#include <virgl_hw.h>
}

#include <sys/uio.h>

#include <dlfcn.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3ext.h>

#include <drm/drm_fourcc.h>

#include <OpenGLESDispatch/EGLDispatch.h>
#include <OpenGLESDispatch/GLESv1Dispatch.h>
#include <OpenGLESDispatch/GLESv3Dispatch.h>

#include "OpenglRender/IOStream.h"

#include "Context.h"
#include "EglConfig.h"
#include "EglContext.h"
#include "EglSurface.h"
#include "EglSync.h"
#include "Resource.h"

#include <VirtioGpuCmd.h>

// for debug only
#include <sys/syscall.h>
#include <unistd.h>
#define gettid() (int)syscall(__NR_gettid)

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#define MAX_CMDRESPBUF_SIZE (10 * PAGE_SIZE)

#define ALIGN(A, B) (((A) + (B)-1) / (B) * (B))

// Enable passing scanout buffers as texture names to sdl2 backend
#define QEMU_HARDWARE_GL_INTEROP

#ifdef QEMU_HARDWARE_GL_INTEROP
typedef GLenum (*PFNGLGETERROR)(void);
typedef void (*PFNGLBINDTEXTURE)(GLenum target, GLuint texture);
typedef void (*PFNGLGENTEXTURES)(GLsizei n, GLuint* textures);
typedef void (*PFNGLTEXPARAMETERI)(GLenum target, GLenum pname, GLint param);
typedef void (*PFNGLPIXELSTOREI)(GLenum pname, GLint param);
typedef void (*PFNGLTEXIMAGE2D)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                GLsizei height, GLint border, GLenum format, GLenum type,
                                const void* pixels);
static PFNGLBINDTEXTURE g_glBindTexture;
static PFNGLGENTEXTURES g_glGenTextures;
static PFNGLTEXPARAMETERI g_glTexParameteri;
static PFNGLPIXELSTOREI g_glPixelStorei;
static PFNGLTEXIMAGE2D g_glTexImage2D;
static virgl_renderer_gl_context g_ctx0_alt;
#endif

// Global state
std::map<uint32_t, EglContext*> EglContext::map;
std::map<uint32_t, EglSurface*> EglSurface::map;
std::map<uint32_t, EglImage*> EglImage::map;
std::map<uint32_t, Resource*> Resource::map;
std::map<uint64_t, EglSync*> EglSync::map;
std::map<uint32_t, Context*> Context::map;
std::vector<EglConfig*> EglConfig::vec;
static virgl_renderer_callbacks* g_cb;
const EGLint EglConfig::kAttribs[];
uint32_t EglContext::nextId = 1U;
uint32_t EglSurface::nextId = 1U;
uint32_t EglImage::nextId = 1U;
uint64_t EglSync::nextId = 1U;
static void* g_cookie;

// Fence queue, must be thread safe
static std::mutex g_fence_deque_mutex;
static std::deque<int> g_fence_deque;

// Other GPU context and state
static EGLSurface g_ctx0_surface;
static EGLContext g_ctx0_es1;
static EGLContext g_ctx0_es2;
static EGLDisplay g_dpy;

// Last context receiving a command. Allows us to find the context a fence is
// being created for. Works around the poorly designed virgl interface.
static Context* g_last_submit_cmd_ctx;

#ifdef OPENGL_DEBUG_PRINTOUT

#include "emugl/common/logging.h"

// For logging from the protocol decoders

void default_logger(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

emugl_logger_t emugl_cxt_logger = default_logger;

#endif

static void dump_global_state(void) {
    printf("AVDVIRGLRENDERER GLOBAL STATE\n\n");

    printf("Resources:\n");
    for (auto const& it : Resource::map) {
        Resource const* res = it.second;

        printf(
            "  Resource %u: %ux%u 0x%x %p (%zub) t=%u b=%u d=%u a=%u l=%u "
            "n=%u f=%u\n",
            res->args.handle, res->args.width, res->args.height, res->args.format,
            res->iov ? res->iov[0].iov_base : nullptr, res->iov ? res->iov[0].iov_len : 0,
            res->args.target, res->args.bind, res->args.depth, res->args.array_size,
            res->args.last_level, res->args.nr_samples, res->args.flags);

        for (auto const& it : res->context_map) {
            Context const* ctx = it.second;

            printf("    Context %u, pid=%d, tid=%d\n", ctx->handle, ctx->pid, ctx->tid);
        }
    }

    printf("Contexts:\n");
    for (auto const& it : Context::map) {
        Context const* ctx = it.second;

        printf("  Context %u: %s pid=%u tid=%u\n", ctx->handle, ctx->name.c_str(), ctx->pid,
               ctx->tid);

        for (auto const& it : ctx->resource_map) {
            Resource const* res = it.second;

            printf("    Resource %u\n", res->args.handle);
        }
    }
}

static int sync_linear_to_iovec(Resource* res, uint64_t offset, const virgl_box* box) {
    uint32_t bpp;
    switch (res->args.format) {
        case VIRGL_FORMAT_R8_UNORM:
            bpp = 1U;
            break;
        case VIRGL_FORMAT_B5G6R5_UNORM:
            bpp = 2U;
            break;
        default:
            bpp = 4U;
            break;
    }

    if (box->x > res->args.width || box->y > res->args.height)
        return 0;
    if (box->w == 0U || box->h == 0U)
        return 0;
    uint32_t w = std::min(box->w, res->args.width - box->x);
    uint32_t h = std::min(box->h, res->args.height - box->y);
    uint32_t stride = ALIGN(res->args.width * bpp, 16U);
    offset += box->y * stride + box->x * bpp;
    size_t length = (h - 1U) * stride + w * bpp;
    if (offset + length > res->linearSize)
        return EINVAL;

    if (res->num_iovs > 1) {
        const char* linear = static_cast<const char*>(res->linear);
        for (uint32_t i = 0, iovOffset = 0U; length && i < res->num_iovs; i++) {
            if (iovOffset + res->iov[i].iov_len > offset) {
                char* iov_base = static_cast<char*>(res->iov[i].iov_base);
                size_t copyLength = std::min(length, res->iov[i].iov_len);
                memcpy(iov_base + offset - iovOffset, linear, copyLength);
                linear += copyLength;
                offset += copyLength;
                length -= copyLength;
            }
            iovOffset += res->iov[i].iov_len;
        }
    }

    return 0;
}

static int sync_iovec_to_linear(Resource* res, uint64_t offset, const virgl_box* box) {
    uint32_t bpp;
    switch (res->args.format) {
        case VIRGL_FORMAT_R8_UNORM:
            bpp = 1U;
            break;
        case VIRGL_FORMAT_B5G6R5_UNORM:
            bpp = 2U;
            break;
        default:
            bpp = 4U;
            break;
    }

    if (box->x > res->args.width || box->y > res->args.height)
        return 0;
    if (box->w == 0U || box->h == 0U)
        return 0;
    uint32_t w = std::min(box->w, res->args.width - box->x);
    uint32_t h = std::min(box->h, res->args.height - box->y);
    uint32_t stride = ALIGN(res->args.width * bpp, 16U);
    offset += box->y * stride + box->x * bpp;
    size_t length = (h - 1U) * stride + w * bpp;
    if (offset + length > res->linearSize)
        return EINVAL;

    if (res->num_iovs > 1) {
        char* linear = static_cast<char*>(res->linear);
        for (uint32_t i = 0, iovOffset = 0U; length && i < res->num_iovs; i++) {
            if (iovOffset + res->iov[i].iov_len > offset) {
                const char* iov_base = static_cast<const char*>(res->iov[i].iov_base);
                size_t copyLength = std::min(length, res->iov[i].iov_len);
                memcpy(linear, iov_base + offset - iovOffset, copyLength);
                linear += copyLength;
                offset += copyLength;
                length -= copyLength;
            }
            iovOffset += res->iov[i].iov_len;
        }
    }

    return 0;
}

// The below API was defined by virglrenderer 'master', but does not seem to
// be used by QEMU, so just ignore it for now..
//
// virgl_renderer_get_rect
// virgl_renderer_get_fd_for_texture
// virgl_renderer_cleanup
// virgl_renderer_reset
// virgl_renderer_get_poll_fd

int virgl_renderer_init(void* cookie, int flags, virgl_renderer_callbacks* cb) {
    if (!cookie || !cb)
        return EINVAL;

    if (flags != 0)
        return ENOSYS;

    if (cb->version != 1)
        return ENOSYS;

#ifdef QEMU_HARDWARE_GL_INTEROP
    // FIXME: If we just use "libGL.so" here, mesa's interception library returns
    //        stub dlsyms that do nothing at runtime, even after binding..
    void* handle = dlopen(
        "/usr/lib/x86_64-linux-gnu/nvidia/"
        "current/libGL.so.384.111",
        RTLD_NOW);
    assert(handle != nullptr);
    g_glBindTexture = (PFNGLBINDTEXTURE)dlsym(handle, "glBindTexture");
    assert(g_glBindTexture != nullptr);
    g_glGenTextures = (PFNGLGENTEXTURES)dlsym(handle, "glGenTextures");
    assert(g_glGenTextures != nullptr);
    g_glTexParameteri = (PFNGLTEXPARAMETERI)dlsym(handle, "glTexParameteri");
    assert(g_glTexParameteri != nullptr);
    g_glPixelStorei = (PFNGLPIXELSTOREI)dlsym(handle, "glPixelStorei");
    assert(g_glPixelStorei != nullptr);
    g_glTexImage2D = (PFNGLTEXIMAGE2D)dlsym(handle, "glTexImage2D");
    assert(g_glTexImage2D != nullptr);
#endif

    if (!egl_dispatch_init())
        return ENOENT;

    if (!gles1_dispatch_init())
        return ENOENT;

    if (!gles3_dispatch_init())
        return ENOENT;

    g_dpy = s_egl.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_dpy == EGL_NO_DISPLAY) {
        printf("Failed to open default EGL display\n");
        return ENOENT;
    }

    if (!s_egl.eglInitialize(g_dpy, nullptr, nullptr)) {
        printf("Failed to initialize EGL display\n");
        g_dpy = EGL_NO_DISPLAY;
        return ENOENT;
    }

    EGLint nConfigs;
    if (!s_egl.eglGetConfigs(g_dpy, nullptr, 0, &nConfigs)) {
        printf("Failed to retrieve number of EGL configs\n");
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        return ENOENT;
    }

    EGLConfig configs[nConfigs];
    if (!s_egl.eglGetConfigs(g_dpy, configs, nConfigs, &nConfigs)) {
        printf("Failed to retrieve EGL configs\n");
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        return ENOENT;
    }

    // Our static analyzer sees the `new`ing of `config` below without any sort
    // of attempt to free it, and warns about it. Normally, it would catch that
    // we're pushing it into a vector in the constructor, but it hits an
    // internal evaluation limit when trying to evaluate the loop inside of the
    // ctor. So, it never gets to see that we escape our newly-allocated
    // `config` instance. Silence the warning, since it's incorrect.
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    for (EGLint c = 0; c < nConfigs; c++) {
        EGLint configId;
        if (!s_egl.eglGetConfigAttrib(g_dpy, configs[c], EGL_CONFIG_ID, &configId)) {
            printf("Failed to retrieve EGL config ID\n");
            s_egl.eglTerminate(g_dpy);
            g_dpy = EGL_NO_DISPLAY;
            return ENOENT;
        }
        EglConfig* config =
            new (std::nothrow) EglConfig(g_dpy, configs[c], s_egl.eglGetConfigAttrib);
        if (!config)
            return ENOMEM;
    }

    // clang-format off
    EGLint const attrib_list[] = {
        EGL_CONFORMANT,   EGL_OPENGL_ES_BIT | EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NONE,
     };
    // clang-format on
    EGLint num_config = 0;
    EGLConfig config;
    if (!s_egl.eglChooseConfig(g_dpy, attrib_list, &config, 1, &num_config) || num_config != 1) {
        printf("Failed to select ES1 & ES3 capable EGL config\n");
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        return ENOENT;
    }

    // clang-format off
    EGLint const pbuffer_attrib_list[] = {
        EGL_WIDTH,  1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    // clang-format on
    g_ctx0_surface = s_egl.eglCreatePbufferSurface(g_dpy, config, pbuffer_attrib_list);
    if (!g_ctx0_surface) {
        printf("Failed to create pbuffer surface for context 0\n");
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        return ENOENT;
    }

    // clang-format off
    EGLint const es1_attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION, 1,
        EGL_NONE
    };
    // clang-format on
    g_ctx0_es1 = s_egl.eglCreateContext(g_dpy, config, EGL_NO_CONTEXT, es1_attrib_list);
    if (g_ctx0_es1 == EGL_NO_CONTEXT) {
        printf("Failed to create ES1 context 0\n");
        s_egl.eglDestroySurface(g_dpy, g_ctx0_surface);
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
        return ENOENT;
    }

    // clang-format off
    EGLint const es2_attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3, // yes, 3
        EGL_NONE
    };
    // clang-format on
    g_ctx0_es2 = s_egl.eglCreateContext(g_dpy, config, EGL_NO_CONTEXT, es2_attrib_list);
    if (g_ctx0_es2 == EGL_NO_CONTEXT) {
        printf("Failed to create ES2 context 0\n");
        s_egl.eglDestroySurface(g_dpy, g_ctx0_surface);
        s_egl.eglDestroyContext(g_dpy, g_ctx0_es1);
        g_ctx0_es1 = EGL_NO_CONTEXT;
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
    }

#ifdef QEMU_HARDWARE_GL_INTEROP
    // This is the hardware GPU context. In future, this code should probably
    // be removed and SwiftShader be used for all presentation blits.
    virgl_renderer_gl_ctx_param ctx_params = {
        .major_ver = 3,
        .minor_ver = 0,
    };
    g_ctx0_alt = cb->create_gl_context(cookie, 0, &ctx_params);
    if (!g_ctx0_alt) {
        printf("Failed to create hardware GL context 0\n");
        s_egl.eglDestroySurface(g_dpy, g_ctx0_surface);
        s_egl.eglDestroyContext(g_dpy, g_ctx0_es1);
        g_ctx0_es1 = EGL_NO_CONTEXT;
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
    }

    // Test we can actually make it current; otherwise, bail
    if (cb->make_current(cookie, 0, g_ctx0_alt)) {
        printf("Failed to make hardware GL context 0 current\n");
        cb->destroy_gl_context(cookie, g_ctx0_alt);
        g_ctx0_alt = nullptr;
        s_egl.eglDestroySurface(g_dpy, g_ctx0_surface);
        s_egl.eglDestroyContext(g_dpy, g_ctx0_es1);
        g_ctx0_es1 = EGL_NO_CONTEXT;
        s_egl.eglTerminate(g_dpy);
        g_dpy = EGL_NO_DISPLAY;
    }
#endif

    EglContext::nextId = 1U;
    g_cookie = cookie;
    g_cb = cb;
    return 0;
}

void virgl_renderer_poll(void) {
    std::lock_guard<std::mutex> lk(g_fence_deque_mutex);
    for (auto fence : g_fence_deque)
        g_cb->write_fence(g_cookie, fence);
    g_fence_deque.clear();
}

void* virgl_renderer_get_cursor_data(uint32_t resource_id, uint32_t* width, uint32_t* height) {
    if (!width || !height)
        return nullptr;

    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find(resource_id);
    if (it == Resource::map.end())
        return nullptr;

    Resource* res = it->second;
    if (res->args.bind != VIRGL_RES_BIND_CURSOR)
        return nullptr;

    void* pixels = malloc(res->linearSize);
    memcpy(pixels, res->linear, res->linearSize);
    *height = res->args.height;
    *width = res->args.width;
    return pixels;
}

// NOTE: This function is called from thread context. Do not touch anything
// without a mutex to protect it from concurrent access. Everything else in
// libvirglrenderer is designed to be single-threaded *only*.

// Hack to serialize all calls into EGL or GLES functions due to bugs in
// swiftshader. This should be removed as soon as possible.
static std::mutex swiftshader_wa_mutex;

static void process_cmd(Context* ctx, char* buf, size_t bufSize, int fence) {
    VirtioGpuCmd* cmd_resp = reinterpret_cast<VirtioGpuCmd*>(ctx->cmd_resp->linear);

    IOStream stream(cmd_resp->buf, MAX_CMDRESPBUF_SIZE - sizeof(*cmd_resp));

    {
        std::lock_guard<std::mutex> lk(swiftshader_wa_mutex);
        size_t decodedBytes;

        decodedBytes = ctx->render_control.decode(buf, bufSize, &stream, &ctx->checksum_calc);
        bufSize -= decodedBytes;
        buf += decodedBytes;

        decodedBytes = ctx->gles1.decode(buf, bufSize, &stream, &ctx->checksum_calc);
        bufSize -= decodedBytes;
        buf += decodedBytes;

        decodedBytes = ctx->gles3.decode(buf, bufSize, &stream, &ctx->checksum_calc);
        bufSize -= decodedBytes;
        buf += decodedBytes;
    }

    assert(bufSize == 0);

    cmd_resp->cmdSize += stream.getFlushSize();

    printf("(tid %d) ctx %d: cmd %u, size %zu, fence %d\n", gettid(), ctx->handle, cmd_resp->op,
           cmd_resp->cmdSize - sizeof(*cmd_resp), fence);
    if (cmd_resp->cmdSize - sizeof(*cmd_resp) > 0) {
        printf("(tid %d) ", gettid());
        for (size_t i = 0; i < cmd_resp->cmdSize - sizeof(*cmd_resp); i++) {
            printf("%.2x ", (unsigned char)cmd_resp->buf[i]);
        }
        printf("\n");
    }

    virgl_box box = {
        .w = cmd_resp->cmdSize,
        .h = 1,
    };
    sync_linear_to_iovec(ctx->cmd_resp, 0, &box);

    {
        std::lock_guard<std::mutex> lk(g_fence_deque_mutex);
        g_fence_deque.push_back(fence);
    }
}

int virgl_renderer_submit_cmd(void* buffer, int ctx_id, int ndw) {
    VirtioGpuCmd* cmd = static_cast<VirtioGpuCmd*>(buffer);
    size_t bufSize = sizeof(uint32_t) * ndw;

    if (bufSize < sizeof(*cmd)) {
        printf("bad buffer size, bufSize=%zu, ctx=%d\n", bufSize, ctx_id);
        return -1;
    }

    printf("ctx %d: cmd %u, size %zu\n", ctx_id, cmd->op, cmd->cmdSize - sizeof(*cmd));

    for (size_t i = 0; i < bufSize - sizeof(*cmd); i++) {
        printf("%.2x ", (unsigned char)cmd->buf[i]);
    }
    printf("\n");

    if (cmd->cmdSize < bufSize) {
        printf("ignoring short command, cmdSize=%u, bufSize=%zu\n", cmd->cmdSize, bufSize);
        return 0;
    }

    if (cmd->cmdSize > bufSize) {
        printf("command would overflow buffer, cmdSize=%u, bufSize=%zu\n", cmd->cmdSize, bufSize);
        return -1;
    }

    std::map<uint32_t, Context*>::iterator it;
    it = Context::map.find((uint32_t)ctx_id);
    if (it == Context::map.end()) {
        printf("command submit from invalid context %d, ignoring\n", ctx_id);
        return 0;
    }

    Context* ctx = it->second;

    // When the context is created, the remote side should send a test command
    // (op == 0) which we use to set up our link to this context's 'response
    // buffer'. Only apps using EGL or GLES have this. Gralloc contexts will
    // never hit this path because they do not submit 3D commands.
    if (cmd->op == 0) {
        std::map<uint32_t, Resource*>::iterator it;
        it = Resource::map.find(*(uint32_t*)cmd->buf);
        if (it != Resource::map.end()) {
            Resource* res = it->second;
            size_t cmdRespBufSize = 0U;
            for (size_t i = 0; i < res->num_iovs; i++)
                cmdRespBufSize += res->iov[i].iov_len;
            if (cmdRespBufSize == MAX_CMDRESPBUF_SIZE)
                ctx->cmd_resp = res;
        }
    }

    if (!ctx->cmd_resp) {
        printf("context command response page not set up, ctx=%d\n", ctx_id);
        return -1;
    }

    VirtioGpuCmd* cmd_resp = reinterpret_cast<VirtioGpuCmd*>(ctx->cmd_resp->linear);

    // We can configure bits of the response now. The size, and any message, will
    // be updated later. This must be done even for the dummy 'op == 0' command.
    cmd_resp->op = cmd->op;
    cmd_resp->cmdSize = sizeof(*cmd_resp);

    if (cmd->op == 0) {
        // Send back a no-op response, just to keep the protocol in check
        virgl_box box = {
            .w = cmd_resp->cmdSize,
            .h = 1,
        };
        sync_linear_to_iovec(ctx->cmd_resp, 0, &box);
    } else {
        // If the rcSetPuid command was already processed, this command will be
        // processed by another thread. If not, the command data will be copied
        // here and responded to when ctx->setFence() is called later.
        ctx->submitCommand(buffer, bufSize);
    }

    g_last_submit_cmd_ctx = ctx;
    return 0;
}

void virgl_renderer_get_cap_set(uint32_t set, uint32_t* max_ver, uint32_t* max_size) {
    if (!max_ver || !max_size)
        return;

    printf("Request for caps version %u\n", set);

    switch (set) {
        case 1:
            *max_ver = 1;
            *max_size = sizeof(virgl_caps_v1);
            break;
        case 2:
            *max_ver = 2;
            *max_size = sizeof(virgl_caps_v2);
            break;
        default:
            *max_ver = 0;
            *max_size = 0;
            break;
    }
}

void virgl_renderer_fill_caps(uint32_t set, uint32_t, void* caps_) {
    union virgl_caps* caps = static_cast<union virgl_caps*>(caps_);
    EGLSurface old_read_surface, old_draw_surface;
    GLfloat range[2] = { 0.0f, 0.0f };
    bool fill_caps_v2 = false;
    EGLContext old_context;
    GLint max = 0;

    if (!caps)
        return;

    // We don't need to handle caps yet, because our guest driver's features
    // should be as close as possible to the host driver's. But maybe some day
    // we'll support gallium shaders and the virgl control stream, so it seems
    // like a good idea to set up the driver caps correctly..

    // If this is broken, nothing will work properly
    old_read_surface = s_egl.eglGetCurrentSurface(EGL_READ);
    old_draw_surface = s_egl.eglGetCurrentSurface(EGL_DRAW);
    old_context = s_egl.eglGetCurrentContext();
    if (!s_egl.eglMakeCurrent(g_dpy, g_ctx0_surface, g_ctx0_surface, g_ctx0_es1)) {
        printf("Failed to make ES1 context current\n");
        return;
    }

    // Don't validate 'version' because it looks like this was misdesigned
    // upstream and won't be set; instead, 'set' was bumped from 1->2.

    switch (set) {
        case 0:
        case 1:
            memset(caps, 0, sizeof(virgl_caps_v1));
            caps->max_version = 1;
            break;
        case 2:
            memset(caps, 0, sizeof(virgl_caps_v2));
            caps->max_version = 2;
            fill_caps_v2 = true;
            break;
        default:
            caps->max_version = 0;
            return;
    }

    if (fill_caps_v2) {
        printf("Will probe and fill caps version 2.\n");
    }

    // Formats supported for textures

    caps->v1.sampler.bitmask[0] = (1 << (VIRGL_FORMAT_B8G8R8A8_UNORM - (0 * 32))) |
                                  (1 << (VIRGL_FORMAT_B5G6R5_UNORM - (0 * 32)));
    caps->v1.sampler.bitmask[2] = (1 << (VIRGL_FORMAT_R8G8B8A8_UNORM - (2 * 32)));
    caps->v1.sampler.bitmask[4] = (1 << (VIRGL_FORMAT_R8G8B8X8_UNORM - (4 * 32)));

    // Formats supported for rendering

    caps->v1.render.bitmask[0] = (1 << (VIRGL_FORMAT_B8G8R8A8_UNORM - (0 * 32))) |
                                 (1 << (VIRGL_FORMAT_B5G6R5_UNORM - (0 * 32)));
    caps->v1.render.bitmask[2] = (1 << (VIRGL_FORMAT_R8G8B8A8_UNORM - (2 * 32)));
    caps->v1.render.bitmask[4] = (1 << (VIRGL_FORMAT_R8G8B8X8_UNORM - (4 * 32)));

    // Could parse s_gles1.glGetString(GL_SHADING_LANGUAGE_VERSION, ...)?
    caps->v1.glsl_level = 300;  // OpenGL ES GLSL 3.00

    // Call with any API (v1, v3) bound

    caps->v1.max_viewports = 1;

    s_gles1.glGetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max);
    caps->v1.max_render_targets = max;

    s_gles1.glGetIntegerv(GL_MAX_SAMPLES_EXT, &max);
    caps->v1.max_samples = max;

    if (fill_caps_v2) {
        s_gles1.glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
        caps->v2.min_aliased_point_size = range[0];
        caps->v2.max_aliased_point_size = range[1];

        s_gles1.glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
        caps->v2.min_aliased_line_width = range[0];
        caps->v2.max_aliased_line_width = range[1];

        // An extension, but everybody has it
        s_gles1.glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max);
        caps->v2.max_vertex_attribs = max;

        // Call with ES 1.0 bound *only*

        s_gles1.glGetFloatv(GL_SMOOTH_POINT_SIZE_RANGE, range);
        caps->v2.min_smooth_point_size = range[0];
        caps->v2.max_smooth_point_size = range[1];

        s_gles1.glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
        caps->v2.min_smooth_line_width = range[0];
        caps->v2.max_smooth_line_width = range[1];
    }

    if (!s_egl.eglMakeCurrent(g_dpy, g_ctx0_surface, g_ctx0_surface, g_ctx0_es2)) {
        s_egl.eglMakeCurrent(g_dpy, old_draw_surface, old_read_surface, old_context);
        printf("Failed to make ES3 context current\n");
        return;
    }

    // Call with ES 3.0 bound *only*

    caps->v1.bset.primitive_restart = 1;
    caps->v1.bset.seamless_cube_map = 1;
    caps->v1.bset.occlusion_query = 1;
    caps->v1.bset.instanceid = 1;
    caps->v1.bset.ubo = 1;

    s_gles1.glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max);
    caps->v1.max_texture_array_layers = max;

    s_gles1.glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &max);
    caps->v1.max_uniform_blocks = max + 1;

    if (fill_caps_v2) {
        s_gles1.glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps->v2.max_texture_lod_bias);

        s_gles1.glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &max);
        caps->v2.max_vertex_outputs = max / 4;

        s_gles1.glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &caps->v2.min_texel_offset);
        s_gles1.glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &caps->v2.max_texel_offset);

        s_gles1.glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &max);
        caps->v2.uniform_buffer_offset_alignment = max;
    }

    // ES 2.0 extensions (fixme)

    // Gallium compatibility; not usable currently.
    caps->v1.prim_mask = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);

    if (!s_egl.eglMakeCurrent(g_dpy, old_draw_surface, old_read_surface, old_context)) {
        printf("Failed to make no context current\n");
    }
}

int virgl_renderer_create_fence(int client_fence_id, uint32_t cmd_type) {
    switch (cmd_type) {
        case VIRTIO_GPU_CMD_SUBMIT_3D:
            if (g_last_submit_cmd_ctx) {
                g_last_submit_cmd_ctx->setFence(client_fence_id);
                break;
            }
            [[fallthrough]];
        default: {
            std::lock_guard<std::mutex> lk(g_fence_deque_mutex);
            g_fence_deque.push_back(client_fence_id);
            break;
        }
    }
    return 0;
}

void virgl_renderer_force_ctx_0(void) {
#ifdef QEMU_HARDWARE_GL_INTEROP
    if (!g_ctx0_alt)
        return;

    if (g_cb->make_current(g_cookie, 0, g_ctx0_alt)) {
        printf("Failed to make hardware GL context 0 current\n");
        g_cb->destroy_gl_context(g_cookie, g_ctx0_alt);
        g_ctx0_alt = nullptr;
    }
#endif
}

int virgl_renderer_resource_create(virgl_renderer_resource_create_args* args, iovec* iov,
                                   uint32_t num_iovs) {
    if (!args)
        return EINVAL;

    if (args->bind == VIRGL_RES_BIND_CURSOR) {
        // Enforce limitation of current virtio-gpu-3d implementation
        if (args->width != 64 || args->height != 64 || args->format != VIRGL_FORMAT_B8G8R8A8_UNORM)
            return EINVAL;
    }

    assert(!Resource::map.count(args->handle) && "Can't insert same resource twice!");

    Resource* res = new (std::nothrow) Resource(args, num_iovs, iov);
    if (!res)
        return ENOMEM;

    printf("Creating Resource %u (num_iovs=%u)\n", args->handle, num_iovs);
    return 0;
}

void virgl_renderer_resource_unref(uint32_t res_handle) {
    std::map<uint32_t, Resource*>::iterator it;

    it = Resource::map.find(res_handle);
    if (it == Resource::map.end())
        return;

    Resource* res = it->second;

    for (auto const& it : Context::map) {
        Context const* ctx = it.second;

        virgl_renderer_ctx_detach_resource(ctx->handle, res->args.handle);
    }

    assert(res->context_map.empty() && "Deleted resource was associated with contexts");

    printf("Deleting Resource %u\n", res_handle);
    delete res;
}

int virgl_renderer_resource_attach_iov(int res_handle, iovec* iov, int num_iovs) {
    std::map<uint32_t, Resource*>::iterator it;

    it = Resource::map.find((uint32_t)res_handle);
    if (it == Resource::map.end())
        return ENOENT;

    Resource* res = it->second;

    if (!res->iov) {
        printf(
            "Attaching backing store for Resource %d "
            "(num_iovs=%d)\n",
            res_handle, num_iovs);

        res->num_iovs = num_iovs;
        res->iov = iov;

        res->reallocLinear();

        // Assumes that when resources are attached, they contain junk, and we
        // don't need to synchronize with the linear buffer
    }

    return 0;
}

void virgl_renderer_resource_detach_iov(int res_handle, iovec** iov, int* num_iovs) {
    std::map<uint32_t, Resource*>::iterator it;

    it = Resource::map.find((uint32_t)res_handle);
    if (it == Resource::map.end())
        return;

    Resource* res = it->second;

    printf("Detaching backing store for Resource %d\n", res_handle);

    // Synchronize our linear buffer, if any, with the iovec that we are about
    // to give up. Most likely this is not required, but it seems cleaner.
    virgl_box box = {
        .w = res->args.width,
        .h = res->args.height,
    };
    sync_linear_to_iovec(res, 0, &box);

    if (num_iovs)
        *num_iovs = res->num_iovs;
    res->num_iovs = 0U;

    if (iov)
        *iov = res->iov;
    res->iov = nullptr;

    res->reallocLinear();
}

int virgl_renderer_resource_get_info(int res_handle, virgl_renderer_resource_info* info) {
    if (!info)
        return EINVAL;

    std::map<uint32_t, Resource*>::iterator it;

    it = Resource::map.find((uint32_t)res_handle);
    if (it == Resource::map.end())
        return ENOENT;

    Resource* res = it->second;

    uint32_t bpp = 4U;
    switch (res->args.format) {
        case VIRGL_FORMAT_B8G8R8A8_UNORM:
            info->drm_fourcc = DRM_FORMAT_BGRA8888;
            break;
        case VIRGL_FORMAT_B5G6R5_UNORM:
            info->drm_fourcc = DRM_FORMAT_BGR565;
            bpp = 2U;
            break;
        case VIRGL_FORMAT_R8G8B8A8_UNORM:
            info->drm_fourcc = DRM_FORMAT_RGBA8888;
            break;
        case VIRGL_FORMAT_R8G8B8X8_UNORM:
            info->drm_fourcc = DRM_FORMAT_RGBX8888;
            break;
        default:
            return EINVAL;
    }

#ifdef QEMU_HARDWARE_GL_INTEROP
    GLenum type = GL_UNSIGNED_BYTE;
    GLenum format = GL_RGBA;
    switch (res->args.format) {
        case VIRGL_FORMAT_B8G8R8A8_UNORM:
            format = 0x80E1;  // GL_BGRA
            break;
        case VIRGL_FORMAT_B5G6R5_UNORM:
            type = GL_UNSIGNED_SHORT_5_6_5;
            format = GL_RGB;
            break;
        case VIRGL_FORMAT_R8G8B8X8_UNORM:
            format = GL_RGB;
            break;
    }

    if (!res->tex_id) {
        g_glGenTextures(1, &res->tex_id);
        g_glBindTexture(GL_TEXTURE_2D, res->tex_id);
        g_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        g_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        g_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        g_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        g_glBindTexture(GL_TEXTURE_2D, res->tex_id);
    }

    g_glPixelStorei(GL_UNPACK_ROW_LENGTH, ALIGN(res->args.width, 16));
    g_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, res->args.width, res->args.height, 0, format, type,
                   res->linear);
#endif

    info->stride = ALIGN(res->args.width * bpp, 16U);
    info->virgl_format = res->args.format;
    info->handle = res->args.handle;
    info->height = res->args.height;
    info->width = res->args.width;
    info->depth = res->args.depth;
    info->flags = res->args.flags;
    info->tex_id = res->tex_id;

    printf("Scanning out Resource %d\n", res_handle);
    dump_global_state();

    return 0;
}

int virgl_renderer_context_create(uint32_t handle, uint32_t nlen, const char* name) {
    assert(!Context::map.count(handle) && "Can't insert same context twice!");

    Context* ctx = new (std::nothrow) Context(handle, name, nlen, process_cmd, g_dpy);
    if (!ctx)
        return ENOMEM;

    printf("Creating Context %u (%.*s)\n", handle, (int)nlen, name);
    return 0;
}

void virgl_renderer_context_destroy(uint32_t handle) {
    std::map<uint32_t, Context*>::iterator it;
    it = Context::map.find(handle);
    if (it == Context::map.end())
        return;

    Context* ctx = it->second;
    printf("Destroying Context %u\n", handle);
    delete ctx;
}

int virgl_renderer_transfer_read_iov(uint32_t handle, uint32_t, uint32_t, uint32_t, uint32_t,
                                     virgl_box* box, uint64_t offset, iovec*, int) {
    // stride, layer_stride and level are not set by minigbm, so don't try to
    // validate them right now. iov and iovec_cnt are always passed as nullptr
    // and 0 by qemu, so ignore those too

    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find((uint32_t)handle);
    if (it == Resource::map.end())
        return EINVAL;

    return sync_linear_to_iovec(it->second, offset, box);
}

int virgl_renderer_transfer_write_iov(uint32_t handle, uint32_t, int, uint32_t, uint32_t,
                                      virgl_box* box, uint64_t offset, iovec*, unsigned int) {
    // stride, layer_stride and level are not set by minigbm, so don't try to
    // validate them right now. iov and iovec_cnt are always passed as nullptr
    // and 0 by qemu, so ignore those too

    std::map<uint32_t, Resource*>::iterator it;
    it = Resource::map.find((uint32_t)handle);
    if (it == Resource::map.end())
        return EINVAL;

    return sync_iovec_to_linear(it->second, offset, box);
}

void virgl_renderer_ctx_attach_resource(int ctx_id, int res_handle) {
    std::map<uint32_t, Context*>::iterator ctx_it;

    ctx_it = Context::map.find((uint32_t)ctx_id);
    if (ctx_it == Context::map.end())
        return;

    Context* ctx = ctx_it->second;

    assert(!ctx->resource_map.count((uint32_t)res_handle) &&
           "Can't attach resource to context twice!");

    std::map<uint32_t, Resource*>::iterator res_it;

    res_it = Resource::map.find((uint32_t)res_handle);
    if (res_it == Resource::map.end())
        return;

    Resource* res = res_it->second;

    printf("Attaching Resource %d to Context %d\n", res_handle, ctx_id);
    res->context_map.emplace((uint32_t)ctx_id, ctx);
    ctx->resource_map.emplace((uint32_t)res_handle, res);
}

void virgl_renderer_ctx_detach_resource(int ctx_id, int res_handle) {
    std::map<uint32_t, Context*>::iterator ctx_it;

    ctx_it = Context::map.find((uint32_t)ctx_id);
    if (ctx_it == Context::map.end())
        return;

    Context* ctx = ctx_it->second;

    std::map<uint32_t, Resource*>::iterator res_it;

    res_it = ctx->resource_map.find((uint32_t)res_handle);
    if (res_it == ctx->resource_map.end())
        return;

    Resource* res = res_it->second;

    ctx_it = res->context_map.find((uint32_t)ctx_id);
    if (ctx_it == res->context_map.end())
        return;

    printf("Detaching Resource %d from Context %d\n", res_handle, ctx_id);
    if (ctx->cmd_resp && ctx->cmd_resp->args.handle == (uint32_t)res_handle)
        ctx->cmd_resp = nullptr;
    ctx->resource_map.erase(res_it);
    res->context_map.erase(ctx_it);
}
