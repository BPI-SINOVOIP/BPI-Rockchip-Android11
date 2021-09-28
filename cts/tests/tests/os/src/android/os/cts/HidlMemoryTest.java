/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.HidlMemory;
import android.os.NativeHandle;

import org.junit.Test;

import java.io.IOException;

public class HidlMemoryTest {
    @Test
    public void testAccessors() {
        NativeHandle handle = new NativeHandle();
        HidlMemory mem = new HidlMemory("some name", 123, handle);
        assertEquals("some name", mem.getName());
        assertEquals(123, mem.getSize());
        assertTrue(handle == mem.getHandle());
    }

    @Test
    public void testClose() throws IOException {
        NativeHandle handle = new NativeHandle();
        HidlMemory mem = new HidlMemory("some name", 123, handle);
        assertFalse(isHandleClosed(handle));
        mem.close();
        assertTrue(isHandleClosed(handle));
    }

    @Test
    public void testCloseNullHandle() throws IOException {
        HidlMemory mem = new HidlMemory("some name", 123, null);
        mem.close();
    }

    @Test
    public void testRelease() throws IOException {
        NativeHandle handle = new NativeHandle();
        HidlMemory mem = new HidlMemory("some name", 123, handle);
        assertTrue(mem.getHandle() == handle);
        NativeHandle released = mem.releaseHandle();
        assertTrue(mem.getHandle() == null);
        assertTrue(released == handle);
        assertFalse(isHandleClosed(handle));
        handle.close();
    }

    @Test
    public void testDup() throws IOException {
        NativeHandle handle = new NativeHandle();
        HidlMemory mem = new HidlMemory("some name", 123, handle);

        HidlMemory dup = mem.dup();
        assertEquals("some name", dup.getName());
        assertEquals(123, dup.getSize());
        assertFalse(dup.getHandle() == handle);

        assertFalse(isHandleClosed(handle));
        assertFalse(isHandleClosed(dup.getHandle()));

        mem.close();
        assertTrue(isHandleClosed(handle));
        assertFalse(isHandleClosed(dup.getHandle()));
    }

    @Test
    public void testDupNullHandle() throws IOException {
        HidlMemory mem = new HidlMemory("some name", 123, null);

        HidlMemory dup = mem.dup();
        assertEquals("some name", dup.getName());
        assertEquals(123, dup.getSize());
        assertTrue(dup.getHandle() == null);
    }

    private static boolean isHandleClosed(NativeHandle handle) {
        // There is no API for actually checking whether a handle is closed, and since it is a
        // final class we can't use Mockito.spy() on it, so we rely on it throwing when trying to
        // use it after closing.
        try {
            handle.getFileDescriptors();
            return false;
        } catch (IllegalStateException e) {
            return true;
        }
    }
}
