/*
 * Copyright 2020 The Android Open Source Project
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

package android.hardware.hdmi.cts;

import static com.google.common.truth.Truth.assertThat;

import android.hardware.hdmi.HdmiControlManager;
import android.hardware.hdmi.HdmiControlServiceWrapper;
import android.hardware.hdmi.HdmiPortInfo;
import android.hardware.hdmi.HdmiSwitchClient;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.android.compatibility.common.util.AdoptShellPermissionsRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class HdmiSwitchClientTest {

    private HdmiControlManager mManager;
    private HdmiSwitchClient mHdmiSwitchClient;
    private HdmiControlServiceWrapper mService;
    private final List<HdmiPortInfo> mExpectedInfo = new ArrayList();

    @Rule
    public final AdoptShellPermissionsRule shellPermRule = new AdoptShellPermissionsRule();

    @Before
    public void setUp() {
        mService = new HdmiControlServiceWrapper();
        int[] types = {HdmiControlServiceWrapper.DEVICE_PURE_CEC_SWITCH};
        mService.setDeviceTypes(types);

        mManager = mService.createHdmiControlManager();
        mHdmiSwitchClient = mManager.getSwitchClient();
        assertThat(mManager).isNotNull();
        assertThat(mHdmiSwitchClient).isNotNull();
    }

    @After
    public void tearDown() {
        mExpectedInfo.clear();
    }

    @Test
    public void testGetPortInfo() {
        final int id = 0;
        final int address = 0x1000;
        final boolean cec = true;
        final boolean mhl = false;
        final boolean arc = true;
        final HdmiPortInfo info =
                new HdmiPortInfo(id, HdmiPortInfo.PORT_INPUT, address, cec, mhl, arc);
        mExpectedInfo.add(info);
        mService.setPortInfo(mExpectedInfo);

        final List<HdmiPortInfo> portInfo = mHdmiSwitchClient.getPortInfo();
        assertThat(portInfo).isEqualTo(mExpectedInfo);
    }
}
