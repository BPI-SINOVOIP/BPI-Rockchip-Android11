/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * TetherControllerTest.cpp - unit tests for TetherController.cpp
 */

#include <string>
#include <vector>

#include <fcntl.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <gmock/gmock.h>
#include <netdutils/StatusOr.h>

#include "IptablesBaseTest.h"
#include "OffloadUtils.h"
#include "TetherController.h"

using android::base::Join;
using android::base::StringPrintf;
using android::bpf::BpfMap;
using android::netdutils::StatusOr;
using ::testing::Contains;
using TetherStats = android::net::TetherController::TetherStats;
using TetherStatsList = android::net::TetherController::TetherStatsList;
using TetherOffloadStats = android::net::TetherController::TetherOffloadStats;
using TetherOffloadStatsList = android::net::TetherController::TetherOffloadStatsList;

namespace android {
namespace net {

constexpr int TEST_MAP_SIZE = 10;

// Comparison for TetherOffloadStats. Need to override operator== because class TetherOffloadStats
// doesn't have one.
// TODO: once C++20 is used, use default operator== in TetherOffloadStats and remove the overriding
// here.
bool operator==(const TetherOffloadStats& lhs, const TetherOffloadStats& rhs) {
    return lhs.ifIndex == rhs.ifIndex && lhs.rxBytes == rhs.rxBytes && lhs.txBytes == rhs.txBytes &&
           lhs.rxPackets == rhs.rxPackets && lhs.txPackets == rhs.txPackets;
}

class TetherControllerTest : public IptablesBaseTest {
public:
    TetherControllerTest() {
        TetherController::iptablesRestoreFunction = fakeExecIptablesRestoreWithOutput;
    }

protected:
    TetherController mTetherCtrl;
    BpfMap<uint32_t, TetherStatsValue> mFakeTetherStatsMap{BPF_MAP_TYPE_HASH, TEST_MAP_SIZE};
    BpfMap<uint32_t, uint64_t> mFakeTetherLimitMap{BPF_MAP_TYPE_HASH, TEST_MAP_SIZE};

    void SetUp() {
        SKIP_IF_BPF_NOT_SUPPORTED;

        ASSERT_TRUE(mFakeTetherStatsMap.isValid());
        ASSERT_TRUE(mFakeTetherLimitMap.isValid());

        mTetherCtrl.mBpfStatsMap = mFakeTetherStatsMap;
        ASSERT_TRUE(mTetherCtrl.mBpfStatsMap.isValid());
        mTetherCtrl.mBpfLimitMap = mFakeTetherLimitMap;
        ASSERT_TRUE(mTetherCtrl.mBpfLimitMap.isValid());
    }

    std::string toString(const TetherOffloadStatsList& statsList) {
        std::string result;
        for (const auto& stats : statsList) {
            result += StringPrintf("%d, %" PRId64 ", %" PRId64 ", %" PRId64 ", %" PRId64 "\n",
                                   stats.ifIndex, stats.rxBytes, stats.rxPackets, stats.txBytes,
                                   stats.txPackets);
        }
        return result;
    }

    void updateMaps(uint32_t ifaceIndex, uint64_t rxBytes, uint64_t rxPackets, uint64_t txBytes,
                    uint64_t txPackets) {
        // {rx, tx}Errors in |tetherStats| are set zero because getTetherStats doesn't use them.
        const TetherStatsValue tetherStats = {rxPackets, rxBytes, 0 /*unused*/,
                                              txPackets, txBytes, 0 /*unused*/};
        ASSERT_RESULT_OK(mFakeTetherStatsMap.writeValue(ifaceIndex, tetherStats, BPF_ANY));
    };

    int setDefaults() {
        return mTetherCtrl.setDefaults();
    }

    const ExpectedIptablesCommands FLUSH_COMMANDS = {
            {V4,
             "*filter\n"
             ":tetherctrl_FORWARD -\n"
             "-A tetherctrl_FORWARD -j DROP\n"
             "COMMIT\n"
             "*nat\n"
             ":tetherctrl_nat_POSTROUTING -\n"
             "COMMIT\n"},
            {V6,
             "*filter\n"
             ":tetherctrl_FORWARD -\n"
             "COMMIT\n"
             "*raw\n"
             ":tetherctrl_raw_PREROUTING -\n"
             "COMMIT\n"},
    };

