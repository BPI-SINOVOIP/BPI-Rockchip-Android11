/*
 * Copyright (C) 2019 Google Inc.
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

package android.os.cts;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import android.hidl.manager.V1_0.IServiceManager;
import android.hidl.manager.V1_0.IServiceNotification;
import android.os.HwBlob;
import android.os.NativeHandle;
import android.os.RemoteException;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileDescriptor;
import java.io.IOException;
import java.util.Calendar;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Unit tests for HwBinder.
 *
 * If you want to create a real hwbinder service, using hidl-gen will be
 * *much* *much* easier. Using something like this is strongly not recommended
 * because you can't take advantage of the versioning tools, the C++ and Java
 * interoperability, etc..
 */
@RunWith(AndroidJUnit4.class)
public class HwBinderTest {

    private static class MarshalCase {
        interface DoMarshalCase {
            void test(HwBlob blob, int offset);
        }

        MarshalCase(String name, int size, DoMarshalCase method) {
            mName = name;
            mSize = size;
            mMethod = method;
        }

        private String mName;
        private int mSize;
        private DoMarshalCase mMethod;

        public String getName() {
            return mName;
        }

        void test(int deltaSize, int offset) {
            HwBlob blob = new HwBlob(mSize + deltaSize);
            mMethod.test(blob, offset);
        }
    }

    private static final MarshalCase[] sMarshalCases = {
        new MarshalCase("Bool", 1, (blob, offset) -> {
            blob.putBool(offset, true);
            assertEquals(true, blob.getBool(offset));
        }),
        new MarshalCase("Int8", 1, (blob, offset) -> {
            blob.putInt8(offset, (byte) 3);
            assertEquals((byte) 3, blob.getInt8(offset));
        }),
        new MarshalCase("Int16", 2, (blob, offset) -> {
            blob.putInt16(offset, (short) 3);
            assertEquals((short) 3, blob.getInt16(offset));
        }),
        new MarshalCase("Int32", 4, (blob, offset) -> {
            blob.putInt32(offset, 3);
            assertEquals(3, blob.getInt32(offset));
        }),
        new MarshalCase("Int64", 8, (blob, offset) -> {
            blob.putInt64(offset, 3);
            assertEquals(3, blob.getInt64(offset));
        }),
        new MarshalCase("Float", 4, (blob, offset) -> {
            blob.putFloat(offset, 3.0f);
            assertEquals(3.0f, blob.getFloat(offset), 0.0f);
        }),
        new MarshalCase("Double", 8, (blob, offset) -> {
            blob.putDouble(offset, 3.0);
            assertEquals(3.0, blob.getDouble(offset), 0.0);
        }),
        new MarshalCase("String", 16, (blob, offset) -> {
            blob.putString(offset, "foo");
            assertEquals("foo", blob.getString(offset));
        }),
        new MarshalCase("BoolArray", 2, (blob, offset) -> {
            blob.putBoolArray(offset, new boolean[]{true, false});
            assertEquals(true, blob.getBool(offset));
            assertEquals(false, blob.getBool(offset + 1));
        }),
        new MarshalCase("Int8Array", 2, (blob, offset) -> {
            blob.putInt8Array(offset, new byte[]{(byte) 3, (byte) 2});
            assertEquals((byte) 3, blob.getInt8(offset));
            assertEquals((byte) 2, blob.getInt8(offset + 1));
        }),
        new MarshalCase("Int16Array", 4, (blob, offset) -> {
            blob.putInt16Array(offset, new short[]{(short) 3, (short) 2});
            assertEquals((short) 3, blob.getInt16(offset));
            assertEquals((short) 2, blob.getInt16(offset + 2));
        }),
        new MarshalCase("Int32Array", 8, (blob, offset) -> {
            blob.putInt32Array(offset, new int[]{3, 2});
            assertEquals(3, blob.getInt32(offset));
            assertEquals(2, blob.getInt32(offset + 4));
        }),
        new MarshalCase("Int64Array", 16, (blob, offset) -> {
            blob.putInt64Array(offset, new long[]{3, 2});
            assertEquals(3, blob.getInt64(offset));
            assertEquals(2, blob.getInt64(offset + 8));
        }),
        new MarshalCase("FloatArray", 8, (blob, offset) -> {
            blob.putFloatArray(offset, new float[]{3.0f, 2.0f});
            assertEquals(3.0f, blob.getFloat(offset), 0.0f);
            assertEquals(2.0f, blob.getFloat(offset + 4), 0.0f);
        }),
        new MarshalCase("DoubleArray", 16, (blob, offset) -> {
            blob.putDoubleArray(offset, new double[]{3.0, 2.0});
            assertEquals(3.0, blob.getDouble(offset), 0.0);
            assertEquals(2.0, blob.getDouble(offset + 8), 0.0);
        }),
    };

