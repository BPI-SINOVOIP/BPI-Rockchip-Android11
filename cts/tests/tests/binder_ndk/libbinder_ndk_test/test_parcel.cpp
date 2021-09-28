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

#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/log.h>
#include <gtest/gtest.h>

#include "utilities.h"

#include <limits>
#include <vector>

class NdkBinderTest_AParcel : public NdkBinderTest {};

template <typename T, typename Enable = void>
struct WriteFrom {
  using type = const T;
};
template <>
struct WriteFrom<AStatus*> {
  // not 'const T' = 'AStatus* const' where T = AStatus*.
  using type = const AStatus*;
};

template <typename T>
bool NdkBinderSenseOfEquality(T a, T b) {
  return a == b;
}
template <>
bool NdkBinderSenseOfEquality<const AStatus*>(const AStatus* a,
                                              const AStatus* b) {
  if (a == b) return true;

  return AStatus_isOk(a) == AStatus_isOk(b) &&
         AStatus_getExceptionCode(a) == AStatus_getExceptionCode(b) &&
         AStatus_getServiceSpecificError(a) ==
             AStatus_getServiceSpecificError(b) &&
         AStatus_getStatus(a) == AStatus_getStatus(b) &&
         std::string(AStatus_getMessage(a)) == AStatus_getMessage(b);
}
template <>
bool NdkBinderSenseOfEquality<AStatus*>(AStatus* a, AStatus* b) {
  return NdkBinderSenseOfEquality<const AStatus*>(a, b);
}

// These reads and writes an array of possible values all of the same type.
template <typename T,
          binder_status_t (*write)(AParcel*, typename WriteFrom<T>::type),
          binder_status_t (*read)(const AParcel*, T*)>
void ExpectInOut(std::vector<T> in) {
  AIBinder* binder = SampleData::newBinder(
      [](transaction_code_t, const AParcel* in, AParcel* out) {
        T readTarget = {};
        EXPECT_OK(read(in, &readTarget));
        EXPECT_OK(write(out, readTarget));
        return STATUS_OK;
      },
      ExpectLifetimeTransactions(in.size()));

  for (const auto& value : in) {
    EXPECT_OK(SampleData::transact(
        binder, kCode,
        [&](AParcel* in) {
          EXPECT_OK(write(in, value));
          return STATUS_OK;
        },
        [&](const AParcel* out) {
          T readTarget = {};
          EXPECT_OK(read(out, &readTarget));
          EXPECT_TRUE(NdkBinderSenseOfEquality<T>(value, readTarget))
              << value << " is not " << readTarget;
          return STATUS_OK;
        }));
  }

  AIBinder_decStrong(binder);
}

template <typename T,
          binder_status_t (*write)(AParcel*, typename WriteFrom<T>::type),
          binder_status_t (*read)(const AParcel*, T*)>
void ExpectInOutMinMax() {
  ExpectInOut<T, write, read>(
      {std::numeric_limits<T>::min(), std::numeric_limits<T>::max()});
}

TEST_F(NdkBinderTest_AParcel, BindersInMustComeOut) {
  AIBinder* binder = SampleData::newBinder();

  ExpectInOut<AIBinder*, AParcel_writeStrongBinder, AParcel_readStrongBinder>(
      {binder});
  // copy which is read when this binder is sent in a transaction to this
  // process
  AIBinder_decStrong(binder);
  // copy which is read when this binder is returned in a transaction within
  // this same process and is read again
  AIBinder_decStrong(binder);

  ExpectInOut<AIBinder*, AParcel_writeStrongBinder, AParcel_readStrongBinder>(
      {nullptr, binder});
  // copy which is read when this binder is sent in a transaction to this
  // process
  AIBinder_decStrong(binder);
  // copy which is read when this binder is returned in a transaction within
  // this same process and is read again
  AIBinder_decStrong(binder);

  AIBinder_decStrong(binder);
}

TEST_F(NdkBinderTest_AParcel, StatusesInMustComeOut) {
  // This does not clean up status objects.
  ExpectInOut<AStatus*, AParcel_writeStatusHeader, AParcel_readStatusHeader>({
      AStatus_newOk(),
      AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT),
      AStatus_fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                           "+++++++++[->++++++++<]>.+."),
      AStatus_fromServiceSpecificError(1776),
      AStatus_fromServiceSpecificErrorWithMessage(0xBEA, "utiful!"),
  });
}

