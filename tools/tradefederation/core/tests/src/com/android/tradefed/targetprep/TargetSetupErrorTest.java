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
package com.android.tradefed.targetprep;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.SerializationUtil;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link TargetSetupError}. */
@RunWith(JUnit4.class)
public class TargetSetupErrorTest {

    @Test
    public void testOrigin() {
        TargetSetupError error =
                new TargetSetupError("reason", null, null, true, InfraErrorIdentifier.UNDETERMINED);
        assertEquals("com.android.tradefed.targetprep.TargetSetupErrorTest", error.getOrigin());
    }

    @Test
    public void testSerialization() throws Exception {
        TargetSetupError exception = new TargetSetupError("reason", new DeviceDescriptor());
        File s = SerializationUtil.serialize(exception);
        try {
            assertTrue(s.exists());
            assertTrue(s.isFile());
            Object o = SerializationUtil.deserialize(s, true);
            assertTrue(o instanceof TargetSetupError);
            assertEquals("reason [null null:null null]", ((TargetSetupError) o).getMessage());
        } finally {
            FileUtil.deleteFile(s);
        }
    }

    @Test
    public void testSerialization_withIDevice() throws Exception {
        DeviceDescriptor descriptor =
                new DeviceDescriptor(
                        "serial",
                        true,
                        DeviceAllocationState.Allocated,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel",
                        StubDevice.class.getName(),
                        "macAddress",
                        "simState",
                        "simOperator",
                        new StubDevice("serial"));
        TargetSetupError exception = new TargetSetupError("reason", descriptor);
        File s = SerializationUtil.serialize(exception);
        try {
            assertTrue(s.exists());
            assertTrue(s.isFile());
            Object o = SerializationUtil.deserialize(s, true);
            assertTrue(o instanceof TargetSetupError);
            assertEquals(
                    "reason [serial product:productVariant buildId]",
                    ((TargetSetupError) o).getMessage());
        } finally {
            FileUtil.deleteFile(s);
        }
    }
}
