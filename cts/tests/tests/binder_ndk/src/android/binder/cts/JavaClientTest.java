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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import test_package.Bar;
import test_package.ByteEnum;
import test_package.Foo;
import test_package.IEmpty;
import test_package.ITest;
import test_package.IntEnum;
import test_package.LongEnum;
import test_package.RegularPolygon;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.ArrayList;
import java.util.function.BiPredicate;

@RunWith(Parameterized.class)
public class JavaClientTest {
    private final String TAG = "JavaClientTest";

    private Class mServiceClass;
    private ITest mInterface;
    private String mExpectedName;
    private boolean mShouldBeRemote;
    private boolean mShouldBeOld;

    public JavaClientTest(Class serviceClass, String expectedName, boolean shouldBeRemote, boolean shouldBeOld) {
        mServiceClass = serviceClass;
        mExpectedName = expectedName;
        mShouldBeRemote = shouldBeRemote;
        mShouldBeOld = shouldBeOld;
    }

    @Parameterized.Parameters( name = "{0}" )
    public static Collection<Object[]> data() {
        // For local interfaces, this test will parcel the data locally.
        // Whenever possible, the desired service should be accessed directly
        // in order to avoid this additional overhead.
        return Arrays.asList(new Object[][] {
                {NativeService.Local.class, "CPP", false /*shouldBeRemote*/, false /*shouldBeOld*/},
                {JavaService.Local.class, "JAVA", false /*shouldBeRemote*/, false /*shouldBeOld*/},
                {NativeService.Remote.class, "CPP", true /*shouldBeRemote*/, false /*shouldBeOld*/},
                {NativeService.RemoteOld.class, "CPP", true /*shouldBeRemote*/, true /*shouldBeOld*/},
                {JavaService.Remote.class, "JAVA", true /*shouldBeRemote*/, false /*shouldBeOld*/},
            });
    }

    @Before
    public void setUp() {
        Log.e(TAG, "Setting up");

        SyncTestServiceConnection connection = new SyncTestServiceConnection(
            InstrumentationRegistry.getTargetContext(), mServiceClass);

        mInterface = connection.get();
        assertNotEquals(null, mInterface);
    }

    @Test
    public void testSanityCheckSource() throws RemoteException {
        String name = mInterface.GetName();

        Log.i(TAG, "Service GetName: " + name);
        assertEquals(mExpectedName, name);
    }

    @Test
    public void testTrivial() throws RemoteException {
        mInterface.TestVoidReturn();
        mInterface.TestOneway();
    }

    private void checkDump(String expected, String[] args) throws RemoteException, IOException {
        ParcelFileDescriptor[] sockets = ParcelFileDescriptor.createReliableSocketPair();
        ParcelFileDescriptor socketIn = sockets[0];
        ParcelFileDescriptor socketOut = sockets[1];

        mInterface.asBinder().dump(socketIn.getFileDescriptor(), args);
        socketIn.close();

        FileInputStream fileInputStream = new ParcelFileDescriptor.AutoCloseInputStream(socketOut);

        byte[] expectedBytes = expected.getBytes();
        byte[] input = new byte[expectedBytes.length];

        assertEquals(input.length, fileInputStream.read(input));
        Assert.assertArrayEquals(input, expectedBytes);
    }

    @Test
    public void testDump() throws RemoteException, IOException {
        checkDump("", new String[]{});
        checkDump("", new String[]{"", ""});
        checkDump("Hello World!", new String[]{"Hello ", "World!"});
        checkDump("ABC", new String[]{"A", "B", "C"});
    }

    @Test
    public void testCallingInfo() throws RemoteException {
      mInterface.CacheCallingInfoFromOneway();

      assertEquals(Process.myPid(), mInterface.GiveMeMyCallingPid());
      assertEquals(Process.myUid(), mInterface.GiveMeMyCallingUid());

      if (mShouldBeRemote) {
        // PID is hidden from oneway calls
        assertEquals(0, mInterface.GiveMeMyCallingPidFromOneway());
      } else {
        assertEquals(Process.myPid(), mInterface.GiveMeMyCallingPidFromOneway());
      }

      assertEquals(Process.myUid(), mInterface.GiveMeMyCallingUidFromOneway());
    }

