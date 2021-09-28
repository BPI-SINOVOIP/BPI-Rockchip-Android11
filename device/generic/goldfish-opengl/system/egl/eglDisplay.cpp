/*
* Copyright (C) 2011 The Android Open Source Project
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
#include "eglDisplay.h"
#include "HostConnection.h"
#include "KeyedVectorUtils.h"

#ifdef HOST_BUILD
#include "android/base/files/PathUtils.cpp"
#include "android/base/system/System.cpp"
#endif

#include <string>

#include <dlfcn.h>

static const int systemEGLVersionMajor = 1;
static const int systemEGLVersionMinor = 4;
static const char systemEGLVendor[] = "Google Android emulator";

// list of extensions supported by this EGL implementation
//  NOTE that each extension name should be suffixed with space
static const char systemStaticEGLExtensions[] =
            "EGL_ANDROID_image_native_buffer "
            "EGL_KHR_fence_sync "
            "EGL_KHR_image_base "
            "EGL_KHR_gl_texture_2d_image ";

// extensions to add dynamically depending on host-side support
static const char kDynamicEGLExtNativeSync[] = "EGL_ANDROID_native_fence_sync ";
static const char kDynamicEGLExtWaitSync[] = "EGL_KHR_wait_sync ";

static void *s_gles_lib = NULL;
static void *s_gles2_lib = NULL;

// The following function will be called when we (libEGL)
// gets unloaded
// At this point we want to unload the gles libraries we
// might have loaded during initialization
static void __attribute__ ((destructor)) do_on_unload(void)
{
    if (s_gles_lib) {
        dlclose(s_gles_lib);
    }

    if (s_gles2_lib) {
        dlclose(s_gles2_lib);
    }
}

eglDisplay::eglDisplay() :
    m_initialized(false),
    m_major(0),
    m_minor(0),
    m_hostRendererVersion(0),
    m_numConfigs(0),
    m_numConfigAttribs(0),
    m_attribs(),
    m_configs(NULL),
    m_gles_iface(NULL),
    m_gles2_iface(NULL),
    m_versionString(NULL),
    m_vendorString(NULL),
    m_extensionString(NULL)
{
    pthread_mutex_init(&m_lock, NULL);
    pthread_mutex_init(&m_ctxLock, NULL);
    pthread_mutex_init(&m_surfaceLock, NULL);
}

eglDisplay::~eglDisplay()
{
    terminate();
    pthread_mutex_destroy(&m_lock);
    pthread_mutex_destroy(&m_ctxLock);
    pthread_mutex_destroy(&m_surfaceLock);
}



bool eglDisplay::initialize(EGLClient_eglInterface *eglIface)
{
    pthread_mutex_lock(&m_lock);
    if (!m_initialized) {

        //
        // load GLES client API
        //
        m_gles_iface = loadGLESClientAPI("libGLESv1_CM_emulation",
                                         eglIface,
                                         &s_gles_lib);
        if (!m_gles_iface) {
            pthread_mutex_unlock(&m_lock);
            ALOGE("Failed to load gles1 iface");
            return false;
        }

#ifdef WITH_GLES2
        m_gles2_iface = loadGLESClientAPI("libGLESv2_emulation",
                                          eglIface,
                                          &s_gles2_lib);
        // Note that if loading gles2 failed, we can still run with no
        // GLES2 support, having GLES2 is not mandatory.
#endif

        //
        // establish connection with the host
        //
        HostConnection *hcon = HostConnection::get();
        if (!hcon) {
            pthread_mutex_unlock(&m_lock);
            ALOGE("Failed to establish connection with the host\n");
            return false;
        }
        hcon->setGrallocOnly(false);

        //
        // get renderControl encoder instance
        //
        renderControl_encoder_context_t *rcEnc = hcon->rcEncoder();
        if (!rcEnc) {
            pthread_mutex_unlock(&m_lock);
            ALOGE("Failed to get renderControl encoder instance");
            return false;
        }

        //
        // Query host reneder and EGL version
        //
        m_hostRendererVersion = rcEnc->rcGetRendererVersion(rcEnc);
        EGLint status = rcEnc->rcGetEGLVersion(rcEnc, &m_major, &m_minor);
        if (status != EGL_TRUE) {
            // host EGL initialization failed !!
            pthread_mutex_unlock(&m_lock);
            return false;
        }

        //
        // Take minimum version beween what we support and what the host support
        //
        if (m_major > systemEGLVersionMajor) {
            m_major = systemEGLVersionMajor;
            m_minor = systemEGLVersionMinor;
        }
        else if (m_major == systemEGLVersionMajor &&
                 m_minor > systemEGLVersionMinor) {
            m_minor = systemEGLVersionMinor;
        }

        //
        // Query the host for the set of configs
        //
        m_numConfigs = rcEnc->rcGetNumConfigs(rcEnc, (uint32_t*)&m_numConfigAttribs);
        if (m_numConfigs <= 0 || m_numConfigAttribs <= 0) {
            // just sanity check - should never happen
            pthread_mutex_unlock(&m_lock);
            return false;
        }

        uint32_t nInts = m_numConfigAttribs * (m_numConfigs + 1);
        EGLint tmp_buf[nInts];
        uint32_t configCount = nInts - m_numConfigAttribs;

        m_configs = new EGLint[nInts-m_numConfigAttribs];

        if (!m_configs) {
            pthread_mutex_unlock(&m_lock);
            return false;
        }

        EGLint n = rcEnc->rcGetConfigs(rcEnc, nInts*sizeof(EGLint), (GLuint*)tmp_buf);
        if (n != m_numConfigs) {
            pthread_mutex_unlock(&m_lock);
            return false;
        }

        // Fill the attributes vector.
        // The first m_numConfigAttribs values of tmp_buf are the actual attributes enums.
        for (int i=0; i<m_numConfigAttribs; i++) {
            m_attribs[tmp_buf[i]] = i;
        }

        memcpy(m_configs, tmp_buf + m_numConfigAttribs,
               m_numConfigs*m_numConfigAttribs*sizeof(EGLint));

        m_initialized = true;
    }
    pthread_mutex_unlock(&m_lock);

    processConfigs();

    return true;
}

void eglDisplay::processConfigs()
{
    for (intptr_t i=0; i<m_numConfigs; i++) {
        EGLConfig config = getConfigAtIndex(i);
        PixelFormat format;
        if (getConfigNativePixelFormat(config, &format)) {
            setConfigAttrib(config, EGL_NATIVE_VISUAL_ID, format);
        }
    }
}

void eglDisplay::terminate()
{
    pthread_mutex_lock(&m_lock);
    if (m_initialized) {
        // Cannot use the for loop in the following code because
        // eglDestroyContext may erase elements.
        EGLContextSet::iterator ctxIte = m_contexts.begin();
        while (ctxIte != m_contexts.end()) {
            EGLContextSet::iterator ctxToDelete = ctxIte;
            ctxIte ++;
            eglDestroyContext(static_cast<EGLDisplay>(this), *ctxToDelete);
        }
        EGLSurfaceSet::iterator surfaceIte = m_surfaces.begin();
        while (surfaceIte != m_surfaces.end()) {
            EGLSurfaceSet::iterator surfaceToDelete = surfaceIte;
            surfaceIte ++;
            eglDestroySurface(static_cast<EGLDisplay>(this), *surfaceToDelete);
        }
        m_initialized = false;
        delete [] m_configs;
        m_configs = NULL;

        if (m_versionString) {
            free(m_versionString);
            m_versionString = NULL;
        }
        if (m_vendorString) {
            free(m_vendorString);
            m_vendorString = NULL;
        }
        if (m_extensionString) {
            free(m_extensionString);
            m_extensionString = NULL;
        }
    }
    pthread_mutex_unlock(&m_lock);
}

#ifdef __APPLE__
#define LIBSUFFIX ".dylib"
#else
#ifdef _WIN32
#define LIBSUFFIX ".dll"
#else
#define LIBSUFFIX ".so"
#endif // !_WIN32 (linux)
#endif // !__APPLE__

#ifndef HOST_BUILD
#if PLATFORM_SDK_VERSION >= 26
#define PARTITION "/vendor"
#else
#define PARTITION "/system"
#endif // !PLATFORM_SDK_VERSION >= 26
#if __LP64__
#define LIBDIR "/lib64/egl/"
#else
#define LIBDIR "/lib/egl/"
#endif // !__LP64__
#endif // !HOST_BUILD

EGLClient_glesInterface *eglDisplay::loadGLESClientAPI(const char *basename,
                                                       EGLClient_eglInterface *eglIface,
                                                       void **libHandle)
{
#ifdef HOST_BUILD
    std::string baseDir =
        android::base::System::get()->getProgramDirectory();
    std::string path =
        android::base::pj(
            baseDir, "lib64", std::string(basename) + LIBSUFFIX);
    void *lib = dlopen(path.c_str(), RTLD_NOW);
#else
    std::string path(PARTITION);
    path += LIBDIR;
    path += basename;
    path += LIBSUFFIX;
    void *lib = dlopen(path.c_str(), RTLD_NOW);
#endif

    if (!lib) {
        ALOGE("Failed to dlopen %s", basename);
        return NULL;
    }

    init_emul_gles_t init_gles_func = (init_emul_gles_t)dlsym(lib,"init_emul_gles");
    if (!init_gles_func) {
        ALOGE("Failed to find init_emul_gles");
        dlclose((void*)lib);
        return NULL;
    }

    *libHandle = lib;
    return (*init_gles_func)(eglIface);
}

static char *queryHostEGLString(EGLint name)
{
    HostConnection *hcon = HostConnection::get();
    if (hcon) {
        renderControl_encoder_context_t *rcEnc = hcon->rcEncoder();
        if (rcEnc) {
            int n = rcEnc->rcQueryEGLString(rcEnc, name, NULL, 0);
            if (n < 0) {
                // allocate space for the string.
                char *str = (char *)malloc(-n);
                n = rcEnc->rcQueryEGLString(rcEnc, name, str, -n);
                if (n > 0) {
                    return str;
                }

                free(str);
            }
        }
    }

    return NULL;
}

static char *buildExtensionString()
{
    //Query host extension string
    char *hostExt = queryHostEGLString(EGL_EXTENSIONS);
    if (!hostExt || (hostExt[1] == '\0')) {
        // no extensions on host - only static extension list supported
        return strdup(systemStaticEGLExtensions);
    }

    int n = strlen(hostExt);
    if (n > 0) {
        char *initialEGLExts;
        char *finalEGLExts;

        HostConnection *hcon = HostConnection::get();
        // If we got here, we must have succeeded in queryHostEGLString
        // and we thus should have a valid connection
        assert(hcon);

        asprintf(&initialEGLExts,"%s%s", systemStaticEGLExtensions, hostExt);

        std::string dynamicEGLExtensions;

        if (hcon->rcEncoder()->hasNativeSync() &&
            !strstr(initialEGLExts, kDynamicEGLExtNativeSync)) {
            dynamicEGLExtensions += kDynamicEGLExtNativeSync;

            if (hcon->rcEncoder()->hasNativeSyncV3()) {
                dynamicEGLExtensions += kDynamicEGLExtWaitSync;
            }
        }

        asprintf(&finalEGLExts, "%s%s", initialEGLExts, dynamicEGLExtensions.c_str());

        free((char*)hostExt);
        return finalEGLExts;
    }
    else {
        free((char*)hostExt);
        return strdup(systemStaticEGLExtensions);
    }
}

const char *eglDisplay::queryString(EGLint name)
{
    if (name == EGL_CLIENT_APIS) {
        return "OpenGL_ES";
    }
    else if (name == EGL_VERSION) {
        pthread_mutex_lock(&m_lock);
        if (m_versionString) {
            pthread_mutex_unlock(&m_lock);
            return m_versionString;
        }

        // build version string
        asprintf(&m_versionString, "%d.%d", m_major, m_minor);
        pthread_mutex_unlock(&m_lock);

        return m_versionString;
    }
    else if (name == EGL_VENDOR) {
        pthread_mutex_lock(&m_lock);
        if (m_vendorString) {
            pthread_mutex_unlock(&m_lock);
            return m_vendorString;
        }

        // build vendor string
        const char *hostVendor = queryHostEGLString(EGL_VENDOR);

        if (hostVendor) {
            asprintf(&m_vendorString, "%s Host: %s",
                                     systemEGLVendor, hostVendor);
            free((char*)hostVendor);
        }
        else {
            m_vendorString = (char *)systemEGLVendor;
        }
        pthread_mutex_unlock(&m_lock);

        return m_vendorString;
    }
    else if (name == EGL_EXTENSIONS) {
        pthread_mutex_lock(&m_lock);
        if (m_extensionString) {
            pthread_mutex_unlock(&m_lock);
            return m_extensionString;
        }

        // build extension string
        m_extensionString = buildExtensionString();
        pthread_mutex_unlock(&m_lock);

        return m_extensionString;
    }
    else {
        ALOGE("[%s] Unknown name %d\n", __FUNCTION__, name);
        return NULL;
    }
}

/* To get the value of attribute <a> of config <c> use the following formula:
 * value = *(m_configs + (int)c*m_numConfigAttribs + a);
 */