    const ExpectedIptablesCommands SETUP_COMMANDS = {
            {V4,
             "*filter\n"
             ":tetherctrl_FORWARD -\n"
             "-A tetherctrl_FORWARD -j DROP\n"
             "COMMIT\n"
             "*nat\n"
             ":tetherctrl_nat_POSTROUTING -\n"
             "COMMIT\n"},
            {V6,
             "*filter\n"
             ":tetherctrl_FORWARD -\n"
             "COMMIT\n"
             "*raw\n"
             ":tetherctrl_raw_PREROUTING -\n"
             "COMMIT\n"},
            {V4,
             "*mangle\n"
             "-A tetherctrl_mangle_FORWARD -p tcp --tcp-flags SYN SYN "
             "-j TCPMSS --clamp-mss-to-pmtu\n"
             "COMMIT\n"},
            {V4V6,
             "*filter\n"
             ":tetherctrl_counters -\n"
             "COMMIT\n"},
    };

    const ExpectedIptablesCommands ALERT_ADD_COMMAND = {
            {V4V6,
             "*filter\n"
             "-I tetherctrl_FORWARD -j bw_global_alert\n"
             "COMMIT\n"},
    };

    ExpectedIptablesCommands firstIPv4UpstreamCommands(const char *extIf) {
        std::string v4Cmd = StringPrintf(
            "*nat\n"
            "-A tetherctrl_nat_POSTROUTING -o %s -j MASQUERADE\n"
            "COMMIT\n", extIf);
        return {
            { V4, v4Cmd },
        };
    }

    ExpectedIptablesCommands firstIPv6UpstreamCommands() {
        std::string v6Cmd =
                "*filter\n"
                "-A tetherctrl_FORWARD -g tetherctrl_counters\n"
                "COMMIT\n";
        return {
            { V6, v6Cmd },
        };
    }

    template<typename T>
    void appendAll(std::vector<T>& cmds, const std::vector<T>& appendCmds) {
        cmds.insert(cmds.end(), appendCmds.begin(), appendCmds.end());
    }

    ExpectedIptablesCommands startNatCommands(const char *intIf, const char *extIf,
            bool withCounterChainRules) {
        std::string rpfilterCmd = StringPrintf(
            "*raw\n"
            "-A tetherctrl_raw_PREROUTING -i %s -m rpfilter --invert ! -s fe80::/64 -j DROP\n"
            "COMMIT\n", intIf);

        std::vector<std::string> v4Cmds = {
                "*raw",
                StringPrintf(
                        "-A tetherctrl_raw_PREROUTING -p tcp --dport 21 -i %s -j CT --helper ftp",
                        intIf),
                StringPrintf("-A tetherctrl_raw_PREROUTING -p tcp --dport 1723 -i %s -j CT "
                             "--helper pptp",
                             intIf),
                "COMMIT",
                "*filter",
                StringPrintf("-A tetherctrl_FORWARD -i %s -o %s -m state --state"
                             " ESTABLISHED,RELATED -g tetherctrl_counters",
                             extIf, intIf),
                StringPrintf("-A tetherctrl_FORWARD -i %s -o %s -m state --state INVALID -j DROP",
                             intIf, extIf),
                StringPrintf("-A tetherctrl_FORWARD -i %s -o %s -g tetherctrl_counters", intIf,
                             extIf),
        };

        std::vector<std::string> v6Cmds = {
            "*filter",
        };

        if (withCounterChainRules) {
            const std::vector<std::string> counterRules = {
                StringPrintf("-A tetherctrl_counters -i %s -o %s -j RETURN", intIf, extIf),
                StringPrintf("-A tetherctrl_counters -i %s -o %s -j RETURN", extIf, intIf),
            };

            appendAll(v4Cmds, counterRules);
            appendAll(v6Cmds, counterRules);
        }

        appendAll(v4Cmds, {
            "-D tetherctrl_FORWARD -j DROP",
            "-A tetherctrl_FORWARD -j DROP",
            "COMMIT\n",
        });

        v6Cmds.push_back("COMMIT\n");

        return {
            { V6, rpfilterCmd },
            { V4, Join(v4Cmds, '\n') },
            { V6, Join(v6Cmds, '\n') },
        };
    }

