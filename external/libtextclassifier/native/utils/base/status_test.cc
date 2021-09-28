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

#include "utils/base/status.h"

#include "utils/base/logging.h"
#include "utils/base/status_macros.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(StatusTest, PrintsAbortedStatus) {
  logging::LoggingStringStream stream;
  stream << Status::UNKNOWN;
  EXPECT_EQ(Status::UNKNOWN.error_code(), 2);
  EXPECT_EQ(Status::UNKNOWN.CanonicalCode(), StatusCode::UNKNOWN);
  EXPECT_EQ(Status::UNKNOWN.error_message(), "");
  EXPECT_EQ(stream.message, "2");
}

TEST(StatusTest, PrintsOKStatus) {
  logging::LoggingStringStream stream;
  stream << Status::OK;
  EXPECT_EQ(Status::OK.error_code(), 0);
  EXPECT_EQ(Status::OK.CanonicalCode(), StatusCode::OK);
  EXPECT_EQ(Status::OK.error_message(), "");
  EXPECT_EQ(stream.message, "0");
}

TEST(StatusTest, UnknownStatusHasRightAttributes) {
  EXPECT_EQ(Status::UNKNOWN.error_code(), 2);
  EXPECT_EQ(Status::UNKNOWN.CanonicalCode(), StatusCode::UNKNOWN);
  EXPECT_EQ(Status::UNKNOWN.error_message(), "");
}

TEST(StatusTest, OkStatusHasRightAttributes) {
  EXPECT_EQ(Status::OK.error_code(), 0);
  EXPECT_EQ(Status::OK.CanonicalCode(), StatusCode::OK);
  EXPECT_EQ(Status::OK.error_message(), "");
}

TEST(StatusTest, CustomStatusHasRightAttributes) {
  Status status(StatusCode::INVALID_ARGUMENT, "You can't put this here!");
  EXPECT_EQ(status.error_code(), 3);
  EXPECT_EQ(status.CanonicalCode(), StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_message(), "You can't put this here!");
}

TEST(StatusTest, AssignmentPreservesMembers) {
  Status status(StatusCode::INVALID_ARGUMENT, "You can't put this here!");

  Status status2 = status;

  EXPECT_EQ(status2.error_code(), 3);
  EXPECT_EQ(status2.CanonicalCode(), StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status2.error_message(), "You can't put this here!");
}

TEST(StatusTest, ReturnIfErrorOkStatus) {
  bool returned_due_to_error = true;
  auto lambda = [&returned_due_to_error](const Status& s) {
    TC3_RETURN_IF_ERROR(s);
    returned_due_to_error = false;
    return Status::OK;
  };

  // OK should allow execution to continue and the returned status should also
  // be OK.
  Status status = lambda(Status());
  EXPECT_EQ(status.error_code(), 0);
  EXPECT_EQ(status.CanonicalCode(), StatusCode::OK);
  EXPECT_EQ(status.error_message(), "");
  EXPECT_FALSE(returned_due_to_error);
}

TEST(StatusTest, ReturnIfErrorInvalidArgumentStatus) {
  bool returned_due_to_error = true;
  auto lambda = [&returned_due_to_error](const Status& s) {
    TC3_RETURN_IF_ERROR(s);
    returned_due_to_error = false;
    return Status::OK;
  };

  // INVALID_ARGUMENT should cause an early return.
  Status invalid_arg_status(StatusCode::INVALID_ARGUMENT, "You can't do that!");
  Status status = lambda(invalid_arg_status);
  EXPECT_EQ(status.error_code(), 3);
  EXPECT_EQ(status.CanonicalCode(), StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_message(), "You can't do that!");
  EXPECT_TRUE(returned_due_to_error);
}

TEST(StatusTest, ReturnIfErrorUnknownStatus) {
  bool returned_due_to_error = true;
  auto lambda = [&returned_due_to_error](const Status& s) {
    TC3_RETURN_IF_ERROR(s);
    returned_due_to_error = false;
    return Status::OK;
  };

  // UNKNOWN should cause an early return.
  Status unknown_status(StatusCode::UNKNOWN,
                        "We also know there are known unknowns.");
  libtextclassifier3::Status status = lambda(unknown_status);
  EXPECT_EQ(status.error_code(), 2);
  EXPECT_EQ(status.CanonicalCode(), StatusCode::UNKNOWN);
  EXPECT_EQ(status.error_message(), "We also know there are known unknowns.");
  EXPECT_TRUE(returned_due_to_error);
}

TEST(StatusTest, ReturnIfErrorOnlyInvokesExpressionOnce) {
  int num_invocations = 0;
  auto ok_internal_expr = [&num_invocations]() {
    ++num_invocations;
    return Status::OK;
  };
  auto ok_lambda = [&ok_internal_expr]() {
    TC3_RETURN_IF_ERROR(ok_internal_expr());
    return Status::OK;
  };

  libtextclassifier3::Status status = ok_lambda();
  EXPECT_EQ(status.CanonicalCode(), StatusCode::OK);
  EXPECT_EQ(num_invocations, 1);

  num_invocations = 0;
  auto error_internal_expr = [&num_invocations]() {
    ++num_invocations;
    return Status::UNKNOWN;
  };
  auto error_lambda = [&error_internal_expr]() {
    TC3_RETURN_IF_ERROR(error_internal_expr());
    return Status::OK;
  };

  status = error_lambda();
  EXPECT_EQ(status.CanonicalCode(), StatusCode::UNKNOWN);
  EXPECT_EQ(num_invocations, 1);
}

}  // namespace
}  // namespace libtextclassifier3