TEST_F(NdkBinderTest_AParcel, LowLevelErrorsHaveNoStatusHeader) {
  AIBinder* binder =
      SampleData::newBinder(nullptr, ExpectLifetimeTransactions(0));

  EXPECT_EQ(
      STATUS_UNKNOWN_ERROR,
      SampleData::transact(binder, kCode, [&](AParcel* in) {
        AStatus* status = nullptr;

        status = AStatus_fromExceptionCode(EX_TRANSACTION_FAILED);
        EXPECT_EQ(STATUS_FAILED_TRANSACTION,
                  AParcel_writeStatusHeader(in, status));
        AStatus_delete(status);

        status = AStatus_fromExceptionCodeWithMessage(EX_TRANSACTION_FAILED,
                                                      "something or other");
        EXPECT_EQ(STATUS_FAILED_TRANSACTION,
                  AParcel_writeStatusHeader(in, status));
        AStatus_delete(status);

        status = AStatus_fromStatus(STATUS_UNKNOWN_ERROR);
        EXPECT_EQ(STATUS_UNKNOWN_ERROR, AParcel_writeStatusHeader(in, status));
        AStatus_delete(status);

        status = AStatus_fromStatus(STATUS_BAD_VALUE);
        EXPECT_EQ(STATUS_BAD_VALUE, AParcel_writeStatusHeader(in, status));
        AStatus_delete(status);

        return STATUS_UNKNOWN_ERROR;
      }));

  AIBinder_decStrong(binder);
}

TEST_F(NdkBinderTest_AParcel, WhatGoesInMustComeOut) {
  ExpectInOut<int32_t, AParcel_writeInt32, AParcel_readInt32>(
      {-7, -1, 0, 1, 45});
  ExpectInOut<uint32_t, AParcel_writeUint32, AParcel_readUint32>(
      {0, 1, 2, 100});
  ExpectInOut<int64_t, AParcel_writeInt64, AParcel_readInt64>(
      {-7, -1, 0, 1, 45});
  ExpectInOut<uint64_t, AParcel_writeUint64, AParcel_readUint64>(
      {0, 1, 2, 100});
  ExpectInOut<float, AParcel_writeFloat, AParcel_readFloat>(
      {-1.0f, 0.0f, 1.0f, 0.24975586f, 0.3f});
  ExpectInOut<double, AParcel_writeDouble, AParcel_readDouble>(
      {-1.0, 0.0, 1.0, 0.24975586, 0.3});

  ExpectInOut<bool, AParcel_writeBool, AParcel_readBool>({true, false});
  ExpectInOut<char16_t, AParcel_writeChar, AParcel_readChar>(
      {L'\0', L'S', L'@', L'\n'});
  ExpectInOut<int8_t, AParcel_writeByte, AParcel_readByte>({-7, -1, 0, 1, 45});
}

TEST_F(NdkBinderTest_AParcel, ExtremeValues) {
  ExpectInOutMinMax<int32_t, AParcel_writeInt32, AParcel_readInt32>();
  ExpectInOutMinMax<uint32_t, AParcel_writeUint32, AParcel_readUint32>();
  ExpectInOutMinMax<int64_t, AParcel_writeInt64, AParcel_readInt64>();
  ExpectInOutMinMax<uint64_t, AParcel_writeUint64, AParcel_readUint64>();
  ExpectInOutMinMax<float, AParcel_writeFloat, AParcel_readFloat>();
  ExpectInOutMinMax<double, AParcel_writeDouble, AParcel_readDouble>();
  ExpectInOutMinMax<bool, AParcel_writeBool, AParcel_readBool>();
  ExpectInOutMinMax<char16_t, AParcel_writeChar, AParcel_readChar>();
  ExpectInOutMinMax<int8_t, AParcel_writeByte, AParcel_readByte>();
}

TEST_F(NdkBinderTest_AParcel, NonNullTerminatedString) {
  // This is a helper to write a vector of strings which are not
  // null-terminated. It has infinite length, and every element is the same
  // value (element.substr(0, elementLen)). However, when it is written, no
  // copies of element are made to produce a null-terminated string.
  struct PartialStringCycle {
    // every element of the vector is a prefix of this string
    const std::string& element;
    // this is the number of characters of the string to write, < element.size()
    int32_t elementLen;

    binder_status_t writeToParcel(AParcel* p, size_t length) const {
      return AParcel_writeStringArray(p, static_cast<const void*>(this), length,
                                      ElementGetter);
    }

   private:
    static const char* ElementGetter(const void* vectorData, size_t /*index*/, int32_t* outLength) {
      const PartialStringCycle* vector =
          static_cast<const PartialStringCycle*>(vectorData);

      *outLength = vector->elementLen;
      return vector->element.c_str();
    }
  };

  const std::string kTestcase = "aoeuhtns";

  for (size_t i = 0; i < kTestcase.size(); i++) {
    const std::string expectedString = kTestcase.substr(0, i);
    const std::vector<std::string> expectedVector = {expectedString,
                                                     expectedString};

    const PartialStringCycle writeVector{.element = kTestcase,
                                         .elementLen = static_cast<int32_t>(i)};

    AIBinder* binder = SampleData::newBinder(
        [&](transaction_code_t, const AParcel* in, AParcel* /*out*/) {
          std::string readString;
          EXPECT_OK(::ndk::AParcel_readString(in, &readString));
          EXPECT_EQ(expectedString, readString);

          std::vector<std::string> readVector;
          EXPECT_OK(::ndk::AParcel_readVector(in, &readVector));
          EXPECT_EQ(expectedVector, readVector);

          return STATUS_OK;
        },
        ExpectLifetimeTransactions(1));

    EXPECT_OK(SampleData::transact(
        binder, kCode,
        [&](AParcel* in) {
          EXPECT_OK(AParcel_writeString(in, kTestcase.c_str(), i));
          EXPECT_OK(writeVector.writeToParcel(in, expectedVector.size()));
          return STATUS_OK;
        },
        ReadNothingFromParcel));

    AIBinder_decStrong(binder);
  }
}

