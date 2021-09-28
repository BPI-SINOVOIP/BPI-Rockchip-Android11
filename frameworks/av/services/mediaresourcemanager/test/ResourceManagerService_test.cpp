/*
 * Copyright 2015 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "ResourceManagerService_test"
#include <utils/Log.h>

#include <gtest/gtest.h>

#include "ResourceManagerService.h"
#include <aidl/android/media/BnResourceManagerClient.h>
#include <media/MediaResource.h>
#include <media/MediaResourcePolicy.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/ProcessInfoInterface.h>

namespace aidl {
namespace android {
namespace media {
bool operator== (const MediaResourceParcel& lhs, const MediaResourceParcel& rhs) {
    return lhs.type == rhs.type && lhs.subType == rhs.subType &&
            lhs.id == rhs.id && lhs.value == rhs.value;
}}}}

namespace android {

using Status = ::ndk::ScopedAStatus;
using ::aidl::android::media::BnResourceManagerClient;
using ::aidl::android::media::IResourceManagerService;
using ::aidl::android::media::IResourceManagerClient;

static int64_t getId(const std::shared_ptr<IResourceManagerClient>& client) {
    return (int64_t) client.get();
}

struct TestProcessInfo : public ProcessInfoInterface {
    TestProcessInfo() {}
    virtual ~TestProcessInfo() {}

    virtual bool getPriority(int pid, int *priority) {
        // For testing, use pid as priority.
        // Lower the value higher the priority.
        *priority = pid;
        return true;
    }

    virtual bool isValidPid(int /* pid */) {
        return true;
    }

private:
    DISALLOW_EVIL_CONSTRUCTORS(TestProcessInfo);
};

struct TestSystemCallback :
        public ResourceManagerService::SystemCallbackInterface {
    TestSystemCallback() :
        mLastEvent({EventType::INVALID, 0}), mEventCount(0) {}

    enum EventType {
        INVALID          = -1,
        VIDEO_ON         = 0,
        VIDEO_OFF        = 1,
        VIDEO_RESET      = 2,
        CPUSET_ENABLE    = 3,
        CPUSET_DISABLE   = 4,
    };

    struct EventEntry {
        EventType type;
        int arg;
    };

    virtual void noteStartVideo(int uid) override {
        mLastEvent = {EventType::VIDEO_ON, uid};
        mEventCount++;
    }

    virtual void noteStopVideo(int uid) override {
        mLastEvent = {EventType::VIDEO_OFF, uid};
        mEventCount++;
    }

    virtual void noteResetVideo() override {
        mLastEvent = {EventType::VIDEO_RESET, 0};
        mEventCount++;
    }

    virtual bool requestCpusetBoost(bool enable) override {
        mLastEvent = {enable ? EventType::CPUSET_ENABLE : EventType::CPUSET_DISABLE, 0};
        mEventCount++;
        return true;
    }

    size_t eventCount() { return mEventCount; }
    EventType lastEventType() { return mLastEvent.type; }
    EventEntry lastEvent() { return mLastEvent; }

protected:
    virtual ~TestSystemCallback() {}

private:
    EventEntry mLastEvent;
    size_t mEventCount;

    DISALLOW_EVIL_CONSTRUCTORS(TestSystemCallback);
};


struct TestClient : public BnResourceManagerClient {
    TestClient(int pid, const std::shared_ptr<ResourceManagerService> &service)
        : mReclaimed(false), mPid(pid), mService(service) {}

    Status reclaimResource(bool* _aidl_return) override {
        mService->removeClient(mPid, getId(ref<TestClient>()));
        mReclaimed = true;
        *_aidl_return = true;
        return Status::ok();
    }

    Status getName(::std::string* _aidl_return) override {
        *_aidl_return = "test_client";
        return Status::ok();
    }

    bool reclaimed() const {
        return mReclaimed;
    }

    void reset() {
        mReclaimed = false;
    }

    virtual ~TestClient() {}

private:
    bool mReclaimed;
    int mPid;
    std::shared_ptr<ResourceManagerService> mService;
    DISALLOW_EVIL_CONSTRUCTORS(TestClient);
};

static const int kTestPid1 = 30;
static const int kTestUid1 = 1010;

static const int kTestPid2 = 20;
static const int kTestUid2 = 1011;

