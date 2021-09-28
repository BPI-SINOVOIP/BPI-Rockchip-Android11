/*
 * Copyright 2020 The Android Open Source Project
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

#define LOG_TAG "SurroundView3dSessionTests"

#include "AnimationModule.h"
#include "IOModule.h"
#include "SurroundView3dSession.h"
#include "VhalHandler.h"
#include "mock-evs/MockEvsEnumerator.h"
#include "mock-evs/MockSurroundViewCallback.h"

#include <android-base/logging.h>
#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <gtest/gtest.h>
#include <hidlmemory/mapping.h>

#include <time.h>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {
namespace {

using ::android::sp;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::automotive::sv::V1_0::OverlayMemoryDesc;
using ::android::hidl::allocator::V1_0::IAllocator;
using ::android::hidl::memory::V1_0::IMemory;

const char* kSvConfigFilename = "vendor/etc/automotive/sv/sv_sample_config.xml";

// Sv 3D output height and width set by the config file.
const int kSv3dWidth = 1920;
const int kSv3dHeight = 1080;

// Constants for overlays.
const int kVertexByteSize = (3 * sizeof(float)) + 4;
const int kIdByteSize = 2;

class SurroundView3dSessionTests : public ::testing::Test {
protected:
    // Setup sv3d session without vhal and animations.
    void SetupSv3dSession() {
        mFakeEvs = new MockEvsEnumerator();
        mIoModule = new IOModule(kSvConfigFilename);
        EXPECT_EQ(mIoModule->initialize(), IOStatus::OK);

        mIoModule->getConfig(&mIoModuleConfig);

        mSv3dSession = new SurroundView3dSession(mFakeEvs, nullptr,
                                                 nullptr,
                                                 &mIoModuleConfig);

        EXPECT_TRUE(mSv3dSession->initialize());
        mSv3dCallback = new MockSurroundViewCallback(mSv3dSession);
        vector<View3d> views = {
        {
            .viewId = 0,
            .pose = {
                .rotation = {.x = 0, .y = 0, .z = 0, .w = 1.0f},
                .translation = {.x = 0, .y = 0, .z = 0},
            },
            .horizontalFov = 90,
        }};
        mSv3dSession->setViews(views);
    }

    // Setup sv3d session with vhal and animations.
    void SetupSv3dSessionVhalAnimation() {
        mFakeEvs = new MockEvsEnumerator();
        mIoModule = new IOModule(kSvConfigFilename);
        EXPECT_EQ(mIoModule->initialize(), IOStatus::OK);

        mIoModule->getConfig(&mIoModuleConfig);

        mVhalHandler = new VhalHandler();
        ASSERT_TRUE(mVhalHandler->initialize(VhalHandler::UpdateMethod::GET, 10));

        mAnimationModule = new AnimationModule(mIoModuleConfig.carModelConfig.carModel.partsMap,
                                    mIoModuleConfig.carModelConfig.carModel.texturesMap,
                                    mIoModuleConfig.carModelConfig.animationConfig.animations);

        const std::vector<uint64_t> animationPropertiesToRead =
                getAnimationPropertiesToRead(mIoModuleConfig.carModelConfig.animationConfig);
        ASSERT_TRUE(mVhalHandler->setPropertiesToRead(animationPropertiesToRead));

        mSv3dSessionAnimations = new SurroundView3dSession(mFakeEvs, mVhalHandler,
                                                 mAnimationModule,
                                                 &mIoModuleConfig);

        EXPECT_TRUE(mSv3dSessionAnimations->initialize());

        mSv3dCallbackAnimations = new MockSurroundViewCallback(mSv3dSessionAnimations);
        vector<View3d> views = {
        // View 0
        {
            .viewId = 0,
            .pose = {
                .rotation = {.x = 0, .y = 0, .z = 0, .w = 1.0f},
                .translation = {.x = 0, .y = 0, .z = 0},
            },
            .horizontalFov = 90,
        }};

        mSv3dSessionAnimations->setViews(views);
    }

    // Helper function to get list of vhal properties to read from car config file for animations.
    std::vector<uint64_t> getAnimationPropertiesToRead(const AnimationConfig& animationConfig) {
        std::set<uint64_t> propertiesSet;
        for (const auto& animation : animationConfig.animations) {
            for (const auto& opPair : animation.gammaOpsMap) {
                propertiesSet.insert(opPair.first);
            }

            for (const auto& opPair : animation.textureOpsMap) {
                propertiesSet.insert(opPair.first);
            }

            for (const auto& opPair : animation.rotationOpsMap) {
                propertiesSet.insert(opPair.first);
            }

            for (const auto& opPair : animation.translationOpsMap) {
                propertiesSet.insert(opPair.first);
            }
        }
        std::vector<uint64_t> propertiesToRead;
        propertiesToRead.assign(propertiesSet.begin(), propertiesSet.end());
        return propertiesToRead;
    }

    void TearDown() override {
        mSv3dSession = nullptr;
        mFakeEvs = nullptr;
        mSv3dCallback = nullptr;
        delete mVhalHandler;
        delete mAnimationModule;
        delete mIoModule;
    }

    sp<IEvsEnumerator> mFakeEvs;
    IOModule* mIoModule;
    IOModuleConfig mIoModuleConfig;
    sp<SurroundView3dSession> mSv3dSession;
    sp<MockSurroundViewCallback> mSv3dCallback;

    VhalHandler* mVhalHandler;
    AnimationModule* mAnimationModule;
    sp<SurroundView3dSession> mSv3dSessionAnimations;
    sp<MockSurroundViewCallback> mSv3dCallbackAnimations;
};

TEST_F(SurroundView3dSessionTests, startAndStop3dSession) {
    SetupSv3dSession();
    EXPECT_EQ(mSv3dSession->startStream(mSv3dCallback), SvResult::OK);
    sleep(5);
    mSv3dSession->stopStream();
    EXPECT_GT(mSv3dCallback->getReceivedFramesCount(), 0);
}

TEST_F(SurroundView3dSessionTests, get3dConfigSuccess) {
    SetupSv3dSession();
    Sv3dConfig sv3dConfig;
    mSv3dSession->get3dConfig([&sv3dConfig](const Sv3dConfig& config) { sv3dConfig = config; });

    EXPECT_EQ(sv3dConfig.width, kSv3dWidth);
    EXPECT_EQ(sv3dConfig.height, kSv3dHeight);
    EXPECT_EQ(sv3dConfig.carDetails, SvQuality::HIGH);
}

// Sets a different config and checks of the received config matches.
TEST_F(SurroundView3dSessionTests, setAndGet3dConfigSuccess) {
    SetupSv3dSession();
    Sv3dConfig sv3dConfigSet = {kSv3dWidth / 2, kSv3dHeight / 2, SvQuality::LOW};

    EXPECT_EQ(mSv3dSession->set3dConfig(sv3dConfigSet), SvResult::OK);

    Sv3dConfig sv3dConfigReceived;
    mSv3dSession->get3dConfig(
            [&sv3dConfigReceived](const Sv3dConfig& config) { sv3dConfigReceived = config; });

    EXPECT_EQ(sv3dConfigReceived.width, sv3dConfigSet.width);
    EXPECT_EQ(sv3dConfigReceived.height, sv3dConfigSet.height);
    EXPECT_EQ(sv3dConfigReceived.carDetails, sv3dConfigSet.carDetails);
}

// Projects center of each cameras and checks if valid projected point is received.
TEST_F(SurroundView3dSessionTests, projectPoints3dSuccess) {
    SetupSv3dSession();
    hidl_vec<Point2dInt> points2dCamera = {
            /*Center point*/ {.x = kSv3dWidth / 2, .y = kSv3dHeight / 2}};

    std::vector<hidl_string> cameraIds = {"/dev/video60", "/dev/video61", "/dev/video62",
                                          "/dev/video63"};

    for (int i = 0; i < cameraIds.size(); i++) {
        mSv3dSession
                ->projectCameraPointsTo3dSurface(points2dCamera, cameraIds[i],
                                                 [](const hidl_vec<Point3dFloat>& projectedPoints) {
                                                     EXPECT_TRUE(projectedPoints[0].isValid);
                                                 });
    }
}