EGLBoolean eglDisplay::getAttribValue(EGLConfig config, EGLint attribIdx, EGLint * value)
{
    if (attribIdx == ATTRIBUTE_NONE)
    {
        ALOGE("[%s] Bad attribute idx\n", __FUNCTION__);
        return EGL_FALSE;
    }
    *value = *(m_configs + (intptr_t)(getIndexOfConfig(config))*m_numConfigAttribs + attribIdx);
    return EGL_TRUE;
}

#define EGL_COLOR_COMPONENT_TYPE_EXT 0x3339
#define EGL_COLOR_COMPONENT_TYPE_FIXED_EXT 0x333A

EGLConfig eglDisplay::getConfigAtIndex(uint32_t index) const {
    uintptr_t asPtr = (uintptr_t)index;
    return (EGLConfig)(asPtr + 1);
}

uint32_t eglDisplay::getIndexOfConfig(EGLConfig config) const {
    uintptr_t asInteger = (uintptr_t)config;
    return (uint32_t)(asInteger - 1);
}

bool eglDisplay::isValidConfig(EGLConfig cfg) const {
    uint32_t index = getIndexOfConfig(cfg);
    intptr_t asInt = (intptr_t)index;
    return !(asInt < 0 || asInt > m_numConfigs);
}

EGLBoolean eglDisplay::getConfigAttrib(EGLConfig config, EGLint attrib, EGLint * value)
{
    if (attrib == EGL_FRAMEBUFFER_TARGET_ANDROID) {
        *value = EGL_TRUE;
        return EGL_TRUE;
    }
    if (attrib == EGL_COVERAGE_SAMPLES_NV ||
        attrib == EGL_COVERAGE_BUFFERS_NV) {
        *value = 0;
        return EGL_TRUE;
    }
    if (attrib == EGL_DEPTH_ENCODING_NV) {
        *value = EGL_DEPTH_ENCODING_NONE_NV;
        return EGL_TRUE;
    }
    if  (attrib == EGL_COLOR_COMPONENT_TYPE_EXT) {
        *value = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;
        return EGL_TRUE;
    }
    //Though it seems that valueFor() is thread-safe, we don't take chanses
    pthread_mutex_lock(&m_lock);
    EGLBoolean ret =
        getAttribValue(
            config,
            findObjectOrDefault(
                m_attribs, attrib, EGL_DONT_CARE),
            value);
    pthread_mutex_unlock(&m_lock);
    return ret;
}

