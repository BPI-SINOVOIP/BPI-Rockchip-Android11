/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <android/hardware/neuralnetworks/1.2/ADevice.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "HalInterfaces.h"
#include "Manager.h"
#include "Memory.h"
#include "SampleDriver.h"
#include "SampleDriverFull.h"
#include "TestNeuralNetworksWrapper.h"
#include "TestUtils.h"

using namespace android::nn;
using namespace hal;
using Result = test_wrapper::Result;
using Type = test_wrapper::Type;

namespace {

// A buffer for test that does nothing.
class TestBuffer : public IBuffer {
   public:
    Return<ErrorStatus> copyTo(const hidl_memory&) override {
        return ErrorStatus::DEVICE_UNAVAILABLE;
    }
    Return<ErrorStatus> copyFrom(const hidl_memory&, const hidl_vec<uint32_t>&) override {
        return ErrorStatus::DEVICE_UNAVAILABLE;
    }
};

enum class AllocateReturn { OK, BAD_TOKEN, BAD_IBUFFER, BAD_STATUS, NOT_SUPPORTED };

// Print AllocateReturn enum for better GTEST failure messages
std::ostream& operator<<(std::ostream& os, AllocateReturn allocateReturn) {
    switch (allocateReturn) {
        case AllocateReturn::OK:
            return os << "OK";
        case AllocateReturn::BAD_IBUFFER:
            return os << "BAD_IBUFFER";
        case AllocateReturn::BAD_TOKEN:
            return os << "BAD_TOKEN";
        case AllocateReturn::BAD_STATUS:
            return os << "BAD_STATUS";
        case AllocateReturn::NOT_SUPPORTED:
            return os << "NOT_SUPPORTED";
    }
    LOG(FATAL) << "Invalid AllocateReturn code " << static_cast<int>(allocateReturn);
    return os;
}

class TestDriverLatest : public sample_driver::SampleDriver {
   public:
    TestDriverLatest(const char* name, std::set<OperationType> supportedOperations,
                     AllocateReturn allocateReturn)
        : SampleDriver(name),
          kSupportedOperations(std::move(supportedOperations)),
          kAllocateReturn(allocateReturn) {}

    Return<void> getCapabilities_1_3(getCapabilities_1_3_cb cb) override {
        android::nn::initVLogMask();
        // Faster than cpu.
        const PerformanceInfo kPerf = {.execTime = 0.1, .powerUsage = 0.1};
        const Capabilities capabilities = {
                .relaxedFloat32toFloat16PerformanceScalar = kPerf,
                .relaxedFloat32toFloat16PerformanceTensor = kPerf,
                .operandPerformance = nonExtensionOperandPerformance<HalVersion::V1_3>(kPerf),
                .ifPerformance = kPerf,
                .whilePerformance = kPerf};
        cb(ErrorStatus::NONE, capabilities);
        return Void();
    }

    Return<void> getSupportedOperations_1_3(const Model& model,
                                            getSupportedOperations_1_3_cb cb) override {
        // The tests will never use a referenced model.
        CHECK(model.referenced.size() == 0);
        std::vector<bool> supported(model.main.operations.size(), false);
        std::transform(
                model.main.operations.begin(), model.main.operations.end(), supported.begin(),
                [this](const Operation& op) { return kSupportedOperations.count(op.type) > 0; });
        cb(ErrorStatus::NONE, supported);
        return Void();
    }

    Return<void> allocate(const BufferDesc&, const hidl_vec<sp<IPreparedModel>>&,
                          const hidl_vec<BufferRole>&, const hidl_vec<BufferRole>&,
                          allocate_cb cb) override {
        switch (kAllocateReturn) {
            case AllocateReturn::OK:
                cb(ErrorStatus::NONE, new TestBuffer(), mValidBufferToken++);
                return Void();
            case AllocateReturn::BAD_IBUFFER:
                cb(ErrorStatus::NONE, nullptr, mValidBufferToken++);
                return Void();
            case AllocateReturn::BAD_TOKEN:
                cb(ErrorStatus::NONE, new TestBuffer(), 0);
                return Void();
            case AllocateReturn::BAD_STATUS:
                cb(ErrorStatus::GENERAL_FAILURE, new TestBuffer(), mValidBufferToken++);
                return Void();
            case AllocateReturn::NOT_SUPPORTED:
                cb(ErrorStatus::GENERAL_FAILURE, nullptr, 0);
                return Void();
        }
        LOG(FATAL) << "Invalid AllocateReturn code " << static_cast<int>(kAllocateReturn);
        return Void();
    }

