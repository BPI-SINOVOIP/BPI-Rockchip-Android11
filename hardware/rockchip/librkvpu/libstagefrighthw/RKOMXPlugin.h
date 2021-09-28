/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef RK_OMX_PLUGIN_H_

#define RK_OMX_PLUGIN_H_

#include <OMXPluginBase.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

namespace android {

struct RKOMXCore {
    typedef OMX_ERRORTYPE (*InitFunc)();
    typedef OMX_ERRORTYPE (*DeinitFunc)();
    typedef OMX_ERRORTYPE (*ComponentNameEnumFunc)(
            OMX_STRING, OMX_U32, OMX_U32);

    typedef OMX_ERRORTYPE (*GetHandleFunc)(
            OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);

    typedef OMX_ERRORTYPE (*FreeHandleFunc)(OMX_HANDLETYPE *);

    typedef OMX_ERRORTYPE (*GetRolesOfComponentFunc)(
            OMX_STRING, OMX_U32 *, OMX_U8 **);

    void *mLibHandle;

    InitFunc mInit;
    DeinitFunc mDeinit;
    ComponentNameEnumFunc mComponentNameEnum;
    GetHandleFunc mGetHandle;
    FreeHandleFunc mFreeHandle;
    GetRolesOfComponentFunc mGetRolesOfComponentHandle;

    OMX_U32 mNumComponents;
};

struct RKOMXComponent {
    OMX_COMPONENTTYPE *mComponent;
    RKOMXCore *mCore;
};

struct RKOMXPlugin : public OMXPluginBase {
    RKOMXPlugin();
    virtual ~RKOMXPlugin();

    virtual OMX_ERRORTYPE makeComponentInstance(
            const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

    virtual OMX_ERRORTYPE destroyComponentInstance(
            OMX_COMPONENTTYPE *component);

    virtual OMX_ERRORTYPE enumerateComponents(
            OMX_STRING name,
            size_t size,
            OMX_U32 index);

    virtual OMX_ERRORTYPE getRolesOfComponent(
            const char *name,
            Vector<String8> *roles);

private:

    Mutex mMutex; // to protect access to mComponents

    Vector<RKOMXCore*> mCores;
    Vector<RKOMXComponent> mComponents;

    OMX_ERRORTYPE AddCore(const char* coreName);

    RKOMXPlugin(const RKOMXPlugin &);
    RKOMXPlugin &operator=(const RKOMXPlugin &);
};

}  // namespace android

#endif  // RK_OMX_PLUGIN_H_