static const int kLowPriorityPid = 40;
static const int kMidPriorityPid = 25;
static const int kHighPriorityPid = 10;

using EventType = TestSystemCallback::EventType;
using EventEntry = TestSystemCallback::EventEntry;
bool operator== (const EventEntry& lhs, const EventEntry& rhs) {
    return lhs.type == rhs.type && lhs.arg == rhs.arg;
}

#define CHECK_STATUS_TRUE(condition) \
    EXPECT_TRUE((condition).isOk() && (result))

#define CHECK_STATUS_FALSE(condition) \
    EXPECT_TRUE((condition).isOk() && !(result))

class ResourceManagerServiceTest : public ::testing::Test {
public:
    ResourceManagerServiceTest()
        : mSystemCB(new TestSystemCallback()),
          mService(::ndk::SharedRefBase::make<ResourceManagerService>(
                  new TestProcessInfo, mSystemCB)),
          mTestClient1(::ndk::SharedRefBase::make<TestClient>(kTestPid1, mService)),
          mTestClient2(::ndk::SharedRefBase::make<TestClient>(kTestPid2, mService)),
          mTestClient3(::ndk::SharedRefBase::make<TestClient>(kTestPid2, mService)) {
    }

protected:
    static bool isEqualResources(const std::vector<MediaResourceParcel> &resources1,
            const ResourceList &resources2) {
        // convert resource1 to ResourceList
        ResourceList r1;
        for (size_t i = 0; i < resources1.size(); ++i) {
            const auto &res = resources1[i];
            const auto resType = std::tuple(res.type, res.subType, res.id);
            r1[resType] = res;
        }
        return r1 == resources2;
    }

    static void expectEqResourceInfo(const ResourceInfo &info,
            int uid,
            std::shared_ptr<IResourceManagerClient> client,
            const std::vector<MediaResourceParcel> &resources) {
        EXPECT_EQ(uid, info.uid);
        EXPECT_EQ(client, info.client);
        EXPECT_TRUE(isEqualResources(resources, info.resources));
    }

    void verifyClients(bool c1, bool c2, bool c3) {
        TestClient *client1 = static_cast<TestClient*>(mTestClient1.get());
        TestClient *client2 = static_cast<TestClient*>(mTestClient2.get());
        TestClient *client3 = static_cast<TestClient*>(mTestClient3.get());

        EXPECT_EQ(c1, client1->reclaimed());
        EXPECT_EQ(c2, client2->reclaimed());
        EXPECT_EQ(c3, client3->reclaimed());

        client1->reset();
        client2->reset();
        client3->reset();
    }

