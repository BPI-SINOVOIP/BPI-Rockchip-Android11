/*
 * Copyright (C) 2018 The Android Open Source Project
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
#define LOG_TAG "Cts-NdkBinderTest"

#include "utilities.h"

#include <android/binder_auto_utils.h>
#include <android/binder_status.h>
#include <gtest/gtest.h>

#include <set>

// clang-format off
const static std::set<binder_status_t> kErrorStatuses = {
  STATUS_UNKNOWN_ERROR,
  STATUS_NO_MEMORY,
  STATUS_INVALID_OPERATION,
  STATUS_BAD_VALUE,
  STATUS_BAD_TYPE,
  STATUS_NAME_NOT_FOUND,
  STATUS_PERMISSION_DENIED,
  STATUS_NO_INIT,
  STATUS_ALREADY_EXISTS,
  STATUS_DEAD_OBJECT,
  STATUS_FAILED_TRANSACTION,
  STATUS_BAD_INDEX,
  STATUS_NOT_ENOUGH_DATA,
  STATUS_WOULD_BLOCK,
  STATUS_TIMED_OUT,
  STATUS_UNKNOWN_TRANSACTION,
  STATUS_FDS_NOT_ALLOWED,
  STATUS_UNEXPECTED_NULL
};
// Not in the API or above list
const static std::set<binder_status_t> kUnknownStatuses = { -77, 1, 404, EX_TRANSACTION_FAILED };

const static std::set<binder_exception_t> kErrorExceptions = {
  EX_SECURITY,
  EX_BAD_PARCELABLE,
  EX_ILLEGAL_ARGUMENT,
  EX_NULL_POINTER,
  EX_ILLEGAL_STATE,
  EX_NETWORK_MAIN_THREAD,
  EX_UNSUPPORTED_OPERATION,
  EX_SERVICE_SPECIFIC,
  EX_PARCELABLE,
  EX_TRANSACTION_FAILED
};
// Not in the API or above list
const static std::set<binder_exception_t> kUnknownExceptions = { -77, 1, 404, STATUS_UNKNOWN_ERROR };
// clang-format on

// Checks the various attributes expected for an okay status.
static void checkIsOkay(const AStatus* status) {
  EXPECT_TRUE(AStatus_isOk(status));
  EXPECT_EQ(std::string(), AStatus_getMessage(status));
  EXPECT_EQ(EX_NONE, AStatus_getExceptionCode(status));
  EXPECT_EQ(0, AStatus_getServiceSpecificError(status));
  EXPECT_EQ(STATUS_OK, AStatus_getStatus(status));
}
static void checkIsErrorException(const AStatus* status,
                                  binder_exception_t exception,
                                  const std::string& message) {
  EXPECT_FALSE(AStatus_isOk(status));
  EXPECT_EQ(message, AStatus_getMessage(status));
  EXPECT_EQ(exception, AStatus_getExceptionCode(status));
  // not a service-specific error, so other errorcodes return the default
  EXPECT_EQ(0, AStatus_getServiceSpecificError(status));
  EXPECT_EQ(exception == EX_TRANSACTION_FAILED ? STATUS_FAILED_TRANSACTION
                                               : STATUS_OK,
            AStatus_getStatus(status));
}
static void checkIsServiceSpecific(const AStatus* status, int32_t error,
                                   const std::string& message) {
  EXPECT_FALSE(AStatus_isOk(status));
  EXPECT_EQ(message, AStatus_getMessage(status));
  EXPECT_EQ(EX_SERVICE_SPECIFIC, AStatus_getExceptionCode(status));
  EXPECT_EQ(error, AStatus_getServiceSpecificError(status));
  // not a service-specific error, so other errorcodes return the default
  EXPECT_EQ(STATUS_OK, AStatus_getStatus(status));
}
static void checkIsErrorStatus(const AStatus* status, binder_status_t statusT) {
  EXPECT_FALSE(AStatus_isOk(status));
  EXPECT_EQ(std::string(), AStatus_getMessage(status));
  EXPECT_EQ(EX_TRANSACTION_FAILED, AStatus_getExceptionCode(status));
  EXPECT_EQ(statusT, AStatus_getStatus(status));
  // not a service-specific error, so other errorcodes return the default
  EXPECT_EQ(0, AStatus_getServiceSpecificError(status));
}

TEST(NdkBinderTest_AStatus, OkIsOk) {
  AStatus* status = AStatus_newOk();
  checkIsOkay(status);
  AStatus_delete(status);
}

TEST(NdkBinderTest_AStatus, NoExceptionIsOkay) {
  AStatus* status = AStatus_fromExceptionCode(EX_NONE);
  checkIsOkay(status);
  AStatus_delete(status);
}

TEST(NdkBinderTest_AStatus, StatusOkIsOkay) {
  AStatus* status = AStatus_fromStatus(STATUS_OK);
  checkIsOkay(status);
  AStatus_delete(status);
}

TEST(NdkBinderTest_AStatus, ExceptionIsNotOkay) {
  for (binder_exception_t exception : kErrorExceptions) {
    AStatus* status = AStatus_fromExceptionCode(exception);
    checkIsErrorException(status, exception, "" /*message*/);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, ExceptionWithMessageIsNotOkay) {
  const std::string kMessage = "Something arbitrary.";
  for (binder_exception_t exception : kErrorExceptions) {
    AStatus* status =
        AStatus_fromExceptionCodeWithMessage(exception, kMessage.c_str());
    checkIsErrorException(status, exception, kMessage);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, ServiceSpecificIsNotOkay) {
  for (int32_t error : {-404, -1, 0, 1, 23, 918}) {
    AStatus* status = AStatus_fromServiceSpecificError(error);
    checkIsServiceSpecific(status, error, "" /*message*/);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, ServiceSpecificWithMessageIsNotOkay) {
  const std::string kMessage = "Something also arbitrary.";
  for (int32_t error : {-404, -1, 0, 1, 23, 918}) {
    AStatus* status =
        AStatus_fromServiceSpecificErrorWithMessage(error, kMessage.c_str());
    checkIsServiceSpecific(status, error, kMessage);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, StatusIsNotOkay) {
  for (binder_status_t statusT : kErrorStatuses) {
    AStatus* status = AStatus_fromStatus(statusT);
    checkIsErrorStatus(status, statusT);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, ExceptionsPruned) {
  for (binder_exception_t exception : kUnknownExceptions) {
    EXPECT_EQ(kErrorExceptions.find(exception), kErrorExceptions.end())
        << exception;

    AStatus* status = AStatus_fromExceptionCode(exception);
    checkIsErrorException(status, EX_TRANSACTION_FAILED, "" /*message*/);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, ExceptionsPrunedWithMessage) {
  const std::string kMessage = "Something else arbitrary.";
  for (binder_exception_t exception : kUnknownExceptions) {
    EXPECT_EQ(kErrorExceptions.find(exception), kErrorExceptions.end())
        << exception;

    AStatus* status =
        AStatus_fromExceptionCodeWithMessage(exception, kMessage.c_str());
    checkIsErrorException(status, EX_TRANSACTION_FAILED, kMessage);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, StatusesPruned) {
  for (binder_status_t statusT : kUnknownStatuses) {
    EXPECT_EQ(kErrorStatuses.find(statusT), kErrorStatuses.end()) << statusT;

    AStatus* status = AStatus_fromStatus(statusT);
    checkIsErrorStatus(status, STATUS_UNKNOWN_ERROR);
    AStatus_delete(status);
  }
}

TEST(NdkBinderTest_AStatus, StatusDescription) {
  using ndk::ScopedAStatus;

  EXPECT_TRUE(
      ContainsSubstring(ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED).getDescription(),
                        "TRANSACTION_FAILED"));
  EXPECT_TRUE(ContainsSubstring(
      ScopedAStatus::fromExceptionCodeWithMessage(EX_TRANSACTION_FAILED, "asdf").getDescription(),
      "asdf"));
  EXPECT_TRUE(
      ContainsSubstring(ScopedAStatus::fromServiceSpecificError(42).getDescription(), "42"));
  EXPECT_TRUE(ContainsSubstring(
      ScopedAStatus::fromServiceSpecificErrorWithMessage(42, "asdf").getDescription(), "asdf"));
  EXPECT_TRUE(
      ContainsSubstring(ScopedAStatus::fromStatus(STATUS_BAD_TYPE).getDescription(), "BAD_TYPE"));
}