    constexpr static const bool WITH_COUNTERS = true;
    constexpr static const bool NO_COUNTERS = false;
    constexpr static const bool WITH_IPV6 = true;
    constexpr static const bool NO_IPV6 = false;
    ExpectedIptablesCommands allNewNatCommands(const char* intIf, const char* extIf,
                                               bool withCounterChainRules, bool withIPv6Upstream,
                                               bool firstEnableNat) {
        ExpectedIptablesCommands commands;
        ExpectedIptablesCommands setupFirstIPv4Commands = firstIPv4UpstreamCommands(extIf);
        ExpectedIptablesCommands startFirstNatCommands = startNatCommands(intIf, extIf,
            withCounterChainRules);

        appendAll(commands, setupFirstIPv4Commands);
        if (withIPv6Upstream) {
            ExpectedIptablesCommands setupFirstIPv6Commands = firstIPv6UpstreamCommands();
            appendAll(commands, setupFirstIPv6Commands);
        }
        if (firstEnableNat) {
            appendAll(commands, ALERT_ADD_COMMAND);
        }
        appendAll(commands, startFirstNatCommands);

        return commands;
    }

    ExpectedIptablesCommands stopNatCommands(const char *intIf, const char *extIf) {
        std::string rpfilterCmd = StringPrintf(
            "*raw\n"
            "-D tetherctrl_raw_PREROUTING -i %s -m rpfilter --invert ! -s fe80::/64 -j DROP\n"
            "COMMIT\n", intIf);

        std::vector<std::string> v4Cmds = {
                "*raw",
                StringPrintf(
                        "-D tetherctrl_raw_PREROUTING -p tcp --dport 21 -i %s -j CT --helper ftp",
                        intIf),
                StringPrintf("-D tetherctrl_raw_PREROUTING -p tcp --dport 1723 -i %s -j CT "
                             "--helper pptp",
                             intIf),
                "COMMIT",
                "*filter",
                StringPrintf("-D tetherctrl_FORWARD -i %s -o %s -m state --state"
                             " ESTABLISHED,RELATED -g tetherctrl_counters",
                             extIf, intIf),
                StringPrintf("-D tetherctrl_FORWARD -i %s -o %s -m state --state INVALID -j DROP",
                             intIf, extIf),
                StringPrintf("-D tetherctrl_FORWARD -i %s -o %s -g tetherctrl_counters", intIf,
                             extIf),
                "COMMIT\n",
        };

        return {
            { V6, rpfilterCmd },
            { V4, Join(v4Cmds, '\n') },
        };

    }
};

TEST_F(TetherControllerTest, TestSetupIptablesHooks) {
    mTetherCtrl.setupIptablesHooks();
    expectIptablesRestoreCommands(SETUP_COMMANDS);
}

TEST_F(TetherControllerTest, TestSetDefaults) {
    setDefaults();
    expectIptablesRestoreCommands(FLUSH_COMMANDS);
}

TEST_F(TetherControllerTest, TestAddAndRemoveNat) {
    // Start first NAT on first upstream interface. Expect the upstream and NAT rules to be created.
    ExpectedIptablesCommands firstNat =
            allNewNatCommands("wlan0", "rmnet0", WITH_COUNTERS, WITH_IPV6, true);
    mTetherCtrl.enableNat("wlan0", "rmnet0");
    expectIptablesRestoreCommands(firstNat);

    // Start second NAT on same upstream. Expect only the counter rules to be created.
    ExpectedIptablesCommands startOtherNatOnSameUpstream = startNatCommands(
            "usb0", "rmnet0", WITH_COUNTERS);
    mTetherCtrl.enableNat("usb0", "rmnet0");
    expectIptablesRestoreCommands(startOtherNatOnSameUpstream);

    // Remove the first NAT.
    ExpectedIptablesCommands stopFirstNat = stopNatCommands("wlan0", "rmnet0");
    mTetherCtrl.disableNat("wlan0", "rmnet0");
    expectIptablesRestoreCommands(stopFirstNat);

    // Remove the last NAT. Expect rules to be cleared.
    ExpectedIptablesCommands stopLastNat = stopNatCommands("usb0", "rmnet0");

    appendAll(stopLastNat, FLUSH_COMMANDS);
    mTetherCtrl.disableNat("usb0", "rmnet0");
    expectIptablesRestoreCommands(stopLastNat);

    // Re-add a NAT removed previously: tetherctrl_counters chain rules are not re-added
    firstNat = allNewNatCommands("wlan0", "rmnet0", NO_COUNTERS, WITH_IPV6, true);
    mTetherCtrl.enableNat("wlan0", "rmnet0");
    expectIptablesRestoreCommands(firstNat);

    // Remove it again. Expect rules to be cleared.
    stopLastNat = stopNatCommands("wlan0", "rmnet0");
    appendAll(stopLastNat, FLUSH_COMMANDS);
    mTetherCtrl.disableNat("wlan0", "rmnet0");
    expectIptablesRestoreCommands(stopLastNat);
}

TEST_F(TetherControllerTest, TestMultipleUpstreams) {
    // Start first NAT on first upstream interface. Expect the upstream and NAT rules to be created.
    ExpectedIptablesCommands firstNat =
            allNewNatCommands("wlan0", "rmnet0", WITH_COUNTERS, WITH_IPV6, true);
    mTetherCtrl.enableNat("wlan0", "rmnet0");
    expectIptablesRestoreCommands(firstNat);

    // Start second NAT, on new upstream. Expect the upstream and NAT rules to be created for IPv4,
    // but no counter rules for IPv6.
    ExpectedIptablesCommands secondNat =
            allNewNatCommands("wlan0", "v4-rmnet0", WITH_COUNTERS, NO_IPV6, false);
    mTetherCtrl.enableNat("wlan0", "v4-rmnet0");
    expectIptablesRestoreCommands(secondNat);

    // Pretend that the caller has forgotten that it set up the second NAT, and asks us to do so
    // again. Expect that we take no action.
    const ExpectedIptablesCommands NONE = {};
    mTetherCtrl.enableNat("wlan0", "v4-rmnet0");
    expectIptablesRestoreCommands(NONE);

    // Remove the second NAT.
    ExpectedIptablesCommands stopSecondNat = stopNatCommands("wlan0", "v4-rmnet0");
    mTetherCtrl.disableNat("wlan0", "v4-rmnet0");
    expectIptablesRestoreCommands(stopSecondNat);

    // Remove the first NAT. Expect rules to be cleared.
    ExpectedIptablesCommands stopFirstNat = stopNatCommands("wlan0", "rmnet0");
    appendAll(stopFirstNat, FLUSH_COMMANDS);
    mTetherCtrl.disableNat("wlan0", "rmnet0");
    expectIptablesRestoreCommands(stopFirstNat);
}

std::string kTetherCounterHeaders = Join(std::vector<std::string> {
    "Chain tetherctrl_counters (4 references)",
    "    pkts      bytes target     prot opt in     out     source               destination",
}, '\n');

std::string kIPv4TetherCounters = Join(std::vector<std::string> {
    "Chain tetherctrl_counters (4 references)",
    "    pkts      bytes target     prot opt in     out     source               destination",
    "      26     2373 RETURN     all  --  wlan0  rmnet0  0.0.0.0/0            0.0.0.0/0",
    "      27     2002 RETURN     all  --  rmnet0 wlan0   0.0.0.0/0            0.0.0.0/0",
    "    1040   107471 RETURN     all  --  bt-pan rmnet0  0.0.0.0/0            0.0.0.0/0",
    "    1450  1708806 RETURN     all  --  rmnet0 bt-pan  0.0.0.0/0            0.0.0.0/0",
}, '\n');

std::string kIPv6TetherCounters = Join(std::vector<std::string> {
    "Chain tetherctrl_counters (2 references)",
    "    pkts      bytes target     prot opt in     out     source               destination",
    "   10000 10000000 RETURN     all      wlan0  rmnet0  ::/0                 ::/0",
    "   20000 20000000 RETURN     all      rmnet0 wlan0   ::/0                 ::/0",
}, '\n');

void expectTetherStatsEqual(const TetherController::TetherStats& expected,
                            const TetherController::TetherStats& actual) {
    EXPECT_EQ(expected.intIface, actual.intIface);
    EXPECT_EQ(expected.extIface, actual.extIface);
    EXPECT_EQ(expected.rxBytes, actual.rxBytes);
    EXPECT_EQ(expected.txBytes, actual.txBytes);
    EXPECT_EQ(expected.rxPackets, actual.rxPackets);
    EXPECT_EQ(expected.txPackets, actual.txPackets);
}

TEST_F(TetherControllerTest, TestGetTetherStats) {
    // Finding no headers is an error.
    ASSERT_FALSE(isOk(mTetherCtrl.getTetherStats()));
    clearIptablesRestoreOutput();

    // Finding only v4 or only v6 headers is an error.
    addIptablesRestoreOutput(kTetherCounterHeaders, "");
    ASSERT_FALSE(isOk(mTetherCtrl.getTetherStats()));
    clearIptablesRestoreOutput();

    addIptablesRestoreOutput("", kTetherCounterHeaders);
    ASSERT_FALSE(isOk(mTetherCtrl.getTetherStats()));
    clearIptablesRestoreOutput();

    // Finding headers but no stats is not an error.
    addIptablesRestoreOutput(kTetherCounterHeaders, kTetherCounterHeaders);
    StatusOr<TetherStatsList> result = mTetherCtrl.getTetherStats();
    ASSERT_TRUE(isOk(result));
    TetherStatsList actual = result.value();
    ASSERT_EQ(0U, actual.size());
    clearIptablesRestoreOutput();


    addIptablesRestoreOutput(kIPv6TetherCounters);
    ASSERT_FALSE(isOk(mTetherCtrl.getTetherStats()));
    clearIptablesRestoreOutput();

    // IPv4 and IPv6 counters are properly added together.
    addIptablesRestoreOutput(kIPv4TetherCounters, kIPv6TetherCounters);
    TetherStats expected0("wlan0", "rmnet0", 20002002, 20027, 10002373, 10026);
    TetherStats expected1("bt-pan", "rmnet0", 1708806, 1450, 107471, 1040);
    result = mTetherCtrl.getTetherStats();
    ASSERT_TRUE(isOk(result));
    actual = result.value();
    ASSERT_EQ(2U, actual.size());
    expectTetherStatsEqual(expected0, result.value()[0]);
    expectTetherStatsEqual(expected1, result.value()[1]);
    clearIptablesRestoreOutput();

    // No stats: error.
    addIptablesRestoreOutput("", kIPv6TetherCounters);
    ASSERT_FALSE(isOk(mTetherCtrl.getTetherStats()));
    clearIptablesRestoreOutput();

    addIptablesRestoreOutput(kIPv4TetherCounters, "");
    ASSERT_FALSE(isOk(mTetherCtrl.getTetherStats()));
    clearIptablesRestoreOutput();

    // Include only one pair of interfaces and things are fine.
    std::vector<std::string> counterLines = android::base::Split(kIPv4TetherCounters, "\n");
    std::vector<std::string> brokenCounterLines = counterLines;
    counterLines.resize(4);
    std::string counters = Join(counterLines, "\n") + "\n";
    addIptablesRestoreOutput(counters, counters);
    TetherStats expected1_0("wlan0", "rmnet0", 4004, 54, 4746, 52);
    result = mTetherCtrl.getTetherStats();
    ASSERT_TRUE(isOk(result));
    actual = result.value();
    ASSERT_EQ(1U, actual.size());
    expectTetherStatsEqual(expected1_0, actual[0]);
    clearIptablesRestoreOutput();

    // But if interfaces aren't paired, it's always an error.
    counterLines.resize(3);
    counters = Join(counterLines, "\n") + "\n";
    addIptablesRestoreOutput(counters, counters);
    result = mTetherCtrl.getTetherStats();
    ASSERT_FALSE(isOk(result));
    clearIptablesRestoreOutput();

    // Token unit test of the fact that we return the stats in the error message which the caller
    // ignores.
    // Skip header since we only saved the last line we parsed.
    std::string expectedError = counterLines[2];
    std::string err = result.status().msg();
    ASSERT_LE(expectedError.size(), err.size());
    EXPECT_TRUE(std::equal(expectedError.rbegin(), expectedError.rend(), err.rbegin()));
}

TEST_F(TetherControllerTest, TestTetherOffloadGetStats) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    updateMaps(101, 100, 10, 200, 20);
    updateMaps(102, 300, 30, 400, 40);
    const TetherOffloadStats expected0{101, 100, 10, 200, 20};
    const TetherOffloadStats expected1{102, 300, 30, 400, 40};

