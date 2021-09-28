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

package com.android.compatibility.common.tradefed.targetprep;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link VtsDeviceInfoCollector}. */
@RunWith(JUnit4.class)
public class VtsDeviceInfoCollectorTest {
    // Device property values.
    private static final String VENDOR_BUILD_VERSION_INCREMENTAL = "1000000";
    private static final String VENDOR_FINGERPRINT =
            "Android/aosp_sailfish/sailfish:8.0.0/OC/1000000:userdebug/test-keys";
    private static final String ODM_DEVICE = "sailfish";
    private static final String HARDWARE_SKU = "sku";
    private static final String ODM_BUILD_VERSION_INCREMENTAL = "4311111";
    private static final String ODM_FINGERPRINT =
            "Android/aosp_sailfish/sailfish:8.0.0/OC/4311111:userdebug/test-keys";
    private static final String ODM_FINGERPRINT_WITH_SKU =
            "Android/aosp_sailfish/sailfish_sku:8.0.0/OC/4311111:userdebug/test-keys";

    @Mock private IBuildInfo mMockBuildInfo;
    @Mock private ITestDevice mMockTestDevice;
    private TestInformation mTestInfo;
    private Map<String, String> mProperties;

    @Before
    public void setUp() throws DeviceNotAvailableException {
        MockitoAnnotations.initMocks(this);

        Mockito.when(mMockTestDevice.getProperty(Mockito.anyString()))
                .then(
                        new Answer<String>() {
                            @Override
                            public String answer(InvocationOnMock invocation) {
                                String name = invocation.getArgument(0);
                                return mProperties.getOrDefault(name, "");
                            }
                        });

        mProperties = new HashMap<String, String>();
        mProperties.put("ro.vendor.build.fingerprint", VENDOR_FINGERPRINT);
        mProperties.put("ro.vendor.build.version.incremental", VENDOR_BUILD_VERSION_INCREMENTAL);
        IInvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        context.addAllocatedDevice("device", mMockTestDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    /** Test reporting vendor properties if ODM fingerprint is empty. */
    @Test
    public void testVendorProperties()
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        new VtsDeviceInfoCollector().setUp(mTestInfo);

        Mockito.verify(mMockBuildInfo)
                .addBuildAttribute("cts:build_fingerprint", VENDOR_FINGERPRINT);
        Mockito.verify(mMockBuildInfo)
                .addBuildAttribute(
                        "cts:build_version_incremental", VENDOR_BUILD_VERSION_INCREMENTAL);
    }

    /**
     * Test reporting ODM properties.
     *
     * <p>If ODM fingerprint is not empty, ODM build version is reported.
     *
     * <p>If ODM fingerprint contains SKU, the SKU is not reported.
     */
    @Test
    public void testOdmProperties()
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        mProperties.put("ro.odm.build.fingerprint", ODM_FINGERPRINT_WITH_SKU);
        mProperties.put("ro.odm.build.version.incremental", ODM_BUILD_VERSION_INCREMENTAL);
        mProperties.put("ro.product.odm.device", ODM_DEVICE);
        mProperties.put("ro.boot.product.hardware.sku", HARDWARE_SKU);

        new VtsDeviceInfoCollector().setUp(mTestInfo);

        Mockito.verify(mMockBuildInfo).addBuildAttribute("cts:build_fingerprint", ODM_FINGERPRINT);
        Mockito.verify(mMockBuildInfo)
                .addBuildAttribute("cts:build_version_incremental", ODM_BUILD_VERSION_INCREMENTAL);
    }
}
