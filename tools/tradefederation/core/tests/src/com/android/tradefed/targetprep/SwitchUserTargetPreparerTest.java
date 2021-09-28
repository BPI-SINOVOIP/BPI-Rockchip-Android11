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

package com.android.tradefed.targetprep;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.UserInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Map;
import java.util.HashMap;

/** Unit tests for {@link SwitchUserTargetPreparer}. */
@RunWith(JUnit4.class)
public class SwitchUserTargetPreparerTest {

    @Mock private ITestDevice mMockDevice;

    private SwitchUserTargetPreparer mSwitchUserTargetPreparer;
    private OptionSetter mOptionSetter;

    @Before
    public void setUp() throws ConfigurationException {
        MockitoAnnotations.initMocks(this);
        mSwitchUserTargetPreparer = new SwitchUserTargetPreparer();
        mOptionSetter = new OptionSetter(mSwitchUserTargetPreparer);
    }

    @Test
    public void testSetUpRunAsPrimary_ifAlreadyInPrimary_noSwitch()
            throws DeviceNotAvailableException, TargetSetupError, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "primary");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(11);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 11},
                /* flags= */ new Integer[] {0, UserInfo.FLAG_PRIMARY});


        // act
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);

        // assert
        verify(mMockDevice, never()).switchUser(anyInt());
    }

    @Test
    public void testSetUpRunAsSystem_ifAlreadyInSystem_noSwitch()
            throws DeviceNotAvailableException, TargetSetupError, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "system");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(0);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 11},
                /* flags= */ new Integer[] {0, UserInfo.FLAG_PRIMARY});

        // act
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);

        // assert
        verify(mMockDevice, never()).switchUser(0);
    }

    @Test
    public void testSetUpRunAsPrimary_ifNotInPrimary_switchToPrimary()
            throws DeviceNotAvailableException, TargetSetupError, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "primary");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(11);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 10, 11},
                /* flags= */ new Integer[] {0, UserInfo.FLAG_PRIMARY, 0});
        when(mMockDevice.switchUser(10)).thenReturn(true);

        // act
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);

        // assert
        verify(mMockDevice, times(1)).switchUser(10);
    }

    @Test
    public void testSetUpRunAsGuest_ifNotInGuest_switchToGuest()
            throws DeviceNotAvailableException, TargetSetupError, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "guest");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(11);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 10, 11},
                /* flags= */ new Integer[] {0, UserInfo.FLAG_GUEST, 0});
        when(mMockDevice.switchUser(10)).thenReturn(true);

        // act
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);

        // assert
        verify(mMockDevice, times(1)).switchUser(10);
    }

    @Test
    public void testSetUpRunAsSystem_ifNotInSystem_switchToSystem()
            throws DeviceNotAvailableException, TargetSetupError, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "system");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(10);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 10},
                /* flags= */ new Integer[] {0, 0});
        when(mMockDevice.switchUser(0)).thenReturn(true);

        // act
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);

        // assert
        verify(mMockDevice, times(1)).switchUser(0);
    }

    @Test
    public void testTearDown_ifStartedInSecondary_switchesBackToSecondary()
            throws DeviceNotAvailableException, TargetSetupError, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "system");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(10);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 10},
                /* flags= */ new Integer[] {0, 0});
        when(mMockDevice.switchUser(0)).thenReturn(true);
        when(mMockDevice.switchUser(10)).thenReturn(true);

        // first switches to primary
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);
        verify(mMockDevice, times(1)).switchUser(0);

        // then switches back to secondary
        mSwitchUserTargetPreparer.tearDown(mMockDevice, /* buildInfo= */ null, null);
        verify(mMockDevice, times(1)).switchUser(10);

    }

    @Test
    public void testSetUp_ifNoSwitchToSpecified_noUserSwitch()
            throws DeviceNotAvailableException, TargetSetupError {
        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(10);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 10},
                /* flags= */ new Integer[] {0, 0});

        // act
        mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);

        // assert
        verify(mMockDevice, never()).switchUser(anyInt());
    }

    @Test
    public void testSetUp_ifSwitchFails_throwsTargetSetupError()
            throws DeviceNotAvailableException, ConfigurationException {
        mOptionSetter.setOptionValue("user-type", "primary");

        // setup
        when(mMockDevice.getCurrentUser()).thenReturn(10);
        mockListUsersInfo(
                mMockDevice,
                /* userIds= */ new Integer[] {0, 10},
                /* flags= */ new Integer[] {UserInfo.FLAG_PRIMARY, 0});
        when(mMockDevice.switchUser(0)).thenReturn(false);

        // act
        try {
            mSwitchUserTargetPreparer.setUp(mMockDevice, /* buildInfo= */ null);
            fail("Should have thrown TargetSetupError exception.");
        } catch (TargetSetupError e) {
            // do nothing
        }
    }

    private void mockListUsersInfo(ITestDevice device, Integer[] userIds, Integer[] flags)
            throws DeviceNotAvailableException {
        Map<Integer, UserInfo> result = new HashMap<>();
        for (int i = 0; i < userIds.length; i++) {
            int userId = userIds[i];
            result.put(
                    userId,
                    new UserInfo(
                            /* userId= */ userId,
                            /* userName= */ "usr" + userId,
                            /* flag= */ flags[i],
                            /* isRunning= */ false));
        }
        when(device.getUserInfos()).thenReturn(result);
    }
}
