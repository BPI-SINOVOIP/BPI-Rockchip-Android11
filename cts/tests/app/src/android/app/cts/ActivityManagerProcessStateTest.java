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

package android.app.cts;

import static android.app.ActivityManager.PROCESS_CAPABILITY_ALL;
import static android.app.ActivityManager.PROCESS_CAPABILITY_ALL_IMPLICIT;
import static android.app.ActivityManager.PROCESS_CAPABILITY_FOREGROUND_LOCATION;
import static android.app.ActivityManager.PROCESS_CAPABILITY_NONE;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_CACHED;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_GONE;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_VISIBLE;

import android.accessibilityservice.AccessibilityService;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.app.Instrumentation;
import android.app.cts.android.app.cts.tools.ServiceConnectionHandler;
import android.app.cts.android.app.cts.tools.ServiceProcessController;
import android.app.cts.android.app.cts.tools.SyncOrderedBroadcast;
import android.app.cts.android.app.cts.tools.UidImportanceListener;
import android.app.cts.android.app.cts.tools.WaitForBroadcast;
import android.app.cts.android.app.cts.tools.WatchUidRunner;
import android.app.stubs.CommandReceiver;
import android.app.stubs.LocalForegroundServiceLocation;
import android.app.stubs.ScreenOnActivity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.SystemClock;
import android.permission.cts.PermissionUtils;
import android.server.wm.WindowManagerState;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiSelector;
import android.test.InstrumentationTestCase;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;

import com.android.compatibility.common.util.SystemUtil;

public class ActivityManagerProcessStateTest extends InstrumentationTestCase {
    private static final String TAG = ActivityManagerProcessStateTest.class.getName();

    private static final String STUB_PACKAGE_NAME = "android.app.stubs";
    private static final String PACKAGE_NAME_APP1 = "com.android.app1";
    private static final String PACKAGE_NAME_APP2 = "com.android.app2";
    private static final String PACKAGE_NAME_APP3 = "com.android.app3";

    private static final String[] PACKAGE_NAMES = {
            PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, PACKAGE_NAME_APP3
    };

    private static final int WAIT_TIME = 10000;
    private static final int WAITFOR_MSEC = 10000;
    // A secondary test activity from another APK.
    static final String SIMPLE_PACKAGE_NAME = "com.android.cts.launcherapps.simpleapp";
    static final String SIMPLE_SERVICE = ".SimpleService";
    static final String SIMPLE_SERVICE2 = ".SimpleService2";
    static final String SIMPLE_SERVICE3 = ".SimpleService3";
    static final String SIMPLE_RECEIVER_START_SERVICE = ".SimpleReceiverStartService";
    static final String SIMPLE_ACTIVITY_START_SERVICE = ".SimpleActivityStartService";
    static final String SIMPLE_ACTIVITY_START_FG_SERVICE = ".SimpleActivityStartFgService";
    public static String ACTION_SIMPLE_ACTIVITY_START_SERVICE_RESULT =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartService.RESULT";
    static final String ACTION_SIMPLE_ACTIVITY_START_FG =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartFgService.START_THEN_FG";
    public static String ACTION_SIMPLE_ACTIVITY_START_FG_SERVICE_RESULT =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartFgService.NOW_FOREGROUND";
    public static String ACTION_FINISH_EVERYTHING =
            "com.android.cts.launcherapps.simpleapp.SimpleActivityStartFgService.FINISH_ALL";

    // APKs for testing heavy weight app interactions.
    static final String CANT_SAVE_STATE_1_PACKAGE_NAME = "com.android.test.cantsavestate1";
    static final String CANT_SAVE_STATE_2_PACKAGE_NAME = "com.android.test.cantsavestate2";

    // Actions
    static final String ACTION_START_FOREGROUND = "com.android.test.action.START_FOREGROUND";
    static final String ACTION_STOP_FOREGROUND = "com.android.test.action.STOP_FOREGROUND";
    static final String ACTION_START_THEN_FG = "com.android.test.action.START_THEN_FG";
    static final String ACTION_STOP_SERVICE = "com.android.test.action.STOP";

    private static final int TEMP_WHITELIST_DURATION_MS = 2000;

    private Context mContext;
    private Context mTargetContext;
    private Instrumentation mInstrumentation;
    private Intent mServiceIntent;
    private Intent mServiceStartForegroundIntent;
    private Intent mServiceStopForegroundIntent;
    private Intent mService2Intent;
    private Intent mService3Intent;
    private Intent mServiceStartForeground3Intent;
    private Intent mMainProcess[];
    private Intent mAllProcesses[];