std::pair<hidl_memory, sp<IMemory>> GetMappedSharedMemory(int bytesSize) {
    const auto nullResult = std::make_pair(hidl_memory(), nullptr);

    sp<IAllocator> ashmemAllocator = IAllocator::getService("ashmem");
    if (ashmemAllocator.get() == nullptr) {
        return nullResult;
    }

    // Allocate shared memory.
    hidl_memory hidlMemory;
    bool allocateSuccess = false;
    Return<void> result =
            ashmemAllocator->allocate(bytesSize, [&](bool success, const hidl_memory& hidlMem) {
                if (!success) {
                    return;
                }
                allocateSuccess = success;
                hidlMemory = hidlMem;
            });

    // Check result of allocated memory.
    if (!result.isOk() || !allocateSuccess) {
        return nullResult;
    }

    // Map shared memory.
    sp<IMemory> pIMemory = mapMemory(hidlMemory);
    if (pIMemory.get() == nullptr) {
        return nullResult;
    }

    return std::make_pair(hidlMemory, pIMemory);
}

void SetIndexOfOverlaysMemory(const std::vector<OverlayMemoryDesc>& overlaysMemDesc,
                              sp<IMemory> pIMemory, int indexPosition, uint16_t indexValue) {
    // Count the number of vertices until the index.
    int totalVerticesCount = 0;
    for (int i = 0; i < indexPosition; i++) {
        totalVerticesCount += overlaysMemDesc[i].verticesCount;
    }

    const int indexBytePosition =
            (indexPosition * kIdByteSize) + (kVertexByteSize * totalVerticesCount);

    uint8_t* pSharedMemoryData = reinterpret_cast<uint8_t*>((void*)pIMemory->getPointer());
    pSharedMemoryData += indexBytePosition;
    uint16_t* pIndex16bit = reinterpret_cast<uint16_t*>(pSharedMemoryData);

    // Modify shared memory.
    pIMemory->update();
    *pIndex16bit = indexValue;
    pIMemory->commit();
}