    // test set up
    // ---------------------------------------------------------------------------------
    //   pid                priority         client           type               number
    // ---------------------------------------------------------------------------------
    //   kTestPid1(30)      30               mTestClient1     secure codec       1
    //                                                        graphic memory     200
    //                                                        graphic memory     200
    // ---------------------------------------------------------------------------------
    //   kTestPid2(20)      20               mTestClient2     non-secure codec   1
    //                                                        graphic memory     300
    //                                       -------------------------------------------
    //                                       mTestClient3     secure codec       1
    //                                                        graphic memory     100
    // ---------------------------------------------------------------------------------
    void addResource() {
        // kTestPid1 mTestClient1
        std::vector<MediaResourceParcel> resources1;
        resources1.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);
        resources1.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 200));
        std::vector<MediaResourceParcel> resources11;
        resources11.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 200));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources11);

        // kTestPid2 mTestClient2
        std::vector<MediaResourceParcel> resources2;
        resources2.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));
        resources2.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 300));
        mService->addResource(kTestPid2, kTestUid2, getId(mTestClient2), mTestClient2, resources2);

        // kTestPid2 mTestClient3
        std::vector<MediaResourceParcel> resources3;
        mService->addResource(kTestPid2, kTestUid2, getId(mTestClient3), mTestClient3, resources3);
        resources3.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        resources3.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 100));
        mService->addResource(kTestPid2, kTestUid2, getId(mTestClient3), mTestClient3, resources3);

        const PidResourceInfosMap &map = mService->mMap;
        EXPECT_EQ(2u, map.size());
        ssize_t index1 = map.indexOfKey(kTestPid1);
        ASSERT_GE(index1, 0);
        const ResourceInfos &infos1 = map[index1];
        EXPECT_EQ(1u, infos1.size());
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, resources1);

        ssize_t index2 = map.indexOfKey(kTestPid2);
        ASSERT_GE(index2, 0);
        const ResourceInfos &infos2 = map[index2];
        EXPECT_EQ(2u, infos2.size());
        expectEqResourceInfo(infos2.valueFor(getId(mTestClient2)), kTestUid2, mTestClient2, resources2);
        expectEqResourceInfo(infos2.valueFor(getId(mTestClient3)), kTestUid2, mTestClient3, resources3);
    }

    void testCombineResourceWithNegativeValues() {
        // kTestPid1 mTestClient1
        std::vector<MediaResourceParcel> resources1;
        resources1.push_back(MediaResource(MediaResource::Type::kDrmSession, -100));
        resources1.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, -100));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        // Expected result:
        // 1) the client should have been added;
        // 2) both resource entries should have been rejected, resource list should be empty.
        const PidResourceInfosMap &map = mService->mMap;
        EXPECT_EQ(1u, map.size());
        ssize_t index1 = map.indexOfKey(kTestPid1);
        ASSERT_GE(index1, 0);
        const ResourceInfos &infos1 = map[index1];
        EXPECT_EQ(1u, infos1.size());
        std::vector<MediaResourceParcel> expected;
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);

        resources1.clear();
        resources1.push_back(MediaResource(MediaResource::Type::kDrmSession, INT64_MAX));
        resources1.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, INT64_MAX));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);
        resources1.clear();
        resources1.push_back(MediaResource(MediaResource::Type::kDrmSession, 10));
        resources1.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 10));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        // Expected result:
        // Both values should saturate to INT64_MAX
        expected.push_back(MediaResource(MediaResource::Type::kDrmSession, INT64_MAX));
        expected.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, INT64_MAX));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);

        resources1.clear();
        resources1.push_back(MediaResource(MediaResource::Type::kDrmSession, -10));
        resources1.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, -10));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        // Expected result:
        // 1) DrmSession resource should allow negative value addition, and value should drop accordingly
        // 2) Non-drm session resource should ignore negative value addition.
        expected.push_back(MediaResource(MediaResource::Type::kDrmSession, INT64_MAX - 10));
        expected.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, INT64_MAX));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);

        resources1.clear();
        resources1.push_back(MediaResource(MediaResource::Type::kDrmSession, INT64_MIN));
        expected.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, INT64_MIN));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        // Expected result:
        // 1) DrmSession resource value should drop to 0, but the entry shouldn't be removed.
        // 2) Non-drm session resource should ignore negative value addition.
        expected.clear();
        expected.push_back(MediaResource(MediaResource::Type::kDrmSession, 0));
        expected.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, INT64_MAX));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);
    }

    void testConfig() {
        EXPECT_TRUE(mService->mSupportsMultipleSecureCodecs);
        EXPECT_TRUE(mService->mSupportsSecureWithNonSecureCodec);

        std::vector<MediaResourcePolicyParcel> policies1;
        policies1.push_back(
                MediaResourcePolicy(
                        IResourceManagerService::kPolicySupportsMultipleSecureCodecs,
                        "true"));
        policies1.push_back(
                MediaResourcePolicy(
                        IResourceManagerService::kPolicySupportsSecureWithNonSecureCodec,
                        "false"));
        mService->config(policies1);
        EXPECT_TRUE(mService->mSupportsMultipleSecureCodecs);
        EXPECT_FALSE(mService->mSupportsSecureWithNonSecureCodec);

        std::vector<MediaResourcePolicyParcel> policies2;
        policies2.push_back(
                MediaResourcePolicy(
                        IResourceManagerService::kPolicySupportsMultipleSecureCodecs,
                        "false"));
        policies2.push_back(
                MediaResourcePolicy(
                        IResourceManagerService::kPolicySupportsSecureWithNonSecureCodec,
                        "true"));
        mService->config(policies2);
        EXPECT_FALSE(mService->mSupportsMultipleSecureCodecs);
        EXPECT_TRUE(mService->mSupportsSecureWithNonSecureCodec);
    }

    void testCombineResource() {
        // kTestPid1 mTestClient1
        std::vector<MediaResourceParcel> resources1;
        resources1.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        std::vector<MediaResourceParcel> resources11;
        resources11.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 200));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources11);

        const PidResourceInfosMap &map = mService->mMap;
        EXPECT_EQ(1u, map.size());
        ssize_t index1 = map.indexOfKey(kTestPid1);
        ASSERT_GE(index1, 0);
        const ResourceInfos &infos1 = map[index1];
        EXPECT_EQ(1u, infos1.size());

        // test adding existing types to combine values
        resources1.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 100));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        std::vector<MediaResourceParcel> expected;
        expected.push_back(MediaResource(MediaResource::Type::kSecureCodec, 2));
        expected.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 300));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);

        // test adding new types (including types that differs only in subType)
        resources11.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));
        resources11.push_back(MediaResource(MediaResource::Type::kSecureCodec, MediaResource::SubType::kVideoCodec, 1));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources11);

        expected.clear();
        expected.push_back(MediaResource(MediaResource::Type::kSecureCodec, 2));
        expected.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));
        expected.push_back(MediaResource(MediaResource::Type::kSecureCodec, MediaResource::SubType::kVideoCodec, 1));
        expected.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 500));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);
    }

    void testRemoveResource() {
        // kTestPid1 mTestClient1
        std::vector<MediaResourceParcel> resources1;
        resources1.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);

        std::vector<MediaResourceParcel> resources11;
        resources11.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 200));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources11);

        const PidResourceInfosMap &map = mService->mMap;
        EXPECT_EQ(1u, map.size());
        ssize_t index1 = map.indexOfKey(kTestPid1);
        ASSERT_GE(index1, 0);
        const ResourceInfos &infos1 = map[index1];
        EXPECT_EQ(1u, infos1.size());

        // test partial removal
        resources11[0].value = 100;
        mService->removeResource(kTestPid1, getId(mTestClient1), resources11);

        std::vector<MediaResourceParcel> expected;
        expected.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        expected.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 100));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);

        // test removal request with negative value, should be ignored
        resources11[0].value = -10000;
        mService->removeResource(kTestPid1, getId(mTestClient1), resources11);

        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);

        // test complete removal with overshoot value
        resources11[0].value = 1000;
        mService->removeResource(kTestPid1, getId(mTestClient1), resources11);

        expected.clear();
        expected.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        expectEqResourceInfo(infos1.valueFor(getId(mTestClient1)), kTestUid1, mTestClient1, expected);
    }

    void testOverridePid() {

        bool result;
        std::vector<MediaResourceParcel> resources;
        resources.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        resources.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 150));

        // ### secure codec can't coexist and secure codec can coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsMultipleSecureCodecs = false;
            mService->mSupportsSecureWithNonSecureCodec = true;

            // priority too low to reclaim resource
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));

            // override Low Priority Pid with High Priority Pid
            mService->overridePid(kLowPriorityPid, kHighPriorityPid);
            CHECK_STATUS_TRUE(mService->reclaimResource(kLowPriorityPid, resources, &result));

            // restore Low Priority Pid
            mService->overridePid(kLowPriorityPid, -1);
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));
        }
    }

    void testMarkClientForPendingRemoval() {
        bool result;

        {
            addResource();
            mService->mSupportsSecureWithNonSecureCodec = true;

            std::vector<MediaResourceParcel> resources;
            resources.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));

            // Remove low priority clients
            mService->removeClient(kTestPid1, getId(mTestClient1));

            // no lower priority client
            CHECK_STATUS_FALSE(mService->reclaimResource(kTestPid2, resources, &result));
            verifyClients(false /* c1 */, false /* c2 */, false /* c3 */);

            mService->markClientForPendingRemoval(kTestPid2, getId(mTestClient2));

            // client marked for pending removal from the same process got reclaimed
            CHECK_STATUS_TRUE(mService->reclaimResource(kTestPid2, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // clean up client 3 which still left
            mService->removeClient(kTestPid2, getId(mTestClient3));
        }

        {
            addResource();
            mService->mSupportsSecureWithNonSecureCodec = true;

            std::vector<MediaResourceParcel> resources;
            resources.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));

            mService->markClientForPendingRemoval(kTestPid2, getId(mTestClient2));

            // client marked for pending removal from the same process got reclaimed
            // first, even though there are lower priority process
            CHECK_STATUS_TRUE(mService->reclaimResource(kTestPid2, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // lower priority client got reclaimed
            CHECK_STATUS_TRUE(mService->reclaimResource(kTestPid2, resources, &result));
            verifyClients(true /* c1 */, false /* c2 */, false /* c3 */);

            // clean up client 3 which still left
            mService->removeClient(kTestPid2, getId(mTestClient3));
        }

        {
            addResource();
            mService->mSupportsSecureWithNonSecureCodec = true;

            mService->markClientForPendingRemoval(kTestPid2, getId(mTestClient2));

            // client marked for pending removal got reclaimed
            EXPECT_TRUE(mService->reclaimResourcesFromClientsPendingRemoval(kTestPid2).isOk());
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // No more clients marked for removal
            EXPECT_TRUE(mService->reclaimResourcesFromClientsPendingRemoval(kTestPid2).isOk());
            verifyClients(false /* c1 */, false /* c2 */, false /* c3 */);

            mService->markClientForPendingRemoval(kTestPid2, getId(mTestClient3));

            // client marked for pending removal got reclaimed
            EXPECT_TRUE(mService->reclaimResourcesFromClientsPendingRemoval(kTestPid2).isOk());
            verifyClients(false /* c1 */, false /* c2 */, true /* c3 */);

            // clean up client 1 which still left
            mService->removeClient(kTestPid1, getId(mTestClient1));
        }
    }

    void testRemoveClient() {
        addResource();

        mService->removeClient(kTestPid2, getId(mTestClient2));

        const PidResourceInfosMap &map = mService->mMap;
        EXPECT_EQ(2u, map.size());
        const ResourceInfos &infos1 = map.valueFor(kTestPid1);
        const ResourceInfos &infos2 = map.valueFor(kTestPid2);
        EXPECT_EQ(1u, infos1.size());
        EXPECT_EQ(1u, infos2.size());
        // mTestClient2 has been removed.
        // (OK to use infos2[0] as there is only 1 entry)
        EXPECT_EQ(mTestClient3, infos2[0].client);
    }

    void testGetAllClients() {
        addResource();

        MediaResource::Type type = MediaResource::Type::kSecureCodec;
        Vector<std::shared_ptr<IResourceManagerClient> > clients;
        EXPECT_FALSE(mService->getAllClients_l(kLowPriorityPid, type, &clients));
        // some higher priority process (e.g. kTestPid2) owns the resource, so getAllClients_l
        // will fail.
        EXPECT_FALSE(mService->getAllClients_l(kMidPriorityPid, type, &clients));
        EXPECT_TRUE(mService->getAllClients_l(kHighPriorityPid, type, &clients));

        EXPECT_EQ(2u, clients.size());
        // (OK to require ordering in clients[], as the pid map is sorted)
        EXPECT_EQ(mTestClient3, clients[0]);
        EXPECT_EQ(mTestClient1, clients[1]);
    }

    void testReclaimResourceSecure() {
        bool result;
        std::vector<MediaResourceParcel> resources;
        resources.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));
        resources.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 150));

        // ### secure codec can't coexist and secure codec can coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsMultipleSecureCodecs = false;
            mService->mSupportsSecureWithNonSecureCodec = true;

            // priority too low
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));
            CHECK_STATUS_FALSE(mService->reclaimResource(kMidPriorityPid, resources, &result));

            // reclaim all secure codecs
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(true /* c1 */, false /* c2 */, true /* c3 */);

            // call again should reclaim one largest graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // nothing left
            CHECK_STATUS_FALSE(mService->reclaimResource(kHighPriorityPid, resources, &result));
        }

        // ### secure codecs can't coexist and secure codec can't coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsMultipleSecureCodecs = false;
            mService->mSupportsSecureWithNonSecureCodec = false;

            // priority too low
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));
            CHECK_STATUS_FALSE(mService->reclaimResource(kMidPriorityPid, resources, &result));

            // reclaim all secure and non-secure codecs
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(true /* c1 */, true /* c2 */, true /* c3 */);

            // nothing left
            CHECK_STATUS_FALSE(mService->reclaimResource(kHighPriorityPid, resources, &result));
        }


        // ### secure codecs can coexist but secure codec can't coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsMultipleSecureCodecs = true;
            mService->mSupportsSecureWithNonSecureCodec = false;

            // priority too low
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));
            CHECK_STATUS_FALSE(mService->reclaimResource(kMidPriorityPid, resources, &result));

            // reclaim all non-secure codecs
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // call again should reclaim one largest graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(true /* c1 */, false /* c2 */, false /* c3 */);

            // call again should reclaim another largest graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, false /* c2 */, true /* c3 */);

            // nothing left
            CHECK_STATUS_FALSE(mService->reclaimResource(kHighPriorityPid, resources, &result));
        }

        // ### secure codecs can coexist and secure codec can coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsMultipleSecureCodecs = true;
            mService->mSupportsSecureWithNonSecureCodec = true;

            // priority too low
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));

            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            // one largest graphic memory from lowest process got reclaimed
            verifyClients(true /* c1 */, false /* c2 */, false /* c3 */);

            // call again should reclaim another graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // call again should reclaim another graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, false /* c2 */, true /* c3 */);

            // nothing left
            CHECK_STATUS_FALSE(mService->reclaimResource(kHighPriorityPid, resources, &result));
        }

        // ### secure codecs can coexist and secure codec can coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsMultipleSecureCodecs = true;
            mService->mSupportsSecureWithNonSecureCodec = true;

            std::vector<MediaResourceParcel> resources;
            resources.push_back(MediaResource(MediaResource::Type::kSecureCodec, 1));

            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            // secure codec from lowest process got reclaimed
            verifyClients(true /* c1 */, false /* c2 */, false /* c3 */);

            // call again should reclaim another secure codec from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, false /* c2 */, true /* c3 */);

            // no more secure codec, non-secure codec will be reclaimed.
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);
        }
    }

    void testReclaimResourceNonSecure() {
        bool result;
        std::vector<MediaResourceParcel> resources;
        resources.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));
        resources.push_back(MediaResource(MediaResource::Type::kGraphicMemory, 150));

        // ### secure codec can't coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsSecureWithNonSecureCodec = false;

            // priority too low
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));
            CHECK_STATUS_FALSE(mService->reclaimResource(kMidPriorityPid, resources, &result));

            // reclaim all secure codecs
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(true /* c1 */, false /* c2 */, true /* c3 */);

            // call again should reclaim one graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // nothing left
            CHECK_STATUS_FALSE(mService->reclaimResource(kHighPriorityPid, resources, &result));
        }


        // ### secure codec can coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsSecureWithNonSecureCodec = true;

            // priority too low
            CHECK_STATUS_FALSE(mService->reclaimResource(kLowPriorityPid, resources, &result));

            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            // one largest graphic memory from lowest process got reclaimed
            verifyClients(true /* c1 */, false /* c2 */, false /* c3 */);

            // call again should reclaim another graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // call again should reclaim another graphic memory from lowest process
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(false /* c1 */, false /* c2 */, true /* c3 */);

            // nothing left
            CHECK_STATUS_FALSE(mService->reclaimResource(kHighPriorityPid, resources, &result));
        }

        // ### secure codec can coexist with non-secure codec ###
        {
            addResource();
            mService->mSupportsSecureWithNonSecureCodec = true;

            std::vector<MediaResourceParcel> resources;
            resources.push_back(MediaResource(MediaResource::Type::kNonSecureCodec, 1));

            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            // one non secure codec from lowest process got reclaimed
            verifyClients(false /* c1 */, true /* c2 */, false /* c3 */);

            // no more non-secure codec, secure codec from lowest priority process will be reclaimed
            CHECK_STATUS_TRUE(mService->reclaimResource(kHighPriorityPid, resources, &result));
            verifyClients(true /* c1 */, false /* c2 */, false /* c3 */);

            // clean up client 3 which still left
            mService->removeClient(kTestPid2, getId(mTestClient3));
        }
    }

    void testGetLowestPriorityBiggestClient() {
        MediaResource::Type type = MediaResource::Type::kGraphicMemory;
        std::shared_ptr<IResourceManagerClient> client;
        EXPECT_FALSE(mService->getLowestPriorityBiggestClient_l(kHighPriorityPid, type, &client));

        addResource();

        EXPECT_FALSE(mService->getLowestPriorityBiggestClient_l(kLowPriorityPid, type, &client));
        EXPECT_TRUE(mService->getLowestPriorityBiggestClient_l(kHighPriorityPid, type, &client));

        // kTestPid1 is the lowest priority process with MediaResource::Type::kGraphicMemory.
        // mTestClient1 has the largest MediaResource::Type::kGraphicMemory within kTestPid1.
        EXPECT_EQ(mTestClient1, client);
    }

    void testGetLowestPriorityPid() {
        int pid;
        int priority;
        TestProcessInfo processInfo;

        MediaResource::Type type = MediaResource::Type::kGraphicMemory;
        EXPECT_FALSE(mService->getLowestPriorityPid_l(type, &pid, &priority));

        addResource();

        EXPECT_TRUE(mService->getLowestPriorityPid_l(type, &pid, &priority));
        EXPECT_EQ(kTestPid1, pid);
        int priority1;
        processInfo.getPriority(kTestPid1, &priority1);
        EXPECT_EQ(priority1, priority);

        type = MediaResource::Type::kNonSecureCodec;
        EXPECT_TRUE(mService->getLowestPriorityPid_l(type, &pid, &priority));
        EXPECT_EQ(kTestPid2, pid);
        int priority2;
        processInfo.getPriority(kTestPid2, &priority2);
        EXPECT_EQ(priority2, priority);
    }

    void testGetBiggestClient() {
        MediaResource::Type type = MediaResource::Type::kGraphicMemory;
        std::shared_ptr<IResourceManagerClient> client;
        EXPECT_FALSE(mService->getBiggestClient_l(kTestPid2, type, &client));

        addResource();

        EXPECT_TRUE(mService->getBiggestClient_l(kTestPid2, type, &client));
        EXPECT_EQ(mTestClient2, client);
    }

    void testIsCallingPriorityHigher() {
        EXPECT_FALSE(mService->isCallingPriorityHigher_l(101, 100));
        EXPECT_FALSE(mService->isCallingPriorityHigher_l(100, 100));
        EXPECT_TRUE(mService->isCallingPriorityHigher_l(99, 100));
    }

    void testBatteryStats() {
        // reset should always be called when ResourceManagerService is created (restarted)
        EXPECT_EQ(1u, mSystemCB->eventCount());
        EXPECT_EQ(EventType::VIDEO_RESET, mSystemCB->lastEventType());

        // new client request should cause VIDEO_ON
        std::vector<MediaResourceParcel> resources1;
        resources1.push_back(MediaResource(MediaResource::Type::kBattery, MediaResource::SubType::kVideoCodec, 1));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);
        EXPECT_EQ(2u, mSystemCB->eventCount());
        EXPECT_EQ(EventEntry({EventType::VIDEO_ON, kTestUid1}), mSystemCB->lastEvent());

        // each client should only cause 1 VIDEO_ON
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);
        EXPECT_EQ(2u, mSystemCB->eventCount());

        // new client request should cause VIDEO_ON
        std::vector<MediaResourceParcel> resources2;
        resources2.push_back(MediaResource(MediaResource::Type::kBattery, MediaResource::SubType::kVideoCodec, 2));
        mService->addResource(kTestPid2, kTestUid2, getId(mTestClient2), mTestClient2, resources2);
        EXPECT_EQ(3u, mSystemCB->eventCount());
        EXPECT_EQ(EventEntry({EventType::VIDEO_ON, kTestUid2}), mSystemCB->lastEvent());

        // partially remove mTestClient1's request, shouldn't be any VIDEO_OFF
        mService->removeResource(kTestPid1, getId(mTestClient1), resources1);
        EXPECT_EQ(3u, mSystemCB->eventCount());

        // remove mTestClient1's request, should be VIDEO_OFF for kTestUid1
        // (use resource2 to test removing more instances than previously requested)
        mService->removeResource(kTestPid1, getId(mTestClient1), resources2);
        EXPECT_EQ(4u, mSystemCB->eventCount());
        EXPECT_EQ(EventEntry({EventType::VIDEO_OFF, kTestUid1}), mSystemCB->lastEvent());

        // remove mTestClient2, should be VIDEO_OFF for kTestUid2
        mService->removeClient(kTestPid2, getId(mTestClient2));
        EXPECT_EQ(5u, mSystemCB->eventCount());
        EXPECT_EQ(EventEntry({EventType::VIDEO_OFF, kTestUid2}), mSystemCB->lastEvent());
    }

    void testCpusetBoost() {
        // reset should always be called when ResourceManagerService is created (restarted)
        EXPECT_EQ(1u, mSystemCB->eventCount());
        EXPECT_EQ(EventType::VIDEO_RESET, mSystemCB->lastEventType());

        // new client request should cause CPUSET_ENABLE
        std::vector<MediaResourceParcel> resources1;
        resources1.push_back(MediaResource(MediaResource::Type::kCpuBoost, 1));
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);
        EXPECT_EQ(2u, mSystemCB->eventCount());
        EXPECT_EQ(EventType::CPUSET_ENABLE, mSystemCB->lastEventType());

        // each client should only cause 1 CPUSET_ENABLE
        mService->addResource(kTestPid1, kTestUid1, getId(mTestClient1), mTestClient1, resources1);
        EXPECT_EQ(2u, mSystemCB->eventCount());

        // new client request should cause CPUSET_ENABLE
        std::vector<MediaResourceParcel> resources2;
        resources2.push_back(MediaResource(MediaResource::Type::kCpuBoost, 2));
        mService->addResource(kTestPid2, kTestUid2, getId(mTestClient2), mTestClient2, resources2);
        EXPECT_EQ(3u, mSystemCB->eventCount());
        EXPECT_EQ(EventType::CPUSET_ENABLE, mSystemCB->lastEventType());

        // remove mTestClient2 should not cause CPUSET_DISABLE, mTestClient1 still active
        mService->removeClient(kTestPid2, getId(mTestClient2));
        EXPECT_EQ(3u, mSystemCB->eventCount());

        // remove 1 cpuboost from mTestClient1, should not be CPUSET_DISABLE (still 1 left)
        mService->removeResource(kTestPid1, getId(mTestClient1), resources1);
        EXPECT_EQ(3u, mSystemCB->eventCount());

        // remove 2 cpuboost from mTestClient1, should be CPUSET_DISABLE
        // (use resource2 to test removing more than previously requested)
        mService->removeResource(kTestPid1, getId(mTestClient1), resources2);
        EXPECT_EQ(4u, mSystemCB->eventCount());
        EXPECT_EQ(EventType::CPUSET_DISABLE, mSystemCB->lastEventType());
    }

    sp<TestSystemCallback> mSystemCB;
    std::shared_ptr<ResourceManagerService> mService;
    std::shared_ptr<IResourceManagerClient> mTestClient1;
    std::shared_ptr<IResourceManagerClient> mTestClient2;
    std::shared_ptr<IResourceManagerClient> mTestClient3;
};