    @Test
    public void testRepeatPrimitives() throws RemoteException {
        assertEquals(1, mInterface.RepeatInt(1));
        assertEquals(2, mInterface.RepeatLong(2));
        assertEquals(1.0f, mInterface.RepeatFloat(1.0f), 0.0f);
        assertEquals(2.0, mInterface.RepeatDouble(2.0), 0.0);
        assertEquals(true, mInterface.RepeatBoolean(true));
        assertEquals('a', mInterface.RepeatChar('a'));
        assertEquals((byte)3, mInterface.RepeatByte((byte)3));
        assertEquals(ByteEnum.FOO, mInterface.RepeatByteEnum(ByteEnum.FOO));
        assertEquals(IntEnum.FOO, mInterface.RepeatIntEnum(IntEnum.FOO));
        assertEquals(LongEnum.FOO, mInterface.RepeatLongEnum(LongEnum.FOO));
    }

    @Test
    public void testRepeatBinder() throws RemoteException {
        IBinder binder = mInterface.asBinder();

        assertEquals(binder, mInterface.RepeatBinder(binder));
        assertEquals(binder, mInterface.RepeatNullableBinder(binder));
        assertEquals(null, mInterface.RepeatNullableBinder(null));
    }

    private static class Empty extends IEmpty.Stub {
        @Override
        public int getInterfaceVersion() { return Empty.VERSION; }

        @Override
        public String getInterfaceHash() { return Empty.HASH; }
    }

    @Test
    public void testRepeatInterface() throws RemoteException {
        IEmpty empty = new Empty();

        assertEquals(empty, mInterface.RepeatInterface(empty));
        assertEquals(empty, mInterface.RepeatNullableInterface(empty));
        assertEquals(null, mInterface.RepeatNullableInterface(null));
    }

    private static interface IRepeatFd {
        ParcelFileDescriptor repeat(ParcelFileDescriptor fd) throws RemoteException;
    }

    private void checkFdRepeated(IRepeatFd transformer) throws RemoteException, IOException {
        ParcelFileDescriptor[] sockets = ParcelFileDescriptor.createReliableSocketPair();
        ParcelFileDescriptor socketIn = sockets[0];
        ParcelFileDescriptor socketOut = sockets[1];

        ParcelFileDescriptor repeatFd = transformer.repeat(socketIn);

        boolean isNativeRemote = mInterface.GetName().equals("CPP");
        try {
            socketOut.checkError();

            // Either native didn't properly call detach, or native properly handles detach, and
            // we should change the test to enforce that socket comms work.
            assertFalse("Native doesn't implement comm fd but did not get detach.", isNativeRemote);
        } catch (ParcelFileDescriptor.FileDescriptorDetachedException e) {
            assertTrue("Detach, so remote should be native", isNativeRemote);
        }

        // Both backends support these.
        socketIn.checkError();
        repeatFd.checkError();

        checkInOutSockets(repeatFd, socketOut);
    }

    @Test
    public void testRepeatFd() throws RemoteException, IOException {
        checkFdRepeated((fd) -> mInterface.RepeatFd(fd));
    }

    @Test
    public void testRepeatNullableFd() throws RemoteException, IOException {
        checkFdRepeated((fd) -> mInterface.RepeatNullableFd(fd));
        assertEquals(null, mInterface.RepeatNullableFd(null));
    }

    private void checkInOutSockets(ParcelFileDescriptor in, ParcelFileDescriptor out) throws IOException {
        FileOutputStream repeatFdStream = new ParcelFileDescriptor.AutoCloseOutputStream(in);
        String testData = "asdf";
        byte[] output = testData.getBytes();
        repeatFdStream.write(output);
        repeatFdStream.close();

        FileInputStream fileInputStream = new ParcelFileDescriptor.AutoCloseInputStream(out);
        byte[] input = new byte[output.length];

        assertEquals(input.length, fileInputStream.read(input));
        Assert.assertArrayEquals(input, output);
    }

    @Test
    public void testRepeatFdArray() throws RemoteException, IOException {
        ParcelFileDescriptor[] sockets1 = ParcelFileDescriptor.createReliableSocketPair();
        ParcelFileDescriptor[] sockets2 = ParcelFileDescriptor.createReliableSocketPair();
        ParcelFileDescriptor[] inputs = {sockets1[0], sockets2[0]};
        ParcelFileDescriptor[] repeatFdArray = new ParcelFileDescriptor[inputs.length];
        mInterface.RepeatFdArray(inputs, repeatFdArray);

        checkInOutSockets(repeatFdArray[0], sockets1[1]);
        checkInOutSockets(repeatFdArray[1], sockets2[1]);
    }

    @Test
    public void testRepeatString() throws RemoteException {
        assertEquals("", mInterface.RepeatString(""));
        assertEquals("a", mInterface.RepeatString("a"));
        assertEquals("foo", mInterface.RepeatString("foo"));
    }