    const StatusOr<TetherOffloadStatsList> result = mTetherCtrl.getTetherOffloadStats();
    ASSERT_OK(result);
    const TetherOffloadStatsList& actual = result.value();
    ASSERT_EQ(2U, actual.size());
    EXPECT_THAT(actual, Contains(expected0)) << toString(actual);
    EXPECT_THAT(actual, Contains(expected1)) << toString(actual);
    clearIptablesRestoreOutput();
}

TEST_F(TetherControllerTest, TestTetherOffloadSetQuota) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    const uint32_t ifindex = 100;
    const uint64_t minQuota = 0;
    const uint64_t maxQuota = std::numeric_limits<int64_t>::max();
    const uint64_t infinityQuota = std::numeric_limits<uint64_t>::max();

    // Create a stats entry with zeroes in the first time set limit.
    ASSERT_EQ(0, mTetherCtrl.setTetherOffloadInterfaceQuota(ifindex, minQuota));
    const StatusOr<TetherOffloadStatsList> result = mTetherCtrl.getTetherOffloadStats();
    ASSERT_OK(result);
    const TetherOffloadStatsList& actual = result.value();
    ASSERT_EQ(1U, actual.size());
    EXPECT_THAT(actual, Contains(TetherOffloadStats{ifindex, 0, 0, 0, 0})) << toString(actual);

