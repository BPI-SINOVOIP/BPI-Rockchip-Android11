/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "aidl_test_client_primitives.h"

#include <iostream>
#include <iterator>
#include <vector>

#include <utils/String16.h>
#include <utils/String8.h>

#include "android/aidl/tests/ByteEnum.h"
#include "android/aidl/tests/INamedCallback.h"
#include "android/aidl/tests/IntEnum.h"
#include "android/aidl/tests/LongEnum.h"

#include "test_helpers.h"

// libutils:
using android::sp;
using android::String16;
using android::String8;

// libbinder:
using android::binder::Status;

// generated
using android::aidl::tests::ITestService;
using android::aidl::tests::INamedCallback;

using std::cerr;
using std::cout;
using std::endl;
using std::vector;

namespace android {
namespace aidl {
namespace tests {
namespace client {

bool ConfirmPrimitiveRepeat(const sp<ITestService>& s) {
  cout << "Confirming passing and returning primitives works." << endl;

  if (!RepeatPrimitive(s, &ITestService::RepeatBoolean, true) ||
      !RepeatPrimitive(s, &ITestService::RepeatByte, int8_t{-128}) ||
      !RepeatPrimitive(s, &ITestService::RepeatChar, char16_t{'A'}) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, int32_t{1 << 30}) ||
      !RepeatPrimitive(s, &ITestService::RepeatLong, int64_t{1LL << 60}) ||
      !RepeatPrimitive(s, &ITestService::RepeatFloat, float{1.0f / 3.0f}) ||
      !RepeatPrimitive(s, &ITestService::RepeatDouble, double{1.0 / 3.0}) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT2) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT3) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT4) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT5) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT6) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT7) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT8) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT9) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT10) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT11) ||
      !RepeatPrimitive(s, &ITestService::RepeatInt, ITestService::TEST_CONSTANT12) ||
      !RepeatPrimitive(s, &ITestService::RepeatByteEnum, ByteEnum::FOO) ||
      !RepeatPrimitive(s, &ITestService::RepeatIntEnum, IntEnum::BAR) ||
      !RepeatPrimitive(s, &ITestService::RepeatLongEnum, LongEnum::FOO)) {
    return false;
  }

  vector<String16> inputs = {
      String16("Deliver us from evil."),
      String16(),
      String16("\0\0", 2),
      // This is actually two unicode code points:
      //   U+10437: The 'small letter yee' character in the deseret alphabet
      //   U+20AC: A euro sign
      String16("\xD8\x01\xDC\x37\x20\xAC"),
      ITestService::STRING_TEST_CONSTANT(),
      ITestService::STRING_TEST_CONSTANT2(),
  };
  for (const auto& input : inputs) {
    String16 reply;
    Status status = s->RepeatString(input, &reply);
    if (!status.isOk() || input != reply) {
      cerr << "Failed while requesting service to repeat String16=\""
           << String8(input).string()
           << "\". Got status=" << status.toString8() << endl;
      return false;
    }
  }
  return true;
}

bool ConfirmReverseArrays(const sp<ITestService>& s) {
  cout << "Confirming passing and returning arrays works." << endl;

  if (!ReverseArray(s, &ITestService::ReverseBoolean, {true, false, false}) ||
      !ReverseArray(s, &ITestService::ReverseByte, {uint8_t{255}, uint8_t{0}, uint8_t{127}}) ||
      !ReverseArray(s, &ITestService::ReverseChar, {char16_t{'A'}, char16_t{'B'}, char16_t{'C'}}) ||
      !ReverseArray(s, &ITestService::ReverseInt, {1, 2, 3}) ||
      !ReverseArray(s, &ITestService::ReverseLong, {-1LL, 0LL, int64_t{1LL << 60}}) ||
      !ReverseArray(s, &ITestService::ReverseFloat, {-0.3f, -0.7f, 8.0f}) ||
      !ReverseArray(s, &ITestService::ReverseDouble, {1.0 / 3.0, 1.0 / 7.0, 42.0}) ||
      !ReverseArray(s, &ITestService::ReverseString,
                    {String16{"f"}, String16{"a"}, String16{"b"}}) ||
      !ReverseArray(s, &ITestService::ReverseByteEnum,
                    {ByteEnum::FOO, ByteEnum::BAR, ByteEnum::BAR}) ||
      !ReverseArray(s, &ITestService::ReverseByteEnum,
                    {std::begin(::android::enum_range<ByteEnum>()),
                     std::end(::android::enum_range<ByteEnum>())}) ||
      !ReverseArray(s, &ITestService::ReverseIntEnum, {IntEnum::FOO, IntEnum::BAR, IntEnum::BAR}) ||
      !ReverseArray(s, &ITestService::ReverseLongEnum,
                    {LongEnum::FOO, LongEnum::BAR, LongEnum::BAR})) {
    return false;
  }

  return true;
}