std::pair<OverlaysData, sp<IMemory>> GetSampleOverlaysData() {
    OverlaysData overlaysData;
    overlaysData.overlaysMemoryDesc.resize(2);

    int sharedMemBytesSize = 0;
    OverlayMemoryDesc overlayMemDesc1, overlayMemDesc2;
    overlayMemDesc1.id = 0;
    overlayMemDesc1.verticesCount = 6;
    overlayMemDesc1.overlayPrimitive = OverlayPrimitive::TRIANGLES;
    overlaysData.overlaysMemoryDesc[0] = overlayMemDesc1;
    sharedMemBytesSize += kIdByteSize + kVertexByteSize * overlayMemDesc1.verticesCount;

    overlayMemDesc2.id = 1;
    overlayMemDesc2.verticesCount = 4;
    overlayMemDesc2.overlayPrimitive = OverlayPrimitive::TRIANGLES_STRIP;
    overlaysData.overlaysMemoryDesc[1] = overlayMemDesc2;
    sharedMemBytesSize += kIdByteSize + kVertexByteSize * overlayMemDesc2.verticesCount;

    std::pair<hidl_memory, sp<IMemory>> sharedMem = GetMappedSharedMemory(sharedMemBytesSize);
    sp<IMemory> pIMemory = sharedMem.second;
    if (pIMemory.get() == nullptr) {
        return std::make_pair(OverlaysData(), nullptr);
    }

    // Get pointer to shared memory data and set all bytes to 0.
    uint8_t* pSharedMemoryData = reinterpret_cast<uint8_t*>((void*)pIMemory->getPointer());
    pIMemory->update();
    memset(pSharedMemoryData, 0, sharedMemBytesSize);
    pIMemory->commit();

    std::vector<OverlayMemoryDesc> overlaysDesc = {overlayMemDesc1, overlayMemDesc2};

    // Set indexes in shared memory.
    SetIndexOfOverlaysMemory(overlaysDesc, pIMemory, 0, overlayMemDesc1.id);
    SetIndexOfOverlaysMemory(overlaysDesc, pIMemory, 1, overlayMemDesc2.id);

    overlaysData.overlaysMemoryDesc = overlaysDesc;
    overlaysData.overlaysMemory = sharedMem.first;

    return std::make_pair(overlaysData, pIMemory);
}

// Verifies a valid overlay can be updated while streaming.
TEST_F(SurroundView3dSessionTests, updateOverlaysSuccess) {
    SetupSv3dSession();
    std::pair<OverlaysData, sp<IMemory>> overlaysData = GetSampleOverlaysData();
    ASSERT_NE(overlaysData.second, nullptr);
    EXPECT_EQ(mSv3dSession->startStream(mSv3dCallback), SvResult::OK);
    SvResult result = mSv3dSession->updateOverlays(overlaysData.first);
    mSv3dSession->stopStream();
    EXPECT_EQ(result, SvResult::OK);
}

// Setup sv 3d sessin with vhal and animations and verify frames are received successfully.
TEST_F(SurroundView3dSessionTests, vhalAnimationSuccess) {
    SetupSv3dSessionVhalAnimation();
    EXPECT_EQ(mSv3dSessionAnimations->startStream(mSv3dCallbackAnimations), SvResult::OK);
    sleep(5);
    mSv3dSessionAnimations->stopStream();
    EXPECT_GT(mSv3dCallbackAnimations->getReceivedFramesCount(), 0);
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