    @Test
    public void testAccurateMarshall() {
        for (MarshalCase marshalCase : sMarshalCases) {
            marshalCase.test(0 /* deltaSize */, 0 /* offset */);
        }
    }
    @Test
    public void testAccurateMarshallWithExtraSpace() {
        for (MarshalCase marshalCase : sMarshalCases) {
            marshalCase.test(1 /* deltaSize */, 0 /* offset */);
        }
    }
    @Test
    public void testAccurateMarshallWithExtraSpaceAndOffset() {
        for (MarshalCase marshalCase : sMarshalCases) {
            marshalCase.test(1 /* deltaSize */, 1 /* offset */);
        }
    }
    @Test
    public void testNotEnoughSpaceBecauseOfSize() {
        for (MarshalCase marshalCase : sMarshalCases) {
            try {
                marshalCase.test(-1 /* deltaSize */, 0 /* offset */);
                fail("Marshal succeeded but not enough space for " + marshalCase.getName());
            } catch (IndexOutOfBoundsException e) {
                // we expect to see this
            }
        }
    }
    @Test
    public void testNotEnoughSpaceBecauseOfOffset() {
        for (MarshalCase marshalCase : sMarshalCases) {
            try {
                marshalCase.test(0 /* deltaSize */, 1 /* offset */);
                fail("Marshal succeeded but not enough space for " + marshalCase.getName());
            } catch (IndexOutOfBoundsException e) {
                // we expect to see this
            }
        }
    }
    @Test
    public void testNotEnoughSpaceBecauseOfSizeAndOffset() {
        for (MarshalCase marshalCase : sMarshalCases) {
            try {
                marshalCase.test(-1 /* deltaSize */, 1 /* offset */);
                fail("Marshal succeeded but not enough space for " + marshalCase.getName());
            } catch (IndexOutOfBoundsException e) {
                // we expect to see this
            }
        }
    }

    private static class ServiceNotification extends IServiceNotification.Stub {
        public Lock lock = new ReentrantLock();
        public Condition condition = lock.newCondition();
        public boolean registered = false;

        @Override
        public void onRegistration(String fqName, String name, boolean preexisting) {
            registered = true;
            condition.signal();
        }
    }

    @Test
    public void testHwBinder() throws RemoteException {
        ServiceNotification notification = new ServiceNotification();

        IServiceManager manager = IServiceManager.getService();
        manager.registerForNotifications(IServiceManager.kInterfaceName, "default", notification);

        Calendar deadline = Calendar.getInstance();
        deadline.add(Calendar.SECOND, 10);

        notification.lock.lock();
        try {
            while (!notification.registered && Calendar.getInstance().before(deadline)) {
                try {
                    notification.condition.awaitUntil(deadline.getTime());
                } catch (InterruptedException e) {

                }
            }
        } finally {
            notification.lock.unlock();
        }

        assertTrue(notification.registered);
    }

    @Test
    public void testEmptyNativeHandle() throws IOException {
        NativeHandle handle = new NativeHandle();

        assertFalse(handle.hasSingleFileDescriptor());
        assertArrayEquals(new FileDescriptor[]{}, handle.getFileDescriptors());
        assertArrayEquals(new int[]{}, handle.getInts());

        handle.close();
    }

    @Test(expected = IllegalStateException.class)
    public void testEmptyHandleInvalidatedAfterClose() throws IOException {
        NativeHandle handle = new NativeHandle();
        handle.close();
        handle.close();
    }

    @Test(expected = IllegalStateException.class)
    public void testEmptyHandleHasNoSingleFileDescriptor() {
        NativeHandle handle = new NativeHandle();
        handle.getFileDescriptor();
    }

    @Test(expected = IllegalStateException.class)
    public void testFullHandleHasNoSingleFileDescriptor() {
        NativeHandle handle = new NativeHandle(
            new FileDescriptor[]{FileDescriptor.in, FileDescriptor.in},
            new int[]{}, false /*owns*/);
        handle.getFileDescriptor();
    }

    @Test
    public void testSingleFileDescriptor() {
        NativeHandle handle = new NativeHandle(FileDescriptor.in, false /*own*/);

        assertTrue(handle.hasSingleFileDescriptor());
        assertArrayEquals(new FileDescriptor[]{FileDescriptor.in}, handle.getFileDescriptors());
        assertArrayEquals(new int[]{}, handle.getInts());
    }
}
