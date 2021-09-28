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

package com.android.server.wm;

import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Display.FLAG_PRIVATE;
import static android.view.Display.FLAG_TRUSTED;
import static android.view.Display.INVALID_DISPLAY;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.mockitoSession;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.when;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;

import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.ActivityTaskManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.os.UserHandle;
import android.view.Display;
import android.view.SurfaceControl;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.server.AttributeCache;
import com.android.server.LocalServices;
import com.android.server.display.color.ColorDisplayService;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

import java.util.Arrays;

@RunWith(AndroidJUnit4.class)
public class CarLaunchParamsModifierTest {
    private static final int PASSENGER_DISPLAY_ID_10 = 10;
    private static final int PASSENGER_DISPLAY_ID_11 = 11;
    private static final int VIRTUAL_DISPLAY_ID_2 = 2;

    private MockitoSession mMockingSession;

    private CarLaunchParamsModifier mModifier;

    @Mock
    private Context mContext;
    @Mock
    private DisplayManager mDisplayManager;
    @Mock
    private ActivityTaskManagerService mActivityTaskManagerService;
    @Mock
    private ActivityStackSupervisor mActivityStackSupervisor;
    @Mock
    private RecentTasks mRecentTasks;
    @Mock
    private WindowManagerService mWindowManagerService;
    @Mock
    private ColorDisplayService.ColorDisplayServiceInternal mColorDisplayServiceInternal;
    @Mock
    private RootWindowContainer mRootWindowContainer;
    @Mock
    private LaunchParamsController mLaunchParamsController;

    @Mock
    private Display mDisplay0ForDriver;
    @Mock
    private TaskDisplayArea mDisplayArea0ForDriver;
    @Mock
    private Display mDisplay1Private;
    @Mock
    private TaskDisplayArea mDisplayArea1Private;
    @Mock
    private Display mDisplay10ForPassenger;
    @Mock
    private TaskDisplayArea mDisplayArea10ForPassenger;
    @Mock
    private Display mDisplay11ForPassenger;
    @Mock
    private TaskDisplayArea mDisplayArea11ForPassenger;
    @Mock
    private Display mDisplay2Virtual;
    @Mock
    private TaskDisplayArea mDisplayArea2Virtual;

    // All mocks from here before CarLaunchParamsModifier are arguments for
    // LaunchParamsModifier.onCalculate() call.
    @Mock
    private Task mTask;
    @Mock
    private ActivityInfo.WindowLayout mWindowLayout;
    @Mock
    private ActivityRecord mActivityRecordActivity;
    @Mock
    private ActivityRecord mActivityRecordSource;
    @Mock
    private ActivityOptions mActivityOptions;
    @Mock
    private LaunchParamsController.LaunchParams mCurrentParams;
    @Mock
    private LaunchParamsController.LaunchParams mOutParams;

    private void mockDisplay(Display display, TaskDisplayArea defaultTaskDisplayArea,
            int displayId, int flags, int type) {
        when(mDisplayManager.getDisplay(displayId)).thenReturn(display);
        when(display.getDisplayId()).thenReturn(displayId);
        when(display.getFlags()).thenReturn(flags);
        when(display.getType()).thenReturn(type);

        // Return the same id as the display for simplicity
        DisplayContent dc = mock(DisplayContent.class);
        defaultTaskDisplayArea.mDisplayContent = dc;
        when(mRootWindowContainer.getDisplayContentOrCreate(displayId)).thenReturn(dc);
        when(dc.getDisplay()).thenReturn(display);
        when(dc.getDefaultTaskDisplayArea()).thenReturn(defaultTaskDisplayArea);
        when(dc.isTrusted()).thenReturn((flags & FLAG_TRUSTED) == FLAG_TRUSTED);
    }

