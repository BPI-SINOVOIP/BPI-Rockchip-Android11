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

package com.android.car.am;

import static android.car.test.mocks.AndroidMockitoHelper.mockAmGetCurrentUser;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.ActivityManager.StackInfo;
import android.app.ActivityOptions;
import android.app.IActivityManager;
import android.app.IActivityTaskManager;
import android.app.TaskStackListener;
import android.car.hardware.power.CarPowerManager;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.view.Display;

import com.android.car.CarLocalServices;
import com.android.car.CarServiceUtils;
import com.android.car.user.CarUserService;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.stubbing.OngoingStubbing;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class FixedActivityServiceTest extends AbstractExtendedMockitoTestCase {

    private static final long RECHECK_INTERVAL_MARGIN_MS = 600;

    private final int mValidDisplayId = 1;

    @Mock
    private Context mContext;
    @Mock
    private IActivityManager mActivityManager;
    @Mock
    private IActivityTaskManager mActivityTaskManager;
    @Mock
    private UserManager mUserManager;
    @Mock
    private DisplayManager mDisplayManager;
    @Mock
    private PackageManager mPackageManager;
    @Mock
    private CarUserService mCarUserService;
    @Mock
    private CarPowerManager mCarPowerManager;

    private FixedActivityService mFixedActivityService;

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder builder) {
        builder
            .spyStatic(ActivityManager.class)
            .spyStatic(CarLocalServices.class);
    }

    @Before
    public void setUp() {
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        doReturn(mCarUserService).when(() -> CarLocalServices.getService(CarUserService.class));
        doReturn(mCarPowerManager).when(() -> CarLocalServices.createCarPowerManager(mContext));
        mFixedActivityService = new FixedActivityService(mContext, mActivityManager,
                mActivityTaskManager, mUserManager, mDisplayManager);
    }

    @After
    public void tearDown() {
        if (mFixedActivityService != null) {
            mFixedActivityService.release();
        }
        CarServiceUtils.finishAllHandlerTasks();
    }

    @Test
    public void testStartFixedActivityModeForDisplayAndUser_noRunningActivity()
            throws Exception {
        int userId = 10;
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        mockAmGetCurrentUser(userId);
        expectNoActivityStack();

        // No running activities
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        verify(mContext).startActivityAsUser(eq(intent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();
    }

    @Test
    public void testStartFixedActivityModeForDisplayAndUser_alreadyRunningActivity()
            throws Exception {
        int userId = 10;
        int[] userIds = new int[] { userId };
        int[] taskIds = new int[] { 1234 };
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        mockAmGetCurrentUser(userId);
        expectActivityStackInfo(
                createEmptyStackInfo(),
                createStackInfoList(intent, userIds, mValidDisplayId, taskIds)
        );

        // No running activities
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        verify(mContext).startActivityAsUser(eq(intent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();

        ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        // startActivityAsUser should not called at this time. So, total called count is 1.
        verify(mContext).startActivityAsUser(eq(intent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();
    }

    @Test
    public void testStartFixedActivityModeForDisplayAndUser_runNewActivity() throws Exception {
        int userId = 10;
        int[] userIds = new int[] { userId };
        int[] taskIds = new int[] { 1234 };
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        Intent anotherIntent = expectComponentAvailable("test_package_II", "com.test.dude_II",
                userId);
        mockAmGetCurrentUser(userId);
        expectActivityStackInfo(
                createEmptyStackInfo(),
                createStackInfoList(intent, userIds, mValidDisplayId, taskIds)
        );

        // No running activities
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        assertThat(ret).isTrue();

        // Start activity with new package
        ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(anotherIntent,
                options, mValidDisplayId, userId);
        verify(mContext).startActivityAsUser(eq(anotherIntent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();
    }

    @Test
    public void testStartFixedActivityModeForDisplay_relaunchWithPackageUpdated() throws Exception {
        int userId = 10;
        int[] userIds = new int[] { userId };
        int[] taskIds = new int[] { 1234 };
        String packageName = "test_package";
        String className = "com.test.dude";
        ActivityOptions options = new ActivityOptions(new Bundle());
        ArgumentCaptor<BroadcastReceiver> receiverCaptor =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        Intent intent = expectComponentAvailable(packageName, className, userId);
        mockAmGetCurrentUser(userId);
        expectActivityStackInfo(
                createEmptyStackInfo(),
                createStackInfoList(intent, userIds, mValidDisplayId, taskIds)
        );

        // No running activities
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        verify(mContext).registerReceiverAsUser(receiverCaptor.capture(), eq(UserHandle.ALL),
                any(IntentFilter.class), eq(null), eq(null));
        verify(mContext).startActivityAsUser(eq(intent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();

        // Update package
        SystemClock.sleep(RECHECK_INTERVAL_MARGIN_MS);
        int appId = 987;
        BroadcastReceiver receiver = receiverCaptor.getValue();
        Intent packageIntent = new Intent(Intent.ACTION_PACKAGE_CHANGED);
        packageIntent.setData(new Uri.Builder().path("Any package").build());
        packageIntent.putExtra(Intent.EXTRA_UID, UserHandle.getUid(userId, appId));
        receiver.onReceive(mContext, packageIntent);
        verify(mContext).startActivityAsUser(eq(intent), any(Bundle.class),
                eq(UserHandle.of(userId)));

        SystemClock.sleep(RECHECK_INTERVAL_MARGIN_MS);
        ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        // Activity should not be launched.
        verify(mContext).startActivityAsUser(eq(intent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();
    }


    @Test
    public void testStartFixedActivityModeForDisplayAndUser_runOnDifferentDisplay()
            throws Exception {
        int userId = 10;
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        Intent anotherIntent = expectComponentAvailable("test_package_II", "com.test.dude_II",
                userId);
        mockAmGetCurrentUser(userId);
        expectNoActivityStack();

        // No running activities
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        assertThat(ret).isTrue();

        int anotherValidDisplayId = mValidDisplayId + 1;
        ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(anotherIntent,
                options, anotherValidDisplayId, userId);
        verify(mContext).startActivityAsUser(eq(anotherIntent), any(Bundle.class),
                eq(UserHandle.of(userId)));
        assertThat(ret).isTrue();
    }

    @Test
    public void testStartFixedActivityModeForDisplayAndUser_invalidDisplay() {
        int userId = 10;
        Intent intent = new Intent(Intent.ACTION_MAIN);
        ActivityOptions options = new ActivityOptions(new Bundle());
        int invalidDisplayId = Display.DEFAULT_DISPLAY;

        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent, options,
                invalidDisplayId, userId);
        assertThat(ret).isFalse();
    }

    @Test
    public void testStartFixedActivityModeForDisplayAndUser_notAllowedUser() {
        int currentUserId = 10;
        int notAllowedUserId = 11;
        Intent intent = new Intent(Intent.ACTION_MAIN);
        ActivityOptions options = new ActivityOptions(new Bundle());
        int displayId = mValidDisplayId;
        mockAmGetCurrentUser(currentUserId);
        expectNoProfileUser(currentUserId);

        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent, options,
                displayId, notAllowedUserId);
        assertThat(ret).isFalse();
    }

    @Test
    public void testStartFixedActivityModeForDisplayAndUser_invalidComponent() throws Exception {
        int userId = 10;
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent invalidIntent = expectComponentUnavailable("test_package", "com.test.dude", userId);
        mockAmGetCurrentUser(userId);

        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(invalidIntent,
                options, mValidDisplayId, userId);
        assertThat(ret).isFalse();
    }

    @Test
    public void testStopFixedActivityMode() throws Exception {
        int userId = 10;
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        mockAmGetCurrentUser(userId);
        expectNoActivityStack();

        // Start an activity
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        // To check if monitoring is started.
        verify(mActivityManager).registerTaskStackListener(any(TaskStackListener.class));
        assertThat(ret).isTrue();

        mFixedActivityService.stopFixedActivityMode(mValidDisplayId);
        verify(mActivityManager).unregisterTaskStackListener(any(TaskStackListener.class));
    }

    @Test
    public void testStopFixedActivityMode_invalidDisplayId() throws Exception {
        mFixedActivityService.stopFixedActivityMode(Display.DEFAULT_DISPLAY);
        verify(mActivityManager, never()).unregisterTaskStackListener(any(TaskStackListener.class));
    }

    @Test
    public void testHandleTaskFocusChanged_returnForcusBack() throws Exception {
        int userId = 10;
        int testTaskId = 1234;
        int[] userIds = new int[]{userId};
        int[] taskIds = new int[]{testTaskId};
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        mockAmGetCurrentUser(userId);
        expectActivityStackInfo(createStackInfoList(intent, userIds, mValidDisplayId, taskIds));

        // Make FixedActivityService to update the taskIds.
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        assertThat(ret).isTrue();

        Intent homeIntent = expectComponentAvailable("home", "homeActivity", userId);
        int homeTaskId = 4567;
        when(mActivityTaskManager.getAllStackInfosOnDisplay(Display.DEFAULT_DISPLAY)).thenReturn(
                createStackInfoList(
                        homeIntent, userIds, Display.DEFAULT_DISPLAY, new int[]{homeTaskId}));

        mFixedActivityService.handleTaskFocusChanged(testTaskId, true);

        verify(mActivityTaskManager).setFocusedTask(homeTaskId);
    }

    // If there are two tasks in one display id, then there is a bug to invalidate
    // RunningActivityInfo.taskId examining 2nd task and it prevents from checking FixedActivity
    // in handleTaskFocusChanged(). This test makes sure if the bug doesn't exist.
    @Test
    public void testHandleTaskFocusChanged_returnForcusBackInTwoStackInfos() throws Exception {
        int userId = 10;
        int testTaskId = 1234;
        int[] userIds = new int[]{userId};
        int[] taskIds = new int[]{testTaskId};
        int[] taskIds2 = new int[]{testTaskId + 1111};
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        Intent intent2 = expectComponentAvailable("test_package2", "com.test.others", userId);
        mockAmGetCurrentUser(userId);
        expectActivityStackInfo(Arrays.asList(
                createStackInfo(intent, userIds, mValidDisplayId, taskIds),
                createStackInfo(intent2, userIds, mValidDisplayId, taskIds2)));

        // Make FixedActivityService to update the taskIds.
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        assertThat(ret).isTrue();

        Intent homeIntent = expectComponentAvailable("home", "homeActivity", userId);
        int homeTaskId = 4567;
        when(mActivityTaskManager.getAllStackInfosOnDisplay(Display.DEFAULT_DISPLAY)).thenReturn(
                createStackInfoList(
                        homeIntent, userIds, Display.DEFAULT_DISPLAY, new int[]{homeTaskId}));

        mFixedActivityService.handleTaskFocusChanged(testTaskId, true);

        verify(mActivityTaskManager).setFocusedTask(homeTaskId);
    }

    @Test
    public void testHandleTaskFocusChanged_noForcusBack() throws Exception {
        int userId = 10;
        int testTaskId = 1234;
        int[] userIds = new int[]{userId};
        int[] taskIds = new int[]{testTaskId};
        ActivityOptions options = new ActivityOptions(new Bundle());
        Intent intent = expectComponentAvailable("test_package", "com.test.dude", userId);
        mockAmGetCurrentUser(userId);
        expectActivityStackInfo(createStackInfoList(intent, userIds, mValidDisplayId, taskIds));

        // Make FixedActivityService to update the taskIds.
        boolean ret = mFixedActivityService.startFixedActivityModeForDisplayAndUser(intent,
                options, mValidDisplayId, userId);
        assertThat(ret).isTrue();

        mFixedActivityService.handleTaskFocusChanged(testTaskId + 1, true);

        verify(mActivityTaskManager, never()).setFocusedTask(anyInt());
    }

    private void expectNoProfileUser(@UserIdInt int userId) {
        when(mUserManager.getEnabledProfileIds(userId)).thenReturn(new int[0]);
    }

    private Intent expectComponentUnavailable(String pkgName, String className,
            @UserIdInt int userId) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        ComponentName component = new ComponentName(pkgName, className);
        intent.setComponent(component);
        ActivityInfo activityInfo = new ActivityInfo();
        // To make sure there is no matched activity
        activityInfo.name = component.getClassName() + ".unavailable";
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.activities = new ActivityInfo[] { activityInfo };
        when(mPackageManager.getPackageInfoAsUser(component.getPackageName(),
                PackageManager.GET_ACTIVITIES, userId)).thenReturn(packageInfo);
        return intent;
    }

    private Intent expectComponentAvailable(String pkgName, String className, @UserIdInt int userId)
            throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        ComponentName component = new ComponentName(pkgName, className);
        intent.setComponent(component);
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.name = component.getClassName();
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.activities = new ActivityInfo[] { activityInfo };
        when(mPackageManager.getPackageInfoAsUser(component.getPackageName(),
                PackageManager.GET_ACTIVITIES, userId)).thenReturn(packageInfo);
        return intent;
    }

    private void expectNoActivityStack() throws Exception {
        when(mActivityManager.getAllStackInfos()).thenReturn(createEmptyStackInfo());
    }

    private void expectActivityStackInfo(List<StackInfo> ...stackInfos) throws Exception {
        OngoingStubbing<List<StackInfo>> stub = when(mActivityManager.getAllStackInfos());
        for (List<StackInfo> stackInfo : stackInfos) {
            stub = stub.thenReturn(stackInfo);
        }
    }

    private List<StackInfo> createEmptyStackInfo() {
        return new ArrayList<StackInfo>();
    }

    private StackInfo createStackInfo(Intent intent, @UserIdInt int[] userIds, int displayId,
            int[] taskIds) {
        StackInfo stackInfo = new StackInfo();
        stackInfo.taskUserIds = userIds;
        stackInfo.topActivity = intent.getComponent().clone();
        stackInfo.visible = true;
        stackInfo.displayId = displayId;
        stackInfo.taskIds = taskIds;
        return stackInfo;
    }

    private List<StackInfo> createStackInfoList(Intent intent, @UserIdInt int[] userIds,
            int displayId, int[] taskIds) {
        return Arrays.asList(createStackInfo(intent, userIds, displayId, taskIds));
    }
}
