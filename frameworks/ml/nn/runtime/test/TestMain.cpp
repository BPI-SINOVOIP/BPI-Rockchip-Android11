/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "NeuralNetworksTest"

#include "LogTestCaseToLogcat.h"
#include "TestNeuralNetworksWrapper.h"

#ifndef NNTEST_ONLY_PUBLIC_API
#include "Manager.h"
#include "Utils.h"
#endif

#include <android-base/logging.h>
#include <gtest/gtest.h>

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using namespace android::nn::test_wrapper;

// We run through the test suite several times, by invoking test() several
// times.  Each run is a "pass".

// Bitmask of passes we're allowed to run.
static uint64_t allowedPasses = ~uint64_t(0);

// DeviceManager::setUseCpuOnly() and Execution::setComputeUsesSynchronousAPI()
// according to arguments, and return RUN_ALL_TESTS().  It is unspecified what
// values those settings have when this function returns.
//
// EXCEPTION: If NNTEST_ONLY_PUBLIC_API is defined, then we cannot call
// non-public DeviceManager::setUseCpuOnly(); we assume the setting is always
// false, and if we are asked to set it to true, we return 0 ("success") without
// running tests.
//
// EXCEPTION: If NNTEST_ONLY_PUBLIC_API is defined, then we cannot call
// non-public DeviceManager::setSyncExecHal(); we assume the setting is always
// true, and if we are asked to set it to false, we return 0 ("success") without
// running tests.
static int test(bool useCpuOnly, Execution::ComputeMode computeMode, bool allowSyncExecHal = true) {
    // NOTE: The test mapping configuration (frameworks/ml/nn/TEST_MAPPING) uses
    // the value of 1024 to only run pass 10 of the test, corresponding to
    // "useCpuOnly = 0, computeMode = ComputeMode::ASYNC, allowSyncExecHal = 1".
    // If you change the bit representation here, also make the corresponding
    // change to the TEST_MAPPING file to run the equivalent pass of the test.
    uint32_t passIndex =
            (useCpuOnly << 0) + (static_cast<uint32_t>(computeMode) << 1) + (allowSyncExecHal << 3);

#ifdef NNTEST_ONLY_PUBLIC_API
    if (useCpuOnly || !allowSyncExecHal) {
        return 0;
    }
#else
    android::nn::DeviceManager::get()->setUseCpuOnly(useCpuOnly);
    android::nn::DeviceManager::get()->setSyncExecHal(allowSyncExecHal);
#endif

    Execution::setComputeMode(computeMode);

    auto computeModeText = [computeMode] {
        switch (computeMode) {
            case Execution::ComputeMode::SYNC:
                return "ComputeMode::SYNC";
            case Execution::ComputeMode::ASYNC:
                return "ComputeMode::ASYNC";
            case Execution::ComputeMode::BURST:
                return "ComputeMode::BURST";
            case Execution::ComputeMode::FENCED:
                return "ComputeMode::FENCED";
        }
        return "<unknown ComputeMode>";
    };

    std::stringstream stream;
    stream << "useCpuOnly = " << useCpuOnly << ", computeMode = " << computeModeText()
           << ", allowSyncExecHal = " << allowSyncExecHal << "  // pass " << passIndex;
    const std::string message = stream.str();
    LOG(INFO) << message;
    std::cout << "[**********] " << message << std::endl;
    SCOPED_TRACE(message);

    if (!((uint64_t(1) << passIndex) & allowedPasses)) {
        LOG(INFO) << "SKIPPED PASS";
        std::cout << "SKIPPED PASS" << std::endl;
        return 0;
    }

    return RUN_ALL_TESTS();
}

void checkArgs(int argc, char** argv, int nextArg) {
    if (nextArg != argc) {
        std::cerr << "Unexpected argument: " << argv[nextArg] << std::endl;
        exit(1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    testing::UnitTest::GetInstance()->listeners().Append(new android::nn::LogTestCaseToLogcat());

    if ((argc > 1) && std::isdigit(argv[1][0])) {
        allowedPasses = std::stoull(argv[1]);
        checkArgs(argc, argv, 2);
    } else {
        checkArgs(argc, argv, 1);
    }

#ifndef NNTEST_ONLY_PUBLIC_API
    android::nn::initVLogMask();
#endif

    int n = test(/*useCpuOnly=*/false, Execution::ComputeMode::ASYNC) |
            test(/*useCpuOnly=*/false, Execution::ComputeMode::SYNC) |
            test(/*useCpuOnly=*/true, Execution::ComputeMode::ASYNC) |
            test(/*useCpuOnly=*/true, Execution::ComputeMode::SYNC);

    // Now try disabling use of synchronous execution HAL.
    //
    // Whether or not the use of synchronous execution HAL is enabled should make no
    // difference when useCpuOnly = true; we already ran test(true, *, true) above,
    // so there's no reason to run test(true, *, false) now.
    n |= test(/*useCpuOnly=*/false, Execution::ComputeMode::ASYNC, /*allowSyncExecHal=*/false) |
         test(/*useCpuOnly=*/false, Execution::ComputeMode::SYNC, /*allowSyncExecHal=*/false);

    // Now try execution using a burst.
    //
    // The burst path is off by default in these tests. This is the first case
    // where it is turned on. Both "useCpuOnly" and "allowSyncExecHal" are
    // irrelevant here because the burst path is separate from both.
    n |= test(/*useCpuOnly=*/false, Execution::ComputeMode::BURST);

    return n;
}