    @Test
    public void testRepeatNullableString() throws RemoteException {
        assertEquals(null, mInterface.RepeatNullableString(null));
        assertEquals("", mInterface.RepeatNullableString(""));
        assertEquals("a", mInterface.RepeatNullableString("a"));
        assertEquals("foo", mInterface.RepeatNullableString("foo"));
    }

    public void assertPolygonEquals(RegularPolygon lhs, RegularPolygon rhs) {
        assertEquals(lhs.name, rhs.name);
        assertEquals(lhs.numSides, rhs.numSides);
        assertEquals(lhs.sideLength, rhs.sideLength, 0.0f);
    }
    public void assertPolygonEquals(RegularPolygon[] lhs, RegularPolygon[] rhs) {
        assertEquals(lhs.length, rhs.length);
        for (int i = 0; i < lhs.length; i++) {
            assertPolygonEquals(lhs[i], rhs[i]);
        }
    }

    @Test
    public void testRepeatPolygon() throws RemoteException {
        RegularPolygon polygon = new RegularPolygon();
        polygon.name = "hexagon";
        polygon.numSides = 6;
        polygon.sideLength = 1.0f;

        RegularPolygon result = mInterface.RepeatPolygon(polygon);

        assertPolygonEquals(polygon, result);
    }

    @Test
    public void testRepeatUnexpectedNullPolygon() throws RemoteException {
        try {
           RegularPolygon result = mInterface.RepeatPolygon(null);
        } catch (NullPointerException e) {
           // non-@nullable C++ result can't handle null Polygon
           return;
        }
        // Java always works w/ nullptr
        assertEquals("JAVA", mExpectedName);
    }

    @Test
    public void testRepeatNullNullablePolygon() throws RemoteException {
        RegularPolygon result = mInterface.RepeatNullablePolygon(null);
        assertEquals(null, result);
    }

    @Test
    public void testRepeatPresentNullablePolygon() throws RemoteException {
        RegularPolygon polygon = new RegularPolygon();
        polygon.name = "septagon";
        polygon.numSides = 7;
        polygon.sideLength = 9.0f;

        RegularPolygon result = mInterface.RepeatNullablePolygon(polygon);

        assertPolygonEquals(polygon, result);
    }

    @Test
    public void testInsAndOuts() throws RemoteException {
        RegularPolygon polygon = new RegularPolygon();
        mInterface.RenamePolygon(polygon, "Jerry");
        assertEquals("Jerry", polygon.name);
    }