    @Before
    public void setUp() {
        mMockingSession = mockitoSession()
                .initMocks(this)
                .mockStatic(ActivityTaskManager.class)
                .strictness(Strictness.LENIENT)
                .startMocking();
        when(mContext.getSystemService(DisplayManager.class)).thenReturn(mDisplayManager);
        doReturn(mActivityTaskManagerService).when(() -> ActivityTaskManager.getService());
        mActivityTaskManagerService.mStackSupervisor = mActivityStackSupervisor;
        when(mActivityStackSupervisor.getLaunchParamsController()).thenReturn(
                mLaunchParamsController);
        mActivityTaskManagerService.mRootWindowContainer = mRootWindowContainer;
        mActivityTaskManagerService.mWindowManager = mWindowManagerService;
        when(mActivityTaskManagerService.getRecentTasks()).thenReturn(mRecentTasks);
        mWindowManagerService.mTransactionFactory = () -> new SurfaceControl.Transaction();
        AttributeCache.init(getInstrumentation().getTargetContext());
        LocalServices.addService(ColorDisplayService.ColorDisplayServiceInternal.class,
                mColorDisplayServiceInternal);
        when(mActivityOptions.getLaunchDisplayId()).thenReturn(INVALID_DISPLAY);
        mockDisplay(mDisplay0ForDriver, mDisplayArea0ForDriver, DEFAULT_DISPLAY,
                FLAG_TRUSTED, /* type= */ 0);
        mockDisplay(mDisplay10ForPassenger, mDisplayArea10ForPassenger, PASSENGER_DISPLAY_ID_10,
                FLAG_TRUSTED, /* type= */ 0);
        mockDisplay(mDisplay11ForPassenger, mDisplayArea11ForPassenger, PASSENGER_DISPLAY_ID_11,
                FLAG_TRUSTED, /* type= */ 0);
        mockDisplay(mDisplay1Private, mDisplayArea1Private, 1,
                FLAG_TRUSTED | FLAG_PRIVATE, /* type= */ 0);
        mockDisplay(mDisplay2Virtual, mDisplayArea2Virtual, VIRTUAL_DISPLAY_ID_2,
                FLAG_PRIVATE, /* type= */ 0);
        DisplayContent defaultDc = mRootWindowContainer.getDisplayContentOrCreate(DEFAULT_DISPLAY);
        when(mActivityRecordSource.getDisplayContent()).thenReturn(defaultDc);

        mModifier = new CarLaunchParamsModifier(mContext);
        mModifier.init();
    }

    @After
    public void tearDown() {
        LocalServices.removeServiceForTest(ColorDisplayService.ColorDisplayServiceInternal.class);
        mMockingSession.finishMocking();
    }

    private void assertDisplayIsAllowed(@UserIdInt int userId, Display display) {
        mTask.mUserId = userId;
        mCurrentParams.mPreferredTaskDisplayArea = mModifier
                .getDefaultTaskDisplayAreaOnDisplay(display.getDisplayId());
        assertThat(mModifier.onCalculate(mTask, mWindowLayout, mActivityRecordActivity,
                mActivityRecordSource, mActivityOptions, 0, mCurrentParams, mOutParams))
                .isEqualTo(LaunchParamsController.LaunchParamsModifier.RESULT_SKIP);
    }

    private void assertDisplayIsReassigned(@UserIdInt int userId, Display displayRequested,
            Display displayAssigned) {
        assertThat(displayRequested.getDisplayId()).isNotEqualTo(displayAssigned.getDisplayId());
        mTask.mUserId = userId;
        TaskDisplayArea requestedTaskDisplayArea = mModifier
                .getDefaultTaskDisplayAreaOnDisplay(displayRequested.getDisplayId());
        TaskDisplayArea assignedTaskDisplayArea = mModifier
                .getDefaultTaskDisplayAreaOnDisplay(displayAssigned.getDisplayId());
        mCurrentParams.mPreferredTaskDisplayArea = requestedTaskDisplayArea;
        assertThat(mModifier.onCalculate(mTask, mWindowLayout, mActivityRecordActivity,
                mActivityRecordSource, mActivityOptions, 0, mCurrentParams, mOutParams))
                .isEqualTo(LaunchParamsController.LaunchParamsModifier.RESULT_DONE);
        assertThat(mOutParams.mPreferredTaskDisplayArea).isEqualTo(assignedTaskDisplayArea);
    }

    private void assertDisplayIsAssigned(
            @UserIdInt int userId, TaskDisplayArea expectedDisplayArea) {
        if (mTask != null) {
            mTask.mUserId = userId;
        }
        mCurrentParams.mPreferredTaskDisplayArea = null;
        assertThat(mModifier.onCalculate(mTask, mWindowLayout, mActivityRecordActivity,
                mActivityRecordSource, mActivityOptions, 0, mCurrentParams, mOutParams))
                .isEqualTo(LaunchParamsController.LaunchParamsModifier.RESULT_DONE);
        assertThat(mOutParams.mPreferredTaskDisplayArea).isEqualTo(expectedDisplayArea);
    }