   private:
    const std::set<OperationType> kSupportedOperations;
    const AllocateReturn kAllocateReturn;
    uint32_t mValidBufferToken = 1;
};

// Create the following model for test.
//
//     input0 ---+
//               +--- ADD ---> output0 ---+
//     input1 ---+                        +--- MUL ---> output1 (dynamic shape)
//               +--- SUB ---> temp    ---+
//     input2 ---+
//
test_wrapper::Model createTestModel() {
    test_wrapper::Model model;
    test_wrapper::OperandType tensorTypeFullySpecified(Type::TENSOR_FLOAT32, {1});
    test_wrapper::OperandType tensorTypeDynamicShape(Type::TENSOR_FLOAT32, {0});
    test_wrapper::OperandType actType(Type::INT32, {});
    uint32_t input0 = model.addOperand(&tensorTypeFullySpecified);
    uint32_t input1 = model.addOperand(&tensorTypeFullySpecified);
    uint32_t input2 = model.addOperand(&tensorTypeFullySpecified);
    uint32_t temp = model.addOperand(&tensorTypeFullySpecified);
    uint32_t output0 = model.addOperand(&tensorTypeFullySpecified);
    uint32_t output1 = model.addOperand(&tensorTypeDynamicShape);
    uint32_t act = model.addOperand(&actType);
    int32_t activation = 0;
    model.setOperandValue(act, &activation, sizeof(int32_t));
    model.addOperation(ANEURALNETWORKS_ADD, {input0, input1, act}, {output0});
    model.addOperation(ANEURALNETWORKS_SUB, {input1, input2, act}, {temp});
    model.addOperation(ANEURALNETWORKS_MUL, {output0, temp, act}, {output1});
    model.identifyInputsAndOutputs({input0, input1, input2}, {output0, output1});
    EXPECT_EQ(model.finish(), Result::NO_ERROR);
    return model;
}

class MemoryDomainTestBase : public ::testing::Test {
   protected:
    void SetUp() override {
        ::testing::Test::SetUp();
        if (DeviceManager::get()->getUseCpuOnly()) {
            GTEST_SKIP();
        }
        // Clear the device list.
        DeviceManager::get()->forTest_setDevices({});
    }

    void TearDown() override {
        DeviceManager::get()->forTest_reInitializeDeviceList();
        ::testing::Test::TearDown();
    }

    // If "deviceNames" is not empty, the compilation is created with explicit device list;
    // otherwise, it is created normally.
    test_wrapper::Compilation createCompilation(const std::vector<std::string>& deviceNames) {
        test_wrapper::Compilation compilation;
        if (!deviceNames.empty()) {
            // Map device names to ANeuralNetworksDevice.
            std::map<std::string, ANeuralNetworksDevice*> deviceMap;
            uint32_t numDevices = 0;
            EXPECT_EQ(ANeuralNetworks_getDeviceCount(&numDevices), ANEURALNETWORKS_NO_ERROR);
            for (uint32_t i = 0; i < numDevices; i++) {
                ANeuralNetworksDevice* device = nullptr;
                const char* name = nullptr;
                EXPECT_EQ(ANeuralNetworks_getDevice(i, &device), ANEURALNETWORKS_NO_ERROR);
                EXPECT_EQ(ANeuralNetworksDevice_getName(device, &name), ANEURALNETWORKS_NO_ERROR);
                deviceMap.emplace(name, device);
            }
            std::vector<const ANeuralNetworksDevice*> devices(deviceNames.size());
            std::transform(deviceNames.begin(), deviceNames.end(), devices.begin(),
                           [&deviceMap](const std::string& name) { return deviceMap.at(name); });
            Result result;
            std::tie(result, compilation) =
                    test_wrapper::Compilation::createForDevices(&kModel, devices);
            EXPECT_EQ(result, Result::NO_ERROR);
        } else {
            compilation = test_wrapper::Compilation(&kModel);
        }
        EXPECT_EQ(compilation.finish(), Result::NO_ERROR);
        return compilation;
    }