bool ConfirmReverseLists(const sp<ITestService>& s) {
  cout << "Confirming passing and returning List<T> works." << endl;

  if (!ReverseArray(s, &ITestService::ReverseStringList,
                    {String16{"f"}, String16{"a"}, String16{"b"}})) {
    return false;
  }

  return true;
}

bool ConfirmReverseBinderLists(const sp<ITestService>& s) {
  Status status;
  cout << "Confirming passing and returning List<T> works with binders." << endl;

  vector<String16> names = {
    String16{"Larry"},
    String16{"Curly"},
    String16{"Moe"}
  };

  vector<sp<IBinder>> input;

  for (int i = 0; i < 3; i++) {
    sp<INamedCallback> got;

    status = s->GetOtherTestService(names[i], &got);
    if (!status.isOk()) {
      cerr << "Could not retrieve service for test." << endl;
      return false;
    }

    input.push_back(INamedCallback::asBinder(got));
  }

  vector<sp<IBinder>> output;
  vector<sp<IBinder>> reversed;

  status = s->ReverseNamedCallbackList(input, &output, &reversed);
  if (!status.isOk()) {
    cerr << "Failed to reverse named callback list." << endl;
  }

  if (output.size() != 3) {
    cerr << "ReverseNamedCallbackList gave repetition with wrong length." << endl;
    return false;
  }

  if (reversed.size() != 3) {
    cerr << "ReverseNamedCallbackList gave reversal with wrong length." << endl;
    return false;
  }

  for (int i = 0; i < 3; i++) {
    String16 ret;
    sp<INamedCallback> named_callback =
        android::interface_cast<INamedCallback>(output[i]);
    status = named_callback->GetName(&ret);

    if (!status.isOk()) {
      cerr << "Could not query INamedCallback from output" << endl;
      return false;
    }

    if (ret != names[i]) {
      cerr << "Output had wrong INamedCallback" << endl;
      return false;
    }
  }

  for (int i = 0; i < 3; i++) {
    String16 ret;
    sp<INamedCallback> named_callback =
        android::interface_cast<INamedCallback>(reversed[i]);
    status = named_callback->GetName(&ret);

    if (!status.isOk()) {
      cerr << "Could not query INamedCallback from reversed output" << endl;
      return false;
    }

    if (ret != names[2 - i]) {
      cerr << "Reversed output had wrong INamedCallback" << endl;
      return false;
    }
  }

  return true;
}