    private int mAppCount;
    private ApplicationInfo[] mAppInfo;
    private WatchUidRunner[] mWatchers;


    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mInstrumentation = getInstrumentation();
        mContext = mInstrumentation.getContext();
        mTargetContext = mInstrumentation.getTargetContext();
        mServiceIntent = new Intent();
        mServiceIntent.setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_SERVICE);
        mServiceStartForegroundIntent = new Intent(mServiceIntent);
        mServiceStartForegroundIntent.setAction(ACTION_START_FOREGROUND);
        mServiceStopForegroundIntent = new Intent(mServiceIntent);
        mServiceStopForegroundIntent.setAction(ACTION_STOP_FOREGROUND);
        mService2Intent = new Intent()
                .setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_SERVICE2);
        mService3Intent = new Intent()
                .setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_SERVICE3);
        mMainProcess = new Intent[1];
        mMainProcess[0] = mServiceIntent;
        mAllProcesses = new Intent[2];
        mAllProcesses[0] = mServiceIntent;
        mAllProcesses[1] = mService2Intent;
        mContext.stopService(mServiceIntent);
        mContext.stopService(mService2Intent);
        mContext.stopService(mService3Intent);
        CtsAppTestUtils.turnScreenOn(mInstrumentation, mContext);
        removeTestAppFromWhitelists();
        mAppCount = 0;
        // Make sure we are in Home screen before starting the test
        mInstrumentation.getUiAutomation().performGlobalAction(
                AccessibilityService.GLOBAL_ACTION_HOME);
        // Stop all the packages to avoid residual impact
        final ActivityManager am = mContext.getSystemService(ActivityManager.class);
        for (int i = 0; i < PACKAGE_NAMES.length; i++) {
            final String pkgName = PACKAGE_NAMES[i];
            SystemUtil.runWithShellPermissionIdentity(() -> {
                am.forceStopPackage(pkgName);
            });
        }
    }

    /**
     * Set up count app info objects and WatchUidRunners.
     */
    private void setupWatchers(int count) throws Exception {
        mAppCount = count;
        mAppInfo = new ApplicationInfo[count];
        mWatchers = new WatchUidRunner[count];
        for (int i = 0; i < count; i++) {
            mAppInfo[i] = mContext.getPackageManager().getApplicationInfo(
                    PACKAGE_NAMES[i], 0);
            mWatchers[i] = new WatchUidRunner(mInstrumentation, mAppInfo[i].uid,
                    WAITFOR_MSEC);
        }
    }

    /**
     * Finish all started WatchUidRunners.
     */
    private void shutdownWatchers() throws Exception {
        for (int i = 0; i < mAppCount; i++) {
            mWatchers[i].finish();
        }
    }

    private void removeTestAppFromWhitelists() throws Exception {
        CtsAppTestUtils.executeShellCmd(mInstrumentation,
                "cmd deviceidle whitelist -" + SIMPLE_PACKAGE_NAME);
        CtsAppTestUtils.executeShellCmd(mInstrumentation,
                "cmd deviceidle tempwhitelist -r " + SIMPLE_PACKAGE_NAME);
    }

    private void waitForAppFocus(String waitForApp, long waitTime) {
        long waitUntil = SystemClock.elapsedRealtime() + waitTime;
        while (true) {
            WindowManagerState wms = new WindowManagerState();
            wms.computeState();
            String appName = wms.getFocusedApp();
            if (appName != null) {
                ComponentName comp = ComponentName.unflattenFromString(appName);
                if (waitForApp.equals(comp.getPackageName())) {
                    break;
                }
            }
            if (SystemClock.elapsedRealtime() > waitUntil) {
                throw new IllegalStateException("Timed out waiting for focus on app "
                        + waitForApp + ", last was " + appName);
            }
            Log.i(TAG, "Waiting for app focus, current: " + appName);
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
            }
        }
    }

    private void startActivityAndWaitForShow(final Intent intent) throws Exception {
        mInstrumentation.getUiAutomation().executeAndWaitForEvent(
                () -> {
                    try {
                        mTargetContext.startActivity(intent);
                    } catch (Exception e) {
                        fail("Cannot start activity: " + intent);
                    }
                }, (AccessibilityEvent event) -> event.getEventType()
                        == AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED
                , WAIT_TIME);
    }

    private void maybeClick(UiDevice device, UiSelector sel) {
        try {
            device.findObject(sel).click();
        } catch (Throwable ignored) {
        }
    }

    private void maybeClick(UiDevice device, BySelector sel) {
        try {
            device.findObject(sel).click();
        } catch (Throwable ignored) {
        }
    }

    /**
     * Test basic state changes as processes go up and down due to services running in them.
     */
    public void testUidImportanceListener() throws Exception {
        final Parcel data = Parcel.obtain();
        ServiceConnectionHandler conn = new ServiceConnectionHandler(mContext, mServiceIntent,
                WAIT_TIME);
        ServiceConnectionHandler conn2 = new ServiceConnectionHandler(mContext, mService2Intent,
                WAIT_TIME);

        ActivityManager am = mContext.getSystemService(ActivityManager.class);

        ApplicationInfo appInfo = mContext.getPackageManager().getApplicationInfo(
                SIMPLE_PACKAGE_NAME, 0);
        UidImportanceListener uidForegroundListener = new UidImportanceListener(mContext,
                appInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_SERVICE, WAIT_TIME);

        PermissionUtils.revokePermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        boolean gotException = false;
        try {
            uidForegroundListener.register();
        } catch (SecurityException e) {
            gotException = true;
        }
        assertTrue("Expected SecurityException thrown", gotException);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        /*
        Log.d("XXXX", "Invoke: " + cmd);
        Log.d("XXXX", "Result: " + result);
        Log.d("XXXX", SystemUtil.runShellCommand(mInstrumentation, "dumpsys package "
                + STUB_PACKAGE_NAME));
        */
        uidForegroundListener.register();

        UidImportanceListener uidGoneListener = new UidImportanceListener(mContext,
                appInfo.uid, IMPORTANCE_CACHED, WAIT_TIME);
        uidGoneListener.register();

        WatchUidRunner uidWatcher = new WatchUidRunner(mInstrumentation, appInfo.uid,
                WAIT_TIME);

        try {
            // First kill the processes to start out in a stable state.
            conn.bind();
            conn2.bind();
            IBinder service1 = conn.getServiceIBinder();
            IBinder service2 = conn2.getServiceIBinder();
            conn.unbind();
            conn2.unbind();
            try {
                service1.transact(IBinder.FIRST_CALL_TRANSACTION, data, null, 0);
            } catch (RemoteException e) {
            }
            try {
                service2.transact(IBinder.FIRST_CALL_TRANSACTION, data, null, 0);
            } catch (RemoteException e) {
            }
            service1 = service2 = null;

            // Wait for uid's processes to go away.
            uidGoneListener.waitForValue(IMPORTANCE_GONE, IMPORTANCE_GONE);
            assertEquals(IMPORTANCE_GONE, am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // And wait for the uid report to be gone.
            uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null);

            // Now bind and see if we get told about the uid coming in to the foreground.
            conn.bind();
            uidForegroundListener.waitForValue(IMPORTANCE_FOREGROUND, IMPORTANCE_VISIBLE);
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // Also make sure the uid state reports are as expected.  Wait for active because
            // there may be some intermediate states as the process comes up.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Pull out the service IBinder for a kludy hack...
            IBinder service = conn.getServiceIBinder();

            // Now unbind and see if we get told about it going to the background.
            conn.unbind();
            uidForegroundListener.waitForValue(IMPORTANCE_CACHED, IMPORTANCE_CACHED);
            assertEquals(IMPORTANCE_CACHED, am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Now kill the process and see if we are told about it being gone.
            try {
                service.transact(IBinder.FIRST_CALL_TRANSACTION, data, null, 0);
            } catch (RemoteException e) {
                // It is okay if it is already gone for some reason.
            }

            uidGoneListener.waitForValue(IMPORTANCE_GONE, IMPORTANCE_GONE);
            assertEquals(IMPORTANCE_GONE, am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);
            uidWatcher.expect(WatchUidRunner.CMD_GONE, null);

            // Now we are going to try different combinations of binding to two processes to
            // see if they are correctly combined together for the app.

            // Bring up both services.
            conn.bind();
            conn2.bind();
            uidForegroundListener.waitForValue(IMPORTANCE_FOREGROUND, IMPORTANCE_VISIBLE);
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Bring down one service, app state should remain foreground.
            conn2.unbind();
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // Bring down other service, app state should now be cached.  (If the processes both
            // actually get killed immediately, this is also not a correctly behaving system.)
            conn.unbind();
            uidGoneListener.waitForValue(IMPORTANCE_CACHED, IMPORTANCE_CACHED);
            assertEquals(IMPORTANCE_CACHED,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Bring up one service, this should be sufficient to become foreground.
            conn2.bind();
            uidForegroundListener.waitForValue(IMPORTANCE_FOREGROUND, IMPORTANCE_VISIBLE);
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Bring up other service, should remain foreground.
            conn.bind();
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // Bring down one service, should remain foreground.
            conn.unbind();
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // And bringing down other service should put us back to cached.
            conn2.unbind();
            uidGoneListener.waitForValue(IMPORTANCE_CACHED,
                    IMPORTANCE_CACHED);
            assertEquals(IMPORTANCE_CACHED,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
        } finally {
            data.recycle();
            uidWatcher.finish();
            uidForegroundListener.unregister();
            uidGoneListener.unregister();
        }
    }

    /**
     * Test that background check correctly prevents idle services from running but allows
     * whitelisted apps to bypass the check.
     */
    public void testBackgroundCheckService() throws Exception {
        final Parcel data = Parcel.obtain();
        Intent serviceIntent = new Intent();
        serviceIntent.setClassName(SIMPLE_PACKAGE_NAME,
                SIMPLE_PACKAGE_NAME + SIMPLE_SERVICE);
        ServiceConnectionHandler conn = new ServiceConnectionHandler(mContext, serviceIntent,
                WAIT_TIME);

        ActivityManager am = mContext.getSystemService(ActivityManager.class);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        /*
        Log.d("XXXX", "Invoke: " + cmd);
        Log.d("XXXX", "Result: " + result);
        Log.d("XXXX", SystemUtil.runShellCommand(mInstrumentation, "dumpsys package "
                + STUB_PACKAGE_NAME));
        */

        ApplicationInfo appInfo = mContext.getPackageManager().getApplicationInfo(
                SIMPLE_PACKAGE_NAME, 0);

        UidImportanceListener uidForegroundListener = new UidImportanceListener(mContext,
                appInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_SERVICE, WAIT_TIME);
        uidForegroundListener.register();
        UidImportanceListener uidGoneListener = new UidImportanceListener(mContext,
                appInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_EMPTY, WAIT_TIME);
        uidGoneListener.register();

        WatchUidRunner uidWatcher = new WatchUidRunner(mInstrumentation, appInfo.uid,
                WAIT_TIME);

        // First kill the process to start out in a stable state.
        mContext.stopService(serviceIntent);
        conn.bind();
        IBinder service = conn.getServiceIBinder();
        conn.unbind();
        try {
            service.transact(IBinder.FIRST_CALL_TRANSACTION, data, null, 0);
        } catch (RemoteException e) {
        }
        service = null;

        // Wait for uid's process to go away.
        uidGoneListener.waitForValue(IMPORTANCE_GONE, IMPORTANCE_GONE);
        assertEquals(IMPORTANCE_GONE,
                am.getPackageImportance(SIMPLE_PACKAGE_NAME));

        // And wait for the uid report to be gone.
        uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null);

        String cmd = "appops set " + SIMPLE_PACKAGE_NAME + " RUN_IN_BACKGROUND deny";
        String result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        // This is a side-effect of the app op command.
        uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);
        uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, "NONE");

        // We don't want to wait for the uid to actually go idle, we can force it now.
        cmd = "am make-uid-idle " + SIMPLE_PACKAGE_NAME;
        result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        // Make sure app is not yet on whitelist
        cmd = "cmd deviceidle whitelist -" + SIMPLE_PACKAGE_NAME;
        result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        // We will use this to monitor when the service is running.
        conn.startMonitoring();

        try {
            // Try starting the service.  Should fail!
            boolean failed = false;
            try {
                mContext.startService(serviceIntent);
            } catch (IllegalStateException e) {
                failed = true;
            }
            if (!failed) {
                fail("Service was allowed to start while in the background");
            }

            // Put app on temporary whitelist to see if this allows the service start.
            cmd = String.format("cmd deviceidle tempwhitelist -d %d %s",
                    TEMP_WHITELIST_DURATION_MS, SIMPLE_PACKAGE_NAME);
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

            // Try starting the service now that the app is whitelisted...  should work!
            mContext.startService(serviceIntent);
            conn.waitForConnect();

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Good, now stop the service and give enough time to get off the temp whitelist.
            mContext.stopService(serviceIntent);
            conn.waitForDisconnect();

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            CtsAppTestUtils.executeShellCmd(mInstrumentation,
                    "cmd deviceidle tempwhitelist -r " + SIMPLE_PACKAGE_NAME);

            // Going off the temp whitelist causes a spurious proc state report...  that's
            // not ideal, but okay.
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // We don't want to wait for the uid to actually go idle, we can force it now.
            cmd = "am make-uid-idle " + SIMPLE_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Now that we should be off the temp whitelist, make sure we again can't start.
            failed = false;
            try {
                mContext.startService(serviceIntent);
            } catch (IllegalStateException e) {
                failed = true;
            }
            if (!failed) {
                fail("Service was allowed to start while in the background");
            }

            // Now put app on whitelist, should allow service to run.
            cmd = "cmd deviceidle whitelist +" + SIMPLE_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

            // Try starting the service now that the app is whitelisted...  should work!
            mContext.startService(serviceIntent);
            conn.waitForConnect();

            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Okay, bring down the service.
            mContext.stopService(serviceIntent);
            conn.waitForDisconnect();

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

        } finally {
            mContext.stopService(serviceIntent);
            conn.stopMonitoring();

            uidWatcher.finish();

            cmd = "appops set " + SIMPLE_PACKAGE_NAME + " RUN_IN_BACKGROUND allow";
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);
            cmd = "cmd deviceidle whitelist -" + SIMPLE_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

            uidGoneListener.unregister();
            uidForegroundListener.unregister();

            data.recycle();
        }
    }

    /**
     * Test that background check behaves correctly after a process is no longer foreground: first
     * allowing a service to be started, then stopped by the system when idle.
     */
    public void testBackgroundCheckStopsService() throws Exception {
        final Parcel data = Parcel.obtain();
        ServiceConnectionHandler conn = new ServiceConnectionHandler(mContext, mServiceIntent,
                WAIT_TIME);
        ServiceConnectionHandler conn2 = new ServiceConnectionHandler(mContext, mService2Intent,
                WAIT_TIME);

        ActivityManager am = mContext.getSystemService(ActivityManager.class);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        /*
        Log.d("XXXX", "Invoke: " + cmd);
        Log.d("XXXX", "Result: " + result);
        Log.d("XXXX", SystemUtil.runShellCommand(mInstrumentation, "dumpsys package "
                + STUB_PACKAGE_NAME));
        */

        ApplicationInfo appInfo = mContext.getPackageManager().getApplicationInfo(
                SIMPLE_PACKAGE_NAME, 0);

        UidImportanceListener uidServiceListener = new UidImportanceListener(mContext,
                appInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_SERVICE, WAIT_TIME);
        uidServiceListener.register();
        UidImportanceListener uidGoneListener = new UidImportanceListener(mContext,
                appInfo.uid, IMPORTANCE_CACHED, WAIT_TIME);
        uidGoneListener.register();

        WatchUidRunner uidWatcher = new WatchUidRunner(mInstrumentation, appInfo.uid,
                WAIT_TIME);

        // First kill the process to start out in a stable state.
        mContext.stopService(mServiceIntent);
        mContext.stopService(mService2Intent);
        conn.bind();
        conn2.bind();
        IBinder service = conn.getServiceIBinder();
        IBinder service2 = conn2.getServiceIBinder();
        conn.unbind();
        conn2.unbind();
        try {
            service.transact(IBinder.FIRST_CALL_TRANSACTION, data, null, 0);
        } catch (RemoteException e) {
        }
        try {
            service2.transact(IBinder.FIRST_CALL_TRANSACTION, data, null, 0);
        } catch (RemoteException e) {
        }
        service = service2 = null;

        // Wait for uid's process to go away.
        uidGoneListener.waitForValue(IMPORTANCE_GONE,
                IMPORTANCE_GONE);
        assertEquals(IMPORTANCE_GONE,
                am.getPackageImportance(SIMPLE_PACKAGE_NAME));

        // And wait for the uid report to be gone.
        uidWatcher.waitFor(WatchUidRunner.CMD_GONE, null, WAIT_TIME);

        String cmd = "appops set " + SIMPLE_PACKAGE_NAME + " RUN_IN_BACKGROUND deny";
        String result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        // This is a side-effect of the app op command.
        uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);
        uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_NONEXISTENT);

        // We don't want to wait for the uid to actually go idle, we can force it now.
        cmd = "am make-uid-idle " + SIMPLE_PACKAGE_NAME;
        result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        // Make sure app is not yet on whitelist
        cmd = "cmd deviceidle whitelist -" + SIMPLE_PACKAGE_NAME;
        result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        // We will use this to monitor when the service is running.
        conn.startMonitoring();

        try {
            // Try starting the service.  Should fail!
            boolean failed = false;
            try {
                mContext.startService(mServiceIntent);
            } catch (IllegalStateException e) {
                failed = true;
            }
            if (!failed) {
                fail("Service was allowed to start while in the background");
            }

            // First poke the process into the foreground, so we can avoid background check.
            conn2.bind();
            conn2.waitForConnect();

            // Wait for process state to reflect running service.
            uidServiceListener.waitForValue(
                    IMPORTANCE_FOREGROUND_SERVICE,
                    ActivityManager.RunningAppProcessInfo.IMPORTANCE_PERCEPTIBLE);
            assertEquals(IMPORTANCE_FOREGROUND_SERVICE,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            conn2.unbind();

            // Wait for process to recover back down to being cached.
            uidServiceListener.waitForValue(IMPORTANCE_CACHED,
                    IMPORTANCE_GONE);
            assertEquals(IMPORTANCE_CACHED,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Try starting the service now that the app is waiting to idle...  should work!
            mContext.startService(mServiceIntent);
            conn.waitForConnect();

            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // And also start the second service.
            conn2.startMonitoring();
            mContext.startService(mService2Intent);
            conn2.waitForConnect();

            // Force app to go idle now
            cmd = "am make-uid-idle " + SIMPLE_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

            // Wait for services to be stopped by system.
            uidServiceListener.waitForValue(IMPORTANCE_CACHED,
                    IMPORTANCE_GONE);
            assertEquals(IMPORTANCE_CACHED,
                    am.getPackageImportance(SIMPLE_PACKAGE_NAME));

            // And service should be stopped by system, so just make sure it is disconnected.
            conn.waitForDisconnect();
            conn2.waitForDisconnect();

            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);
            // There may be a transient 'SVC' proc state here.
            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

        } finally {
            mContext.stopService(mServiceIntent);
            mContext.stopService(mService2Intent);
            conn.cleanup();
            conn2.cleanup();

            uidWatcher.finish();

            cmd = "appops set " + SIMPLE_PACKAGE_NAME + " RUN_IN_BACKGROUND allow";
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);
            cmd = "cmd deviceidle whitelist -" + SIMPLE_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

            uidGoneListener.unregister();
            uidServiceListener.unregister();

            data.recycle();
        }
    }

    /**
     * Test the background check doesn't allow services to be started from broadcasts except when in
     * the correct states.
     */
    public void testBackgroundCheckBroadcastService() throws Exception {
        final Intent broadcastIntent = new Intent();
        broadcastIntent.setFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        broadcastIntent.setClassName(SIMPLE_PACKAGE_NAME,
                SIMPLE_PACKAGE_NAME + SIMPLE_RECEIVER_START_SERVICE);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        final ServiceProcessController controller = new ServiceProcessController(mContext,
                mInstrumentation, STUB_PACKAGE_NAME, mAllProcesses, WAIT_TIME);
        final ServiceConnectionHandler conn = new ServiceConnectionHandler(mContext,
                mServiceIntent, WAIT_TIME);
        final WatchUidRunner uidWatcher = controller.getUidWatcher();

        try {
            // First kill the process to start out in a stable state.
            controller.ensureProcessGone();

            // Do initial setup.
            controller.denyBackgroundOp();
            controller.makeUidIdle();
            controller.removeFromWhitelist();

            // We will use this to monitor when the service is running.
            conn.startMonitoring();

            // Try sending broadcast to start the service.  Should fail!
            SyncOrderedBroadcast br = new SyncOrderedBroadcast();
            broadcastIntent.putExtra("service", mServiceIntent);
            br.sendAndWait(mContext, broadcastIntent, Activity.RESULT_OK, null, null, WAIT_TIME);
            int brCode = br.getReceivedCode();
            if (brCode != Activity.RESULT_CANCELED) {
                fail("Didn't fail starting service, result=" + brCode);
            }

            // Track the uid proc state changes from the broadcast (but not service execution)
            uidWatcher.waitFor(WatchUidRunner.CMD_IDLE, null, WAIT_TIME);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null, WAIT_TIME);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_RECEIVER,
                    WAIT_TIME);
            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null, WAIT_TIME);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY,
                    WAIT_TIME);

            // Put app on temporary whitelist to see if this allows the service start.
            controller.tempWhitelist(TEMP_WHITELIST_DURATION_MS);

            // Being on the whitelist means the uid is now active.
            uidWatcher.expect(WatchUidRunner.CMD_ACTIVE, null, WAIT_TIME);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY,
                    WAIT_TIME);

            // Try starting the service now that the app is whitelisted...  should work!
            br.sendAndWait(mContext, broadcastIntent, Activity.RESULT_OK, null, null, WAIT_TIME);
            brCode = br.getReceivedCode();
            if (brCode != Activity.RESULT_FIRST_USER) {
                fail("Failed starting service, result=" + brCode);
            }
            conn.waitForConnect();

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            // We are going to wait until 'SVC', because we may see an intermediate 'RCVR'
            // proc state depending on timing.
            uidWatcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Good, now stop the service and give enough time to get off the temp whitelist.
            mContext.stopService(mServiceIntent);
            conn.waitForDisconnect();

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            controller.removeFromTempWhitelist();

            // Going off the temp whitelist causes a spurious proc state report...  that's
            // not ideal, but okay.
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // We don't want to wait for the uid to actually go idle, we can force it now.
            controller.makeUidIdle();

            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);

            // Make sure the process is gone so we start over fresh.
            controller.ensureProcessGone();

            // Now that we should be off the temp whitelist, make sure we again can't start.
            br.sendAndWait(mContext, broadcastIntent, Activity.RESULT_OK, null, null, WAIT_TIME);
            brCode = br.getReceivedCode();
            if (brCode != Activity.RESULT_CANCELED) {
                fail("Didn't fail starting service, result=" + brCode);
            }

            // Track the uid proc state changes from the broadcast (but not service execution)
            uidWatcher.waitFor(WatchUidRunner.CMD_IDLE, null);
            // There could be a transient 'cached' state here before 'uncached' if uid state
            // changes are dispatched before receiver is started.
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_RECEIVER);
            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Now put app on whitelist, should allow service to run.
            controller.addToWhitelist();

            // Try starting the service now that the app is whitelisted...  should work!
            br.sendAndWait(mContext, broadcastIntent, Activity.RESULT_OK, null, null, WAIT_TIME);
            brCode = br.getReceivedCode();
            if (brCode != Activity.RESULT_FIRST_USER) {
                fail("Failed starting service, result=" + brCode);
            }
            conn.waitForConnect();

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Okay, bring down the service.
            mContext.stopService(mServiceIntent);
            conn.waitForDisconnect();

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

        } finally {
            mContext.stopService(mServiceIntent);
            conn.stopMonitoringIfNeeded();
            controller.cleanup();
        }
    }

    /**
     * Test that background check does allow services to be started from activities.
     */
    public void testBackgroundCheckActivityService() throws Exception {
        final Intent activityIntent = new Intent();
        activityIntent.setClassName(SIMPLE_PACKAGE_NAME,
                SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY_START_SERVICE);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        final ServiceProcessController controller = new ServiceProcessController(mContext,
                mInstrumentation, STUB_PACKAGE_NAME, mAllProcesses, WAIT_TIME);
        final ServiceConnectionHandler conn = new ServiceConnectionHandler(mContext,
                mServiceIntent, WAIT_TIME);
        final WatchUidRunner uidWatcher = controller.getUidWatcher();

        try {
            // First kill the process to start out in a stable state.
            controller.ensureProcessGone();

            // Do initial setup.
            controller.denyBackgroundOp();
            controller.makeUidIdle();
            controller.removeFromWhitelist();

            // We will use this to monitor when the service is running.
            conn.startMonitoring();

            // Try starting activity that will start the service.  This should be okay.
            WaitForBroadcast waiter = new WaitForBroadcast(mInstrumentation.getTargetContext());
            waiter.prepare(ACTION_SIMPLE_ACTIVITY_START_SERVICE_RESULT);
            activityIntent.putExtra("service", mServiceIntent);
            mTargetContext.startActivity(activityIntent);
            Intent resultIntent = waiter.doWait(WAIT_TIME * 2);
            int brCode = resultIntent.getIntExtra("result", Activity.RESULT_CANCELED);
            if (brCode != Activity.RESULT_FIRST_USER) {
                fail("Failed starting service, result=" + brCode);
            }
            conn.waitForConnect();

            final String expectedActivityState = (CtsAppTestUtils.isScreenInteractive(mContext)
                    && !CtsAppTestUtils.isKeyguardLocked(mContext))
                    ? WatchUidRunner.STATE_TOP : WatchUidRunner.STATE_TOP_SLEEPING;
            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, expectedActivityState);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Okay, bring down the service.
            mContext.stopService(mServiceIntent);
            conn.waitForDisconnect();

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // App isn't yet idle, so we should be able to start the service again.
            mContext.startService(mServiceIntent);
            conn.waitForConnect();
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // And now fast-forward to the app going idle, service should be stopped.
            controller.makeUidIdle();
            uidWatcher.waitFor(WatchUidRunner.CMD_IDLE, null);

            conn.waitForDisconnect();
            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // No longer should be able to start service.
            boolean failed = false;
            try {
                mContext.startService(mServiceIntent);
            } catch (IllegalStateException e) {
                failed = true;
            }
            if (!failed) {
                fail("Service was allowed to start while in the background");
            }

        } finally {
            mContext.stopService(mServiceIntent);
            conn.stopMonitoringIfNeeded();
            controller.cleanup();
        }
    }

    /**
     * Test that the foreground service app op does prevent the foreground state.
     */
    public void testForegroundServiceAppOp() throws Exception {
        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        // Use default timeout value 5000
        final ServiceProcessController controller = new ServiceProcessController(mContext,
                mInstrumentation, STUB_PACKAGE_NAME, mAllProcesses);
        // Use default timeout value 5000
        final ServiceConnectionHandler conn = new ServiceConnectionHandler(mContext,
                mServiceIntent);
        final WatchUidRunner uidWatcher = controller.getUidWatcher();

        try {
            // First kill the process to start out in a stable state.
            controller.ensureProcessGone();

            // Do initial setup.
            controller.makeUidIdle();
            controller.removeFromWhitelist();
            controller.setAppOpMode(AppOpsManager.OPSTR_START_FOREGROUND, "allow");

            // Put app on whitelist, to allow service to run.
            controller.addToWhitelist();

            // We will use this to monitor when the service is running.
            conn.startMonitoring();

            // -------- START SERVICE AND THEN SUCCESSFULLY GO TO FOREGROUND

            // Now start the service and wait for it to come up.
            mContext.startService(mServiceStartForegroundIntent);
            conn.waitForConnect();

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Now take it out of foreground and confirm.
            mContext.startService(mServiceStopForegroundIntent);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Good, now stop the service and wait for it to go away.
            mContext.stopService(mServiceStartForegroundIntent);
            conn.waitForDisconnect();

            // There may be a transient STATE_SERVICE we don't care about, so waitFor.
            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // We don't want to wait for the uid to actually go idle, we can force it now.
            controller.makeUidIdle();
            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);

            // Make sure the process is gone so we start over fresh.
            controller.ensureProcessGone();

            // -------- START SERVICE AND BLOCK GOING TO FOREGROUND

            // Now we will deny the app op and ensure the service can't become foreground.
            controller.setAppOpMode(AppOpsManager.OPSTR_START_FOREGROUND, "ignore");

            // Now start the service and wait for it to come up.
            mContext.startService(mServiceStartForegroundIntent);
            conn.waitForConnect();

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Good, now stop the service and wait for it to go away.
            mContext.stopService(mServiceStartForegroundIntent);
            conn.waitForDisconnect();

            // THIS MUST BE AN EXPECT: we want to make sure we don't get in to STATE_FG_SERVICE.
            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Make sure the uid is idle (it should be anyway, it never went active here).
            controller.makeUidIdle();

            // Make sure the process is gone so we start over fresh.
            controller.ensureProcessGone();

            // -------- DIRECT START FOREGROUND SERVICE SUCCESSFULLY

            controller.setAppOpMode(AppOpsManager.OPSTR_START_FOREGROUND, "allow");

            // Now start the service and wait for it to come up.
            mContext.startForegroundService(mServiceStartForegroundIntent);
            conn.waitForConnect();

            // Make sure it becomes a foreground service.  The process state changes here
            // are weird looking because we first need to force the app out of idle to allow
            // it to start the service.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);
            // Remove tempwhitelist avoid temp white list block idle command and app crash occur.
            CtsAppTestUtils.executeShellCmd(mInstrumentation,
                    "cmd deviceidle tempwhitelist -r " + SIMPLE_PACKAGE_NAME);
            // Good, now stop the service and wait for it to go away.
            mContext.stopService(mServiceStartForegroundIntent);
            conn.waitForDisconnect();

            // There may be a transient STATE_SERVICE we don't care about, so waitFor.
            uidWatcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // We don't want to wait for the uid to actually go idle, we can force it now.
            controller.makeUidIdle();

            // Make sure the process is gone so we start over fresh.
            controller.ensureProcessGone();

            // -------- DIRECT START FOREGROUND SERVICE BLOCKED

            // Now we will deny the app op and ensure the service can't become foreground.
            controller.setAppOpMode(AppOpsManager.OPSTR_START_FOREGROUND, "ignore");

            // But we will put it on the whitelist so the service is still allowed to start.
            controller.addToWhitelist();

            // Now start the service and wait for it to come up.
            mContext.startForegroundService(mServiceStartForegroundIntent);
            conn.waitForConnect();

            // In this case we only get to run it as a regular service.
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);

            // Good, now stop the service and wait for it to go away.
            mContext.stopService(mServiceStartForegroundIntent);
            conn.waitForDisconnect();

            // THIS MUST BE AN EXPECT: we want to make sure we don't get in to STATE_FG_SERVICE.
            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);

            // Make sure the uid is idle (it should be anyway, it never went active here).
            controller.makeUidIdle();

            // Make sure the process is gone so we start over fresh.
            controller.ensureProcessGone();

            // -------- XXX NEED TO TEST NON-WHITELIST CASE WHERE NOTHING HAPPENS

        } finally {
            mContext.stopService(mServiceStartForegroundIntent);
            conn.stopMonitoringIfNeeded();
            controller.cleanup();
            controller.setAppOpMode(AppOpsManager.OPSTR_START_FOREGROUND, "allow");
            controller.removeFromWhitelist();
        }
    }

    /**
     * Verify that an app under background restrictions has its foreground services demoted to
     * ordinary service state when it is no longer the top app.
     */
    public void testBgRestrictedForegroundService() throws Exception {
        final Intent activityIntent = new Intent()
                .setClassName(SIMPLE_PACKAGE_NAME,
                        SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY_START_FG_SERVICE)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        final ServiceProcessController controller = new ServiceProcessController(mContext,
                getInstrumentation(), STUB_PACKAGE_NAME, mAllProcesses, WAIT_TIME);
        final WatchUidRunner uidWatcher = controller.getUidWatcher();

        final Intent homeIntent = new Intent()
                .setAction(Intent.ACTION_MAIN)
                .addCategory(Intent.CATEGORY_HOME)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                        | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);

        final Intent serviceStartIntent = new Intent(mService3Intent)
                .setAction(ACTION_START_THEN_FG);
        activityIntent.putExtra("service", serviceStartIntent);
        boolean activityStarted = false;

        try {
            // First kill the process to start out in a stable state.
            controller.ensureProcessGone();

            // Do initial setup.
            controller.denyAnyInBackgroundOp();
            controller.makeUidIdle();
            controller.removeFromWhitelist();
            controller.setAppOpMode(AppOpsManager.OPSTR_START_FOREGROUND, "allow");

            // Start the activity, which will start the fg service as well, and wait
            // for the report that it's all up and running.
            WaitForBroadcast waiter = new WaitForBroadcast(mInstrumentation.getTargetContext());
            waiter.prepare(ACTION_SIMPLE_ACTIVITY_START_FG_SERVICE_RESULT);

            activityIntent.setAction(ACTION_SIMPLE_ACTIVITY_START_FG);
            mTargetContext.startActivity(activityIntent);
            activityStarted = true;

            Intent resultIntent = waiter.doWait(WAIT_TIME);
            int brCode = resultIntent.getIntExtra("result", Activity.RESULT_CANCELED);
            if (brCode != Activity.RESULT_FIRST_USER) {
                fail("Failed starting service, result=" + brCode);
            }

            // activity is in front, fg service is running.  make sure that we see
            // the expected state at this point.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Switch to the home app; make sure the test app drops all the way
            // down to SERVICE, not FG_SERVICE
            mTargetContext.startActivity(homeIntent);
            uidWatcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_SERVICE);
        } finally {
            // tear down everything and we're done
            if (activityStarted) {
                activityIntent.setAction(ACTION_FINISH_EVERYTHING);
                mTargetContext.startActivity(activityIntent);
            }

            controller.cleanup();
        }

    }

    private boolean supportsCantSaveState() {
        if (mContext.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_CANT_SAVE_STATE)) {
            return true;
        }

        return false;
    }

    /**
     * Test that a single "can't save state" app has the proper process management semantics.
     */
    public void testCantSaveStateLaunchAndBackground() throws Exception {
        if (!supportsCantSaveState()) {
            return;
        }

        final Intent activityIntent = new Intent();
        activityIntent.setPackage(CANT_SAVE_STATE_1_PACKAGE_NAME);
        activityIntent.setAction(Intent.ACTION_MAIN);
        activityIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final Intent homeIntent = new Intent();
        homeIntent.setAction(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        homeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ActivityManager am = mContext.getSystemService(ActivityManager.class);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);

        // We don't want to wait for the uid to actually go idle, we can force it now.
        String cmd = "am make-uid-idle " + CANT_SAVE_STATE_1_PACKAGE_NAME;
        String result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        ApplicationInfo appInfo = mContext.getPackageManager().getApplicationInfo(
                CANT_SAVE_STATE_1_PACKAGE_NAME, 0);

        // This test is also using UidImportanceListener to make sure the correct
        // heavy-weight state is reported there.
        UidImportanceListener uidForegroundListener = new UidImportanceListener(mContext,
                appInfo.uid, IMPORTANCE_FOREGROUND,
                WAIT_TIME);
        uidForegroundListener.register();
        UidImportanceListener uidBackgroundListener = new UidImportanceListener(mContext,
                appInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_CANT_SAVE_STATE - 1,
                WAIT_TIME);
        uidBackgroundListener.register();
        UidImportanceListener uidCachedListener = new UidImportanceListener(mContext,
                appInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_CANT_SAVE_STATE + 1,
                WAIT_TIME);
        uidCachedListener.register();

        WatchUidRunner uidWatcher = new WatchUidRunner(mInstrumentation, appInfo.uid,
                WAIT_TIME);

        UiDevice device = UiDevice.getInstance(mInstrumentation);

        try {
            // Start the heavy-weight app, should launch like a normal app.
            mTargetContext.startActivity(activityIntent);
            waitForAppFocus(CANT_SAVE_STATE_1_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();

            // Wait for process state to reflect running activity.
            uidForegroundListener.waitForValue(
                    IMPORTANCE_FOREGROUND,
                    IMPORTANCE_FOREGROUND);
            assertEquals(IMPORTANCE_FOREGROUND,
                    am.getPackageImportance(CANT_SAVE_STATE_1_PACKAGE_NAME));

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Now go to home, leaving the app.  It should be put in the heavy weight state.
            mTargetContext.startActivity(homeIntent);

            final int expectedImportance =
                    (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.O)
                            ? ActivityManager.RunningAppProcessInfo.IMPORTANCE_CANT_SAVE_STATE
                            : ActivityManager.RunningAppProcessInfo.IMPORTANCE_CANT_SAVE_STATE_PRE_26;
            // Wait for process to go down to background heavy-weight.
            uidBackgroundListener.waitForValue(expectedImportance, expectedImportance);
            assertEquals(expectedImportance,
                    am.getPackageImportance(CANT_SAVE_STATE_1_PACKAGE_NAME));

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_HEAVY_WEIGHT);

            // While in background, should go in to normal idle state.
            // Force app to go idle now
            cmd = "am make-uid-idle " + CANT_SAVE_STATE_1_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);
            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);

            // Switch back to heavy-weight app to see if it correctly returns to foreground.
            mTargetContext.startActivity(activityIntent);

            // Wait for process state to reflect running activity.
            uidForegroundListener.waitForValue(
                    IMPORTANCE_FOREGROUND,
                    IMPORTANCE_FOREGROUND);
            assertEquals(IMPORTANCE_FOREGROUND,
                    am.getPackageImportance(CANT_SAVE_STATE_1_PACKAGE_NAME));

            // Also make sure the uid state reports are as expected.
            uidWatcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uidWatcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            waitForAppFocus(CANT_SAVE_STATE_1_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();

            // Exit activity, check to see if we are now cached.
            mInstrumentation.getUiAutomation().performGlobalAction(
                    AccessibilityService.GLOBAL_ACTION_BACK);
            // Hit back again in case the notification curtain is open
            mInstrumentation.getUiAutomation().performGlobalAction(
                    AccessibilityService.GLOBAL_ACTION_BACK);

            // Wait for process to become cached
            uidCachedListener.waitForValue(
                    IMPORTANCE_CACHED,
                    IMPORTANCE_CACHED);
            assertEquals(IMPORTANCE_CACHED,
                    am.getPackageImportance(CANT_SAVE_STATE_1_PACKAGE_NAME));

            uidWatcher.expect(WatchUidRunner.CMD_CACHED, null);
            uidWatcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_RECENT);

            // While in background, should go in to normal idle state.
            // Force app to go idle now
            cmd = "am make-uid-idle " + CANT_SAVE_STATE_1_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);
            uidWatcher.expect(WatchUidRunner.CMD_IDLE, null);

        } finally {
            uidWatcher.finish();
            uidForegroundListener.unregister();
            uidBackgroundListener.unregister();
            uidCachedListener.unregister();
        }
    }

    /**
     * Test that switching between two "can't save state" apps is handled properly.
     */
    public void testCantSaveStateLaunchAndSwitch() throws Exception {
        if (!supportsCantSaveState()) {
            return;
        }

        final Intent activity1Intent = new Intent();
        activity1Intent.setPackage(CANT_SAVE_STATE_1_PACKAGE_NAME);
        activity1Intent.setAction(Intent.ACTION_MAIN);
        activity1Intent.addCategory(Intent.CATEGORY_LAUNCHER);
        activity1Intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final Intent activity2Intent = new Intent();
        activity2Intent.setPackage(CANT_SAVE_STATE_2_PACKAGE_NAME);
        activity2Intent.setAction(Intent.ACTION_MAIN);
        activity2Intent.addCategory(Intent.CATEGORY_LAUNCHER);
        activity2Intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final Intent homeIntent = new Intent();
        homeIntent.setAction(Intent.ACTION_MAIN);
        homeIntent.addCategory(Intent.CATEGORY_HOME);
        homeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ActivityManager am = mContext.getSystemService(ActivityManager.class);
        UiDevice device = UiDevice.getInstance(mInstrumentation);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);

        // We don't want to wait for the uid to actually go idle, we can force it now.
        String cmd = "am make-uid-idle " + CANT_SAVE_STATE_1_PACKAGE_NAME;
        String result = SystemUtil.runShellCommand(mInstrumentation, cmd);
        cmd = "am make-uid-idle " + CANT_SAVE_STATE_2_PACKAGE_NAME;
        result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        ApplicationInfo app1Info = mContext.getPackageManager().getApplicationInfo(
                CANT_SAVE_STATE_1_PACKAGE_NAME, 0);
        WatchUidRunner uid1Watcher = new WatchUidRunner(mInstrumentation, app1Info.uid,
                WAIT_TIME);

        ApplicationInfo app2Info = mContext.getPackageManager().getApplicationInfo(
                CANT_SAVE_STATE_2_PACKAGE_NAME, 0);
        WatchUidRunner uid2Watcher = new WatchUidRunner(mInstrumentation, app2Info.uid,
                WAIT_TIME);

        try {
            // Start the first heavy-weight app, should launch like a normal app.
            mTargetContext.startActivity(activity1Intent);
            waitForAppFocus(CANT_SAVE_STATE_1_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();

            // Make sure the uid state reports are as expected.
            uid1Watcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uid1Watcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uid1Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Now go to home, leaving the app.  It should be put in the heavy weight state.
            mTargetContext.startActivity(homeIntent);

            // Wait for process to go down to background heavy-weight.
            uid1Watcher.expect(WatchUidRunner.CMD_CACHED, null);
            uid1Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_HEAVY_WEIGHT);

            // Start the second heavy-weight app, should ask us what to do with the two apps
            startActivityAndWaitForShow(activity2Intent);

            // First, let's try returning to the original app.
            maybeClick(device, new UiSelector().resourceId("android:id/switch_old"));
            waitForAppFocus(CANT_SAVE_STATE_1_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();

            // App should now be back in foreground.
            uid1Watcher.expect(WatchUidRunner.CMD_UNCACHED, null);
            uid1Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Return to home.
            mTargetContext.startActivity(homeIntent);
            uid1Watcher.expect(WatchUidRunner.CMD_CACHED, null);
            uid1Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_HEAVY_WEIGHT);

            // Again try starting second heavy-weight app to get prompt.
            startActivityAndWaitForShow(activity2Intent);

            // Now we'll switch to the new app.
            maybeClick(device, new UiSelector().resourceId("android:id/switch_new"));
            waitForAppFocus(CANT_SAVE_STATE_2_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();

            // The original app should now become cached.
            uid1Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_RECENT);

            // And the new app should start.
            uid2Watcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uid2Watcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uid2Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Make sure the original app is idle for cleanliness
            cmd = "am make-uid-idle " + CANT_SAVE_STATE_1_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);
            uid1Watcher.expect(WatchUidRunner.CMD_IDLE, null);

            // Return to home.
            mTargetContext.startActivity(homeIntent);
            uid2Watcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uid2Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_HEAVY_WEIGHT);

            // Try starting the first heavy weight app, but return to the existing second.
            startActivityAndWaitForShow(activity1Intent);
            maybeClick(device, new UiSelector().resourceId("android:id/switch_old"));
            waitForAppFocus(CANT_SAVE_STATE_2_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();
            uid2Watcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uid2Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Return to home.
            mTargetContext.startActivity(homeIntent);
            uid2Watcher.waitFor(WatchUidRunner.CMD_CACHED, null);
            uid2Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_HEAVY_WEIGHT);

            // Again start the first heavy weight app, this time actually switching to it
            startActivityAndWaitForShow(activity1Intent);
            maybeClick(device, new UiSelector().resourceId("android:id/switch_new"));
            waitForAppFocus(CANT_SAVE_STATE_1_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();

            // The second app should now become cached.
            uid2Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_RECENT);

            // And the first app should start.
            uid1Watcher.waitFor(WatchUidRunner.CMD_ACTIVE, null);
            uid1Watcher.waitFor(WatchUidRunner.CMD_UNCACHED, null);
            uid1Watcher.expect(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_TOP);

            // Exit activity, check to see if we are now cached.
            waitForAppFocus(CANT_SAVE_STATE_1_PACKAGE_NAME, WAIT_TIME);
            device.waitForIdle();
            mInstrumentation.getUiAutomation().performGlobalAction(
                    AccessibilityService.GLOBAL_ACTION_BACK);
            // Hit back again in case the notification curtain is open
            mInstrumentation.getUiAutomation().performGlobalAction(
                    AccessibilityService.GLOBAL_ACTION_BACK);
            uid1Watcher.expect(WatchUidRunner.CMD_CACHED, null);
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_RECENT);

            // Make both apps idle for cleanliness.
            cmd = "am make-uid-idle " + CANT_SAVE_STATE_1_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);
            cmd = "am make-uid-idle " + CANT_SAVE_STATE_2_PACKAGE_NAME;
            result = SystemUtil.runShellCommand(mInstrumentation, cmd);

        } finally {
            uid2Watcher.finish();
            uid1Watcher.finish();
        }
    }

    /**
     * Test a service binding cycle between two apps, with one of them also running a foreground
     * service. The other app should also get an FGS proc state. On stopping the foreground service,
     * app should go back to cached state.
     *
     * @throws Exception
     */
    public void testCycleFgs() throws Exception {
        ApplicationInfo app1Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP1, 0);
        ApplicationInfo app3Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP3, 0);
        WatchUidRunner uid1Watcher = new WatchUidRunner(mInstrumentation, app1Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid3Watcher = new WatchUidRunner(mInstrumentation, app3Info.uid,
                WAITFOR_MSEC);

        try {
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP3, 0, null);

            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Create a cycle
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            try {
                uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE,
                        WatchUidRunner.STATE_CACHED_EMPTY);
                fail("App3 should not be demoted to cached");
            } catch (IllegalStateException ise) {
                // Didn't go to cached in spite of cycle. Good!
            }

            // Stop the foreground service
            CommandReceiver.sendCommand(mContext, CommandReceiver
                            .COMMAND_STOP_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);

            // Check that the app's proc state has fallen
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
        } finally {
            // Clean up: unbind services to avoid from interferences with other tests
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP3, 0, null);

            uid1Watcher.finish();
            uid3Watcher.finish();
        }
    }

    /**
     * Test a service binding cycle between three apps, with one of them also running a foreground
     * service. The other apps should also get an FGS proc state. On stopping the foreground
     * service, app should go back to cached state.
     *
     * @throws Exception
     */
    public void testCycleFgsTriangle() throws Exception {
        ApplicationInfo app1Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP1, 0);
        ApplicationInfo app2Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP2, 0);
        ApplicationInfo app3Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP3, 0);
        WatchUidRunner uid1Watcher = new WatchUidRunner(mInstrumentation, app1Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid2Watcher = new WatchUidRunner(mInstrumentation, app2Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid3Watcher = new WatchUidRunner(mInstrumentation, app3Info.uid,
                WAITFOR_MSEC);

        try {
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, 0, null);

            uid2Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Bind from 2 to 3
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP3, 0, null);
            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Create a cycle
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            try {
                uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE,
                        WatchUidRunner.STATE_CACHED_EMPTY);
                fail("App3 should not be demoted to cached");
            } catch (IllegalStateException ise) {
                // Didn't go to cached in spite of cycle. Good!
            }

            try {
                uid2Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE,
                        WatchUidRunner.STATE_CACHED_EMPTY);
                fail("App2 should not be demoted to cached");
            } catch (IllegalStateException ise) {
                // Didn't go to cached in spite of cycle. Good!
            }

            try {
                uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE,
                        WatchUidRunner.STATE_CACHED_EMPTY);
                fail("App1 should not be demoted to cached");
            } catch (IllegalStateException ise) {
                // Didn't go to cached in spite of cycle. Good!
            }

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, 0, null);

            uid2Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
        } finally {
            // Clean up: unbind services to avoid from interferences with other tests
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP3, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);
            // Stop the foreground service
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_STOP_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);

            uid1Watcher.finish();
            uid2Watcher.finish();
            uid3Watcher.finish();
        }
    }

    /**
     * Test a service binding cycle between three apps, with one of them also running a foreground
     * service. The other apps should also get an FGS proc state. On stopping the foreground
     * service, app should go back to cached state.
     *
     * @throws Exception
     */
    public void testCycleFgsTriangleBiDi() throws Exception {
        ApplicationInfo app1Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP1, 0);
        ApplicationInfo app2Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP2, 0);
        ApplicationInfo app3Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP3, 0);
        WatchUidRunner uid1Watcher = new WatchUidRunner(mInstrumentation, app1Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid2Watcher = new WatchUidRunner(mInstrumentation, app2Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid3Watcher = new WatchUidRunner(mInstrumentation, app3Info.uid,
                WAITFOR_MSEC);

        try {
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Bind from 1 to 2, 1 to 3
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP3, 0, null);

            uid2Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);
            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_FG_SERVICE);

            // Bind from 2 to 3, 3 to 2, 3 to 1
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP3, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP2, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            try {
                uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE,
                        WatchUidRunner.STATE_CACHED_EMPTY);
                fail("App3 should not be demoted to cached");
            } catch (IllegalStateException ise) {
                // Didn't go to cached in spite of cycle. Good!
            }

            // Stop the foreground service
            CommandReceiver.sendCommand(mContext, CommandReceiver
                            .COMMAND_STOP_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);

            // Check that the apps' proc state has fallen
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
            uid2Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
        } finally {
            // Clean up: unbind services to avoid from interferences with other tests
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP3, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP3, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP2, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            uid1Watcher.finish();
            uid2Watcher.finish();
            uid3Watcher.finish();
        }
    }

    /**
     * Test process states for foreground service binding to another app, with and without
     * BIND_INCLUDE_CAPABILITIES. With BIND_INCLUDE_CAPABILITIES flag,
     * PROCESS_CAPABILITY_FOREGROUND_LOCATION can be passed from client to service. Without
     * BIND_INCLUDE_CAPABILITIES flag, PROCESS_CAPABILITY_FOREGROUND_LOCATION can not be passed from
     * client to service.
     * @throws Exception
     */
    public void testFgsLocationBind() throws Exception {
        setupWatchers(3);

        try {
            // Put Package1 in TOP state, now it gets all capability (because the TOP process
            // gets all while-in-use permission (not from FGSL).
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_ACTIVITY,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);
            mWatchers[0].waitFor(WatchUidRunner.CMD_PROCSTATE,
                    WatchUidRunner.STATE_TOP,
                    new Integer(PROCESS_CAPABILITY_FOREGROUND_LOCATION
                            | PROCESS_CAPABILITY_ALL));

            // Start a FGS
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE,
                    mAppInfo[0].packageName, mAppInfo[0].packageName, 0, null);

            // Start a FGSL
            Bundle bundle = new Bundle();
            bundle.putInt(LocalForegroundServiceLocation.EXTRA_FOREGROUND_SERVICE_TYPE,
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION);
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE_LOCATION,
                    mAppInfo[0].packageName, mAppInfo[0].packageName, 0, bundle);

            // Stop the activity.
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_STOP_ACTIVITY,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);

            mWatchers[0].waitFor(WatchUidRunner.CMD_PROCSTATE,
                    WatchUidRunner.STATE_FG_SERVICE,
                    new Integer(PROCESS_CAPABILITY_FOREGROUND_LOCATION
                            | PROCESS_CAPABILITY_ALL_IMPLICIT));

            // Bind App 0 -> App 1, verify doesn't include capability.
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    mAppInfo[0].packageName, mAppInfo[1].packageName, 0, null);
            // Verify app1 does NOT have capability.
            mWatchers[1].waitFor(WatchUidRunner.CMD_PROCSTATE,
                    WatchUidRunner.STATE_FG_SERVICE,
                    new Integer(PROCESS_CAPABILITY_ALL_IMPLICIT));

            // Bind App 0 -> App 2, include capability.
            bundle = new Bundle();
            bundle.putInt(CommandReceiver.EXTRA_FLAGS, Context.BIND_INCLUDE_CAPABILITIES);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    mAppInfo[0].packageName, mAppInfo[2].packageName, 0, bundle);
            // Verify app2 has FOREGROUND_LOCATION capability.
            mWatchers[2].waitFor(WatchUidRunner.CMD_PROCSTATE,
                    WatchUidRunner.STATE_FG_SERVICE,
                    new Integer(PROCESS_CAPABILITY_FOREGROUND_LOCATION
                            | PROCESS_CAPABILITY_ALL_IMPLICIT));

            // Back down to foreground service
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_STOP_FOREGROUND_SERVICE_LOCATION,
                    mAppInfo[0].packageName, mAppInfo[0].packageName, 0, null);
            // Verify app0 does NOT have FOREGROUND_LOCATION capability.
            mWatchers[0].waitFor(WatchUidRunner.CMD_PROCSTATE,
                    WatchUidRunner.STATE_FG_SERVICE,
                    new Integer(PROCESS_CAPABILITY_ALL_IMPLICIT));

            // Remove foreground service as well
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_STOP_FOREGROUND_SERVICE,
                    mAppInfo[0].packageName, mAppInfo[0].packageName, 0, null);
            mWatchers[0].waitFor(WatchUidRunner.CMD_PROCSTATE,
                    WatchUidRunner.STATE_CACHED_EMPTY,
                    new Integer(PROCESS_CAPABILITY_NONE));
        } finally {
            // Clean up: unbind services to avoid from interferences with other tests
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    mAppInfo[0].packageName, mAppInfo[1].packageName, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    mAppInfo[0].packageName, mAppInfo[2].packageName, 0, null);

            shutdownWatchers();
        }
    }

    /**
     * Test process states for top app binding with and without BIND_INCLUDE_CAPABILITIES flag.
     * Bound app should be TOP w/flag and BTOP without flag.
     * @throws Exception
     */
    public void testTopBind() throws Exception {
        setupWatchers(2);

        Activity activity = null;

        try {
            // This will start an activity in App0
            activity = startSubActivity(ScreenOnActivity.class);

            // Bind Stub -> App 0, verify doesn't include capability (only BTOP, not TOP)
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    STUB_PACKAGE_NAME, mAppInfo[0].packageName, 0, null);
            mWatchers[0].waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_BOUND_TOP,
                    new Integer(PROCESS_CAPABILITY_NONE));

            // Bind Stub -> App 1, include capability (TOP)
            Bundle bundle = new Bundle();
            bundle.putInt(CommandReceiver.EXTRA_FLAGS, Context.BIND_INCLUDE_CAPABILITIES);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    STUB_PACKAGE_NAME, mAppInfo[1].packageName, 0, bundle);
            mWatchers[1].waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_BOUND_TOP,
                    new Integer(PROCESS_CAPABILITY_ALL));
        } finally {
            // Clean up: unbind services to avoid from interferences with other tests
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    STUB_PACKAGE_NAME, mAppInfo[0].packageName, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    STUB_PACKAGE_NAME, mAppInfo[1].packageName, 0, null);

            shutdownWatchers();
            if (activity != null) {
                activity.finish();
            }
        }
    }

    private final <T extends Activity> Activity startSubActivity(Class<T> activityClass) {
        final Instrumentation.ActivityResult result = new Instrumentation.ActivityResult(
                0, new Intent());
        final Instrumentation.ActivityMonitor monitor = new Instrumentation.ActivityMonitor(
                activityClass.getName(), result, false);
        mInstrumentation.addMonitor(monitor);
        launchActivity(STUB_PACKAGE_NAME, activityClass, null);
        return monitor.waitForActivity();
    }

    public void testCycleTop() throws Exception {
        ApplicationInfo app1Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP1, 0);
        ApplicationInfo app2Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP2, 0);
        ApplicationInfo app3Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP3, 0);

        PermissionUtils.grantPermission(
                PACKAGE_NAME_APP1, android.Manifest.permission.PACKAGE_USAGE_STATS);
        PermissionUtils.grantPermission(
                PACKAGE_NAME_APP2, android.Manifest.permission.PACKAGE_USAGE_STATS);
        PermissionUtils.grantPermission(
                PACKAGE_NAME_APP3, android.Manifest.permission.PACKAGE_USAGE_STATS);

        UidImportanceListener uid1Listener = new UidImportanceListener(mContext,
                app1Info.uid, IMPORTANCE_VISIBLE,
                WAITFOR_MSEC);
        uid1Listener.register();

        UidImportanceListener uid1ServiceListener = new UidImportanceListener(mContext,
                app1Info.uid, IMPORTANCE_CACHED,
                WAITFOR_MSEC);
        uid1ServiceListener.register();

        UidImportanceListener uid2Listener = new UidImportanceListener(mContext,
                app2Info.uid, IMPORTANCE_VISIBLE,
                WAITFOR_MSEC);
        uid2Listener.register();

        UidImportanceListener uid2ServiceListener = new UidImportanceListener(mContext,
                app2Info.uid, IMPORTANCE_CACHED,
                WAITFOR_MSEC);
        uid2ServiceListener.register();

        UidImportanceListener uid3Listener = new UidImportanceListener(mContext,
                app3Info.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_SERVICE,
                WAITFOR_MSEC);
        uid3Listener.register();

        UidImportanceListener uid3ServiceListener = new UidImportanceListener(mContext,
                app3Info.uid, IMPORTANCE_CACHED,
                WAITFOR_MSEC);
        uid3ServiceListener.register();

        Activity activity = null;

        try {
            // Start an activity
            activity = startSubActivity(ScreenOnActivity.class);

            // Start a FGS in app2
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE, PACKAGE_NAME_APP2,
                    PACKAGE_NAME_APP2, 0, null);

            uid2Listener.waitForValue(
                    IMPORTANCE_FOREGROUND_SERVICE,
                    IMPORTANCE_FOREGROUND_SERVICE);

            // Bind from TOP to the service in app1
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    STUB_PACKAGE_NAME, PACKAGE_NAME_APP1, 0, null);

            uid1Listener.waitForValue(IMPORTANCE_FOREGROUND,
                    IMPORTANCE_FOREGROUND_SERVICE);

            // Bind from app1 to a service in app2
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, 0, null);

            // Bind from app2 to a service in app3
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP3, 0, null);

            uid3Listener.waitForValue(IMPORTANCE_FOREGROUND,
                    IMPORTANCE_FOREGROUND_SERVICE);

            // Create a cycle
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            try {
                uid3Listener.waitForValue(IMPORTANCE_CACHED,
                        IMPORTANCE_CACHED);
                fail("App3 should not be demoted to cached, expecting FGS");
            } catch (IllegalStateException e) {
                // Didn't go to cached in spite of cycle. Good!
            }

            // Unbind from the TOP app
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    STUB_PACKAGE_NAME, PACKAGE_NAME_APP1, 0, null);

            // Check that the apps' proc state is FOREGROUND_SERVICE
            uid2Listener.waitForValue(
                    IMPORTANCE_FOREGROUND_SERVICE,
                    IMPORTANCE_FOREGROUND_SERVICE);

            // Stop the foreground service
            CommandReceiver.sendCommand(mContext, CommandReceiver
                            .COMMAND_STOP_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP2, 0, null);

            // Check that the apps fall down to cached state
            uid1ServiceListener.waitForValue(
                    IMPORTANCE_CACHED,
                    IMPORTANCE_CACHED);

            uid2ServiceListener.waitForValue(
                    IMPORTANCE_CACHED,
                    IMPORTANCE_CACHED);

            uid3ServiceListener.waitForValue(
                    IMPORTANCE_CACHED,
                    IMPORTANCE_CACHED);
        } finally {
            // Clean up: unbind services to avoid from interferences with other tests
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP2, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP3, 0, null);
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            uid1Listener.unregister();
            uid1ServiceListener.unregister();
            uid2Listener.unregister();
            uid2ServiceListener.unregister();
            uid3Listener.unregister();
            uid3ServiceListener.unregister();
            if (activity != null) {
                activity.finish();
            }
        }
    }

    public void testCycleFgAppAndAlert() throws Exception {
        ApplicationInfo stubInfo = mContext.getPackageManager().getApplicationInfo(
                STUB_PACKAGE_NAME, 0);
        ApplicationInfo app1Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP1, 0);
        ApplicationInfo app2Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP2, 0);
        ApplicationInfo app3Info = mContext.getPackageManager().getApplicationInfo(
                PACKAGE_NAME_APP3, 0);

        PermissionUtils.grantPermission(
                STUB_PACKAGE_NAME, android.Manifest.permission.PACKAGE_USAGE_STATS);
        PermissionUtils.grantPermission(
                PACKAGE_NAME_APP1, android.Manifest.permission.PACKAGE_USAGE_STATS);
        PermissionUtils.grantPermission(
                PACKAGE_NAME_APP2, android.Manifest.permission.PACKAGE_USAGE_STATS);
        PermissionUtils.grantPermission(
                PACKAGE_NAME_APP3, android.Manifest.permission.PACKAGE_USAGE_STATS);

        UidImportanceListener stubListener = new UidImportanceListener(mContext,
                stubInfo.uid, ActivityManager.RunningAppProcessInfo.IMPORTANCE_PERCEPTIBLE,
                WAITFOR_MSEC);
        stubListener.register();

        UidImportanceListener uid1Listener = new UidImportanceListener(mContext,
                app1Info.uid, IMPORTANCE_VISIBLE,
                WAITFOR_MSEC);
        uid1Listener.register();

        UidImportanceListener uid2Listener = new UidImportanceListener(mContext,
                app2Info.uid, IMPORTANCE_VISIBLE,
                WAITFOR_MSEC);
        uid2Listener.register();

        UidImportanceListener uid3Listener = new UidImportanceListener(mContext,
                app3Info.uid, IMPORTANCE_VISIBLE,
                WAITFOR_MSEC);
        uid3Listener.register();

        WatchUidRunner uid1Watcher = new WatchUidRunner(mInstrumentation, app1Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid2Watcher = new WatchUidRunner(mInstrumentation, app2Info.uid,
                WAITFOR_MSEC);
        WatchUidRunner uid3Watcher = new WatchUidRunner(mInstrumentation, app3Info.uid,
                WAITFOR_MSEC);

        try {
            // Stub app should have been in foreground since it's being instrumented.

            PermissionUtils.grantPermission(
                    STUB_PACKAGE_NAME, android.Manifest.permission.SYSTEM_ALERT_WINDOW);
            // Show an alert on app0
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_ALERT_SERVICE, STUB_PACKAGE_NAME,
                    STUB_PACKAGE_NAME, 0, null);

            // Start a FGS in app2
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_START_FOREGROUND_SERVICE, PACKAGE_NAME_APP2,
                    PACKAGE_NAME_APP2, 0, null);

            uid2Listener.waitForValue(IMPORTANCE_FOREGROUND_SERVICE,
                    IMPORTANCE_FOREGROUND_SERVICE);

            // Bind from app0 to a service in app1
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    STUB_PACKAGE_NAME, PACKAGE_NAME_APP1, 0, null);

            // Bind from app2 to a service in app1
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP1, 0, null);

            // Bind from app3 to a service in app1
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            // Create a cycle
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_BIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP3, 0, null);

            uid1Listener.waitForValue(IMPORTANCE_FOREGROUND_SERVICE,
                    IMPORTANCE_FOREGROUND_SERVICE);
            uid3Listener.waitForValue(IMPORTANCE_FOREGROUND_SERVICE,
                    IMPORTANCE_FOREGROUND_SERVICE);

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    STUB_PACKAGE_NAME, PACKAGE_NAME_APP1, 0, null);

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP1, 0, null);

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP3, PACKAGE_NAME_APP1, 0, null);

            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_UNBIND_SERVICE,
                    PACKAGE_NAME_APP1, PACKAGE_NAME_APP3, 0, null);

            // Stop the foreground service
            CommandReceiver.sendCommand(mContext, CommandReceiver.COMMAND_STOP_FOREGROUND_SERVICE,
                    PACKAGE_NAME_APP2, PACKAGE_NAME_APP2, 0, null);

            // hide the alert
            CommandReceiver.sendCommand(mContext,
                    CommandReceiver.COMMAND_STOP_ALERT_SERVICE, STUB_PACKAGE_NAME,
                    STUB_PACKAGE_NAME, 0, null);

            // Check that the apps' proc state has fallen
            uid1Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
            uid2Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
            uid3Watcher.waitFor(WatchUidRunner.CMD_PROCSTATE, WatchUidRunner.STATE_CACHED_EMPTY);
        } finally {
            stubListener.unregister();
            uid1Listener.unregister();
            uid2Listener.unregister();
            uid3Listener.unregister();
            uid1Watcher.finish();
            uid2Watcher.finish();
            uid3Watcher.finish();
        }
    }
}