void eglDisplay::dumpConfig(EGLConfig config)
{
    EGLint value = 0;
    DBG("^^^^^^^^^^ dumpConfig %p ^^^^^^^^^^^^^^^^^^", config);
    for (int i=0; i<m_numConfigAttribs; i++) {
        getAttribValue(config, i, &value);
        DBG("Config %p: {%u}[%d] %d\n", config, getIndexOfConfig(config), i, value);
    }
}

/* To set the value of attribute <a> of config <c> use the following formula:
 * *(m_configs + (int)c*m_numConfigAttribs + a) = value;
 */
EGLBoolean eglDisplay::setAttribValue(EGLConfig config, EGLint attribIdx, EGLint value)
{
    if (attribIdx == ATTRIBUTE_NONE)
    {
        ALOGE("[%s] Bad attribute idx\n", __FUNCTION__);
        return EGL_FALSE;
    }
    *(m_configs + (intptr_t)(getIndexOfConfig(config))*m_numConfigAttribs + attribIdx) = value;
    return EGL_TRUE;
}

EGLBoolean eglDisplay::setConfigAttrib(EGLConfig config, EGLint attrib, EGLint value)
{
    //Though it seems that valueFor() is thread-safe, we don't take chanses
    pthread_mutex_lock(&m_lock);
    EGLBoolean ret =
        setAttribValue(
            config,
            findObjectOrDefault(
                m_attribs,
                attrib,
                EGL_DONT_CARE),
            value);
    pthread_mutex_unlock(&m_lock);
    return ret;
}


