// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_STORE_V4L2_COMPONENT_STORE_H
#define ANDROID_V4L2_CODEC2_STORE_V4L2_COMPONENT_STORE_H

#include <map>
#include <mutex>

#include <android-base/thread_annotations.h>
#include <C2Component.h>
#include <C2ComponentFactory.h>
#include <util/C2InterfaceHelper.h>

namespace android {

class V4L2ComponentStore : public C2ComponentStore {
public:
    static std::shared_ptr<C2ComponentStore> Create();
    ~V4L2ComponentStore();

    // C2ComponentStore implementation.
    C2String getName() const override;
    c2_status_t createComponent(C2String name,
                                std::shared_ptr<C2Component>* const component) override;
    c2_status_t createInterface(C2String name,
                                std::shared_ptr<C2ComponentInterface>* const interface) override;
    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override;
    std::shared_ptr<C2ParamReflector> getParamReflector() const override;
    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer> src,
                           std::shared_ptr<C2GraphicBuffer> dst) override;
    c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const override;
    c2_status_t query_sm(const std::vector<C2Param*>& stackParams,
                         const std::vector<C2Param::Index>& heapParamIndices,
                         std::vector<std::unique_ptr<C2Param>>* const heapParams) const override;
    c2_status_t config_sm(const std::vector<C2Param*>& params,
                          std::vector<std::unique_ptr<C2SettingResult>>* const failures) override;
    c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery>& fields) const override;

private:
    using CreateV4L2FactoryFunc = ::C2ComponentFactory* (*)(const char* /* componentName */);
    using DestroyV4L2FactoryFunc = void (*)(::C2ComponentFactory*);

    V4L2ComponentStore(void* libHandle, CreateV4L2FactoryFunc createFactoryFunc,
                       DestroyV4L2FactoryFunc destroyFactoryFunc);

    ::C2ComponentFactory* GetFactory(const C2String& name);
    std::shared_ptr<const C2Component::Traits> GetTraits(const C2String& name);

    void* mLibHandle;
    CreateV4L2FactoryFunc mCreateFactoryFunc;
    DestroyV4L2FactoryFunc mDestroyFactoryFunc;

    std::shared_ptr<C2ReflectorHelper> mReflector;

    std::mutex mCachedFactoriesLock;
    std::map<C2String, ::C2ComponentFactory*> mCachedFactories GUARDED_BY(mCachedFactoriesLock);
    std::mutex mCachedTraitsLock;
    std::map<C2String, std::shared_ptr<const C2Component::Traits>> mCachedTraits GUARDED_BY(mCachedTraitsLock);
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_STORE_V4L2_COMPONENT_STORE_H