    // Verify the quota with the boundary {min, max, infinity}.
    const uint64_t rxBytes = 1000;
    const uint64_t txBytes = 2000;
    updateMaps(ifindex, rxBytes, 0 /*unused*/, txBytes, 0 /*unused*/);

    for (const uint64_t quota : {minQuota, maxQuota, infinityQuota}) {
        ASSERT_EQ(0, mTetherCtrl.setTetherOffloadInterfaceQuota(ifindex, quota));
        base::Result<uint64_t> result = mFakeTetherLimitMap.readValue(ifindex);
        ASSERT_RESULT_OK(result);

        const uint64_t expectedQuota =
                (quota == infinityQuota) ? infinityQuota : quota + rxBytes + txBytes;
        EXPECT_EQ(expectedQuota, result.value());
    }

    // The valid range of interface index is 1..max_int64.
    const uint32_t invalidIfindex = 0;
    int ret = mTetherCtrl.setTetherOffloadInterfaceQuota(invalidIfindex /*bad*/, infinityQuota);
    ASSERT_EQ(-ENODEV, ret);

    // The valid range of quota is 0..max_int64 or -1 (unlimited).
    const uint64_t invalidQuota = std::numeric_limits<int64_t>::min();
    ret = mTetherCtrl.setTetherOffloadInterfaceQuota(ifindex, invalidQuota /*bad*/);
    ASSERT_EQ(-ERANGE, ret);
}

}  // namespace net
}  // namespace android
