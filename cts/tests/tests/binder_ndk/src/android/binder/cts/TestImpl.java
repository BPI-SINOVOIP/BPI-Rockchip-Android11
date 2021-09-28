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

package android.binder.cts;

import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import java.io.IOException;

import test_package.IEmpty;
import test_package.ITest;
import test_package.RegularPolygon;
import test_package.Foo;
import test_package.Bar;

import java.util.concurrent.CountDownLatch;
import java.io.FileDescriptor;
import java.io.PrintWriter;

public class TestImpl extends ITest.Stub {
  @Override
  public int getInterfaceVersion() { return TestImpl.VERSION; }

  @Override
  public String getInterfaceHash() { return TestImpl.HASH; }

  @Override
  protected void dump(FileDescriptor fd, PrintWriter pw, String[] args) {
    for (String arg : args) {
      pw.print(arg);
    }
  }

  @Override
  public String GetName() {
    return "JAVA";
  }

  @Override
  public void TestVoidReturn() {}

  @Override
  public void TestOneway() {}

  @Override
  public int GiveMeMyCallingPid() {
    return Binder.getCallingPid();
  }

  @Override
  public int GiveMeMyCallingUid() {
    return Binder.getCallingUid();
  }

  private CountDownLatch mCachedLatch = new CountDownLatch(1);
  private int mCachedPid = -1;
  private int mCachedUid = -1;

  private void waitForCachedOnewayInfo() {
    try {
      mCachedLatch.await();
    } catch (InterruptedException e) {
      // This thread is never expected to be interrupted during this test. This will be
      // converted to a RemoteException on the other side and cause the test to fail.

      throw new RuntimeException(e.toString());
    }
  }

  @Override
  public void CacheCallingInfoFromOneway() {
    mCachedPid = Binder.getCallingPid();
    mCachedUid = Binder.getCallingUid();
    mCachedLatch.countDown();
  }

  @Override
  public int GiveMeMyCallingPidFromOneway() {
    waitForCachedOnewayInfo();
    return mCachedPid;
  }

  @Override
  public int GiveMeMyCallingUidFromOneway() {
    waitForCachedOnewayInfo();
    return mCachedUid;
  }

  @Override
  public int RepeatInt(int in_value) {
    return in_value;
  }

  @Override
  public long RepeatLong(long in_value) {
    return in_value;
  }

  @Override
  public float RepeatFloat(float in_value) {
    return in_value;
  }

  @Override
  public double RepeatDouble(double in_value) {
    return in_value;
  }

  @Override
  public boolean RepeatBoolean(boolean in_value) {
    return in_value;
  }

  @Override
  public char RepeatChar(char in_value) {
    return in_value;
  }

  @Override
  public byte RepeatByte(byte in_value) {
    return in_value;
  }

  @Override
  public byte RepeatByteEnum(byte in_value) {
    return in_value;
  }

  @Override
  public int RepeatIntEnum(int in_value) {
    return in_value;
  }

  @Override
  public long RepeatLongEnum(long in_value) {
    return in_value;
  }

  @Override
  public IBinder RepeatBinder(IBinder in_value) {
    return in_value;
  }

  @Override
  public IBinder RepeatNullableBinder(IBinder in_value) {
    return in_value;
  }

  @Override
  public IEmpty RepeatInterface(IEmpty in_value) {
    return in_value;
  }

  @Override
  public IEmpty RepeatNullableInterface(IEmpty in_value) {
    return in_value;
  }

  @Override
  public ParcelFileDescriptor RepeatFd(ParcelFileDescriptor in_value) {
    return in_value;
  }

  @Override
  public ParcelFileDescriptor RepeatNullableFd(ParcelFileDescriptor in_value) {
    return in_value;
  }

  @Override
  public String RepeatString(String in_value) {
    return in_value;
  }

  @Override
  public String RepeatNullableString(String in_value) {
    return in_value;
  }

  @Override
  public RegularPolygon RepeatPolygon(RegularPolygon in_value) {
    return in_value;
  }

