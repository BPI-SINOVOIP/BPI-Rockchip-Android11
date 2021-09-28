///////////////////////////////////////////////////////////////////////////////
// THIS FILE IS IMMUTABLE. DO NOT EDIT IN ANY CASE.                          //
///////////////////////////////////////////////////////////////////////////////

// This file is a snapshot of an AIDL interface (or parcelable). Do not try to
// edit this file. It looks like you are doing that because you have modified
// an AIDL interface in a backward-incompatible way, e.g., deleting a function
// from an interface or a field from a parcelable and it broke the build. That
// breakage is intended.
//
// You must not make a backward incompatible changes to the AIDL files built
// with the aidl_interface module type with versions property set. The module
// type is used to build AIDL files in a way that they can be used across
// independently updatable components of the system. If a device is shipped
// with such a backward incompatible change, it has a high risk of breaking
// later when a module using the interface is updated, e.g., Mainline modules.

package test_package;
interface ITest {
  String GetName();
  void TestVoidReturn();
  oneway void TestOneway();
  int GiveMeMyCallingPid();
  int GiveMeMyCallingUid();
  oneway void CacheCallingInfoFromOneway();
  int GiveMeMyCallingPidFromOneway();
  int GiveMeMyCallingUidFromOneway();
  int RepeatInt(int value);
  long RepeatLong(long value);
  float RepeatFloat(float value);
  double RepeatDouble(double value);
  boolean RepeatBoolean(boolean value);
  char RepeatChar(char value);
  byte RepeatByte(byte value);
  test_package.ByteEnum RepeatByteEnum(test_package.ByteEnum value);
  test_package.IntEnum RepeatIntEnum(test_package.IntEnum value);
  test_package.LongEnum RepeatLongEnum(test_package.LongEnum value);
  IBinder RepeatBinder(IBinder value);
  @nullable IBinder RepeatNullableBinder(@nullable IBinder value);
  test_package.IEmpty RepeatInterface(test_package.IEmpty value);
  @nullable test_package.IEmpty RepeatNullableInterface(@nullable test_package.IEmpty value);
  ParcelFileDescriptor RepeatFd(in ParcelFileDescriptor fd);
  @nullable ParcelFileDescriptor RepeatNullableFd(in @nullable ParcelFileDescriptor fd);
  String RepeatString(String value);
  @nullable String RepeatNullableString(@nullable String value);
  test_package.RegularPolygon RepeatPolygon(in test_package.RegularPolygon value);
  @nullable test_package.RegularPolygon RepeatNullablePolygon(in @nullable test_package.RegularPolygon value);
  void RenamePolygon(inout test_package.RegularPolygon value, String newName);
  boolean[] RepeatBooleanArray(in boolean[] input, out boolean[] repeated);
  byte[] RepeatByteArray(in byte[] input, out byte[] repeated);
  char[] RepeatCharArray(in char[] input, out char[] repeated);
  int[] RepeatIntArray(in int[] input, out int[] repeated);
  long[] RepeatLongArray(in long[] input, out long[] repeated);
  float[] RepeatFloatArray(in float[] input, out float[] repeated);
  double[] RepeatDoubleArray(in double[] input, out double[] repeated);
  test_package.ByteEnum[] RepeatByteEnumArray(in test_package.ByteEnum[] input, out test_package.ByteEnum[] repeated);
  test_package.IntEnum[] RepeatIntEnumArray(in test_package.IntEnum[] input, out test_package.IntEnum[] repeated);
  test_package.LongEnum[] RepeatLongEnumArray(in test_package.LongEnum[] input, out test_package.LongEnum[] repeated);
  String[] RepeatStringArray(in String[] input, out String[] repeated);
  test_package.RegularPolygon[] RepeatRegularPolygonArray(in test_package.RegularPolygon[] input, out test_package.RegularPolygon[] repeated);
  ParcelFileDescriptor[] RepeatFdArray(in ParcelFileDescriptor[] input, out ParcelFileDescriptor[] repeated);
  List<String> Repeat2StringList(in List<String> input, out List<String> repeated);
  List<test_package.RegularPolygon> Repeat2RegularPolygonList(in List<test_package.RegularPolygon> input, out List<test_package.RegularPolygon> repeated);
  @nullable boolean[] RepeatNullableBooleanArray(in @nullable boolean[] input);
  @nullable byte[] RepeatNullableByteArray(in @nullable byte[] input);
  @nullable char[] RepeatNullableCharArray(in @nullable char[] input);
  @nullable int[] RepeatNullableIntArray(in @nullable int[] input);
  @nullable long[] RepeatNullableLongArray(in @nullable long[] input);
  @nullable float[] RepeatNullableFloatArray(in @nullable float[] input);
  @nullable double[] RepeatNullableDoubleArray(in @nullable double[] input);
  @nullable test_package.ByteEnum[] RepeatNullableByteEnumArray(in @nullable test_package.ByteEnum[] input);
  @nullable test_package.IntEnum[] RepeatNullableIntEnumArray(in @nullable test_package.IntEnum[] input);
  @nullable test_package.LongEnum[] RepeatNullableLongEnumArray(in @nullable test_package.LongEnum[] input);
  @nullable String[] RepeatNullableStringArray(in @nullable String[] input);
  @nullable String[] DoubleRepeatNullableStringArray(in @nullable String[] input, out @nullable String[] repeated);
  test_package.Foo repeatFoo(in test_package.Foo inFoo);
  void renameFoo(inout test_package.Foo foo, String name);
  void renameBar(inout test_package.Foo foo, String name);
  int getF(in test_package.Foo foo);
  @nullable String RepeatStringNullableLater(@nullable String repeated);
  int NewMethodThatReturns10();
  const int kZero = 0;
  const int kOne = 1;
  const int kOnes = -1;
  const String kEmpty = "";
  const String kFoo = "foo";
}