EGLBoolean eglDisplay::getConfigNativePixelFormat(EGLConfig config, PixelFormat * format)
{
    EGLint redSize, blueSize, greenSize, alphaSize;

    if (!(
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_RED_SIZE, EGL_DONT_CARE),
                &redSize) &&
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_BLUE_SIZE, EGL_DONT_CARE),
                &blueSize) &&
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_GREEN_SIZE, EGL_DONT_CARE),
                &greenSize) &&
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_ALPHA_SIZE, EGL_DONT_CARE),
                &alphaSize))) {
        ALOGE("Couldn't find value for one of the pixel format attributes");
        return EGL_FALSE;
    }

    //calculate the GL internal format
    if ((redSize==8)&&(greenSize==8)&&(blueSize==8)&&(alphaSize==8)) *format = PIXEL_FORMAT_RGBA_8888; //XXX: BGR?
    else if ((redSize==8)&&(greenSize==8)&&(blueSize==8)&&(alphaSize==0)) *format = PIXEL_FORMAT_RGBX_8888; //XXX or PIXEL_FORMAT_RGB_888
    else if ((redSize==5)&&(greenSize==6)&&(blueSize==5)&&(alphaSize==0)) *format = PIXEL_FORMAT_RGB_565;
    else if ((redSize==5)&&(greenSize==5)&&(blueSize==5)&&(alphaSize==1)) *format = PIXEL_FORMAT_RGBA_5551;
    else if ((redSize==4)&&(greenSize==4)&&(blueSize==4)&&(alphaSize==4)) *format = PIXEL_FORMAT_RGBA_4444;
    else {
        return EGL_FALSE;
    }
    return EGL_TRUE;
}
EGLBoolean eglDisplay::getConfigGLPixelFormat(EGLConfig config, GLenum * format)
{
    EGLint redSize, blueSize, greenSize, alphaSize;

    if (!(
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_RED_SIZE, EGL_DONT_CARE),
                &redSize) &&
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_BLUE_SIZE, EGL_DONT_CARE),
                &blueSize) &&
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_GREEN_SIZE, EGL_DONT_CARE),
                &greenSize) &&
            getAttribValue(
                config,
                findObjectOrDefault(m_attribs, EGL_ALPHA_SIZE, EGL_DONT_CARE),
                &alphaSize))) {
        ALOGE("Couldn't find value for one of the pixel format attributes");
        return EGL_FALSE;
    }

    //calculate the GL internal format
    if ((redSize == greenSize) && (redSize == blueSize) &&
        ((redSize == 8) || (redSize == 16) || (redSize == 32)))
    {
        if (alphaSize == 0) *format = GL_RGB;
        else *format = GL_RGBA;
    }
    else if ((redSize==5)&&(greenSize==6)&&(blueSize==5)&&(alphaSize==0)) *format = GL_RGB565_OES;
    else if ((redSize==5)&&(greenSize==5)&&(blueSize==5)&&(alphaSize==1)) *format = GL_RGB5_A1_OES;
    else if ((redSize==4)&&(greenSize==4)&&(blueSize==4)&&(alphaSize==4)) *format = GL_RGBA4_OES;
    else return EGL_FALSE;

    return EGL_TRUE;
}