bool ConfirmIntfConstantExpressions(const sp<ITestService>& s) {
  (void)s;
  bool ret = true;

  if (ITestService::A1 != 1) {
    cerr << "ITestService::A1 should be 1 but is " << ITestService::A1 << endl;
    ret = false;
  }
  if (ITestService::A2 != 1) {
    cerr << "ITestService::A2 should be 1 but is " << ITestService::A2 << endl;
    ret = false;
  }
  if (ITestService::A3 != 1) {
    cerr << "ITestService::A3 should be 1 but is " << ITestService::A3 << endl;
    ret = false;
  }
  if (ITestService::A4 != 1) {
    cerr << "ITestService::A4 should be 1 but is " << ITestService::A4 << endl;
    ret = false;
  }
  if (ITestService::A5 != 1) {
    cerr << "ITestService::A5 should be 1 but is " << ITestService::A5 << endl;
    ret = false;
  }
  if (ITestService::A6 != 1) {
    cerr << "ITestService::A6 should be 1 but is " << ITestService::A6 << endl;
    ret = false;
  }
  if (ITestService::A7 != 1) {
    cerr << "ITestService::A7 should be 1 but is " << ITestService::A7 << endl;
    ret = false;
  }
  if (ITestService::A8 != 1) {
    cerr << "ITestService::A8 should be 1 but is " << ITestService::A8 << endl;
    ret = false;
  }
  if (ITestService::A9 != 1) {
    cerr << "ITestService::A9 should be 1 but is " << ITestService::A9 << endl;
    ret = false;
  }
  if (ITestService::A10 != 1) {
    cerr << "ITestService::A10 should be 1 but is " << ITestService::A10 << endl;
    ret = false;
  }
  if (ITestService::A11 != 1) {
    cerr << "ITestService::A11 should be 1 but is " << ITestService::A11 << endl;
    ret = false;
  }
  if (ITestService::A12 != 1) {
    cerr << "ITestService::A12 should be 1 but is " << ITestService::A12 << endl;
    ret = false;
  }
  if (ITestService::A13 != 1) {
    cerr << "ITestService::A13 should be 1 but is " << ITestService::A13 << endl;
    ret = false;
  }
  if (ITestService::A14 != 1) {
    cerr << "ITestService::A14 should be 1 but is " << ITestService::A14 << endl;
    ret = false;
  }
  if (ITestService::A15 != 1) {
    cerr << "ITestService::A15 should be 1 but is " << ITestService::A15 << endl;
    ret = false;
  }
  if (ITestService::A16 != 1) {
    cerr << "ITestService::A16 should be 1 but is " << ITestService::A16 << endl;
    ret = false;
  }
  if (ITestService::A17 != 1) {
    cerr << "ITestService::A17 should be 1 but is " << ITestService::A17 << endl;
    ret = false;
  }
  if (ITestService::A18 != 1) {
    cerr << "ITestService::A18 should be 1 but is " << ITestService::A18 << endl;
    ret = false;
  }
  if (ITestService::A19 != 1) {
    cerr << "ITestService::A19 should be 1 but is " << ITestService::A19 << endl;
    ret = false;
  }
  if (ITestService::A20 != 1) {
    cerr << "ITestService::A20 should be 1 but is " << ITestService::A20 << endl;
    ret = false;
  }
  if (ITestService::A21 != 1) {
    cerr << "ITestService::A21 should be 1 but is " << ITestService::A21 << endl;
    ret = false;
  }
  if (ITestService::A22 != 1) {
    cerr << "ITestService::A22 should be 1 but is " << ITestService::A22 << endl;
    ret = false;
  }
  if (ITestService::A23 != 1) {
    cerr << "ITestService::A23 should be 1 but is " << ITestService::A23 << endl;
    ret = false;
  }
  if (ITestService::A24 != 1) {
    cerr << "ITestService::A24 should be 1 but is " << ITestService::A24 << endl;
    ret = false;
  }
  if (ITestService::A25 != 1) {
    cerr << "ITestService::A25 should be 1 but is " << ITestService::A25 << endl;
    ret = false;
  }
  if (ITestService::A26 != 1) {
    cerr << "ITestService::A26 should be 1 but is " << ITestService::A26 << endl;
    ret = false;
  }
  if (ITestService::A27 != 1) {
    cerr << "ITestService::A27 should be 1 but is " << ITestService::A27 << endl;
    ret = false;
  }
  if (ITestService::A28 != 1) {
    cerr << "ITestService::A28 should be 1 but is " << ITestService::A28 << endl;
    ret = false;
  }
  if (ITestService::A29 != 1) {
    cerr << "ITestService::A29 should be 1 but is " << ITestService::A29 << endl;
    ret = false;
  }
  if (ITestService::A30 != 1) {
    cerr << "ITestService::A30 should be 1 but is " << ITestService::A30 << endl;
    ret = false;
  }
  if (ITestService::A31 != 1) {
    cerr << "ITestService::A31 should be 1 but is " << ITestService::A31 << endl;
    ret = false;
  }
  if (ITestService::A32 != 1) {
    cerr << "ITestService::A32 should be 1 but is " << ITestService::A32 << endl;
    ret = false;
  }
  if (ITestService::A33 != 1) {
    cerr << "ITestService::A33 should be 1 but is " << ITestService::A33 << endl;
    ret = false;
  }
  if (ITestService::A34 != 1) {
    cerr << "ITestService::A34 should be 1 but is " << ITestService::A34 << endl;
    ret = false;
  }
  if (ITestService::A35 != 1) {
    cerr << "ITestService::A35 should be 1 but is " << ITestService::A35 << endl;
    ret = false;
  }
  if (ITestService::A36 != 1) {
    cerr << "ITestService::A36 should be 1 but is " << ITestService::A36 << endl;
    ret = false;
  }
  if (ITestService::A37 != 1) {
    cerr << "ITestService::A37 should be 1 but is " << ITestService::A37 << endl;
    ret = false;
  }
  if (ITestService::A38 != 1) {
    cerr << "ITestService::A38 should be 1 but is " << ITestService::A38 << endl;
    ret = false;
  }
  if (ITestService::A39 != 1) {
    cerr << "ITestService::A39 should be 1 but is " << ITestService::A39 << endl;
    ret = false;
  }
  if (ITestService::A40 != 1) {
    cerr << "ITestService::A40 should be 1 but is " << ITestService::A40 << endl;
    ret = false;
  }
  if (ITestService::A41 != 1) {
    cerr << "ITestService::A41 should be 1 but is " << ITestService::A41 << endl;
    ret = false;
  }
  if (ITestService::A42 != 1) {
    cerr << "ITestService::A42 should be 1 but is " << ITestService::A42 << endl;
    ret = false;
  }
  if (ITestService::A43 != 1) {
    cerr << "ITestService::A43 should be 1 but is " << ITestService::A43 << endl;
    ret = false;
  }
  if (ITestService::A44 != 1) {
    cerr << "ITestService::A44 should be 1 but is " << ITestService::A44 << endl;
    ret = false;
  }
  if (ITestService::A45 != 1) {
    cerr << "ITestService::A45 should be 1 but is " << ITestService::A45 << endl;
    ret = false;
  }
  if (ITestService::A46 != 1) {
    cerr << "ITestService::A46 should be 1 but is " << ITestService::A46 << endl;
    ret = false;
  }
  if (ITestService::A47 != 1) {
    cerr << "ITestService::A47 should be 1 but is " << ITestService::A47 << endl;
    ret = false;
  }
  if (ITestService::A48 != 1) {
    cerr << "ITestService::A48 should be 1 but is " << ITestService::A48 << endl;
    ret = false;
  }
  if (ITestService::A49 != 1) {
    cerr << "ITestService::A49 should be 1 but is " << ITestService::A49 << endl;
    ret = false;
  }
  if (ITestService::A50 != 1) {
    cerr << "ITestService::A50 should be 1 but is " << ITestService::A50 << endl;
    ret = false;
  }
  if (ITestService::A51 != 1) {
    cerr << "ITestService::A51 should be 1 but is " << ITestService::A51 << endl;
    ret = false;
  }
  if (ITestService::A52 != 1) {
    cerr << "ITestService::A52 should be 1 but is " << ITestService::A52 << endl;
    ret = false;
  }
  if (ITestService::A53 != 1) {
    cerr << "ITestService::A53 should be 1 but is " << ITestService::A53 << endl;
    ret = false;
  }
  if (ITestService::A54 != 1) {
    cerr << "ITestService::A54 should be 1 but is " << ITestService::A54 << endl;
    ret = false;
  }
  if (ITestService::A55 != 1) {
    cerr << "ITestService::A55 should be 1 but is " << ITestService::A55 << endl;
    ret = false;
  }
  if (ITestService::A56 != 1) {
    cerr << "ITestService::A56 should be 1 but is " << ITestService::A56 << endl;
    ret = false;
  }
  if (ITestService::A57 != 1) {
    cerr << "ITestService::A57 should be 1 but is " << ITestService::A57 << endl;
    ret = false;
  }

  return ret;
}

}  // namespace client
}  // namespace tests
}  // namespace aidl
}  // namespace android
