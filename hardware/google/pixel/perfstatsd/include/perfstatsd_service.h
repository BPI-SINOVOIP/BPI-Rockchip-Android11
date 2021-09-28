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

#ifndef _PERFSTATSD_SERVICE_H_
#define _PERFSTATSD_SERVICE_H_

#include <binder/BinderService.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include "android/pixel/perfstatsd/BnPerfstatsdPrivate.h"

using namespace android::pixel::perfstatsd;

extern android::sp<Perfstatsd> perfstatsdSp;

class PerfstatsdPrivateService : public android::BinderService<PerfstatsdPrivateService>,
                                 public BnPerfstatsdPrivate {
  public:
    static android::status_t start();
    static char const *getServiceName() { return "perfstatsd_pri"; }

    android::binder::Status dumpHistory(std::string *_aidl_return);
    android::binder::Status setOptions(const std::string &key, const std::string &value);
};

android::sp<IPerfstatsdPrivate> getPerfstatsdPrivateService();

#endif /* _PERFSTATSD_SERVICE_H_ */