TEST_F(ResourceManagerServiceTest, config) {
    testConfig();
}

TEST_F(ResourceManagerServiceTest, addResource) {
    addResource();
}

TEST_F(ResourceManagerServiceTest, combineResource) {
    testCombineResource();
}

TEST_F(ResourceManagerServiceTest, combineResourceNegative) {
    testCombineResourceWithNegativeValues();
}

TEST_F(ResourceManagerServiceTest, removeResource) {
    testRemoveResource();
}

TEST_F(ResourceManagerServiceTest, removeClient) {
    testRemoveClient();
}

TEST_F(ResourceManagerServiceTest, reclaimResource) {
    testReclaimResourceSecure();
    testReclaimResourceNonSecure();
}

TEST_F(ResourceManagerServiceTest, getAllClients_l) {
    testGetAllClients();
}

TEST_F(ResourceManagerServiceTest, getLowestPriorityBiggestClient_l) {
    testGetLowestPriorityBiggestClient();
}

TEST_F(ResourceManagerServiceTest, getLowestPriorityPid_l) {
    testGetLowestPriorityPid();
}

TEST_F(ResourceManagerServiceTest, getBiggestClient_l) {
    testGetBiggestClient();
}

TEST_F(ResourceManagerServiceTest, isCallingPriorityHigher_l) {
    testIsCallingPriorityHigher();
}

TEST_F(ResourceManagerServiceTest, testBatteryStats) {
    testBatteryStats();
}

TEST_F(ResourceManagerServiceTest, testCpusetBoost) {
    testCpusetBoost();
}

TEST_F(ResourceManagerServiceTest, overridePid) {
    testOverridePid();
}

TEST_F(ResourceManagerServiceTest, markClientForPendingRemoval) {
    testMarkClientForPendingRemoval();
}

} // namespace android