    std::pair<int, test_wrapper::Memory> allocateDeviceMemory(
            const test_wrapper::Compilation& compilation, const std::vector<uint32_t>& inputIndexes,
            const std::vector<uint32_t>& outputIndexes) {
        const auto* annCompilation = compilation.getHandle();
        ANeuralNetworksMemoryDesc* desc = nullptr;
        EXPECT_EQ(ANeuralNetworksMemoryDesc_create(&desc), ANEURALNETWORKS_NO_ERROR);
        for (uint32_t index : inputIndexes) {
            EXPECT_EQ(ANeuralNetworksMemoryDesc_addInputRole(desc, annCompilation, index, 1.0f),
                      ANEURALNETWORKS_NO_ERROR);
        }
        for (uint32_t index : outputIndexes) {
            EXPECT_EQ(ANeuralNetworksMemoryDesc_addOutputRole(desc, annCompilation, index, 1.0f),
                      ANEURALNETWORKS_NO_ERROR);
        }
        EXPECT_EQ(ANeuralNetworksMemoryDesc_finish(desc), ANEURALNETWORKS_NO_ERROR);

        ANeuralNetworksMemory* memory;
        int n = ANeuralNetworksMemory_createFromDesc(desc, &memory);
        ANeuralNetworksMemoryDesc_free(desc);
        return {n, test_wrapper::Memory(memory)};
    }

