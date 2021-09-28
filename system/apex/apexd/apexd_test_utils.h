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

#include <android/apex/ApexInfo.h>
#include <android/apex/ApexSessionInfo.h>
#include <binder/IServiceManager.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "session_state.pb.h"

using apex::proto::SessionState;

namespace android {
namespace apex {
namespace testing {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;

template <typename T>
inline ::testing::AssertionResult IsOk(const android::base::Result<T>& result) {
  if (result.ok()) {
    return ::testing::AssertionSuccess() << " is Ok";
  } else {
    return ::testing::AssertionFailure() << " failed with " << result.error();
  }
}

inline ::testing::AssertionResult IsOk(const android::binder::Status& status) {
  if (status.isOk()) {
    return ::testing::AssertionSuccess() << " is Ok";
  } else {
    return ::testing::AssertionFailure()
           << " failed with " << status.exceptionMessage().c_str();
  }
}

MATCHER_P(SessionInfoEq, other, "") {
  return ExplainMatchResult(
      AllOf(
          Field("sessionId", &ApexSessionInfo::sessionId, Eq(other.sessionId)),
          Field("isUnknown", &ApexSessionInfo::isUnknown, Eq(other.isUnknown)),
          Field("isVerified", &ApexSessionInfo::isVerified,
                Eq(other.isVerified)),
          Field("isStaged", &ApexSessionInfo::isStaged, Eq(other.isStaged)),
          Field("isActivated", &ApexSessionInfo::isActivated,
                Eq(other.isActivated)),
          Field("isRevertInProgress", &ApexSessionInfo::isRevertInProgress,
                Eq(other.isRevertInProgress)),
          Field("isActivationFailed", &ApexSessionInfo::isActivationFailed,
                Eq(other.isActivationFailed)),
          Field("isSuccess", &ApexSessionInfo::isSuccess, Eq(other.isSuccess)),
          Field("isReverted", &ApexSessionInfo::isReverted,
                Eq(other.isReverted)),
          Field("isRevertFailed", &ApexSessionInfo::isRevertFailed,
                Eq(other.isRevertFailed))),
      arg, result_listener);
}

MATCHER_P(ApexInfoEq, other, "") {
  return ExplainMatchResult(
      AllOf(Field("moduleName", &ApexInfo::moduleName, Eq(other.moduleName)),
            Field("modulePath", &ApexInfo::modulePath, Eq(other.modulePath)),
            Field("preinstalledModulePath", &ApexInfo::preinstalledModulePath,
                  Eq(other.preinstalledModulePath)),
            Field("versionCode", &ApexInfo::versionCode, Eq(other.versionCode)),
            Field("isFactory", &ApexInfo::isFactory, Eq(other.isFactory)),
            Field("isActive", &ApexInfo::isActive, Eq(other.isActive))),
      arg, result_listener);
}

inline ApexSessionInfo CreateSessionInfo(int session_id) {
  ApexSessionInfo info;
  info.sessionId = session_id;
  info.isUnknown = false;
  info.isVerified = false;
  info.isStaged = false;
  info.isActivated = false;
  info.isRevertInProgress = false;
  info.isActivationFailed = false;
  info.isSuccess = false;
  info.isReverted = false;
  info.isRevertFailed = false;
  return info;
}

}  // namespace testing

// Must be in apex::android namespace, otherwise gtest won't be able to find it.
inline void PrintTo(const ApexSessionInfo& session, std::ostream* os) {
  *os << "apex_session: {\n";
  *os << "  sessionId : " << session.sessionId << "\n";
  *os << "  isUnknown : " << session.isUnknown << "\n";
  *os << "  isVerified : " << session.isVerified << "\n";
  *os << "  isStaged : " << session.isStaged << "\n";
  *os << "  isActivated : " << session.isActivated << "\n";
  *os << "  isActivationFailed : " << session.isActivationFailed << "\n";
  *os << "  isSuccess : " << session.isSuccess << "\n";
  *os << "  isReverted : " << session.isReverted << "\n";
  *os << "  isRevertFailed : " << session.isRevertFailed << "\n";
  *os << "}";
}

inline void PrintTo(const ApexInfo& apex, std::ostream* os) {
  *os << "apex_info: {\n";
  *os << "  moduleName : " << apex.moduleName << "\n";
  *os << "  modulePath : " << apex.modulePath << "\n";
  *os << "  preinstalledModulePath : " << apex.preinstalledModulePath << "\n";
  *os << "  versionCode : " << apex.versionCode << "\n";
  *os << "  isFactory : " << apex.isFactory << "\n";
  *os << "  isActive : " << apex.isActive << "\n";
  *os << "}";
}

}  // namespace apex
}  // namespace android
