//
// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <gtest/gtest.h>

#include <cutils/qtaguid.h>

#include <netdutils/Misc.h>
#include <netdutils/Syscalls.h>
#include "NetlinkListener.h"
#include "TrafficController.h"
#include "bpf/BpfMap.h"
#include "bpf/BpfUtils.h"
#include "netdutils/Netlink.h"

// A test uid that is large enough so normal apps are not likely to take,
constexpr uid_t TEST_UID = UID_MAX - 2;
// A test tag arbitrarily selected.
constexpr uint32_t TEST_TAG = 0xFF0F0F0F;

constexpr uint32_t SOCK_CLOSE_WAIT_US = 20 * 1000;
constexpr uint32_t ENOBUFS_POLL_WAIT_US = 10 * 1000;

using android::base::Result;
using android::base::ResultError;

// This test set up a SkDestroyListener that is runing parallel with the production
// SkDestroyListener. The test will create thousands of sockets and tag them on the
// production cookieUidTagMap and close them in a short time. When the number of
// sockets get closed exceeds the buffer size, it will start to return ENOBUFF
// error. The error will be ignored by the production SkDestroyListener and the
// test will clean up the tags in tearDown if there is any remains.

// TODO: Instead of test the ENOBUFF error, we can test the production
// SkDestroyListener to see if it failed to delete a tagged socket when ENOBUFF
// triggerred.
class NetlinkListenerTest : public testing::Test {
  protected:
    NetlinkListenerTest() {}
    BpfMap<uint64_t, UidTagValue> mCookieTagMap;

    void SetUp() {
        SKIP_IF_BPF_NOT_SUPPORTED;

        mCookieTagMap.reset(android::bpf::mapRetrieveRW(COOKIE_TAG_MAP_PATH));
        ASSERT_TRUE(mCookieTagMap.isValid());
    }

    void TearDown() {
        SKIP_IF_BPF_NOT_SUPPORTED;

        const auto deleteTestCookieEntries = [](const uint64_t& key, const UidTagValue& value,
                                                BpfMap<uint64_t, UidTagValue>& map) {
            if ((value.uid == TEST_UID) && (value.tag == TEST_TAG)) {
                Result<void> res = map.deleteValue(key);
                if (res.ok() || (res.error().code() == ENOENT)) {
                    return Result<void>();
                }
                ALOGE("Failed to delete data(cookie = %" PRIu64 "): %s\n", key,
                      strerror(res.error().code()));
            }
            // Move forward to next cookie in the map.
            return Result<void>();
        };
        EXPECT_RESULT_OK(mCookieTagMap.iterateWithValue(deleteTestCookieEntries));
    }

    Result<void> checkNoGarbageTagsExist() {
        const auto checkGarbageTags = [](const uint64_t&, const UidTagValue& value,
                                         const BpfMap<uint64_t, UidTagValue>&) -> Result<void> {
            if ((TEST_UID == value.uid) && (TEST_TAG == value.tag)) {
                return ResultError("Closed socket is not untagged", EUCLEAN);
            }
            return Result<void>();
        };
        return mCookieTagMap.iterateWithValue(checkGarbageTags);
    }

    void checkMassiveSocketDestroy(int totalNumber, bool expectError) {
        std::unique_ptr<android::net::NetlinkListenerInterface> skDestroyListener;
        auto result = android::net::TrafficController::makeSkDestroyListener();
        if (!isOk(result)) {
            ALOGE("Unable to create SkDestroyListener: %s", toString(result).c_str());
        } else {
            skDestroyListener = std::move(result.value());
        }
        int rxErrorCount = 0;
        // Rx handler extracts nfgenmsg looks up and invokes registered dispatch function.
        const auto rxErrorHandler = [&rxErrorCount](const int, const int) { rxErrorCount++; };
        skDestroyListener->registerSkErrorHandler(rxErrorHandler);
        int fds[totalNumber];
        for (int i = 0; i < totalNumber; i++) {
            fds[i] = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
            // The likely reason for a failure is running out of available file descriptors.
            EXPECT_LE(0, fds[i]) << i << " of " << totalNumber;
            if (fds[i] < 0) {
                // EXPECT_LE already failed above, so test case is a failure, but we don't
                // want potentially tens of thousands of extra failures creating and then
                // closing all these fds cluttering up the logs.
                totalNumber = i;
                break;
            };
            qtaguid_tagSocket(fds[i], TEST_TAG, TEST_UID);
        }

        // TODO: Use a separate thread that has it's own fd table so we can
        // close sockets even faster simply by terminating that thread.
        for (int i = 0; i < totalNumber; i++) {
            EXPECT_EQ(0, close(fds[i]));
        }
        // wait a bit for netlink listener to handle all the messages.
        usleep(SOCK_CLOSE_WAIT_US);
        if (expectError) {
            // If ENOBUFS triggered, check it only called into the handler once, ie.
            // that the netlink handler is not spinning.
            int currentErrorCount = rxErrorCount;
            EXPECT_LT(0, rxErrorCount);
            usleep(ENOBUFS_POLL_WAIT_US);
            EXPECT_EQ(currentErrorCount, rxErrorCount);
        } else {
            EXPECT_RESULT_OK(checkNoGarbageTagsExist());
            EXPECT_EQ(0, rxErrorCount);
        }
    }
};

TEST_F(NetlinkListenerTest, TestAllSocketUntagged) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    checkMassiveSocketDestroy(10, false);
    checkMassiveSocketDestroy(100, false);
}

TEST_F(NetlinkListenerTest, TestSkDestroyError) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    checkMassiveSocketDestroy(32500, true);
}