    @Test
    public void testArrays() throws RemoteException {
        {
            boolean[] value = {};
            boolean[] out1 = new boolean[value.length];
            boolean[] out2 = mInterface.RepeatBooleanArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            boolean[] value = {false, true, false};
            boolean[] out1 = new boolean[value.length];
            boolean[] out2 = mInterface.RepeatBooleanArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            byte[] value = {1, 2, 3};
            byte[] out1 = new byte[value.length];
            byte[] out2 = mInterface.RepeatByteArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            char[] value = {'h', 'a', '!'};
            char[] out1 = new char[value.length];
            char[] out2 = mInterface.RepeatCharArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            int[] value = {1, 2, 3};
            int[] out1 = new int[value.length];
            int[] out2 = mInterface.RepeatIntArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            long[] value = {1, 2, 3};
            long[] out1 = new long[value.length];
            long[] out2 = mInterface.RepeatLongArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            float[] value = {1.0f, 2.0f, 3.0f};
            float[] out1 = new float[value.length];
            float[] out2 = mInterface.RepeatFloatArray(value, out1);

            Assert.assertArrayEquals(value, out1, 0.0f);
            Assert.assertArrayEquals(value, out2, 0.0f);
        }
        {
            double[] value = {1.0, 2.0, 3.0};
            double[] out1 = new double[value.length];
            double[] out2 = mInterface.RepeatDoubleArray(value, out1);

            Assert.assertArrayEquals(value, out1, 0.0);
            Assert.assertArrayEquals(value, out2, 0.0);
        }
        {
            byte[] value = {ByteEnum.FOO, ByteEnum.BAR};
            byte[] out1 = new byte[value.length];
            byte[] out2 = mInterface.RepeatByteEnumArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            int[] value = {IntEnum.FOO, IntEnum.BAR};
            int[] out1 = new int[value.length];
            int[] out2 = mInterface.RepeatIntEnumArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            long[] value = {LongEnum.FOO, LongEnum.BAR};
            long[] out1 = new long[value.length];
            long[] out2 = mInterface.RepeatLongEnumArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {
            String[] value = {"", "aoeu", "lol", "brb"};
            String[] out1 = new String[value.length];
            String[] out2 = mInterface.RepeatStringArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
        {

            RegularPolygon septagon = new RegularPolygon();
            septagon.name = "septagon";
            septagon.numSides = 7;
            septagon.sideLength = 1.0f;

            RegularPolygon[] value = {septagon, new RegularPolygon(), new RegularPolygon()};
            RegularPolygon[] out1 = new RegularPolygon[value.length];
            RegularPolygon[] out2 = mInterface.RepeatRegularPolygonArray(value, out1);

            assertPolygonEquals(value, out1);
            assertPolygonEquals(value, out2);
        }
    }

    @Test
    public void testLists() throws RemoteException {
        {
            List<String> value = Arrays.asList("", "aoeu", "lol", "brb");
            List<String> out1 = new ArrayList<>();
            List<String> out2 = mInterface.Repeat2StringList(value, out1);

            List<String> expected = new ArrayList<>();
            expected.addAll(value);
            expected.addAll(value);
            String[] expectedArray = expected.toArray(new String[0]);

            Assert.assertArrayEquals(expectedArray, out1.toArray(new String[0]));
            Assert.assertArrayEquals(expectedArray, out2.toArray(new String[0]));
        }
        {
            RegularPolygon septagon = new RegularPolygon();
            septagon.name = "septagon";
            septagon.numSides = 7;
            septagon.sideLength = 1.0f;

            List<RegularPolygon> value = Arrays.asList(septagon, new RegularPolygon(), new RegularPolygon());
            List<RegularPolygon> out1 = new ArrayList<>();
            List<RegularPolygon> out2 = mInterface.Repeat2RegularPolygonList(value, out1);

            List<RegularPolygon> expected = new ArrayList<>();
            expected.addAll(value);
            expected.addAll(value);
            RegularPolygon[] expectedArray = expected.toArray(new RegularPolygon[0]);

            assertPolygonEquals(expectedArray, out1.toArray(new RegularPolygon[0]));
            assertPolygonEquals(expectedArray, out1.toArray(new RegularPolygon[0]));
        }
    }

    @Test
    public void testNullableArrays() throws RemoteException {
        {
            boolean[] emptyValue = {};
            boolean[] value = {false, true, false};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableBooleanArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableBooleanArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableBooleanArray(value));
        }
        {
            byte[] emptyValue = {};
            byte[] value = {1, 2, 3};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableByteArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableByteArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableByteArray(value));
        }
        {
            char[] emptyValue = {};
            char[] value = {'h', 'a', '!'};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableCharArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableCharArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableCharArray(value));
        }
        {
            int[] emptyValue = {};
            int[] value = {1, 2, 3};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableIntArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableIntArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableIntArray(value));
        }
        {
            long[] emptyValue = {};
            long[] value = {1, 2, 3};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableLongArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableLongArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableLongArray(value));
        }
        {
            float[] emptyValue = {};
            float[] value = {1.0f, 2.0f, 3.0f};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableFloatArray(null), 0.0f);
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableFloatArray(emptyValue), 0.0f);
            Assert.assertArrayEquals(value, mInterface.RepeatNullableFloatArray(value), 0.0f);
        }
        {
            double[] emptyValue = {};
            double[] value = {1.0, 2.0, 3.0};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableDoubleArray(null), 0.0);
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableDoubleArray(emptyValue), 0.0);
            Assert.assertArrayEquals(value, mInterface.RepeatNullableDoubleArray(value), 0.0);
        }
        {
            byte[] emptyValue = {};
            byte[] value = {ByteEnum.FOO, ByteEnum.BAR};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableByteEnumArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableByteEnumArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableByteEnumArray(value));
        }
        {
            int[] emptyValue = {};
            int[] value = {IntEnum.FOO, IntEnum.BAR};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableIntEnumArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableIntEnumArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableIntEnumArray(value));
        }
        {
            long[] emptyValue = {};
            long[] value = {LongEnum.FOO, LongEnum.BAR};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableLongEnumArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableLongEnumArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableLongEnumArray(value));
        }
        {
            String[] emptyValue = {};
            String[] value = {"", "aoeu", null, "brb"};
            Assert.assertArrayEquals(null, mInterface.RepeatNullableStringArray(null));
            Assert.assertArrayEquals(emptyValue, mInterface.RepeatNullableStringArray(emptyValue));
            Assert.assertArrayEquals(value, mInterface.RepeatNullableStringArray(value));
        }
        {
            String[] emptyValue = {};
            String[] value = {"", "aoeu", null, "brb"};
            String[] out1 = new String[value.length];
            String[] out2 = mInterface.DoubleRepeatNullableStringArray(value, out1);

            Assert.assertArrayEquals(value, out1);
            Assert.assertArrayEquals(value, out2);
        }
    }

    @Test
    public void testGetLastItem() throws RemoteException {
        Foo foo = new Foo();
        foo.d = new Bar();
        foo.e = new Bar();
        foo.f = 15;
        foo.shouldContainTwoByteFoos = new byte[]{};
        foo.shouldContainTwoIntFoos = new int[]{};
        foo.shouldContainTwoLongFoos = new long[]{};

        assertEquals(foo.f, mInterface.getF(foo));
    }

    @Test
    public void testRepeatFoo() throws RemoteException {
        Foo foo = new Foo();

        foo.a = "NEW FOO";
        foo.b = 57;

        foo.d = new Bar();
        foo.d.b = "a";

        foo.e = new Bar();
        foo.e.d = 99;

        foo.shouldBeByteBar = ByteEnum.BAR;
        foo.shouldBeIntBar = IntEnum.BAR;
        foo.shouldBeLongBar = LongEnum.BAR;

        foo.shouldContainTwoByteFoos = new byte[]{ByteEnum.FOO, ByteEnum.FOO};
        foo.shouldContainTwoIntFoos = new int[]{IntEnum.FOO, IntEnum.FOO};
        foo.shouldContainTwoLongFoos = new long[]{LongEnum.FOO, LongEnum.FOO};

        Foo repeatedFoo = mInterface.repeatFoo(foo);

        assertEquals(foo.a, repeatedFoo.a);
        assertEquals(foo.b, repeatedFoo.b);
        assertEquals(foo.d.b, repeatedFoo.d.b);
        assertEquals(foo.e.d, repeatedFoo.e.d);
        assertEquals(foo.shouldBeByteBar, repeatedFoo.shouldBeByteBar);
        assertEquals(foo.shouldBeIntBar, repeatedFoo.shouldBeIntBar);
        assertEquals(foo.shouldBeLongBar, repeatedFoo.shouldBeLongBar);
        Assert.assertArrayEquals(foo.shouldContainTwoByteFoos, repeatedFoo.shouldContainTwoByteFoos);
        Assert.assertArrayEquals(foo.shouldContainTwoIntFoos, repeatedFoo.shouldContainTwoIntFoos);
        Assert.assertArrayEquals(foo.shouldContainTwoLongFoos, repeatedFoo.shouldContainTwoLongFoos);
    }

    @Test
    public void testNewField() throws RemoteException {
        Foo foo = new Foo();
        foo.d = new Bar();
        foo.e = new Bar();
        foo.shouldContainTwoByteFoos = new byte[]{};
        foo.shouldContainTwoIntFoos = new int[]{};
        foo.shouldContainTwoLongFoos = new long[]{};
        foo.g = new String[]{"a", "b", "c"};
        Foo newFoo = mInterface.repeatFoo(foo);
        if (mShouldBeOld) {
            assertEquals(null, newFoo.g);
        } else {
            Assert.assertArrayEquals(foo.g, newFoo.g);
        }
    }
    @Test
    public void testRenameFoo() throws RemoteException {
        Foo foo = new Foo();
        foo.d = new Bar();
        foo.e = new Bar();
        foo.shouldContainTwoByteFoos = new byte[]{};
        foo.shouldContainTwoIntFoos = new int[]{};
        foo.shouldContainTwoLongFoos = new long[]{};
        mInterface.renameFoo(foo, "MYFOO");
        assertEquals("MYFOO", foo.a);
    }
    @Test
    public void testRenameBar() throws RemoteException {
        Foo foo = new Foo();
        foo.d = new Bar();
        foo.e = new Bar();
        foo.shouldContainTwoByteFoos = new byte[]{};
        foo.shouldContainTwoIntFoos = new int[]{};
        foo.shouldContainTwoLongFoos = new long[]{};
        mInterface.renameBar(foo, "MYBAR");
        assertEquals("MYBAR", foo.d.a);
    }

    @Test
    public void testRepeatStringNullableLater() throws RemoteException {
        // see notes in native NdkBinderTest_Aidl RepeatStringNullableLater
        boolean handlesNull = !mShouldBeOld || mExpectedName == "JAVA";
        try {
            assertEquals(null, mInterface.RepeatStringNullableLater(null));
            assertTrue("should reach here if null is handled", handlesNull);
        } catch (NullPointerException e) {
            assertFalse("should reach here if null isn't handled", handlesNull);
        }
        assertEquals("", mInterface.RepeatStringNullableLater(""));
        assertEquals("a", mInterface.RepeatStringNullableLater("a"));
        assertEquals("foo", mInterface.RepeatStringNullableLater("foo"));
    }
}
