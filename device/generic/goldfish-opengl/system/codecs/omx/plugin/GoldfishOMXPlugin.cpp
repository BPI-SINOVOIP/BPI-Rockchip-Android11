/*
 * Copyright 2018 The Android Open Source Project
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

#define LOG_TAG "GoldfishOMXPlugin"
#include "GoldfishOMXPlugin.h"

//#define LOG_NDEBUG 0
#include <vector>

#include <cutils/properties.h>
#include <log/log.h>

#include "GoldfishOMXComponent.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AString.h>

#include <dlfcn.h>

namespace android {

OMXPluginBase *createOMXPlugin() {
    ALOGD("called createOMXPlugin for Goldfish");
    return new GoldfishOMXPlugin;
}

// Each component's name should have it's own feature flag in order to toggle
// individual codecs
struct GoldfishComponent {
    const char *mName;
    const char *mLibNameSuffix;
    const char *mRole;
};

static bool useGoogleGoldfishComponentInstance(const char* libname) {
    // We have a property set indicating whether to use the host side codec
    // or not (ro.kernel.qemu.hwcodec.<mLibNameSuffix>).
    char propValue[PROP_VALUE_MAX];
    AString prop = "ro.kernel.qemu.hwcodec.";
    prop.append(libname);

    bool myret = property_get(prop.c_str(), propValue, "") > 0 &&
           strcmp("1", propValue) == 0;
    if (myret) {
        ALOGD("%s %d found prop %s val %s", __func__, __LINE__, prop.c_str(), propValue);
    }
    return myret;
}

static bool useAndroidGoldfishComponentInstance(const char* libname) {
    // We have a property set indicating whether to use the host side codec
    // or not (ro.kernel.qemu.hwcodec.<mLibNameSuffix>).
    char propValue[PROP_VALUE_MAX];
    AString prop = "ro.kernel.qemu.hwcodec.";
    prop.append(libname);

    bool myret = property_get(prop.c_str(), propValue, "") > 0 &&
           strcmp("2", propValue) == 0;
    if (myret) {
        ALOGD("%s %d found prop %s val %s", __func__, __LINE__, prop.c_str(), propValue);
    }
    return myret;
}

static const GoldfishComponent kComponents[] = {
        {"OMX.google.goldfish.vp8.decoder", "vpxdec", "video_decoder.vp8"},
        {"OMX.google.goldfish.vp9.decoder", "vpxdec", "video_decoder.vp9"},
        {"OMX.google.goldfish.h264.decoder", "avcdec", "video_decoder.avc"},
        {"OMX.android.goldfish.vp8.decoder", "vpxdec", "video_decoder.vp8"},
        {"OMX.android.goldfish.vp9.decoder", "vpxdec", "video_decoder.vp9"},
        {"OMX.android.goldfish.h264.decoder", "avcdec", "video_decoder.avc"},
};

static std::vector<GoldfishComponent> kActiveComponents;

static const size_t kNumComponents =
    sizeof(kComponents) / sizeof(kComponents[0]);

GoldfishOMXPlugin::GoldfishOMXPlugin() {
    for (int i = 0; i < kNumComponents; ++i) {
        if ( !strncmp("OMX.google", kComponents[i].mName, 10) &&
             useGoogleGoldfishComponentInstance(kComponents[i].mLibNameSuffix)) {
            ALOGD("found and use kComponents[i].name %s", kComponents[i].mName);
            kActiveComponents.push_back(kComponents[i]);
        } else if (!strncmp("OMX.android", kComponents[i].mName, 11) &&
                   useAndroidGoldfishComponentInstance(kComponents[i].mLibNameSuffix)) {
            ALOGD("found and use kComponents[i].name %s", kComponents[i].mName);
            kActiveComponents.push_back(kComponents[i]);
        }
    }
}

OMX_ERRORTYPE GoldfishOMXPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component) {
    ALOGI("makeComponentInstance '%s'", name);

    for (size_t i = 0; i < kActiveComponents.size(); ++i) {
        if (strcmp(name, kActiveComponents[i].mName)) {
            continue;
        }

        AString libName;
        AString ldsExport;
        libName = "libstagefright_goldfish_";
        ldsExport = "_Z26createGoldfishOMXComponentPKcPK16OMX_CALLBACKTYPEPvPP17OMX_COMPONENTTYPE";
        ALOGI("Using goldfish codec for '%s'", kActiveComponents[i].mLibNameSuffix);

        libName.append(kActiveComponents[i].mLibNameSuffix);
        libName.append(".so");

        void *libHandle = dlopen(libName.c_str(), RTLD_NOW|RTLD_NODELETE);

        if (libHandle == NULL) {
            ALOGE("unable to dlopen %s: %s", libName.c_str(), dlerror());

            return OMX_ErrorComponentNotFound;
        }

        typedef GoldfishOMXComponent *(*CreateGoldfishOMXComponentFunc)(
                const char *, const OMX_CALLBACKTYPE *,
                OMX_PTR, OMX_COMPONENTTYPE **);

        CreateGoldfishOMXComponentFunc createGoldfishOMXComponent =
            (CreateGoldfishOMXComponentFunc)dlsym(
                    libHandle,
                    ldsExport.c_str()
                    );

        if (createGoldfishOMXComponent == NULL) {
            ALOGE("unable to create component for %s", libName.c_str());
            dlclose(libHandle);
            libHandle = NULL;

            return OMX_ErrorComponentNotFound;
        }

        sp<GoldfishOMXComponent> codec =
            (*createGoldfishOMXComponent)(name, callbacks, appData, component);

        if (codec == NULL) {
            dlclose(libHandle);
            libHandle = NULL;

            return OMX_ErrorInsufficientResources;
        }

        OMX_ERRORTYPE err = codec->initCheck();
        if (err != OMX_ErrorNone) {
            dlclose(libHandle);
            libHandle = NULL;

            return err;
        }

        codec->incStrong(this);
        codec->setLibHandle(libHandle);

        return OMX_ErrorNone;
    }

    return OMX_ErrorInvalidComponentName;
}

OMX_ERRORTYPE GoldfishOMXPlugin::destroyComponentInstance(
        OMX_COMPONENTTYPE *component) {
    GoldfishOMXComponent *me =
        (GoldfishOMXComponent *)
            ((OMX_COMPONENTTYPE *)component)->pComponentPrivate;

    me->prepareForDestruction();

    CHECK_EQ(me->getStrongCount(), 1);
    me->decStrong(this);
    me = NULL;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GoldfishOMXPlugin::enumerateComponents(
        OMX_STRING name,
        size_t /* size */,
        OMX_U32 index) {
    if (index >= kActiveComponents.size()) {
        return OMX_ErrorNoMore;
    }

    ALOGD("enumerate %s component", kActiveComponents[index].mName);
    strcpy(name, kActiveComponents[index].mName);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GoldfishOMXPlugin::getRolesOfComponent(
        const char *name,
        Vector<String8> *roles) {
    for (size_t i = 0; i < kActiveComponents.size(); ++i) {
        if (strcmp(name, kActiveComponents[i].mName)) {
            continue;
        }

        roles->clear();
        roles->push(String8(kActiveComponents[i].mRole));

        return OMX_ErrorNone;
    }

    return OMX_ErrorInvalidComponentName;
}

}  // namespace android
