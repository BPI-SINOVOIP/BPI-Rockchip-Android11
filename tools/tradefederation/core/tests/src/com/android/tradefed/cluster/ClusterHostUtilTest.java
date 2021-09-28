/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.tradefed.cluster;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.IDevice.DeviceState;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceManager;
import com.android.tradefed.device.DeviceManager.FastbootDevice;
import java.security.InvalidParameterException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.easymock.EasyMock;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link ClusterHostUtil}. */
@RunWith(JUnit4.class)
public class ClusterHostUtilTest {

    private static final String DEVICE_SERIAL = "serial";

    @Test
    public void testIsIpPort() {
        Assert.assertTrue(ClusterHostUtil.isIpPort("127.0.0.1:101"));
        Assert.assertTrue(ClusterHostUtil.isIpPort("127.0.0.1"));
        Assert.assertFalse(ClusterHostUtil.isIpPort(DEVICE_SERIAL));
        Assert.assertFalse(ClusterHostUtil.isIpPort("127.0.0.1:notaport"));
    }

    // Test a valid TF version
    @Test
    public void testToValidTfVersion() {
        String version = "12345";
        String actual = ClusterHostUtil.toValidTfVersion(version);
        Assert.assertEquals(version, actual);
    }

    // Test an empty TF version
    @Test
    public void testToValidTfVersionWithEmptyVersion() {
        String version = "";
        String actual = ClusterHostUtil.toValidTfVersion(version);
        Assert.assertEquals(ClusterHostUtil.DEFAULT_TF_VERSION, actual);
    }

    // Test a null TF version
    @Test
    public void testToValidTfVersionWithNullVersion() {
        String version = null;
        String actual = ClusterHostUtil.toValidTfVersion(version);
        Assert.assertEquals(ClusterHostUtil.DEFAULT_TF_VERSION, actual);
    }

    // Test an invalid TF version
    @Test
    public void testToValidTfVersionWithInvalidVersion() {
        String version = "1abcd2efg";
        String actual = ClusterHostUtil.toValidTfVersion(version);
        Assert.assertEquals(ClusterHostUtil.DEFAULT_TF_VERSION, actual);
    }

    // Test default run target if nothing is specified.
    @Test
    public void testGetDefaultRunTarget() {
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Assert.assertEquals(
                "product:productVariant", ClusterHostUtil.getRunTarget(device, null, null));
    }

    // Test default run target if nothing is specified, and product == product variant.
    @Test
    public void testGetDefaultRunTargetWithSameProductAndProductVariant() {
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "product",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Assert.assertEquals("product", ClusterHostUtil.getRunTarget(device, null, null));
    }

    // If a constant string run target pattern is set, always return said pattern.
    @Test
    public void testSimpleConstantRunTargetMatchPattern() {
        String format = "foo";
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Assert.assertEquals("foo", ClusterHostUtil.getRunTarget(device, format, null));
    }

    // Test run target pattern with a device tag map
    @Test
    public void testDeviceTagRunTargetMatchPattern_simple() {
        String format = "{TAG}";
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Map<String, String> deviceTag = new HashMap<>();
        deviceTag.put(DEVICE_SERIAL, "foo");
        Assert.assertEquals("foo", ClusterHostUtil.getRunTarget(device, format, deviceTag));
    }

    // Test run target pattern with a device tag map, but the device serial is not in map
    @Test
    public void testDeviceTagRunTargetMatchPattern_missingSerial() {
        String format = "foo{TAG}bar";
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Map<String, String> deviceTag = Collections.emptyMap();
        Assert.assertEquals("foobar", ClusterHostUtil.getRunTarget(device, format, deviceTag));
    }

    // Ensure that invalid run target pattern throws an exception.
    @Test
    public void testInvalidRunTargetMetachPattern() {
        String format = "foo-{INVALID PATTERN}";
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        try {
            ClusterHostUtil.getRunTarget(device, format, null);
            Assert.fail("Should have thrown an InvalidParameter exception.");
        } catch (InvalidParameterException e) {
            // expected.
        }
    }

    // Test all supported run target match patterns.
    @Test
    public void testSupportedRunTargetMatchPattern() {
        String format = "foo-{PRODUCT}-{PRODUCT_VARIANT}-{API_LEVEL}-{DEVICE_PROP:bar}";
        IDevice mockIDevice = EasyMock.createMock(IDevice.class);
        EasyMock.expect(mockIDevice.getProperty("bar")).andReturn("zzz");
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceState.ONLINE,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel",
                        "",
                        "",
                        "",
                        "",
                        mockIDevice);
        EasyMock.replay(mockIDevice);
        Assert.assertEquals(
                "foo-product-productVariant-sdkVersion-zzz",
                ClusterHostUtil.getRunTarget(device, format, null));
        EasyMock.verify(mockIDevice);
    }

    // Test all supported run target match patterns with unknown property.
    @Test
    public void testSupportedRunTargetMatchPattern_unknownProperty() {
        String format = "foo-{PRODUCT}-{PRODUCT_VARIANT}-{API_LEVEL}";
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        DeviceManager.UNKNOWN_DISPLAY_STRING,
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Assert.assertEquals(
                DeviceManager.UNKNOWN_DISPLAY_STRING,
                ClusterHostUtil.getRunTarget(device, format, null));
    }

    /**
     * Test PRODUCT_OR_DEVICE_CLASS that can return both product type and the stub device class.
     * This allows an host to report both physical and stub devices.
     */
    @Test
    public void testSupportedRunTargetMatchPattern_productAndStub() {
        String format = "{PRODUCT_OR_DEVICE_CLASS}";
        // with non-stub device we use the requested product
        DeviceDescriptor device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        false,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel");
        Assert.assertEquals("product", ClusterHostUtil.getRunTarget(device, format, null));
        // with a stub device we use the device class
        device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        true,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel",
                        "deviceClass",
                        "macAddress",
                        "simState",
                        "simOperator");
        Assert.assertEquals("deviceClass", ClusterHostUtil.getRunTarget(device, format, null));
        // with a fastboot device we use the product
        device =
                new DeviceDescriptor(
                        DEVICE_SERIAL,
                        true,
                        DeviceAllocationState.Available,
                        "product",
                        "productVariant",
                        "sdkVersion",
                        "buildId",
                        "batteryLevel",
                        FastbootDevice.class.getSimpleName(),
                        "macAddress",
                        "simState",
                        "simOperator");
        Assert.assertEquals("product", ClusterHostUtil.getRunTarget(device, format, null));
    }
}
