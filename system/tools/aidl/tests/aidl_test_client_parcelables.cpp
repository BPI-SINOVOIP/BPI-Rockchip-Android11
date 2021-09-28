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

#include "aidl_test_client_parcelables.h"

#include <iostream>
#include <vector>

// libutils:
using android::sp;

// libbinder:
using android::binder::Status;

// generated
using android::aidl::tests::ConstantExpressionEnum;
using android::aidl::tests::ITestService;
using android::aidl::tests::SimpleParcelable;
using android::os::PersistableBundle;

using std::cout;
using std::endl;
using std::vector;

namespace android {
namespace aidl {
namespace tests {
namespace client {

bool ConfirmSimpleParcelables(const sp<ITestService>& s) {
  cout << "Confirming passing and returning SimpleParcelable objects works."
       << endl;

  SimpleParcelable input("Booya", 42);
  SimpleParcelable out_param, returned;
  Status status = s->RepeatSimpleParcelable(input, &out_param, &returned);
  if (!status.isOk()) {
    cout << "Binder call failed." << endl;
    return false;
  }
  if (input != out_param || input != returned) {
    cout << "Failed to repeat SimpleParcelable objects." << endl;
    return false;
  }

  cout << "Attempting to reverse an array of SimpleParcelable objects." << endl;
  const vector<SimpleParcelable> original{SimpleParcelable("first", 0),
                                          SimpleParcelable("second", 1),
                                          SimpleParcelable("third", 2)};
  vector<SimpleParcelable> repeated;
  vector<SimpleParcelable> reversed;
  status = s->ReverseSimpleParcelables(original, &repeated, &reversed);
  if (!status.isOk()) {
    cout << "Binder call failed." << endl;
    return false;
  }
  std::reverse(reversed.begin(), reversed.end());
  if (repeated != original || reversed != original) {
    cout << "Failed to reverse an array of SimpleParcelable objects." << endl;
    return false;
  }

  return true;
}

bool ConfirmPersistableBundles(const sp<ITestService>& s) {
  cout << "Confirming passing and returning PersistableBundle objects works."
       << endl;

  PersistableBundle empty_bundle, returned;
  Status status = s->RepeatPersistableBundle(empty_bundle, &returned);
  if (!status.isOk()) {
    cout << "Binder call failed for empty PersistableBundle." << endl;
    return false;
  }
  if (empty_bundle != returned) {
    cout << "Failed to repeat empty PersistableBundle." << endl;
    return false;
  }

  PersistableBundle non_empty_bundle;
  non_empty_bundle.putBoolean(String16("test_bool"), false);
  non_empty_bundle.putInt(String16("test_int"), 33);
  non_empty_bundle.putLong(String16("test_long"), 34359738368L);
  non_empty_bundle.putDouble(String16("test_double"), 1.1);
  non_empty_bundle.putString(String16("test_string"), String16("Woot!"));
  non_empty_bundle.putBooleanVector(String16("test_bool_vector"),
                                    {true, false, true});
  non_empty_bundle.putIntVector(String16("test_int_vector"), {33, 44, 55, 142});
  non_empty_bundle.putLongVector(String16("test_long_vector"),
                                 {34L, 8371L, 34359738375L});
  non_empty_bundle.putDoubleVector(String16("test_double_vector"), {2.2, 5.4});
  non_empty_bundle.putStringVector(String16("test_string_vector"),
                                   {String16("hello"), String16("world!")});
  PersistableBundle nested_bundle;
  nested_bundle.putInt(String16("test_nested_int"), 345);
  non_empty_bundle.putPersistableBundle(String16("test_persistable_bundle"),
                                        nested_bundle);

  status = s->RepeatPersistableBundle(non_empty_bundle, &returned);
  if (!status.isOk()) {
    cout << "Binder call failed. " << endl;
    return false;
  }
  if (non_empty_bundle != returned) {
    cout << "Failed to repeat PersistableBundle object." << endl;
    return false;
  }

  cout << "Attempting to reverse an array of PersistableBundle objects."
       << endl;
  PersistableBundle first;
  PersistableBundle second;
  PersistableBundle third;
  first.putInt(String16("test_int"), 1231);
  second.putLong(String16("test_long"), 222222L);
  third.putDouble(String16("test_double"), 10.8);
  const vector<PersistableBundle> original{first, second, third};

  vector<PersistableBundle> repeated;
  vector<PersistableBundle> reversed;
  status = s->ReversePersistableBundles(original, &repeated, &reversed);
  if (!status.isOk()) {
    cout << "Binder call failed." << endl;
    return false;
  }
  std::reverse(reversed.begin(), reversed.end());
  if (repeated != original || reversed != original) {
    cout << "Failed to reverse an array of PersistableBundle objects." << endl;
    return false;
  }

  return true;
}

bool ConfirmStructuredParcelablesEquality(const sp<ITestService>& s) {
  StructuredParcelable parcelable1;
  StructuredParcelable parcelable2;

  parcelable1.f = 11;
  parcelable2.f = 11;

  s->FillOutStructuredParcelable(&parcelable1);
  s->FillOutStructuredParcelable(&parcelable2);

  sp<INamedCallback> callback1;
  sp<INamedCallback> callback2;
  s->GetOtherTestService(String16("callback1"), &callback1);
  s->GetOtherTestService(String16("callback2"), &callback2);

  parcelable1.ibinder = IInterface::asBinder(callback1);
  parcelable2.ibinder = IInterface::asBinder(callback1);

  if (parcelable1 != parcelable2) {
    cout << "parcelable1 and parcelable2 should be same." << endl;
    return false;
  }
  parcelable1.f = 0;
  if (parcelable1 >= parcelable2) {
    cout << "parcelable1 and parcelable2 should be different because of shouldContainThreeFs"
         << endl;
    return false;
  }
  parcelable1.f = 11;

  parcelable1.shouldBeJerry = "Jarry";
  if (!(parcelable1 < parcelable2)) {
    cout << "parcelable1 and parcelable2 should be different because of shouldContainThreeFs"
         << endl;
    return false;
  }
  parcelable1.shouldBeJerry = "Jerry";

  parcelable2.shouldContainThreeFs = {};
  if (parcelable1 <= parcelable2) {
    cout << "parcelable1 and parcelable2 should be different because of shouldContainThreeFs"
         << endl;
    return false;
  }
  parcelable2.shouldContainThreeFs = {parcelable2.f, parcelable2.f, parcelable2.f};

  parcelable2.shouldBeIntBar = IntEnum::FOO;
  if (!(parcelable1 > parcelable2)) {
    cout << "parcelable1 and parcelable2 should be different because of shouldBeIntBar" << endl;
    return false;
  }
  parcelable2.shouldBeIntBar = IntEnum::BAR;

  parcelable2.ibinder = IInterface::asBinder(callback2);
  if (parcelable1 == parcelable2) {
    cout << "parcelable1 and parcelable2 should be different because of ibinder" << endl;
    return false;
  }
  return true;
}

bool ConfirmStructuredParcelables(const sp<ITestService>& s) {
  bool success = true;
  constexpr int kDesiredValue = 23;

  StructuredParcelable parcelable;
  parcelable.f = kDesiredValue;

  if (parcelable.stringDefaultsToFoo != String16("foo")) {
    cout << "stringDefaultsToFoo should be 'foo' but is " << parcelable.stringDefaultsToFoo << endl;
    return false;
  }
  if (parcelable.byteDefaultsToFour != 4) {
    cout << "byteDefaultsToFour should be 4 but is " << parcelable.byteDefaultsToFour << endl;
    return false;
  }
  if (parcelable.intDefaultsToFive != 5) {
    cout << "intDefaultsToFive should be 5 but is " << parcelable.intDefaultsToFive << endl;
    return false;
  }
  if (parcelable.longDefaultsToNegativeSeven != -7) {
    cout << "longDefaultsToNegativeSeven should be -7 but is "
         << parcelable.longDefaultsToNegativeSeven << endl;
    return false;
  }
  if (!parcelable.booleanDefaultsToTrue) {
    cout << "booleanDefaultsToTrue isn't true" << endl;
    return false;
  }
  if (parcelable.charDefaultsToC != 'C') {
    cout << "charDefaultsToC is " << parcelable.charDefaultsToC << endl;
    return false;
  }
  if (parcelable.floatDefaultsToPi != 3.14f) {
    cout << "floatDefaultsToPi is " << parcelable.floatDefaultsToPi << endl;
    return false;
  }
  if (parcelable.doubleWithDefault != -3.14e17) {
    cout << "doubleWithDefault is " << parcelable.doubleWithDefault << " but should be -3.14e17"
         << endl;
    return false;
  }
  if (parcelable.arrayDefaultsTo123.size() != 3) {
    cout << "arrayDefaultsTo123 is of length " << parcelable.arrayDefaultsTo123.size() << endl;
    return false;
  }
  for (int i = 0; i < 3; i++) {
    if (parcelable.arrayDefaultsTo123[i] != i + 1) {
      cout << "arrayDefaultsTo123[" << i << "] is " << parcelable.arrayDefaultsTo123[i]
           << " but should be " << i + 1 << endl;
      return false;
    }
  }
  if (!parcelable.arrayDefaultsToEmpty.empty()) {
    cout << "arrayDefaultsToEmpty is not empty " << parcelable.arrayDefaultsToEmpty.size() << endl;
    return false;
  }

  s->FillOutStructuredParcelable(&parcelable);

  if (parcelable.shouldContainThreeFs.size() != 3) {
    cout << "shouldContainThreeFs is of length " << parcelable.shouldContainThreeFs.size() << endl;
    return false;
  }

  for (int i = 0; i < 3; i++) {
    if (parcelable.shouldContainThreeFs[i] != kDesiredValue) {
      cout << "shouldContainThreeFs[" << i << "] is " << parcelable.shouldContainThreeFs[i]
           << " but should be " << kDesiredValue << endl;
      return false;
    }
  }

  if (parcelable.shouldBeJerry != "Jerry") {
    cout << "shouldBeJerry should be 'Jerry' but is " << parcelable.shouldBeJerry << endl;
    return false;
  }

  if (parcelable.int32_min != INT32_MIN) {
    cout << "int32_min should be " << INT32_MIN << "but is " << parcelable.int32_min << endl;
    return false;
  }

  if (parcelable.int32_max != INT32_MAX) {
    cout << "int32_max should be " << INT32_MAX << "but is " << parcelable.int32_max << endl;
    return false;
  }

  if (parcelable.int64_max != INT64_MAX) {
    cout << "int64_max should be " << INT64_MAX << "but is " << parcelable.int64_max << endl;
    return false;
  }

  if (parcelable.hexInt32_neg_1 != -1) {
    cout << "hexInt32_neg_1 should be -1 but is " << parcelable.hexInt32_neg_1 << endl;
    return false;
  }

  for (size_t ndx = 0; ndx < parcelable.int32_1.size(); ndx++) {
    if (parcelable.int32_1[ndx] != 1) {
      cout << "int32_1[" << ndx << "] should be 1 but is " << parcelable.int32_1[ndx] << endl;
      success = false;
    }
  }
  if (!success) {
    return false;
  }

  for (size_t ndx = 0; ndx < parcelable.int64_1.size(); ndx++) {
    if (parcelable.int64_1[ndx] != 1) {
      cout << "int64_1[" << ndx << "] should be 1 but is " << parcelable.int64_1[ndx] << endl;
      success = false;
    }
  }
  if (!success) {
    return false;
  }

  if (static_cast<int>(parcelable.hexInt32_pos_1) != 1) {
    cout << "hexInt32_pos_1 should be 1 but is " << parcelable.hexInt32_pos_1 << endl;
    return false;
  }

  if (parcelable.hexInt64_pos_1 != 1) {
    cout << "hexInt64_pos_1 should be 1 but is " << parcelable.hexInt64_pos_1 << endl;
    return false;
  }

  if (static_cast<int>(parcelable.const_exprs_1) != 1) {
    cout << "parcelable.const_exprs_1 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_1) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_2) != 1) {
    cout << "parcelable.const_exprs_2 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_2) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_3) != 1) {
    cout << "parcelable.const_exprs_3 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_3) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_4) != 1) {
    cout << "parcelable.const_exprs_4 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_4) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_5) != 1) {
    cout << "parcelable.const_exprs_5 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_5) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_6) != 1) {
    cout << "parcelable.const_exprs_6 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_6) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_7) != 1) {
    cout << "parcelable.const_exprs_7 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_7) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_8) != 1) {
    cout << "parcelable.const_exprs_8 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_8) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_9) != 1) {
    cout << "parcelable.const_exprs_9 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_9) << endl;
    return false;
  }
  if (static_cast<int>(parcelable.const_exprs_10) != 1) {
    cout << "parcelable.const_exprs_10 should be 1 but is "
         << static_cast<int>(parcelable.const_exprs_10) << endl;
    return false;
  }

  if (parcelable.addString1 != "hello world!") {
    cout << "parcelable.addString1 should be \"hello world!\" but is \"" << parcelable.addString1
         << "\"" << endl;
    return false;
  }
  if (parcelable.addString2 != "The quick brown fox jumps over the lazy dog.") {
    cout << "parcelable.addString2 should be \"The quick brown fox jumps over the lazy dog.\""
            " but is \""
         << parcelable.addString2 << "\"" << endl;
    return false;
  }

  return true;
}

}  // namespace client
}  // namespace tests
}  // namespace aidl
}  // namespace android
