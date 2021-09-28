/**
 * Copyright (c) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "base_metrics_listener.h"

enum EventFlag : uint32_t {
    onDnsEvent = 1 << 0,
    onPrivateDnsValidationEvent = 1 << 1,
    onConnectEvent = 1 << 2,
    onWakeupEvent = 1 << 3,
    onTcpSocketStatsEvent = 1 << 4,
    onNat64PrefixEvent = 1 << 5,
};

namespace android {
namespace net {
namespace metrics {

// Base class for metrics event unit test. Used for notifications about DNS event changes. Should
// be extended by unit tests wanting notifications.
class BaseTestMetricsEvent : public BaseMetricsListener {
  public:
    // Returns TRUE if the verification was successful. Otherwise, returns FALSE.
    virtual bool isVerified() = 0;

    std::condition_variable& getCv() { return mCv; }
    std::mutex& getCvMutex() { return mCvMutex; }

  private:
    // The verified event(s) as a bitwise-OR combination of enum EventFlag flags.
    uint32_t mVerified{};

    // This lock prevents racing condition between signaling thread(s) and waiting thread(s).
    std::mutex mCvMutex;

    // Condition variable signaled when notify() is called.
    std::condition_variable mCv;

  protected:
    // Notify who is waiting for test results. See also mCvMutex and mCv.
    void notify();

    // Get current verified event(s).
    uint32_t getVerified() const { return mVerified; }

    // Set the specific event as verified if its verification was successful.
    void setVerified(EventFlag event);
};

// Derived class for testing onDnsEvent().
class TestOnDnsEvent : public BaseTestMetricsEvent {
  public:
    // Both latencyMs and uid are not verified. No special reason.
    // TODO: Considering to verify uid.
    struct TestResult {
        int netId;
        int eventType;
        int returnCode;
        int ipAddressesCount;
        std::string hostname;
        std::string ipAddress;  // Check first address only.
    };

    TestOnDnsEvent() = delete;
    TestOnDnsEvent(const std::vector<TestResult>& results) : mResults(results){};

    // BaseTestMetricsEvent::isVerified() override.
    bool isVerified() override { return (getVerified() & EventFlag::onDnsEvent) != 0; }

    // Override for testing verification.
    ::ndk::ScopedAStatus onDnsEvent(int32_t netId, int32_t eventType, int32_t returnCode,
                                    int32_t /*latencyMs*/, const std::string& hostname,
                                    const std::vector<std::string>& ipAddresses,
                                    int32_t ipAddressesCount, int32_t /*uid*/) override;

  private:
    const std::vector<TestResult>& mResults;  // Expected results for test verification.
};

}  // namespace metrics
}  // namespace net
}  // namespace android