    private void assertNoDisplayIsAssigned(@UserIdInt int userId) {
        mTask.mUserId = userId;
        mCurrentParams.mPreferredTaskDisplayArea = null;
        assertThat(mModifier.onCalculate(mTask, mWindowLayout, mActivityRecordActivity,
                mActivityRecordSource, mActivityOptions, 0, mCurrentParams, mOutParams))
                .isEqualTo(LaunchParamsController.LaunchParamsModifier.RESULT_SKIP);
        assertThat(mOutParams.mPreferredTaskDisplayArea).isNull();
    }

    private ActivityRecord buildActivityRecord(String packageName, String className) {
        ActivityInfo info = new ActivityInfo();
        info.packageName = packageName;
        info.name = className;
        info.applicationInfo = new ApplicationInfo();
        info.applicationInfo.packageName = packageName;
        Intent intent = new Intent();
        intent.setClassName(packageName, className);
        return new ActivityRecord(mActivityTaskManagerService, /* caller */null,
                /* launchedFromPid */ 0, /* launchedFromUid */ 0, /* launchedFromPackage */ null,
                /* launchedFromFeature */ null, intent, /* resolvedType */ null, info,
                new Configuration(), /* resultTo */ null, /* resultWho */ null, /* reqCode */ 0,
                /*componentSpecified*/ false, /* rootVoiceInteraction */ false,
                mActivityStackSupervisor, /* options */ null, /* sourceRecord */ null);
    }

    @Test
    public void testNoPolicySet() {
        final int randomUserId = 1000;
        // policy not set set, so do not apply any enforcement.
        assertDisplayIsAllowed(randomUserId, mDisplay0ForDriver);
        assertDisplayIsAllowed(randomUserId, mDisplay10ForPassenger);

        assertDisplayIsAllowed(UserHandle.USER_SYSTEM, mDisplay0ForDriver);
        assertDisplayIsAllowed(UserHandle.USER_SYSTEM, mDisplay10ForPassenger);

        assertDisplayIsAllowed(ActivityManager.getCurrentUser(), mDisplay0ForDriver);
        assertDisplayIsAllowed(ActivityManager.getCurrentUser(), mDisplay10ForPassenger);
    }

    private void assertAllDisplaysAllowedForUser(int userId) {
        assertDisplayIsAllowed(userId, mDisplay0ForDriver);
        assertDisplayIsAllowed(userId, mDisplay10ForPassenger);
        assertDisplayIsAllowed(userId, mDisplay11ForPassenger);
    }

