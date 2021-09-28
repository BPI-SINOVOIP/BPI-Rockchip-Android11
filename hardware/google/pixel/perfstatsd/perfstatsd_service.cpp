/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <perfstatsd.h>
#include <perfstatsd_service.h>

android::status_t PerfstatsdPrivateService::start() {
    return BinderService<PerfstatsdPrivateService>::publish();
}

android::binder::Status PerfstatsdPrivateService::dumpHistory(std::string *_aidl_return) {
    perfstatsdSp->getHistory(_aidl_return);
    return android::binder::Status::ok();
}

android::binder::Status PerfstatsdPrivateService::setOptions(const std::string &key,
                                                             const std::string &value) {
    perfstatsdSp->setOptions(std::forward<const std::string>(key),
                             std::forward<const std::string>(value));
    return android::binder::Status::ok();
}

android::sp<IPerfstatsdPrivate> getPerfstatsdPrivateService() {
    android::sp<android::IServiceManager> sm = android::defaultServiceManager();
    if (sm == NULL)
        return NULL;

    android::sp<android::IBinder> binder = sm->getService(android::String16("perfstatsd_pri"));
    if (binder == NULL)
        return NULL;

    return android::interface_cast<IPerfstatsdPrivate>(binder);
}