TEST_F(NdkBinderTest_AParcel, CantReadFromEmptyParcel) {
  AIBinder* binder = SampleData::newBinder(TransactionsReturn(STATUS_OK),
                                           ExpectLifetimeTransactions(1));

  EXPECT_OK(SampleData::transact(
      binder, kCode, WriteNothingToParcel, [&](const AParcel* out) {
        bool readTarget = false;
        EXPECT_EQ(STATUS_NOT_ENOUGH_DATA, AParcel_readBool(out, &readTarget));
        EXPECT_FALSE(readTarget);
        return STATUS_OK;
      }));
  AIBinder_decStrong(binder);
}

TEST_F(NdkBinderTest_AParcel, ReturnParcelPosition) {
  AIBinder* binder = SampleData::newBinder(
      [&](transaction_code_t, const AParcel* /*in*/, AParcel* out) {
        size_t position = AParcel_getDataPosition(out);
        EXPECT_EQ(position, AParcel_getDataPosition(out));
        EXPECT_OK(AParcel_setDataPosition(out, position));
        EXPECT_EQ(position, AParcel_getDataPosition(out));

        return STATUS_OK;
      },
      ExpectLifetimeTransactions(1));

  EXPECT_OK(SampleData::transact(binder, kCode, WriteNothingToParcel,
                                 ReadNothingFromParcel));

  AIBinder_decStrong(binder);
}

TEST_F(NdkBinderTest_AParcel, TooLargePosition) {
  AIBinder* binder = SampleData::newBinder(
      [&](transaction_code_t, const AParcel* /*in*/, AParcel* out) {
        EXPECT_OK(AParcel_setDataPosition(out, 0));
        EXPECT_OK(AParcel_setDataPosition(out, INT32_MAX));
        EXPECT_EQ(STATUS_BAD_VALUE, AParcel_setDataPosition(out, -1));
        EXPECT_EQ(STATUS_BAD_VALUE, AParcel_setDataPosition(out, -2));
        return STATUS_OK;
      },
      ExpectLifetimeTransactions(1));

  EXPECT_OK(SampleData::transact(binder, kCode, WriteNothingToParcel,
                                 ReadNothingFromParcel));

  AIBinder_decStrong(binder);
}

TEST_F(NdkBinderTest_AParcel, RewritePositions) {
  const std::string kTestString1 = "asdf";
  const std::string kTestString2 = "aoeu";

  // v-- position     v-- postPosition
  // | delta | "asdf" | "aoeu" |
  //         ^-- prePosition
  //
  // uint32_t delta = postPosition - prePosition

  AIBinder* binder = SampleData::newBinder(
      [&](transaction_code_t, const AParcel* in, AParcel* /*out*/) {
        uint32_t delta;
        EXPECT_OK(AParcel_readUint32(in, &delta));
        size_t prePosition = AParcel_getDataPosition(in);
        size_t postPosition = prePosition + delta;

        std::string readString;

        EXPECT_OK(AParcel_setDataPosition(in, postPosition));
        EXPECT_OK(::ndk::AParcel_readString(in, &readString));
        EXPECT_EQ(kTestString2, readString);

        EXPECT_OK(AParcel_setDataPosition(in, prePosition));
        EXPECT_OK(::ndk::AParcel_readString(in, &readString));
        EXPECT_EQ(kTestString1, readString);

        EXPECT_EQ(postPosition, AParcel_getDataPosition(in));

        return STATUS_OK;
      },
      ExpectLifetimeTransactions(1));

  EXPECT_OK(SampleData::transact(
      binder, kCode,
      [&](AParcel* in) {
        size_t position = AParcel_getDataPosition(in);
        EXPECT_OK(AParcel_writeUint32(in, 0));  // placeholder
        size_t prePosition = AParcel_getDataPosition(in);
        EXPECT_OK(::ndk::AParcel_writeString(in, kTestString1));
        size_t postPosition = AParcel_getDataPosition(in);
        EXPECT_OK(::ndk::AParcel_writeString(in, kTestString2));

        size_t delta = postPosition - prePosition;
        EXPECT_OK(AParcel_setDataPosition(in, position));
        EXPECT_OK(AParcel_writeUint32(in, static_cast<uint32_t>(delta)));

        return STATUS_OK;
      },
      ReadNothingFromParcel));

  AIBinder_decStrong(binder);
}
