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

package android.aidl.tests;

import android.aidl.tests.ByteEnum;
import android.aidl.tests.IntEnum;
import android.aidl.tests.LongEnum;
import android.aidl.tests.ConstantExpressionEnum;

parcelable StructuredParcelable {
    int[] shouldContainThreeFs;
    int f;
    @utf8InCpp String shouldBeJerry;
    ByteEnum shouldBeByteBar;
    IntEnum shouldBeIntBar;
    LongEnum shouldBeLongBar;
    ByteEnum[] shouldContainTwoByteFoos;
    IntEnum[] shouldContainTwoIntFoos;
    LongEnum[] shouldContainTwoLongFoos;

    String stringDefaultsToFoo = "foo";
    byte byteDefaultsToFour = 4;
    int intDefaultsToFive = 5;
    long longDefaultsToNegativeSeven = -7;
    boolean booleanDefaultsToTrue = true;
    char charDefaultsToC = 'C';
    float floatDefaultsToPi = 3.14f;
    double doubleWithDefault = -3.14e17;
    int[] arrayDefaultsTo123 = { 1, 2, 3 };
    int[] arrayDefaultsToEmpty = { };

    // parse checks only
    double checkDoubleFromFloat = 3.14f;
    String[] checkStringArray1 = { "a", "b" };
    @utf8InCpp String[] checkStringArray2 = { "a", "b" };

    // Add test to verify corner cases
    int int32_min = -2147483648;
    int int32_max =  2147483647;
    long int64_max =  9223372036854775807;
    int hexInt32_neg_1 = 0xffffffff;

    @nullable IBinder ibinder;

    // Constant expressions that evaluate to 1
    int[] int32_1 = {
      (~(-1)) == 0,
      -(1 << 31) == (1 << 31),
      -0x7fffffff < 0,
      -0x80000000 < 0,

      // both treated int32_t, sum = (int32_t)0x80000000 = -2147483648
      (1 + 0x7fffffff) == -2147483648,

      // Shifting for more than 31 bits are undefined. Not tested.
      (1 << 31) == 0x80000000,

      // Should be all true / ones.
      (1 + 2) == 3,
      (8 - 9) == -1,
      (9 * 9) == 81,
      (29 / 3) == 9,
      (29 % 3) == 2,
      (0xC0010000 | 0xF00D) == (0xC001F00D),
      (10 | 6) == 14,
      (10 & 6) == 2,
      (10 ^ 6) == 12,
      6 < 10,
      (10 < 10) == 0,
      (6 > 10) == 0,
      (10 > 10) == 0,
      19 >= 10,
      10 >= 10,
      5 <= 10,
      (19 <= 10) == 0,
      19 != 10,
      (10 != 10) == 0,
      (22 << 1) == 44,
      (11 >> 1) == 5,
      (1 || 0) == 1,
      (1 || 1) == 1,
      (0 || 0) == 0,
      (0 || 1) == 1,
      (1 && 0) == 0,
      (1 && 1) == 1,
      (0 && 0) == 0,
      (0 && 1) == 0,

      // precedence tests -- all 1s
      4 == 4,
      -4 < 0,
      0xffffffff == -1,
      4 + 1 == 5,
      2 + 3 - 4,
      2 - 3 + 4 == 3,
      1 == 4 == 0,
      1 && 1,
      1 || 1 && 0,  // && higher than ||
      1 < 2,
      !!((3 != 4 || (2 < 3 <= 3 > 4)) >= 0),
      !(1 == 7) && ((3 != 4 || (2 < 3 <= 3 > 4)) >= 0),
      (1 << 2) >= 0,
      (4 >> 1) == 2,
      (8 << -1) == 4,
      (1 << 31 >> 31) == -1,
      (1 | 16 >> 2) == 5,
      (0x0f ^ 0x33 & 0x99) == 0x1e, // & higher than ^
      (~42 & (1 << 3 | 16 >> 2) ^ 7) == 3,
      (2 + 3 - 4 * -7 / (10 % 3)) - 33 == 0,
      (2 + (-3&4 / 7)) == 2,
      (((((1 + 0)))))
    };

    long[] int64_1 = {
      (~(-1)) == 0,
      (~4294967295) != 0,
      (~4294967295) != 0,
      -(1 << 63) == (1 << 63),
      -0x7FFFFFFFFFFFFFFF < 0,

      // both treated int32_t, sum = (int64_t)(int32_t)0x80000000 = (int64_t)(-2147483648)
      (1 + 0x7fffffff) == -2147483648,

      // 0x80000000 is uint32_t, sum = (int64_t)(uint32_t)0x7fffffff = (int64_t)(2147483647)
      (0x80000000 - 1) == 2147483647,
      (0x80000000 + 1) == -2147483647,
      (1L << 63)+1 == -9223372036854775807,
      0xfffffffff == 68719476735
    };
    int hexInt32_pos_1 = -0xffffffff;
    int hexInt64_pos_1 = -0xfffffffffff < 0;

    ConstantExpressionEnum const_exprs_1;
    ConstantExpressionEnum const_exprs_2;
    ConstantExpressionEnum const_exprs_3;
    ConstantExpressionEnum const_exprs_4;
    ConstantExpressionEnum const_exprs_5;
    ConstantExpressionEnum const_exprs_6;
    ConstantExpressionEnum const_exprs_7;
    ConstantExpressionEnum const_exprs_8;
    ConstantExpressionEnum const_exprs_9;
    ConstantExpressionEnum const_exprs_10;

    // String expressions
    @utf8InCpp String addString1 = "hello" + " world!";
    @utf8InCpp String addString2 = "The quick brown fox jumps " + "over the lazy dog.";
}

