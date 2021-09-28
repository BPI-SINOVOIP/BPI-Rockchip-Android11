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

#include <regex>
#include <thread>

#include <android-base/strings.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>

#include "DnsQueryLog.h"

using namespace std::chrono_literals;

namespace android::net {

namespace {

// Dump the log to STDOUT and capture it.
std::string captureDumpOutput(const DnsQueryLog& queryLog) {
    netdutils::DumpWriter dw(STDOUT_FILENO);
    CapturedStdout captured;
    queryLog.dump(dw);
    return captured.str();
}

// A simple check for the dump result by checking the netIds one by one.
void verifyDumpOutput(const std::string& dumpLog, const std::vector<int>& expectedNetIds) {
    // Capture three matches: netId, hostname, and answer (empty allowed).
    static const std::regex pattern(
            R"(netId=(\d+).* hostname=([\w\*]+) answer=\[([\w:,\.\*\s]*)\])");

    std::string str(dumpLog);
    std::smatch sm;
    for (const auto& netId : expectedNetIds) {
        SCOPED_TRACE(netId);
        EXPECT_TRUE(std::regex_search(str, sm, pattern));
        EXPECT_EQ(sm[1], std::to_string(netId));
        str = sm.suffix();
    }

    // Ensure the dumpLog is exactly as expected.
    EXPECT_FALSE(std::regex_search(str, sm, pattern));
}

}  // namespace

class DnsQueryLogTest : public ::testing::Test {
  protected:
    const std::vector<std::string> serversV4 = {"127.0.0.1", "1.2.3.4"};
    const std::vector<std::string> serversV4V6 = {"127.0.0.1", "1.2.3.4", "2001:db8::1",
                                                  "fe80:1::2%testnet"};
};

TEST_F(DnsQueryLogTest, Push) {
    std::vector<DnsQueryLog::Record> records = {
            DnsQueryLog::Record(30, 1000, 1000, "example.com", serversV4, 10),
            DnsQueryLog::Record(31, 1000, 1000, "", serversV4, 10),      // Empty hostname.
            DnsQueryLog::Record(32, 1000, 1000, "example.com", {}, 10),  // No answer.
            DnsQueryLog::Record(33, 1000, 1000, "example.com", serversV4V6, 10),
    };
    DnsQueryLog queryLog;
    for (auto& r : records) {
        queryLog.push(std::move(r));
    }

    std::string output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, {30, 31, 32, 33});
}

TEST_F(DnsQueryLogTest, PushStressTest) {
    const int threadNum = 100;
    const int pushNum = 1000;
    const size_t size = 500;
    DnsQueryLog queryLog(size);
    std::vector<std::thread> threads(threadNum);

    // Launch 'threadNum' threads to push the same queryLog 'pushNum' times.
    for (auto& thread : threads) {
        thread = std::thread([&]() {
            for (int i = 0; i < pushNum; i++) {
                DnsQueryLog::Record record(30, 1000, 1000, "www.example.com", serversV4, 10);
                queryLog.push(std::move(record));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify there are exact 'size' records in queryLog.
    std::string output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, std::vector(size, 30));
}

TEST_F(DnsQueryLogTest, ZeroSize) {
    const size_t size = 0;
    DnsQueryLog::Record r1(30, 1000, 1000, "www.example1.com", serversV4V6, 10);
    DnsQueryLog::Record r2(31, 1000, 1000, "www.example2.com", serversV4V6, 10);
    DnsQueryLog::Record r3(32, 1000, 1000, "www.example3.com", serversV4V6, 10);

    DnsQueryLog queryLog(size);
    queryLog.push(std::move(r1));
    queryLog.push(std::move(r2));
    queryLog.push(std::move(r3));

    std::string output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, {});
}

TEST_F(DnsQueryLogTest, CapacityFull) {
    const size_t size = 3;
    DnsQueryLog::Record r1(30, 1000, 1000, "www.example1.com", serversV4V6, 10);
    DnsQueryLog::Record r2(31, 1000, 1000, "www.example2.com", serversV4V6, 10);
    DnsQueryLog::Record r3(32, 1000, 1000, "www.example3.com", serversV4V6, 10);
    DnsQueryLog::Record r4(33, 1000, 1000, "www.example4.com", serversV4V6, 10);
    const std::vector<int> expectedNetIds = {31, 32, 33};

    DnsQueryLog queryLog(size);
    queryLog.push(std::move(r1));
    queryLog.push(std::move(r2));
    queryLog.push(std::move(r3));
    queryLog.push(std::move(r4));

    std::string output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, expectedNetIds);
}

TEST_F(DnsQueryLogTest, ValidityTime) {
    DnsQueryLog::Record r1(30, 1000, 1000, "www.example.com", serversV4, 10);
    DnsQueryLog queryLog(3, 100ms);
    queryLog.push(std::move(r1));

    // Dump the output and verify the correctness by checking netId.
    std::string output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, {30});

    std::this_thread::sleep_for(150ms);

    // The record is expired thus not shown in the output.
    output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, {});

    // Push another record to ensure it still works.
    DnsQueryLog::Record r2(31, 1000, 1000, "example.com", serversV4V6, 10);
    queryLog.push(std::move(r2));
    output = captureDumpOutput(queryLog);
    verifyDumpOutput(output, {31});
}

}  // namespace android::net
