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

package com.android.tradefed.targetprep.multi;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anySet;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.util.Sl4aBluetoothUtil;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothProfile;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothAccessLevel;
import com.android.tradefed.util.Sl4aBluetoothUtil.BluetoothPriorityLevel;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link PairingMultiTargetPreparer}. */
@RunWith(JUnit4.class)
public class PairingMultiTargetPreparerTest {

    private static final String PRIMARY_DEVICE_NAME = "primary";
    private static final String SECONDARY_DEVICE_NAME = "secondary";

    @Mock private IInvocationContext mContext;
    @Mock private ITestDevice mPrimary;
    @Mock private IBuildInfo mPrimaryBuild;
    @Mock private ITestDevice mSecondary;
    @Mock private IBuildInfo mSecondaryBuild;
    @Mock private Sl4aBluetoothUtil mUtil;
    private PairingMultiTargetPreparer mPreparer;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        when(mPrimary.getProductType()).thenReturn(PRIMARY_DEVICE_NAME);
        when(mSecondary.getProductType()).thenReturn(SECONDARY_DEVICE_NAME);
        Map<ITestDevice, IBuildInfo> deviceBuildMap = new HashMap<>();
        deviceBuildMap.put(mPrimary, mPrimaryBuild);
        deviceBuildMap.put(mSecondary, mSecondaryBuild);
        when(mContext.getDeviceBuildMap()).thenReturn(deviceBuildMap);

        when(mUtil.enable(any(ITestDevice.class))).thenReturn(true);
        when(mUtil.pair(any(ITestDevice.class), any(ITestDevice.class))).thenReturn(true);
        when(mUtil.changeProfileAccessPermission(
                        any(ITestDevice.class),
                        any(ITestDevice.class),
                        any(BluetoothProfile.class),
                        any(BluetoothAccessLevel.class)))
                .thenReturn(true);
        when(mUtil.setProfilePriority(
                        any(ITestDevice.class),
                        any(ITestDevice.class),
                        anySet(),
                        any(BluetoothPriorityLevel.class)))
                .thenReturn(true);

        mPreparer = new PairingMultiTargetPreparer();
        mPreparer.setBluetoothUtil(mUtil);

        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("bt-connection-primary-device", PRIMARY_DEVICE_NAME);
    }

    @Test
    public void testPairWithConnection() throws Exception {
        setBluetoothProfiles();
        when(mUtil.connect(any(ITestDevice.class), any(ITestDevice.class), anySet()))
                .thenReturn(true);
        mPreparer.setUp(mContext);
        verify(mUtil).enable(mPrimary);
        verify(mUtil).enable(mSecondary);
        verify(mUtil).pair(mPrimary, mSecondary);
        verify(mUtil)
                .connect(
                        eq(mPrimary),
                        eq(mSecondary),
                        argThat((Set<BluetoothProfile> profiles) -> profiles.size() == 3));
        verify(mUtil).stopSl4a();
    }

    @Test
    public void testPairWithoutConnection() throws Exception {
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("with-connection", "false");
        setBluetoothProfiles();
        mPreparer.setUp(mContext);
        verify(mUtil).enable(mPrimary);
        verify(mUtil).enable(mSecondary);
        verify(mUtil).pair(mPrimary, mSecondary);
        verify(mUtil, never()).connect(any(ITestDevice.class), any(ITestDevice.class), anySet());
        verify(mUtil).stopSl4a();
    }

    @Test
    public void testPairWithEmptyProfiles() throws Exception {
        mPreparer.setUp(mContext);
        verify(mUtil).enable(mPrimary);
        verify(mUtil).enable(mSecondary);
        verify(mUtil).pair(mPrimary, mSecondary);
        verify(mUtil, never()).connect(any(ITestDevice.class), any(ITestDevice.class), anySet());
        verify(mUtil).stopSl4a();
    }

    private void setBluetoothProfiles() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mPreparer);
        setter.setOptionValue("bt-connection-primary-device", PRIMARY_DEVICE_NAME);
        setter.setOptionValue("bt-profile", BluetoothProfile.A2DP.name());
        setter.setOptionValue("bt-profile", BluetoothProfile.HEADSET.name());
        setter.setOptionValue("bt-profile", BluetoothProfile.MAP.name());
    }
}
