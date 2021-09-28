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

#ifndef _GTS_NANOAPPS_SHARED_MACROS_H_
#define _GTS_NANOAPPS_SHARED_MACROS_H_

#include "send_message.h"

/**
 * Asserts the two provided values are equal. If the assertion fails, then a
 * fatal failure occurs.
 */
#define ASSERT_EQ(val1, val2, failureMessage) \
  if ((val1) != (val2)) sendFatalFailureToHost(failureMessage)

/**
 * Asserts the two provided values are not equal. If the assertion fails, then
 * a fatal failure occurs.
 */
#define ASSERT_NE(val1, val2, failureMessage) \
  if ((val1) == (val2)) sendFatalFailureToHost(failureMessage)

/**
 * Asserts the given value is greater than or equal to value of lower. If the
 * value fails this assertion, then a fatal failure occurs.
 */
#define ASSERT_GE(value, lower, failureMessage) \
  if ((value) < (lower)) sendFatalFailureToHost(failureMessage)

/**
 * Asserts the given value is less than or equal to value of upper. If the value
 * fails this assertion, then a fatal failure occurs.
 */
#define ASSERT_LE(value, upper, failureMessage) \
  if ((value) > (upper)) sendFatalFailureToHost(failureMessage)

/**
 * Asserts the given value is less than the value of upper. If the value fails
 * this assertion, then a fatal failure occurs.
 */
#define ASSERT_LT(value, upper, failureMessage) \
  if ((value) >= (upper)) sendFatalFailureToHost(failureMessage)

/**
 * Asserts the given value is within the range defined by lower and upper
 * (inclusive). If the value is outside the range, then a fatal failure occurs.
 */
#define ASSERT_IN_RANGE(value, lower, upper, failureMessage) \
  ASSERT_GE((value), (lower), failureMessage);               \
  ASSERT_LE((value), (upper), failureMessage)

#endif  // _GTS_NANOAPPS_SHARED_MACROS_H_