    static const test_wrapper::Model kModel;
};

const test_wrapper::Model MemoryDomainTestBase::kModel = createTestModel();

// Test memory domain with the following parameters
// - If true, use a V1_2 driver, otherwise, use the latest version;
// - If true, compile with explicit device list, otherwise, compile in the default way;
// - The return of the allocate function.
using MemoryDomainTestParam = std::tuple<bool, bool, AllocateReturn>;

class MemoryDomainTest : public MemoryDomainTestBase,
                         public ::testing::WithParamInterface<MemoryDomainTestParam> {
   protected:
    // If kUseV1_2Driver, allocateReturn must be AllocateReturn::NOT_SUPPORTED.
    void createAndRegisterDriver(const char* name, std::set<OperationType> supportedOperations,
                                 AllocateReturn allocateReturn) {
        sp<V1_0::IDevice> driver;
        if (kUseV1_2Driver) {
            CHECK(allocateReturn == AllocateReturn::NOT_SUPPORTED);
            const sp<TestDriverLatest> testDriver =
                    new TestDriverLatest(name, supportedOperations, AllocateReturn::NOT_SUPPORTED);
            driver = new V1_2::ADevice(testDriver);
        } else {
            driver = new TestDriverLatest(name, std::move(supportedOperations), allocateReturn);
        }
        DeviceManager::get()->forTest_registerDevice(name, driver);
    }

    // If not kCompileWithExplicitDeviceList, the input argument "deviceNames" is ignored.
    test_wrapper::Compilation createCompilation(const std::vector<std::string>& deviceNames) {
        if (kCompileWithExplicitDeviceList) {
            return MemoryDomainTestBase::createCompilation(deviceNames);
        } else {
            return MemoryDomainTestBase::createCompilation({});
        }
    }

    const bool kUseV1_2Driver = std::get<0>(GetParam());
    const bool kCompileWithExplicitDeviceList = std::get<1>(GetParam());
    const AllocateReturn kAllocateReturn = std::get<2>(GetParam());
};

// Test device memory allocation on a compilation with only a single partition.
TEST_P(MemoryDomainTest, SinglePartition) {
    createAndRegisterDriver("test_driver",
                            {OperationType::ADD, OperationType::SUB, OperationType::MUL},
                            kAllocateReturn);
    auto compilation = createCompilation({"test_driver"});
    ASSERT_NE(compilation.getHandle(), nullptr);

    auto [n, memory] = allocateDeviceMemory(compilation, {0}, {0});
    if (kAllocateReturn == AllocateReturn::OK) {
        // The memory should be backed by the IBuffer returned from the driver.
        ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
        const Memory* m = reinterpret_cast<const Memory*>(memory.get());
        ASSERT_NE(m, nullptr);
        EXPECT_NE(m->getIBuffer(), nullptr);
    } else {
        if (kCompileWithExplicitDeviceList) {
            // Should not fallback when the compiled with explicit device list.
            ASSERT_EQ(n, ANEURALNETWORKS_OP_FAILED);
        } else {
            // The memory should fallback to ashmem or blob ahwb based on the driver version.
            ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
            const Memory* m = reinterpret_cast<const Memory*>(memory.get());
            ASSERT_NE(m, nullptr);
            EXPECT_EQ(m->getIBuffer(), nullptr);
            const auto& hidlMemory = m->getHidlMemory();
            EXPECT_TRUE(hidlMemory.valid());
            if (kUseV1_2Driver) {
                EXPECT_EQ(hidlMemory.name(), "ashmem");
            } else {
                EXPECT_EQ(hidlMemory.name(), "hardware_buffer_blob");
            }
        }
    }
}

// Test device memory allocation on a compilation with multiple partitions.
TEST_P(MemoryDomainTest, MultiplePartitions) {
    createAndRegisterDriver("test_driver_add", {OperationType::ADD}, kAllocateReturn);
    createAndRegisterDriver("test_driver_sub", {OperationType::SUB}, kAllocateReturn);
    createAndRegisterDriver("test_driver_mul", {OperationType::MUL}, kAllocateReturn);
    auto compilation = createCompilation({"test_driver_add", "test_driver_sub", "test_driver_mul"});
    ASSERT_NE(compilation.getHandle(), nullptr);

    {
        // input0 is only used in one single partition.
        auto [n, memory] = allocateDeviceMemory(compilation, {0}, {});
        if (kAllocateReturn == AllocateReturn::OK) {
            // The memory should be backed by the IBuffer returned from the driver.
            ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
            const Memory* m = reinterpret_cast<const Memory*>(memory.get());
            ASSERT_NE(m, nullptr);
            EXPECT_NE(m->getIBuffer(), nullptr);
        } else {
            if (kCompileWithExplicitDeviceList) {
                // Should not fallback when the compiled with explicit device list.
                ASSERT_EQ(n, ANEURALNETWORKS_OP_FAILED);
            } else {
                // The memory should fallback to ashmem or blob ahwb based on the driver version.
                ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
                const Memory* m = reinterpret_cast<const Memory*>(memory.get());
                ASSERT_NE(m, nullptr);
                EXPECT_EQ(m->getIBuffer(), nullptr);
                const auto& hidlMemory = m->getHidlMemory();
                EXPECT_TRUE(hidlMemory.valid());
                if (kUseV1_2Driver) {
                    EXPECT_EQ(hidlMemory.name(), "ashmem");
                } else {
                    EXPECT_EQ(hidlMemory.name(), "hardware_buffer_blob");
                }
            }
        }
    }

    {
        // input1 is shared by two partitions with different drivers, so the runtime will not
        // attempt to allocate on device.
        auto [n, memory] = allocateDeviceMemory(compilation, {1}, {});
        if (kCompileWithExplicitDeviceList) {
            // Should not fallback when the compiled with explicit device list.
            ASSERT_EQ(n, ANEURALNETWORKS_OP_FAILED);
        } else {
            // The memory should fallback to ashmem or blob ahwb based on the driver version.
            ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
            const Memory* m = reinterpret_cast<const Memory*>(memory.get());
            ASSERT_NE(m, nullptr);
            EXPECT_EQ(m->getIBuffer(), nullptr);
            const auto& hidlMemory = m->getHidlMemory();
            EXPECT_TRUE(hidlMemory.valid());
            if (kUseV1_2Driver) {
                EXPECT_EQ(hidlMemory.name(), "ashmem");
            } else {
                EXPECT_EQ(hidlMemory.name(), "hardware_buffer_blob");
            }
        }
    }

    {
        // output0 is shared by two partitions with different drivers, so the runtime will not
        // attempt to allocate on device.
        auto [n, memory] = allocateDeviceMemory(compilation, {}, {0});
        if (kCompileWithExplicitDeviceList) {
            // Should not fallback when the compiled with explicit device list.
            ASSERT_EQ(n, ANEURALNETWORKS_OP_FAILED);
        } else {
            // The memory should fallback to ashmem or blob ahwb based on the driver version.
            ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
            const Memory* m = reinterpret_cast<const Memory*>(memory.get());
            ASSERT_NE(m, nullptr);
            EXPECT_EQ(m->getIBuffer(), nullptr);
            const auto& hidlMemory = m->getHidlMemory();
            EXPECT_TRUE(hidlMemory.valid());
            if (kUseV1_2Driver) {
                EXPECT_EQ(hidlMemory.name(), "ashmem");
            } else {
                EXPECT_EQ(hidlMemory.name(), "hardware_buffer_blob");
            }
        }
    }
}

// Test device memory allocation with dynamic shape.
TEST_P(MemoryDomainTest, DynamicShape) {
    createAndRegisterDriver("test_driver",
                            {OperationType::ADD, OperationType::SUB, OperationType::MUL},
                            kAllocateReturn);
    auto compilation = createCompilation({"test_driver"});
    ASSERT_NE(compilation.getHandle(), nullptr);

    auto [n, memory] = allocateDeviceMemory(compilation, {}, {1});
    if (kAllocateReturn == AllocateReturn::OK) {
        // The memory should be backed by the IBuffer returned from the driver.
        ASSERT_EQ(n, ANEURALNETWORKS_NO_ERROR);
        const Memory* m = reinterpret_cast<const Memory*>(memory.get());
        ASSERT_NE(m, nullptr);
        EXPECT_NE(m->getIBuffer(), nullptr);
    } else {
        // We do not fallback in the case of dynamic shape.
        ASSERT_EQ(n, ANEURALNETWORKS_OP_FAILED);
    }
}

static const auto kAllocateReturnChoices =
        testing::Values(AllocateReturn::OK, AllocateReturn::BAD_TOKEN, AllocateReturn::BAD_IBUFFER,
                        AllocateReturn::BAD_STATUS, AllocateReturn::NOT_SUPPORTED);

INSTANTIATE_TEST_CASE_P(DeviceVersionLatest, MemoryDomainTest,
                        testing::Combine(testing::Values(false), testing::Bool(),
                                         kAllocateReturnChoices));
INSTANTIATE_TEST_CASE_P(DeviceVersionV1_2, MemoryDomainTest,
                        testing::Combine(testing::Values(true), testing::Bool(),
                                         testing::Values(AllocateReturn::NOT_SUPPORTED)));

class MemoryDomainCopyTest : public MemoryDomainTestBase {};

TEST_F(MemoryDomainCopyTest, MemoryCopyTest) {
    sp<sample_driver::SampleDriverFull> driver(new sample_driver::SampleDriverFull(
            "test_driver", {.execTime = 0.1f, .powerUsage = 0.1f}));
    DeviceManager::get()->forTest_registerDevice("test_driver", driver);
    auto compilation = createCompilation({"test_driver"});
    ASSERT_NE(compilation.getHandle(), nullptr);

    // Allocate ashmem.
    const float initValue1 = 3.14f, initValue2 = 2.72f;
    auto ashmem1 = TestAshmem::createFrom(&initValue1, sizeof(float));
    auto ashmem2 = TestAshmem::createFrom(&initValue2, sizeof(float));
    ASSERT_NE(ashmem1, nullptr);
    ASSERT_NE(ashmem2, nullptr);

    // Allocate device memories.
    auto [n1, memory1] = allocateDeviceMemory(compilation, {0}, {});
    auto [n2, memory2] = allocateDeviceMemory(compilation, {0}, {});
    ASSERT_EQ(n1, ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(n2, ANEURALNETWORKS_NO_ERROR);

    // Test memory copying: ashmem1 -> memory1 -> memory2 -> ashmem2
    ASSERT_EQ(ANeuralNetworksMemory_copy(ashmem1->get()->get(), memory1.get()),
              ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksMemory_copy(memory1.get(), memory2.get()), ANEURALNETWORKS_NO_ERROR);
    ASSERT_EQ(ANeuralNetworksMemory_copy(memory2.get(), ashmem2->get()->get()),
              ANEURALNETWORKS_NO_ERROR);

    EXPECT_EQ(ashmem2->dataAs<float>()[0], initValue1);
}

}  // namespace
