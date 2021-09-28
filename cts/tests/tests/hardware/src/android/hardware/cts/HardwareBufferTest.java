/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.hardware.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.hardware.HardwareBuffer;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link HardwareBuffer}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class HardwareBufferTest {
    private static native HardwareBuffer nativeCreateHardwareBuffer(int width, int height,
            int format, int layers, long usage);
    private static native void nativeReleaseHardwareBuffer(HardwareBuffer hardwareBufferObj);

    static {
        System.loadLibrary("ctshardware_jni");
    }

    @Test
    public void testCreate() {
        HardwareBuffer buffer = HardwareBuffer.create(2, 4, HardwareBuffer.RGBA_8888, 1,
                HardwareBuffer.USAGE_CPU_READ_RARELY);
        assertTrue(buffer != null);
        assertEquals(2, buffer.getWidth());
        assertEquals(4, buffer.getHeight());
        assertEquals(HardwareBuffer.RGBA_8888, buffer.getFormat());
        assertEquals(1, buffer.getLayers());
        assertEquals(HardwareBuffer.USAGE_CPU_READ_RARELY, buffer.getUsage());

        buffer = HardwareBuffer.create(2, 4, HardwareBuffer.RGBX_8888, 1,
                HardwareBuffer.USAGE_CPU_READ_RARELY);
        assertEquals(HardwareBuffer.RGBX_8888, buffer.getFormat());
        buffer = HardwareBuffer.create(2, 4, HardwareBuffer.RGB_888, 1,
                HardwareBuffer.USAGE_CPU_READ_RARELY);
        assertEquals(HardwareBuffer.RGB_888, buffer.getFormat());
        buffer = HardwareBuffer.create(2, 4, HardwareBuffer.RGB_565, 1,
                HardwareBuffer.USAGE_CPU_READ_RARELY);
        assertEquals(HardwareBuffer.RGB_565, buffer.getFormat());
        buffer = HardwareBuffer.create(2, 1, HardwareBuffer.BLOB, 1,
                HardwareBuffer.USAGE_CPU_READ_RARELY);
        assertEquals(HardwareBuffer.BLOB, buffer.getFormat());
    }

    @Test
    public void testCreateFailsWithInvalidArguments() {
        HardwareBuffer buffer = null;
        assertEquals(null, buffer);
        try {
            buffer = HardwareBuffer.create(0, 4, HardwareBuffer.RGB_888, 1,
                    HardwareBuffer.USAGE_CPU_READ_RARELY);
        } catch (IllegalArgumentException e) {}
        assertEquals(null, buffer);
        try {
            buffer = HardwareBuffer.create(2, 0, HardwareBuffer.RGB_888, 1,
                    HardwareBuffer.USAGE_CPU_READ_RARELY);
        } catch (IllegalArgumentException e) {}
        assertEquals(null, buffer);
        try {
            buffer = HardwareBuffer.create(2, 4, 0, 1,
                    HardwareBuffer.USAGE_CPU_READ_RARELY);
        } catch (IllegalArgumentException e) {}
        assertEquals(null, buffer);
        try {
            buffer = HardwareBuffer.create(2, 4, HardwareBuffer.RGB_888, -1,
                    HardwareBuffer.USAGE_CPU_READ_RARELY);
        } catch (IllegalArgumentException e) {}
        assertEquals(null, buffer);
        try {
            buffer = HardwareBuffer.create(2, 2, HardwareBuffer.BLOB, 1,
                    HardwareBuffer.USAGE_CPU_READ_RARELY);
        } catch (IllegalArgumentException e) {}
        assertEquals(null, buffer);

    }

    @Test
    public void testCreateFromNativeObject() {
        HardwareBuffer buffer = nativeCreateHardwareBuffer(2, 4, HardwareBuffer.RGBA_8888, 1,
                    HardwareBuffer.USAGE_CPU_READ_RARELY);
        assertTrue(buffer != null);
        assertEquals(2, buffer.getWidth());
        assertEquals(4, buffer.getHeight());
        assertEquals(HardwareBuffer.RGBA_8888, buffer.getFormat());
        assertEquals(1, buffer.getLayers());
        assertEquals(HardwareBuffer.USAGE_CPU_READ_RARELY, buffer.getUsage());
        nativeReleaseHardwareBuffer(buffer);
    }

    @Test
    public void testIsSupported() {
        assertTrue(HardwareBuffer.isSupported(1, 1, HardwareBuffer.RGBA_8888,
                1, HardwareBuffer.USAGE_CPU_READ_RARELY));
        assertTrue(HardwareBuffer.isSupported(1, 1, HardwareBuffer.RGBA_8888,
                1, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE));
        assertFalse(HardwareBuffer.isSupported(1, 1, HardwareBuffer.BLOB,
                1, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE | HardwareBuffer.USAGE_GPU_COLOR_OUTPUT));
    }
}