  @Override
  public RegularPolygon RepeatNullablePolygon(RegularPolygon in_value) {
    return in_value;
  }

  @Override
  public void RenamePolygon(RegularPolygon value, String name) {
    value.name = name;
  }

  @Override
  public boolean[] RepeatBooleanArray(boolean[] in_value, boolean[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public byte[] RepeatByteArray(byte[] in_value, byte[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public char[] RepeatCharArray(char[] in_value, char[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public int[] RepeatIntArray(int[] in_value, int[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public long[] RepeatLongArray(long[] in_value, long[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public float[] RepeatFloatArray(float[] in_value, float[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public double[] RepeatDoubleArray(double[] in_value, double[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public byte[] RepeatByteEnumArray(byte[] in_value, byte[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public int[] RepeatIntEnumArray(int[] in_value, int[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public long[] RepeatLongEnumArray(long[] in_value, long[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public String[] RepeatStringArray(String[] in_value, String[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public RegularPolygon[] RepeatRegularPolygonArray(RegularPolygon[] in_value, RegularPolygon[] repeated) {
    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public ParcelFileDescriptor[] RepeatFdArray(ParcelFileDescriptor[] in_value, ParcelFileDescriptor[] repeated) {
    ParcelFileDescriptor[] out = new ParcelFileDescriptor[in_value.length];
    for (int i = 0; i < in_value.length; i++) {
      try {
        repeated[i] = in_value[i].dup();
        out[i] = in_value[i].dup();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
    return out;
  }

  public java.util.List<String> Repeat2StringList(java.util.List<String> in_value, java.util.List<String> repeated) {
    repeated.addAll(in_value);
    repeated.addAll(in_value);
    return repeated;
  }

  @Override
  public java.util.List<RegularPolygon> Repeat2RegularPolygonList(java.util.List<RegularPolygon> in_value, java.util.List<RegularPolygon> repeated) {
    repeated.addAll(in_value);
    repeated.addAll(in_value);
    return repeated;
  }

  @Override
  public boolean[] RepeatNullableBooleanArray(boolean[] in_value) {
    return in_value;
  }

  @Override
  public byte[] RepeatNullableByteArray(byte[] in_value) {
    return in_value;
  }

  @Override
  public char[] RepeatNullableCharArray(char[] in_value) {
    return in_value;
  }

  @Override
  public int[] RepeatNullableIntArray(int[] in_value) {
    return in_value;
  }

  @Override
  public long[] RepeatNullableLongArray(long[] in_value) {
    return in_value;
  }

  @Override
  public float[] RepeatNullableFloatArray(float[] in_value) {
    return in_value;
  }

  @Override
  public double[] RepeatNullableDoubleArray(double[] in_value) {
    return in_value;
  }

  @Override
  public byte[] RepeatNullableByteEnumArray(byte[] in_value) {
    return in_value;
  }

  @Override
  public int[] RepeatNullableIntEnumArray(int[] in_value) {
    return in_value;
  }

  @Override
  public long[] RepeatNullableLongEnumArray(long[] in_value) {
    return in_value;
  }

  @Override
  public String[] RepeatNullableStringArray(String[] in_value) {
    return in_value;
  }

  @Override
  public String[] DoubleRepeatNullableStringArray(String[] in_value, String[] repeated) {
    if (in_value == null) {
      return null; // can't do anything to repeated
    }

    System.arraycopy(in_value, 0, repeated, 0, in_value.length);
    return in_value;
  }

  @Override
  public Foo repeatFoo(Foo inFoo) {
    return inFoo;
  }

  @Override
  public void renameFoo(Foo foo, String name) {
    foo.a = name;
  }

  @Override
  public void renameBar(Foo foo, String name) {
    if (foo.d == null) {
      foo.d = new Bar();
    }
    foo.d.a = name;
  }

  @Override
  public int getF(Foo foo) {
    return foo.f;
  }

  @Override
  public String RepeatStringNullableLater(String in_value) {
    return in_value;
  }

  @Override
  public int NewMethodThatReturns10() {
    return 10;
  }
}
