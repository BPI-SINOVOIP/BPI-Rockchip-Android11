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
package com.android.tradefed.suite.checker;

import java.util.Arrays;
import java.util.Map;
import java.util.HashMap;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.UserInfo;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;


/** Unit tests for {@link UserChecker} */
@RunWith(JUnit4.class)
public class UserCheckerTest {
    @Test
    public void testNoWarningsIsSuccess() throws Exception {
        UserChecker checker = new UserChecker();

        ITestDevice preDevice =
                mockDeviceUserState(/* currentUser=  */ 0, /* userIds= */ new Integer[] {0});
        assertEquals(CheckStatus.SUCCESS, checker.preExecutionCheck(preDevice).getStatus());

        ITestDevice postDevice =
                mockDeviceUserState(/* currentUser=  */ 0, /* userIds= */ new Integer[] {0});
        assertEquals(CheckStatus.SUCCESS, checker.postExecutionCheck(postDevice).getStatus());
    }

    @Test
    public void testSwitchIsSuccess() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "system");
        ITestDevice preDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 10,
                        /* userIds=        */ new Integer[] {0, 10},
                        /* flags=        */ new Integer[] {0, 0},
                        /* isRunning= */ new Boolean[] {true, true});
        when(preDevice.switchUser(0)).thenReturn(true);

        assertEquals(CheckStatus.SUCCESS, checker.preExecutionCheck(preDevice).getStatus());
        verify(preDevice, never()).createUser(any(), anyBoolean(), anyBoolean());
        verify(preDevice, times(1)).switchUser(0);

        ITestDevice postDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 0,
                        /* userIds=        */ new Integer[] {0, 10},
                        /* flags=        */ new Integer[] {0, 0},
                        /* isRunning= */ new Boolean[] {true, false});
        assertEquals(CheckStatus.SUCCESS, checker.postExecutionCheck(postDevice).getStatus());
        verify(postDevice, never()).createUser(any(), anyBoolean(), anyBoolean());
        verify(postDevice, never()).switchUser(anyInt());
    }

    @Test
    public void testCreateIsSuccess() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "secondary");
        ITestDevice preDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 0,
                        /* userIds=        */ new Integer[] {0},
                        /* flags=        */ new Integer[] {0},
                        /* isRunning= */ new Boolean[] {true});
        when(preDevice.createUser("Tfsecondary", false, false)).thenReturn(10);
        when(preDevice.switchUser(10)).thenReturn(true);

        assertEquals(CheckStatus.SUCCESS, checker.preExecutionCheck(preDevice).getStatus());
        verify(preDevice, times(1)).createUser("Tfsecondary", false, false);
        verify(preDevice, times(1)).switchUser(10);

        ITestDevice postDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 10,
                        /* userIds=        */ new Integer[] {0, 10},
                        /* flags=        */ new Integer[] {0, 0},
                        /* isRunning= */ new Boolean[] {true, true});
        assertEquals(CheckStatus.SUCCESS, checker.postExecutionCheck(postDevice).getStatus());
        verify(postDevice, never()).removeUser(anyInt());
        verify(postDevice, never()).switchUser(anyInt());
    }

    @Test
    public void testCreateCleanup() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "secondary");
        mOptionSetter.setOptionValue("user-cleanup", "true");
        ITestDevice preDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 0,
                        /* userIds=        */ new Integer[] {0},
                        /* flags=        */ new Integer[] {0},
                        /* isRunning= */ new Boolean[] {true});
        when(preDevice.createUser("Tfsecondary", false, false)).thenReturn(10);
        when(preDevice.switchUser(10)).thenReturn(true);

        assertEquals(CheckStatus.SUCCESS, checker.preExecutionCheck(preDevice).getStatus());
        verify(preDevice, times(1)).createUser("Tfsecondary", false, false);
        verify(preDevice, times(1)).switchUser(10);

        ITestDevice postDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 10,
                        /* userIds=        */ new Integer[] {0, 10},
                        /* flags=        */ new Integer[] {0, 0},
                        /* isRunning= */ new Boolean[] {true, true});
        when(postDevice.switchUser(0)).thenReturn(true);
        when(postDevice.removeUser(10)).thenReturn(true);
        assertEquals(CheckStatus.SUCCESS, checker.postExecutionCheck(postDevice).getStatus());
        verify(postDevice, times(1)).switchUser(0);
        verify(postDevice, times(1)).removeUser(10);
    }

    @Test
    public void testCreateCleanup_cleanupFail() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "secondary");
        mOptionSetter.setOptionValue("user-cleanup", "true");
        ITestDevice preDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 0,
                        /* userIds=        */ new Integer[] {0},
                        /* flags=        */ new Integer[] {0},
                        /* isRunning= */ new Boolean[] {true});
        when(preDevice.createUser("Tfsecondary", false, false)).thenReturn(10);
        when(preDevice.switchUser(10)).thenReturn(true);

        assertEquals(CheckStatus.SUCCESS, checker.preExecutionCheck(preDevice).getStatus());
        verify(preDevice, times(1)).createUser("Tfsecondary", false, false);
        verify(preDevice, times(1)).switchUser(10);

        ITestDevice postDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 10,
                        /* userIds=        */ new Integer[] {0, 10},
                        /* flags=        */ new Integer[] {0, 0},
                        /* isRunning= */ new Boolean[] {true, true});
        when(postDevice.switchUser(0)).thenReturn(false);
        when(postDevice.removeUser(10)).thenReturn(false);
        StatusCheckerResult result = checker.postExecutionCheck(postDevice);
        verify(postDevice, times(1)).switchUser(0);
        verify(postDevice, times(1)).removeUser(10);
        assertEquals(CheckStatus.FAILED, result.getStatus());
        assertTrue(
                result.getErrorMessage()
                        .contains("Failed to switch back to previous current user 0"));
        assertTrue(result.getErrorMessage().contains("Failed to remove new user 10"));
    }

    @Test
    /** Returns FAILED in the precessense of errors */
    public void testAllErrorsIsFailed() throws Exception {
        UserChecker checker = new UserChecker();

        ITestDevice preDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 10,
                        /* userIds=        */ new Integer[] {0, 10, 11},
                        /* flags=        */ new Integer[] {0, 0, 0},
                        /* isRunning= */ new Boolean[] {true, true, false});
        assertEquals(CheckStatus.SUCCESS, checker.preExecutionCheck(preDevice).getStatus());

        // User12 created, User11 deleted, User10 stopped, currentUser changed
        ITestDevice postDevice =
                mockDeviceUserState(
                        /* currentUser=  */ 0,
                        /* userIds= */ new Integer[] {0, 10, 12},
                        /* flags=        */ new Integer[] {0, 0, 0},
                        /* isRunning= */ new Boolean[] {true, false, false});
        assertEquals(CheckStatus.FAILED, checker.postExecutionCheck(postDevice).getStatus());
    }

    @Test
    public void testSwitchToSystem() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "system");
        ITestDevice device =
                mockDeviceUserState(/* currentUser=  */ 10, /* userIds= */ new Integer[] {0, 10});

        when(device.switchUser(0)).thenReturn(true);

        StatusCheckerResult result = checker.preExecutionCheck(device);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
        verify(device, never()).createUser(any(), anyBoolean(), anyBoolean());
        verify(device, times(1)).switchUser(0);
    }

    @Test
    public void testSwitchToSecondary() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "secondary");
        ITestDevice device =
                mockDeviceUserState(/* currentUser=  */ 0, /* userIds= */ new Integer[] {0, 10});

        when(device.switchUser(10)).thenReturn(true);

        StatusCheckerResult result = checker.preExecutionCheck(device);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
        verify(device, never()).createUser(any(), anyBoolean(), anyBoolean());
        verify(device, times(1)).switchUser(10);
    }

    @Test
    public void testSwitchToSecondary_fail() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "secondary");
        ITestDevice device =
                mockDeviceUserState(/* currentUser=  */ 0, /* userIds= */ new Integer[] {0, 10});

        when(device.switchUser(10)).thenReturn(false);

        StatusCheckerResult result = checker.preExecutionCheck(device);
        assertEquals(CheckStatus.FAILED, result.getStatus());
        verify(device, never()).createUser(any(), anyBoolean(), anyBoolean());
        verify(device, times(1)).switchUser(10);
    }

    @Test
    public void testSwitchToGuest() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "guest");
        ITestDevice device =
                mockDeviceUserState(
                        /* currentUser=  */ 0,
                        /* userIds= */ new Integer[] {0, 10},
                        /* flags=        */ new Integer[] {0, UserInfo.FLAG_GUEST},
                        /* isRunning= */ new Boolean[] {true, false});

        when(device.switchUser(10)).thenReturn(true);

        StatusCheckerResult result = checker.preExecutionCheck(device);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
        verify(device, never()).createUser(any(), anyBoolean(), anyBoolean());
        verify(device, times(1)).switchUser(10);
    }

    @Test
    public void testCreateSecondary() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "secondary");
        ITestDevice device =
                mockDeviceUserState(/* currentUser=  */ 0, /* userIds= */ new Integer[] {0});

        when(device.createUser("Tfsecondary", false, false)).thenReturn(10);
        when(device.switchUser(10)).thenReturn(true);

        StatusCheckerResult result = checker.preExecutionCheck(device);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
        verify(device, times(1)).createUser("Tfsecondary", false, false);
        verify(device, times(1)).switchUser(10);
    }

    @Test
    public void testCreateGuest() throws Exception {
        UserChecker checker = new UserChecker();
        OptionSetter mOptionSetter = new OptionSetter(checker);
        mOptionSetter.setOptionValue("user-type", "guest");
        ITestDevice device =
                mockDeviceUserState(/* currentUser=  */ 0, /* userIds= */ new Integer[] {0});

        when(device.createUser("Tfguest", /* guest= */ true, /* ephemeral= */ false))
                .thenReturn(10);
        when(device.switchUser(10)).thenReturn(true);

        StatusCheckerResult result = checker.preExecutionCheck(device);
        assertEquals(CheckStatus.SUCCESS, result.getStatus());
        verify(device, times(1)).createUser("Tfguest", /* guest= */ true, /* ephemeral= */ false);
        verify(device, times(1)).switchUser(10);
    }

    // // TEST HELPERS

    /** Return a device with the user state calls mocked. */
    private ITestDevice mockDeviceUserState(int currentUser, Integer[] userIds) throws Exception {
        Integer[] flags = new Integer[userIds.length];
        Arrays.fill(flags, 0);

        Boolean[] isRunning = new Boolean[userIds.length];
        Arrays.fill(isRunning, false);
        isRunning[0] = true;

        return mockDeviceUserState(currentUser, userIds, flags, isRunning);
    }

    /** Return a device with the user state calls mocked. */
    private ITestDevice mockDeviceUserState(
            int currentUser, Integer[] userIds, Integer[] flags, Boolean[] isRunning)
            throws Exception {
        ITestDevice device = mock(ITestDevice.class);

        when(device.getCurrentUser()).thenReturn(currentUser);
        mockListUsersInfo(device, userIds, flags, isRunning);

        return device;
    }

    private void mockListUsersInfo(
            ITestDevice device, Integer[] userIds, Integer[] flags, Boolean[] isRunning)
            throws Exception {
        Map<Integer, UserInfo> result = new HashMap<>();
        for (int i = 0; i < userIds.length; i++) {
            int userId = userIds[i];
            result.put(
                    userId,
                    new UserInfo(
                            /* userId= */ userId,
                            /* userName= */ "usr" + userId,
                            /* flag= */ flags[i],
                            /* isRunning= */ isRunning[i]));
        }
        when(device.getUserInfos()).thenReturn(result);
    }
}
