/*
 * Copyright 2019 The Android Open Source Project
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

#include <android/system/suspend/1.0/ISystemSuspend.h>
#include <android/system/suspend/ISuspendControlService.h>
#include <benchmark/benchmark.h>
#include <binder/IServiceManager.h>

using android::IBinder;
using android::sp;
using android::system::suspend::ISuspendControlService;
using android::system::suspend::WakeLockInfo;
using android::system::suspend::V1_0::ISystemSuspend;
using android::system::suspend::V1_0::IWakeLock;
using android::system::suspend::V1_0::WakeLockType;

static void BM_acquireWakeLock(benchmark::State& state) {
    static sp<ISystemSuspend> suspendService = ISystemSuspend::getService();

    while (state.KeepRunning()) {
        suspendService->acquireWakeLock(WakeLockType::PARTIAL, "BenchmarkWakeLock");
    }
}
BENCHMARK(BM_acquireWakeLock);

static void BM_getWakeLockStats(benchmark::State& state) {
    static sp<IBinder> control =
        android::defaultServiceManager()->getService(android::String16("suspend_control"));
    static sp<ISuspendControlService> controlService =
        android::interface_cast<ISuspendControlService>(control);

    while (state.KeepRunning()) {
        std::vector<WakeLockInfo> wlStats;
        controlService->getWakeLockStats(&wlStats);
    }
}
BENCHMARK(BM_getWakeLockStats);

BENCHMARK_MAIN();
