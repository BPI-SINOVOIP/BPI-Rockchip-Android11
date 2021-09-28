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

package test_package;

import test_package.ByteEnum;
import test_package.Foo;
import test_package.IEmpty;
import test_package.IntEnum;
import test_package.LongEnum;
import test_package.RegularPolygon;

// This test interface is used in order to test the all of the things that AIDL can generate which
// build on top of the NDK.
//
// Repeat => return the same value. This is used to keep the clients/tests simple.
interface ITest {
    const int kZero = 0;
    const int kOne = 1;
    const int kOnes = 0xffffffff;
    const String kEmpty = "";
    const String kFoo = "foo";

    String GetName();

    void TestVoidReturn();
    oneway void TestOneway();

    int GiveMeMyCallingPid();
    int GiveMeMyCallingUid();

    // This must be called before calling one of the give-me methods below
    oneway void CacheCallingInfoFromOneway();
    int GiveMeMyCallingPidFromOneway();
    int GiveMeMyCallingUidFromOneway();

    // Sending/receiving primitive types.
    int RepeatInt(int value);
    long RepeatLong(long value);
    float RepeatFloat(float value);
    double RepeatDouble(double value);
    boolean RepeatBoolean(boolean value);
    char RepeatChar(char value);
    byte RepeatByte(byte value);
    ByteEnum RepeatByteEnum(ByteEnum value);
    IntEnum RepeatIntEnum(IntEnum value);
    LongEnum RepeatLongEnum(LongEnum value);

    IBinder RepeatBinder(IBinder value);
    @nullable IBinder RepeatNullableBinder(@nullable IBinder value);
    IEmpty RepeatInterface(IEmpty value);
    @nullable IEmpty RepeatNullableInterface(@nullable IEmpty value);

    ParcelFileDescriptor RepeatFd(in ParcelFileDescriptor fd);
    @nullable ParcelFileDescriptor RepeatNullableFd(in @nullable ParcelFileDescriptor fd);

    String RepeatString(String value);
    @nullable String RepeatNullableString(@nullable String value);

    RegularPolygon RepeatPolygon(in RegularPolygon value);
    @nullable RegularPolygon RepeatNullablePolygon(in @nullable RegularPolygon value);

    // Testing inout
    void RenamePolygon(inout RegularPolygon value, String newName);

    // Arrays
    boolean[] RepeatBooleanArray(in boolean[] input, out boolean[] repeated);
    byte[] RepeatByteArray(in byte[] input, out byte[] repeated);
    char[] RepeatCharArray(in char[] input, out char[] repeated);
    int[] RepeatIntArray(in int[] input, out int[] repeated);
    long[] RepeatLongArray(in long[] input, out long[] repeated);
    float[] RepeatFloatArray(in float[] input, out float[] repeated);
    double[] RepeatDoubleArray(in double[] input, out double[] repeated);
    ByteEnum[] RepeatByteEnumArray(in ByteEnum[] input, out ByteEnum[] repeated);
    IntEnum[] RepeatIntEnumArray(in IntEnum[] input, out IntEnum[] repeated);
    LongEnum[] RepeatLongEnumArray(in LongEnum[] input, out LongEnum[] repeated);
    String[] RepeatStringArray(in String[] input, out String[] repeated);
    RegularPolygon[] RepeatRegularPolygonArray(in RegularPolygon[] input, out RegularPolygon[] repeated);
    ParcelFileDescriptor[] RepeatFdArray(in ParcelFileDescriptor[] input, out ParcelFileDescriptor[] repeated);

    // Lists
    List<String> Repeat2StringList(in List<String> input, out List<String> repeated);
    List<RegularPolygon> Repeat2RegularPolygonList(in List<RegularPolygon> input, out List<RegularPolygon> repeated);

    // Nullable Arrays
    @nullable boolean[] RepeatNullableBooleanArray(in @nullable boolean[] input);
    @nullable byte[] RepeatNullableByteArray(in @nullable byte[] input);
    @nullable char[] RepeatNullableCharArray(in @nullable char[] input);
    @nullable int[] RepeatNullableIntArray(in @nullable int[] input);
    @nullable long[] RepeatNullableLongArray(in @nullable long[] input);
    @nullable float[] RepeatNullableFloatArray(in @nullable float[] input);
    @nullable double[] RepeatNullableDoubleArray(in @nullable double[] input);
    @nullable ByteEnum[] RepeatNullableByteEnumArray(in @nullable ByteEnum[] input);
    @nullable IntEnum[] RepeatNullableIntEnumArray(in @nullable IntEnum[] input);
    @nullable LongEnum[] RepeatNullableLongEnumArray(in @nullable LongEnum[] input);
    @nullable String[] RepeatNullableStringArray(in @nullable String[] input);

    // Nullable Arrays where each individual element can be nullable
    // (specifically for testing out parameters)
    @nullable String[] DoubleRepeatNullableStringArray(
        in @nullable String[] input, out @nullable String[] repeated);

    Foo repeatFoo(in Foo inFoo);
    void renameFoo(inout Foo foo, String name);
    void renameBar(inout Foo foo, String name);
    int getF(in Foo foo);

    // Method which is not nullable in version 1, but is nullable in version 2
    @nullable String RepeatStringNullableLater(@nullable String repeated);

    // Methods that do not exist in version 1
    int NewMethodThatReturns10();
}