void eglDisplay::onCreateContext(EGLContext ctx) {
    pthread_mutex_lock(&m_ctxLock);
    m_contexts.insert(ctx);
    pthread_mutex_unlock(&m_ctxLock);
}

void eglDisplay::onCreateSurface(EGLSurface surface) {
    pthread_mutex_lock(&m_surfaceLock);
    m_surfaces.insert(surface);
    pthread_mutex_unlock(&m_surfaceLock);
}

void eglDisplay::onDestroyContext(EGLContext ctx) {
    pthread_mutex_lock(&m_ctxLock);
    m_contexts.erase(ctx);
    pthread_mutex_unlock(&m_ctxLock);
}

void eglDisplay::onDestroySurface(EGLSurface surface) {
    pthread_mutex_lock(&m_surfaceLock);
    m_surfaces.erase(surface);
    pthread_mutex_unlock(&m_surfaceLock);
}

bool eglDisplay::isContext(EGLContext ctx) {
    pthread_mutex_lock(&m_ctxLock);
    bool res = m_contexts.find(ctx) != m_contexts.end();
    pthread_mutex_unlock(&m_ctxLock);
    return res;
}

bool eglDisplay::isSurface(EGLSurface surface) {
    pthread_mutex_lock(&m_surfaceLock);
    bool res = m_surfaces.find(surface) != m_surfaces.end();
    pthread_mutex_unlock(&m_surfaceLock);
    return res;
}