    @Test
    public void testAllowAllForDriverDuringBoot() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay10ForPassenger.getDisplayId()});

        // USER_SYSTEM should be allowed always
        assertAllDisplaysAllowedForUser(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testAllowAllForDriverAfterUserSwitching() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay10ForPassenger.getDisplayId()});

        final int driver1 = 10;
        mModifier.handleCurrentUserSwitching(driver1);
        assertAllDisplaysAllowedForUser(driver1);
        assertAllDisplaysAllowedForUser(UserHandle.USER_SYSTEM);

        final int driver2 = 10;
        mModifier.handleCurrentUserSwitching(driver2);
        assertAllDisplaysAllowedForUser(driver2);
        assertAllDisplaysAllowedForUser(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testPassengerAllowed() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        final int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(passengerUserId,
                new int[]{mDisplay10ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId, mDisplay10ForPassenger);
    }

    @Test
    public void testPassengerChange() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        int passengerUserId1 = 100;
        mModifier.setDisplayWhitelistForUser(passengerUserId1,
                new int[]{mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId1, mDisplay11ForPassenger);

        int passengerUserId2 = 101;
        mModifier.setDisplayWhitelistForUser(passengerUserId2,
                new int[]{mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId2, mDisplay11ForPassenger);
        // 11 not allowed, so reassigned to the 1st passenger display
        assertDisplayIsReassigned(passengerUserId1, mDisplay11ForPassenger, mDisplay10ForPassenger);
    }

    @Test
    public void testPassengerNotAllowed() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        final int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(
                passengerUserId, new int[]{mDisplay10ForPassenger.getDisplayId()});

        assertDisplayIsReassigned(passengerUserId, mDisplay0ForDriver, mDisplay10ForPassenger);
        assertDisplayIsReassigned(passengerUserId, mDisplay11ForPassenger, mDisplay10ForPassenger);
    }

    @Test
    public void testPassengerNotAllowedAfterUserSwitch() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(
                passengerUserId, new int[]{mDisplay11ForPassenger.getDisplayId()});
        assertDisplayIsAllowed(passengerUserId, mDisplay11ForPassenger);

        mModifier.handleCurrentUserSwitching(2);

        assertDisplayIsReassigned(passengerUserId, mDisplay0ForDriver, mDisplay10ForPassenger);
        assertDisplayIsReassigned(passengerUserId, mDisplay11ForPassenger, mDisplay10ForPassenger);
    }

    @Test
    public void testPassengerNotAllowedAfterAssigningCurrentUser() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(
                passengerUserId, new int[]{mDisplay11ForPassenger.getDisplayId()});
        assertDisplayIsAllowed(passengerUserId, mDisplay11ForPassenger);

        mModifier.setDisplayWhitelistForUser(
                UserHandle.USER_SYSTEM, new int[]{mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsReassigned(passengerUserId, mDisplay0ForDriver, mDisplay10ForPassenger);
        assertDisplayIsReassigned(passengerUserId, mDisplay11ForPassenger, mDisplay10ForPassenger);
    }

    @Test
    public void testPassengerDisplayRemoved() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        final int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(passengerUserId,
                new int[]{mDisplay10ForPassenger.getDisplayId(),
                        mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId, mDisplay10ForPassenger);
        assertDisplayIsAllowed(passengerUserId, mDisplay11ForPassenger);

        mModifier.mDisplayListener.onDisplayRemoved(mDisplay11ForPassenger.getDisplayId());

        assertDisplayIsAllowed(passengerUserId, mDisplay10ForPassenger);
        assertDisplayIsReassigned(passengerUserId, mDisplay11ForPassenger, mDisplay10ForPassenger);
    }

    @Test
    public void testPassengerDisplayRemovedFromSetPassengerDisplays() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        final int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(passengerUserId,
                new int[]{mDisplay10ForPassenger.getDisplayId(),
                        mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId, mDisplay10ForPassenger);
        assertDisplayIsAllowed(passengerUserId, mDisplay11ForPassenger);

        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId, mDisplay10ForPassenger);
        assertDisplayIsReassigned(passengerUserId, mDisplay11ForPassenger, mDisplay10ForPassenger);
    }

    @Test
    public void testIgnorePrivateDisplay() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        final int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(passengerUserId,
                new int[]{mDisplay10ForPassenger.getDisplayId(),
                        mDisplay10ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(passengerUserId, mDisplay1Private);
    }

    @Test
    public void testDriverPassengerSwap() {
        mModifier.setPassengerDisplays(new int[]{mDisplay10ForPassenger.getDisplayId(),
                mDisplay11ForPassenger.getDisplayId()});

        final int wasDriver = 10;
        final int wasPassenger = 11;
        mModifier.handleCurrentUserSwitching(wasDriver);
        mModifier.setDisplayWhitelistForUser(wasPassenger,
                new int[]{mDisplay10ForPassenger.getDisplayId(),
                        mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(wasDriver, mDisplay0ForDriver);
        assertDisplayIsAllowed(wasDriver, mDisplay10ForPassenger);
        assertDisplayIsAllowed(wasDriver, mDisplay11ForPassenger);
        assertDisplayIsReassigned(wasPassenger, mDisplay0ForDriver, mDisplay10ForPassenger);
        assertDisplayIsAllowed(wasPassenger, mDisplay10ForPassenger);
        assertDisplayIsAllowed(wasPassenger, mDisplay11ForPassenger);

        final int driver = wasPassenger;
        final int passenger = wasDriver;
        mModifier.handleCurrentUserSwitching(driver);
        mModifier.setDisplayWhitelistForUser(passenger,
                new int[]{mDisplay10ForPassenger.getDisplayId(),
                        mDisplay11ForPassenger.getDisplayId()});

        assertDisplayIsAllowed(driver, mDisplay0ForDriver);
        assertDisplayIsAllowed(driver, mDisplay10ForPassenger);
        assertDisplayIsAllowed(driver, mDisplay11ForPassenger);
        assertDisplayIsReassigned(passenger, mDisplay0ForDriver, mDisplay10ForPassenger);
        assertDisplayIsAllowed(passenger, mDisplay10ForPassenger);
        assertDisplayIsAllowed(passenger, mDisplay11ForPassenger);
    }

    @Test
    public void testPreferSourceForDriver() {
        when(mActivityRecordSource.getDisplayArea()).thenReturn(mDisplayArea0ForDriver);

        // When no sourcePreferredComponents is set, it doesn't set the display for system user.
        assertNoDisplayIsAssigned(UserHandle.USER_SYSTEM);

        mModifier.setSourcePreferredComponents(true, null);
        assertDisplayIsAssigned(UserHandle.USER_SYSTEM, mDisplayArea0ForDriver);
    }

    @Test
    public void testPreferSourceForPassenger() {
        mModifier.setPassengerDisplays(new int[]{PASSENGER_DISPLAY_ID_10, PASSENGER_DISPLAY_ID_11});
        int passengerUserId = 100;
        mModifier.setDisplayWhitelistForUser(passengerUserId,
                new int[]{PASSENGER_DISPLAY_ID_10, PASSENGER_DISPLAY_ID_11});
        when(mActivityRecordSource.getDisplayArea()).thenReturn(mDisplayArea11ForPassenger);

        // When no sourcePreferredComponents is set, it returns the default passenger display.
        assertDisplayIsAssigned(passengerUserId, mDisplayArea10ForPassenger);

        mModifier.setSourcePreferredComponents(true, null);
        assertDisplayIsAssigned(passengerUserId, mDisplayArea11ForPassenger);
    }

    @Test
    public void testPreferSourceDoNotOverrideActivityOptions() {
        when(mActivityOptions.getLaunchDisplayId()).thenReturn(PASSENGER_DISPLAY_ID_10);
        when(mActivityRecordSource.getDisplayArea()).thenReturn(mDisplayArea0ForDriver);

        mModifier.setSourcePreferredComponents(true, null);
        assertNoDisplayIsAssigned(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testPreferSourceForSpecifiedActivity() {
        when(mActivityRecordSource.getDisplayArea()).thenReturn(mDisplayArea0ForDriver);
        mActivityRecordActivity = buildActivityRecord("testPackage", "testActivity");
        mModifier.setSourcePreferredComponents(true,
                Arrays.asList(new ComponentName("testPackage", "testActivity")));

        assertDisplayIsAssigned(UserHandle.USER_SYSTEM, mDisplayArea0ForDriver);
    }

    @Test
    public void testPreferSourceDoNotAssignDisplayForNonSpecifiedActivity() {
        when(mActivityRecordSource.getDisplayArea()).thenReturn(mDisplayArea0ForDriver);
        mActivityRecordActivity = buildActivityRecord("dummyPackage", "dummyActivity");
        mModifier.setSourcePreferredComponents(true,
                Arrays.asList(new ComponentName("testPackage", "testActivity")));

        assertNoDisplayIsAssigned(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testEmbeddedActivityCanLaunchOnVirtualDisplay() {
        // The launch request comes from the Activity in Virtual display.
        DisplayContent dc = mRootWindowContainer.getDisplayContentOrCreate(VIRTUAL_DISPLAY_ID_2);
        when(mActivityRecordSource.getDisplayContent()).thenReturn(dc);
        mActivityRecordActivity = buildActivityRecord("testPackage", "testActivity");
        mActivityRecordActivity.info.flags = ActivityInfo.FLAG_ALLOW_EMBEDDED;

        // ATM will launch the Activity in the source display, since no display is assigned.
        assertNoDisplayIsAssigned(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testNonEmbeddedActivityWithExistingTaskDoesNotChangeDisplay() {
        // The launch request comes from the Activity in Virtual display.
        DisplayContent dc = mRootWindowContainer.getDisplayContentOrCreate(VIRTUAL_DISPLAY_ID_2);
        when(mActivityRecordSource.getDisplayContent()).thenReturn(dc);
        mActivityRecordActivity = buildActivityRecord("testPackage", "testActivity");
        // No setting of FLAG_ALLOW_EMBEDDED and mTask.

        assertNoDisplayIsAssigned(UserHandle.USER_SYSTEM);
    }

    @Test
    public void testNonEmbeddedActivityWithNewTaskMoviesToDefaultDisplay() {
        // The launch request comes from the Activity in Virtual display.
        DisplayContent dc = mRootWindowContainer.getDisplayContentOrCreate(VIRTUAL_DISPLAY_ID_2);
        when(mActivityRecordSource.getDisplayContent()).thenReturn(dc);
        mActivityRecordActivity = buildActivityRecord("testPackage", "testActivity");
        // No setting of FLAG_ALLOW_EMBEDDED.
        mTask = null;  // ATM will assign 'null' to 'task' argument for new task case.

        assertDisplayIsAssigned(UserHandle.USER_SYSTEM, mDisplayArea0ForDriver);
    }
}
