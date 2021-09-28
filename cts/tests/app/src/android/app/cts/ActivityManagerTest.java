/*
 * Copyright (C) 2008 The Android Open Source Project
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

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.app.ActivityManager.RunningServiceInfo;
import android.app.ActivityManager.RunningTaskInfo;
import android.app.ActivityOptions;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.app.PendingIntent;
import android.app.stubs.ActivityManagerRecentOneActivity;
import android.app.stubs.ActivityManagerRecentTwoActivity;
import android.app.stubs.CommandReceiver;
import android.app.stubs.MockApplicationActivity;
import android.app.stubs.MockService;
import android.app.stubs.ScreenOnActivity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ConfigurationInfo;
import android.content.res.Resources;
import android.os.SystemClock;
import android.platform.test.annotations.RestrictedBuildTest;
import android.support.test.uiautomator.UiDevice;
import android.test.InstrumentationTestCase;
import android.util.Log;

import com.android.compatibility.common.util.AmMonitor;
import com.android.compatibility.common.util.SystemUtil;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;
import java.util.function.Supplier;

public class ActivityManagerTest extends InstrumentationTestCase {
    private static final String TAG = ActivityManagerTest.class.getSimpleName();
    private static final String STUB_PACKAGE_NAME = "android.app.stubs";
    private static final int WAITFOR_MSEC = 5000;
    private static final String SERVICE_NAME = "android.app.stubs.MockService";
    private static final int WAIT_TIME = 2000;
    // A secondary test activity from another APK.
    static final String SIMPLE_PACKAGE_NAME = "com.android.cts.launcherapps.simpleapp";
    static final String SIMPLE_ACTIVITY = ".SimpleActivity";
    static final String SIMPLE_ACTIVITY_IMMEDIATE_EXIT = ".SimpleActivityImmediateExit";
    static final String SIMPLE_ACTIVITY_CHAIN_EXIT = ".SimpleActivityChainExit";
    static final String SIMPLE_RECEIVER = ".SimpleReceiver";
    static final String SIMPLE_REMOTE_RECEIVER = ".SimpleRemoteReceiver";
    // The action sent back by the SIMPLE_APP after a restart.
    private static final String ACTIVITY_LAUNCHED_ACTION =
            "com.android.cts.launchertests.LauncherAppsTests.LAUNCHED_ACTION";
    // The action sent back by the SIMPLE_APP_IMMEDIATE_EXIT when it terminates.
    private static final String ACTIVITY_EXIT_ACTION =
            "com.android.cts.launchertests.LauncherAppsTests.EXIT_ACTION";
    // The action sent back by the SIMPLE_APP_CHAIN_EXIT when the task chain ends. 
    private static final String ACTIVITY_CHAIN_EXIT_ACTION =
            "com.android.cts.launchertests.LauncherAppsTests.CHAIN_EXIT_ACTION";
    // The action sent to identify the time track info.
    private static final String ACTIVITY_TIME_TRACK_INFO = "com.android.cts.TIME_TRACK_INFO";

    private static final String PACKAGE_NAME_APP1 = "com.android.app1";

    private static final String MCC_TO_UPDATE = "987";
    private static final String MNC_TO_UPDATE = "654";
    private static final String SHELL_COMMAND_GET_CONFIG = "am get-config";
    private static final String SHELL_COMMAND_RESULT_CONFIG_NAME_MCC = "mcc";
    private static final String SHELL_COMMAND_RESULT_CONFIG_NAME_MNC = "mnc";

    // Return states of the ActivityReceiverFilter.
    public static final int RESULT_PASS = 1;
    public static final int RESULT_FAIL = 2;
    public static final int RESULT_TIMEOUT = 3;

    private Context mTargetContext;
    private ActivityManager mActivityManager;
    private Intent mIntent;
    private List<Activity> mStartedActivityList;
    private int mErrorProcessID;
    private Instrumentation mInstrumentation;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mInstrumentation = getInstrumentation();
        mTargetContext = mInstrumentation.getTargetContext();
        mActivityManager = (ActivityManager) mInstrumentation.getContext()
                .getSystemService(Context.ACTIVITY_SERVICE);
        mStartedActivityList = new ArrayList<Activity>();
        mErrorProcessID = -1;
        startSubActivity(ScreenOnActivity.class);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (mIntent != null) {
            mInstrumentation.getContext().stopService(mIntent);
        }
        for (int i = 0; i < mStartedActivityList.size(); i++) {
            mStartedActivityList.get(i).finish();
        }
        if (mErrorProcessID != -1) {
            android.os.Process.killProcess(mErrorProcessID);
        }
    }

    public void testGetRecentTasks() throws Exception {
        int maxNum = 0;
        int flags = 0;

        List<RecentTaskInfo> recentTaskList;
        // Test parameter: maxNum is set to 0
        recentTaskList = mActivityManager.getRecentTasks(maxNum, flags);
        assertNotNull(recentTaskList);
        assertTrue(recentTaskList.size() == 0);
        // Test parameter: maxNum is set to 50
        maxNum = 50;
        recentTaskList = mActivityManager.getRecentTasks(maxNum, flags);
        assertNotNull(recentTaskList);
        // start recent1_activity.
        startSubActivity(ActivityManagerRecentOneActivity.class);
        Thread.sleep(WAIT_TIME);
        // start recent2_activity
        startSubActivity(ActivityManagerRecentTwoActivity.class);
        Thread.sleep(WAIT_TIME);
        /*
         * assert both recent1_activity and recent2_activity exist in the recent
         * tasks list. Moreover,the index of the recent2_activity is smaller
         * than the index of recent1_activity
         */
        recentTaskList = mActivityManager.getRecentTasks(maxNum, flags);
        int indexRecentOne = -1;
        int indexRecentTwo = -1;
        int i = 0;
        for (RecentTaskInfo rti : recentTaskList) {
            if (rti.baseIntent.getComponent().getClassName().equals(
                    ActivityManagerRecentOneActivity.class.getName())) {
                indexRecentOne = i;
            } else if (rti.baseIntent.getComponent().getClassName().equals(
                    ActivityManagerRecentTwoActivity.class.getName())) {
                indexRecentTwo = i;
            }
            i++;
        }
        assertTrue(indexRecentOne != -1 && indexRecentTwo != -1);
        assertTrue(indexRecentTwo < indexRecentOne);

        try {
            mActivityManager.getRecentTasks(-1, 0);
            fail("Should throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // expected exception
        }
    }

    public void testGetRecentTasksLimitedToCurrentAPK() throws Exception {
        int maxNum = 0;
        int flags = 0;

        // Check the number of tasks at this time.
        List<RecentTaskInfo>  recentTaskList;
        recentTaskList = mActivityManager.getRecentTasks(maxNum, flags);
        int numberOfEntriesFirstRun = recentTaskList.size();

        // Start another activity from another APK.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        ActivityReceiverFilter receiver = new ActivityReceiverFilter(ACTIVITY_LAUNCHED_ACTION);
        mTargetContext.startActivity(intent);

        // Make sure the activity has really started.
        assertEquals(RESULT_PASS, receiver.waitForActivity());
        receiver.close();

        // There shouldn't be any more tasks in this list at this time.
        recentTaskList = mActivityManager.getRecentTasks(maxNum, flags);
        int numberOfEntriesSecondRun = recentTaskList.size();
        assertTrue(numberOfEntriesSecondRun == numberOfEntriesFirstRun);

        // Tell the activity to finalize.
        intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        intent.putExtra("finish", true);
        mTargetContext.startActivity(intent);
    }

    // The receiver filter needs to be instantiated with the command to filter for before calling
    // startActivity.
    private class ActivityReceiverFilter extends BroadcastReceiver {
        // The activity we want to filter for.
        private String mActivityToFilter;
        private int result = RESULT_TIMEOUT;
        public long mTimeUsed = 0;
        private static final int TIMEOUT_IN_MS = 5000;

        // Create the filter with the intent to look for.
        public ActivityReceiverFilter(String activityToFilter) {
            mActivityToFilter = activityToFilter;
            IntentFilter filter = new IntentFilter();
            filter.addAction(mActivityToFilter);
            mInstrumentation.getTargetContext().registerReceiver(this, filter);
        }

        // Turn off the filter.
        public void close() {
            mInstrumentation.getTargetContext().unregisterReceiver(this);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(mActivityToFilter)) {
                synchronized(this) {
                   result = RESULT_PASS;
                   if (mActivityToFilter.equals(ACTIVITY_TIME_TRACK_INFO)) {
                       mTimeUsed = intent.getExtras().getLong(
                               ActivityOptions.EXTRA_USAGE_TIME_REPORT);
                   }
                   notifyAll();
                }
            }
        }

        public int waitForActivity() {
            synchronized(this) {
                try {
                    wait(TIMEOUT_IN_MS);
                } catch (InterruptedException e) {
                }
            }
            return result;
        }
    }

    private final <T extends Activity> void startSubActivity(Class<T> activityClass) {
        final Instrumentation.ActivityResult result = new ActivityResult(0, new Intent());
        final ActivityMonitor monitor = new ActivityMonitor(activityClass.getName(), result, false);
        mInstrumentation.addMonitor(monitor);
        launchActivity(STUB_PACKAGE_NAME, activityClass, null);
        mStartedActivityList.add(monitor.waitForActivity());
    }

    public void testGetRunningTasks() {
        // Test illegal parameter
        List<RunningTaskInfo> runningTaskList;
        runningTaskList = mActivityManager.getRunningTasks(-1);
        assertTrue(runningTaskList.size() == 0);

        runningTaskList = mActivityManager.getRunningTasks(0);
        assertTrue(runningTaskList.size() == 0);

        runningTaskList = mActivityManager.getRunningTasks(20);
        int taskSize = runningTaskList.size();
        assertTrue(taskSize >= 0 && taskSize <= 20);

        // start recent1_activity.
        startSubActivity(ActivityManagerRecentOneActivity.class);
        // start recent2_activity
        startSubActivity(ActivityManagerRecentTwoActivity.class);

        /*
         * assert both recent1_activity and recent2_activity exist in the
         * running tasks list. Moreover,the index of the recent2_activity is
         * smaller than the index of recent1_activity
         */
        runningTaskList = mActivityManager.getRunningTasks(20);
        int indexRecentOne = -1;
        int indexRecentTwo = -1;
        int i = 0;
        for (RunningTaskInfo rti : runningTaskList) {
            if (rti.baseActivity.getClassName().equals(
                    ActivityManagerRecentOneActivity.class.getName())) {
                indexRecentOne = i;
            } else if (rti.baseActivity.getClassName().equals(
                    ActivityManagerRecentTwoActivity.class.getName())) {
                indexRecentTwo = i;
            }
            i++;
        }
        assertTrue(indexRecentOne != -1 && indexRecentTwo != -1);
        assertTrue(indexRecentTwo < indexRecentOne);
    }

    public void testGetRunningServices() throws Exception {
        // Test illegal parameter
        List<RunningServiceInfo> runningServiceInfo;
        runningServiceInfo = mActivityManager.getRunningServices(-1);
        assertTrue(runningServiceInfo.isEmpty());

        runningServiceInfo = mActivityManager.getRunningServices(0);
        assertTrue(runningServiceInfo.isEmpty());

        runningServiceInfo = mActivityManager.getRunningServices(5);
        assertTrue(runningServiceInfo.size() <= 5);

        Intent intent = new Intent();
        intent.setClass(mInstrumentation.getTargetContext(), MockService.class);
        intent.putExtra(MockService.EXTRA_NO_STOP, true);
        mInstrumentation.getTargetContext().startService(intent);
        MockService.waitForStart(WAIT_TIME);

        runningServiceInfo = mActivityManager.getRunningServices(Integer.MAX_VALUE);
        boolean foundService = false;
        for (RunningServiceInfo rs : runningServiceInfo) {
            if (rs.service.getClassName().equals(SERVICE_NAME)) {
                foundService = true;
                break;
            }
        }
        assertTrue(foundService);
        MockService.prepareDestroy();
        mTargetContext.stopService(intent);
        boolean destroyed = MockService.waitForDestroy(WAIT_TIME);
        assertTrue(destroyed);
    }

    private void executeAndLogShellCommand(String cmd) throws IOException {
        final UiDevice uiDevice = UiDevice.getInstance(mInstrumentation);
        final String output = uiDevice.executeShellCommand(cmd);
        Log.d(TAG, "executed[" + cmd + "]; output[" + output.trim() + "]");
    }

    private String executeShellCommand(String cmd) throws IOException {
        final UiDevice uiDevice = UiDevice.getInstance(mInstrumentation);
        return uiDevice.executeShellCommand(cmd).trim();
    }

    private void setForcedAppStandby(String packageName, boolean enabled) throws IOException {
        final StringBuilder cmdBuilder = new StringBuilder("appops set ")
                .append(packageName)
                .append(" RUN_ANY_IN_BACKGROUND ")
                .append(enabled ? "ignore" : "allow");
        executeAndLogShellCommand(cmdBuilder.toString());
    }

    public void testIsBackgroundRestricted() throws IOException {
        // This instrumentation runs in the target package's uid.
        final Context targetContext = mInstrumentation.getTargetContext();
        final String targetPackage = targetContext.getPackageName();
        final ActivityManager am = targetContext.getSystemService(ActivityManager.class);
        setForcedAppStandby(targetPackage, true);
        assertTrue(am.isBackgroundRestricted());
        setForcedAppStandby(targetPackage, false);
        assertFalse(am.isBackgroundRestricted());
    }

    public void testGetMemoryInfo() {
        ActivityManager.MemoryInfo outInfo = new ActivityManager.MemoryInfo();
        mActivityManager.getMemoryInfo(outInfo);
        assertTrue(outInfo.lowMemory == (outInfo.availMem <= outInfo.threshold));
    }

    public void testGetRunningAppProcesses() throws Exception {
        List<RunningAppProcessInfo> list = mActivityManager.getRunningAppProcesses();
        assertNotNull(list);
        final String SYSTEM_PROCESS = "system";
        boolean hasSystemProcess = false;
        // The package name is also the default name for the application process
        final String TEST_PROCESS = STUB_PACKAGE_NAME;
        boolean hasTestProcess = false;
        for (RunningAppProcessInfo ra : list) {
            if (ra.processName.equals(SYSTEM_PROCESS)) {
                hasSystemProcess = true;

                // Make sure the importance is a sane value.
                assertTrue(ra.importance >= RunningAppProcessInfo.IMPORTANCE_FOREGROUND);
                assertTrue(ra.importance < RunningAppProcessInfo.IMPORTANCE_GONE);
            } else if (ra.processName.equals(TEST_PROCESS)) {
                hasTestProcess = true;
            }
        }

        // For security reasons the system process is not exposed.
        assertFalse(hasSystemProcess);
        assertTrue(hasTestProcess);

        for (RunningAppProcessInfo ra : list) {
            if (ra.processName.equals("android.app.stubs:remote")) {
                fail("should be no process named android.app.stubs:remote");
            }
        }
        // start a new process
        // XXX would be a lot cleaner to bind instead of start.
        mIntent = new Intent("android.app.REMOTESERVICE");
        mIntent.setPackage("android.app.stubs");
        mInstrumentation.getTargetContext().startService(mIntent);
        Thread.sleep(WAITFOR_MSEC);

        List<RunningAppProcessInfo> listNew = mActivityManager.getRunningAppProcesses();
        mInstrumentation.getTargetContext().stopService(mIntent);

        for (RunningAppProcessInfo ra : listNew) {
            if (ra.processName.equals("android.app.stubs:remote")) {
                return;
            }
        }
        fail("android.app.stubs:remote process should be available");
    }

    public void testGetMyMemoryState() {
        final RunningAppProcessInfo ra = new RunningAppProcessInfo();
        ActivityManager.getMyMemoryState(ra);

        assertEquals(android.os.Process.myUid(), ra.uid);

        // When an instrumentation test is running, the importance is high.
        assertEquals(RunningAppProcessInfo.IMPORTANCE_FOREGROUND, ra.importance);
    }

    public void testGetProcessInErrorState() throws Exception {
        List<ActivityManager.ProcessErrorStateInfo> errList = null;
        errList = mActivityManager.getProcessesInErrorState();
    }

    public void testGetDeviceConfigurationInfo() {
        ConfigurationInfo conInf = mActivityManager.getDeviceConfigurationInfo();
        assertNotNull(conInf);
    }

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
    public void testUpdateMccMncConfiguration() throws Exception {
        // Store the original mcc mnc to set back
        String[] mccMncConfigOriginal = new String[2];
        // Store other configs to check they won't be affected
        Set<String> otherConfigsOriginal = new HashSet<String>();
        getMccMncConfigsAndOthers(mccMncConfigOriginal, otherConfigsOriginal);

        String[] mccMncConfigToUpdate = new String[] {MCC_TO_UPDATE, MNC_TO_UPDATE};
        boolean success = ShellIdentityUtils.invokeMethodWithShellPermissions(mActivityManager,
                (am) -> am.updateMccMncConfiguration(mccMncConfigToUpdate[0],
                        mccMncConfigToUpdate[1]));

        if (success) {
            String[] mccMncConfigUpdated = new String[2];
            Set<String> otherConfigsUpdated = new HashSet<String>();
            getMccMncConfigsAndOthers(mccMncConfigUpdated, otherConfigsUpdated);
            // Check the mcc mnc are updated as expected
            assertTrue(Arrays.equals(mccMncConfigToUpdate, mccMncConfigUpdated));
            // Check other configs are not changed
            assertTrue(otherConfigsOriginal.equals(otherConfigsUpdated));
        }

        // Set mcc mnc configs back in the end of the test
        ShellIdentityUtils.invokeMethodWithShellPermissions(mActivityManager,
                (am) -> am.updateMccMncConfiguration(mccMncConfigOriginal[0],
                        mccMncConfigOriginal[1]));
    }
     */

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this method
     * for test "testUpdateMccMncConfiguration" is commented and will be un-commented in
     * Android S.
     *
    private void getMccMncConfigsAndOthers(String[] mccMncConfigs, Set<String> otherConfigs)
            throws Exception {
        String[] configs = SystemUtil.runShellCommand(
                mInstrumentation, SHELL_COMMAND_GET_CONFIG).split(" |\\-");
        for (String config : configs) {
            if (config.startsWith(SHELL_COMMAND_RESULT_CONFIG_NAME_MCC)) {
                mccMncConfigs[0] = config.substring(
                        SHELL_COMMAND_RESULT_CONFIG_NAME_MCC.length());
            } else if (config.startsWith(SHELL_COMMAND_RESULT_CONFIG_NAME_MNC)) {
                mccMncConfigs[1] = config.substring(
                        SHELL_COMMAND_RESULT_CONFIG_NAME_MNC.length());
            } else {
                otherConfigs.add(config);
            }
        }
    }
    */

    /**
     * Simple test for {@link ActivityManager#isUserAMonkey()} - verifies its false.
     *
     * TODO: test positive case
     */
    public void testIsUserAMonkey() {
        assertFalse(ActivityManager.isUserAMonkey());
    }

    /**
     * Verify that {@link ActivityManager#isRunningInTestHarness()} is false.
     */
    @RestrictedBuildTest
    public void testIsRunningInTestHarness() {
        assertFalse("isRunningInTestHarness must be false in production builds",
                ActivityManager.isRunningInTestHarness());
    }

    /**
     * Go back to the home screen since running applications can interfere with application
     * lifetime tests.
     */
    private void launchHome() throws Exception {
        if (!noHomeScreen()) {
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_HOME);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mTargetContext.startActivity(intent);
            Thread.sleep(WAIT_TIME);
        }
    }

   /**
    * Gets the value of com.android.internal.R.bool.config_noHomeScreen.
    * @return true if no home screen is supported, false otherwise.
    */
   private boolean noHomeScreen() {
       try {
           return getInstrumentation().getContext().getResources().getBoolean(
                   Resources.getSystem().getIdentifier("config_noHomeScreen", "bool", "android"));
       } catch (Resources.NotFoundException e) {
           // Assume there's a home screen.
           return false;
       }
    }

    /**
     * Verify that the TimeTrackingAPI works properly when starting and ending an activity.
     */
    public void testTimeTrackingAPI_SimpleStartExit() throws Exception {
        launchHome();
        // Prepare to start an activity from another APK.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(SIMPLE_PACKAGE_NAME,
                SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY_IMMEDIATE_EXIT);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Prepare the time receiver action.
        Context context = mInstrumentation.getTargetContext();
        ActivityOptions options = ActivityOptions.makeBasic();
        Intent receiveIntent = new Intent(ACTIVITY_TIME_TRACK_INFO);
        options.requestUsageTimeReport(PendingIntent.getBroadcast(context,
                0, receiveIntent, PendingIntent.FLAG_CANCEL_CURRENT));

        // The application finished tracker.
        ActivityReceiverFilter appEndReceiver = new ActivityReceiverFilter(ACTIVITY_EXIT_ACTION);

        // The filter for the time event.
        ActivityReceiverFilter timeReceiver = new ActivityReceiverFilter(ACTIVITY_TIME_TRACK_INFO);

        // Run the activity.
        mTargetContext.startActivity(intent, options.toBundle());

        // Wait until it finishes and end the reciever then.
        assertEquals(RESULT_PASS, appEndReceiver.waitForActivity());
        appEndReceiver.close();

        if (!noHomeScreen()) {
            // At this time the timerReceiver should not fire, even though the activity has shut
            // down, because we are back to the home screen. Going to the home screen does not
            // qualify as the user leaving the activity's flow. The time tracking is considered
            // complete only when the user switches to another activity that is not part of the
            // tracked flow.
            assertEquals(RESULT_TIMEOUT, timeReceiver.waitForActivity());
            assertTrue(timeReceiver.mTimeUsed == 0);
        } else {
            // With platforms that have no home screen, focus is returned to something else that is
            // considered a completion of the tracked activity flow, and hence time tracking is
            // triggered.
            assertEquals(RESULT_PASS, timeReceiver.waitForActivity());
        }

        // Issuing now another activity will trigger the timing information release.
        final Intent dummyIntent = new Intent(context, MockApplicationActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final Activity activity = mInstrumentation.startActivitySync(dummyIntent);

        // Wait until it finishes and end the reciever then.
        assertEquals(RESULT_PASS, timeReceiver.waitForActivity());
        timeReceiver.close();
        assertTrue(timeReceiver.mTimeUsed != 0);
    }

    /**
     * Verify that the TimeTrackingAPI works properly when switching away from the monitored task.
     */
    public void testTimeTrackingAPI_SwitchAwayTriggers() throws Exception {
        launchHome();

        // Prepare to start an activity from another APK.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Prepare the time receiver action.
        Context context = mInstrumentation.getTargetContext();
        ActivityOptions options = ActivityOptions.makeBasic();
        Intent receiveIntent = new Intent(ACTIVITY_TIME_TRACK_INFO);
        options.requestUsageTimeReport(PendingIntent.getBroadcast(context,
                0, receiveIntent, PendingIntent.FLAG_CANCEL_CURRENT));

        // The application started tracker.
        ActivityReceiverFilter appStartedReceiver = new ActivityReceiverFilter(
                ACTIVITY_LAUNCHED_ACTION);

        // The filter for the time event.
        ActivityReceiverFilter timeReceiver = new ActivityReceiverFilter(ACTIVITY_TIME_TRACK_INFO);

        // Run the activity.
        mTargetContext.startActivity(intent, options.toBundle());

        // Wait until it finishes and end the reciever then.
        assertEquals(RESULT_PASS, appStartedReceiver.waitForActivity());
        appStartedReceiver.close();

        // At this time the timerReceiver should not fire since our app is running.
        assertEquals(RESULT_TIMEOUT, timeReceiver.waitForActivity());
        assertTrue(timeReceiver.mTimeUsed == 0);

        // Starting now another activity will put ours into the back hence releasing the timing.
        final Intent dummyIntent = new Intent(context, MockApplicationActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final Activity activity = mInstrumentation.startActivitySync(dummyIntent);

        // Wait until it finishes and end the reciever then.
        assertEquals(RESULT_PASS, timeReceiver.waitForActivity());
        timeReceiver.close();
        assertTrue(timeReceiver.mTimeUsed != 0);
    }

    /**
     * Verify that the TimeTrackingAPI works properly when handling an activity chain gets started
     * and ended.
     */
    public void testTimeTrackingAPI_ChainedActivityExit() throws Exception {
        launchHome();
        // Prepare to start an activity from another APK.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(SIMPLE_PACKAGE_NAME,
                SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY_CHAIN_EXIT);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Prepare the time receiver action.
        Context context = mInstrumentation.getTargetContext();
        ActivityOptions options = ActivityOptions.makeBasic();
        Intent receiveIntent = new Intent(ACTIVITY_TIME_TRACK_INFO);
        options.requestUsageTimeReport(PendingIntent.getBroadcast(context,
                0, receiveIntent, PendingIntent.FLAG_CANCEL_CURRENT));

        // The application finished tracker.
        ActivityReceiverFilter appEndReceiver = new ActivityReceiverFilter(
                ACTIVITY_CHAIN_EXIT_ACTION);

        // The filter for the time event.
        ActivityReceiverFilter timeReceiver = new ActivityReceiverFilter(ACTIVITY_TIME_TRACK_INFO);

        // Run the activity.
        mTargetContext.startActivity(intent, options.toBundle());

        // Wait until it finishes and end the reciever then.
        assertEquals(RESULT_PASS, appEndReceiver.waitForActivity());
        appEndReceiver.close();

        if (!noHomeScreen()) {
            // At this time the timerReceiver should not fire, even though the activity has shut
            // down, because we are back to the home screen. Going to the home screen does not
            // qualify as the user leaving the activity's flow. The time tracking is considered
            // complete only when the user switches to another activity that is not part of the
            // tracked flow.
            assertEquals(RESULT_TIMEOUT, timeReceiver.waitForActivity());
            assertTrue(timeReceiver.mTimeUsed == 0);
        } else {
            // With platforms that have no home screen, focus is returned to something else that is
            // considered a completion of the tracked activity flow, and hence time tracking is
            // triggered.
            assertEquals(RESULT_PASS, timeReceiver.waitForActivity());
        }

        // Issue another activity so that the timing information gets released.
        final Intent dummyIntent = new Intent(context, MockApplicationActivity.class);
        dummyIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final Activity activity = mInstrumentation.startActivitySync(dummyIntent);

        // Wait until it finishes and end the reciever then.
        assertEquals(RESULT_PASS, timeReceiver.waitForActivity());
        timeReceiver.close();
        assertTrue(timeReceiver.mTimeUsed != 0);
    }

    /**
     * Verify that after force-stopping a package which has a foreground task contains multiple
     * activities, the process of the package should not be alive (restarted).
     */
    public void testForceStopPackageWontRestartProcess() throws Exception {
        // Ensure that there are no remaining component records of the test app package.
        SystemUtil.runWithShellPermissionIdentity(
                () -> mActivityManager.forceStopPackage(SIMPLE_PACKAGE_NAME));
        ActivityReceiverFilter appStartedReceiver = new ActivityReceiverFilter(
                ACTIVITY_LAUNCHED_ACTION);
        // Start an activity of another APK.
        Intent intent = new Intent();
        intent.setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mTargetContext.startActivity(intent);
        assertEquals(RESULT_PASS, appStartedReceiver.waitForActivity());

        // Start a new activity in the same task. Here adds an action to make a different to intent
        // filter comparison so another same activity will be created.
        intent.setAction(Intent.ACTION_MAIN);
        mTargetContext.startActivity(intent);
        assertEquals(RESULT_PASS, appStartedReceiver.waitForActivity());
        appStartedReceiver.close();

        // Wait for the first activity to stop so its ActivityRecord.haveState will be true. The
        // condition is required to keep the activity record when its process is died.
        Thread.sleep(WAIT_TIME);

        // The package name is also the default name for the activity process.
        final String testProcess = SIMPLE_PACKAGE_NAME;
        Predicate<RunningAppProcessInfo> processNamePredicate =
                runningApp -> testProcess.equals(runningApp.processName);

        List<RunningAppProcessInfo> runningApps = SystemUtil.callWithShellPermissionIdentity(
                () -> mActivityManager.getRunningAppProcesses());
        assertTrue("Process " + testProcess + " should be found in running process list",
                runningApps.stream().anyMatch(processNamePredicate));

        runningApps = SystemUtil.callWithShellPermissionIdentity(() -> {
            mActivityManager.forceStopPackage(SIMPLE_PACKAGE_NAME);
            // Wait awhile (process starting may be asynchronous) to verify if the process is
            // started again unexpectedly.
            Thread.sleep(WAIT_TIME);
            return mActivityManager.getRunningAppProcesses();
        });

        assertFalse("Process " + testProcess + " should not be alive after force-stop",
                runningApps.stream().anyMatch(processNamePredicate));
    }

    /**
     * This test is to verify that devices are patched with the fix in b/119327603 for b/115384617.
     */
    public void testIsAppForegroundRemoved() throws ClassNotFoundException {
       try {
           Class.forName("android.app.IActivityManager").getDeclaredMethod(
                   "isAppForeground", int.class);
           fail("IActivityManager.isAppForeground() API should not be available.");
       } catch (NoSuchMethodException e) {
           // Patched devices should throw this exception since isAppForeground is removed.
       }
    }

    /**
     * This test verifies the self-induced ANR by ActivityManager.appNotResponding().
     */
    public void testAppNotResponding() throws Exception {
        // Setup the ANR monitor
        AmMonitor monitor = new AmMonitor(mInstrumentation,
                new String[]{AmMonitor.WAIT_FOR_CRASHED});

        // Now tell it goto ANR
        CommandReceiver.sendCommand(mTargetContext, CommandReceiver.COMMAND_SELF_INDUCED_ANR,
                PACKAGE_NAME_APP1, PACKAGE_NAME_APP1, 0, null);

        try {

            // Verify we got the ANR
            assertTrue(monitor.waitFor(AmMonitor.WAIT_FOR_EARLY_ANR, WAITFOR_MSEC));

            // Just kill the test app
            monitor.sendCommand(AmMonitor.CMD_KILL);
        } finally {
            // clean up
            monitor.finish();
            SystemUtil.runWithShellPermissionIdentity(() -> {
                mActivityManager.forceStopPackage(PACKAGE_NAME_APP1);
            });
        }
    }

    /*
     * Verifies the {@link android.app.ActivityManager#killProcessesWhenImperceptible}.
     */
    public void testKillingPidsOnImperceptible() throws Exception {
        // Start remote service process
        final String remoteProcessName = STUB_PACKAGE_NAME + ":remote";
        Intent intent = new Intent("android.app.REMOTESERVICE");
        intent.setPackage(STUB_PACKAGE_NAME);
        mTargetContext.startService(intent);
        Thread.sleep(WAITFOR_MSEC);

        RunningAppProcessInfo remote = getRunningAppProcessInfo(remoteProcessName);
        assertNotNull(remote);

        ActivityReceiverFilter appStartedReceiver = new ActivityReceiverFilter(
                ACTIVITY_LAUNCHED_ACTION);
        boolean disabled = "0".equals(executeShellCommand("cmd deviceidle enabled light"));
        try {
            if (disabled) {
                executeAndLogShellCommand("cmd deviceidle enable light");
            }
            intent = new Intent(Intent.ACTION_MAIN);
            intent.setClassName(SIMPLE_PACKAGE_NAME, SIMPLE_PACKAGE_NAME + SIMPLE_ACTIVITY);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mTargetContext.startActivity(intent);
            assertEquals(RESULT_PASS, appStartedReceiver.waitForActivity());

            RunningAppProcessInfo proc = getRunningAppProcessInfo(SIMPLE_PACKAGE_NAME);
            assertNotNull(proc);

            final String reason = "cts";

            try {
                mActivityManager.killProcessesWhenImperceptible(new int[]{proc.pid}, reason);
                fail("Shouldn't have the permission");
            } catch (SecurityException e) {
                // expected
            }

            final long defaultWaitForKillTimeout = 5_000;

            // Keep the device awake
            toggleScreenOn(true);

            // Kill the remote process
            SystemUtil.runWithShellPermissionIdentity(() -> {
                mActivityManager.killProcessesWhenImperceptible(new int[]{remote.pid}, reason);
            });

            // Kill the activity process
            SystemUtil.runWithShellPermissionIdentity(() -> {
                mActivityManager.killProcessesWhenImperceptible(new int[]{proc.pid}, reason);
            });

            // The processes should be still alive because device isn't in idle
            assertFalse(waitUntilTrue(defaultWaitForKillTimeout, () -> isProcessGone(
                    remote.pid, remoteProcessName)));
            assertFalse(waitUntilTrue(defaultWaitForKillTimeout, () -> isProcessGone(
                    proc.pid, SIMPLE_PACKAGE_NAME)));

            // force device idle
            toggleScreenOn(false);
            triggerIdle(true);

            // Now the remote process should have been killed.
            assertTrue(waitUntilTrue(defaultWaitForKillTimeout, () -> isProcessGone(
                    remote.pid, remoteProcessName)));

            // The activity process should be still alive because it's is on the top (perceptible)
            assertFalse(waitUntilTrue(defaultWaitForKillTimeout, () -> isProcessGone(
                    proc.pid, SIMPLE_PACKAGE_NAME)));

            triggerIdle(false);
            // Toogle screen ON
            toggleScreenOn(true);

            // Now launch home
            executeAndLogShellCommand("input keyevent KEYCODE_HOME");

            // force device idle again
            toggleScreenOn(false);
            triggerIdle(true);

            // Now the activity process should be gone.
            assertTrue(waitUntilTrue(defaultWaitForKillTimeout, () -> isProcessGone(
                    proc.pid, SIMPLE_PACKAGE_NAME)));

        } finally {
            // Clean up code
            triggerIdle(false);
            toggleScreenOn(true);
            appStartedReceiver.close();

            if (disabled) {
                executeAndLogShellCommand("cmd deviceidle disable light");
            }
            SystemUtil.runWithShellPermissionIdentity(() -> {
                mActivityManager.forceStopPackage(SIMPLE_PACKAGE_NAME);
            });
        }
    }

    private RunningAppProcessInfo getRunningAppProcessInfo(String processName) {
        try {
            return SystemUtil.callWithShellPermissionIdentity(()-> {
                return mActivityManager.getRunningAppProcesses().stream().filter(
                        (ra) -> processName.equals(ra.processName)).findFirst().orElse(null);
            });
        } catch (Exception e) {
        }
        return null;
    }

    private boolean isProcessGone(int pid, String processName) {
        RunningAppProcessInfo info = getRunningAppProcessInfo(processName);
        return info == null || info.pid != pid;
    }

    // Copied from DeviceStatesTest
    /**
     * Make sure the screen state.
     */
    private void toggleScreenOn(final boolean screenon) throws Exception {
        if (screenon) {
            executeAndLogShellCommand("input keyevent KEYCODE_WAKEUP");
            executeAndLogShellCommand("wm dismiss-keyguard");
        } else {
            executeAndLogShellCommand("input keyevent KEYCODE_SLEEP");
        }
        // Since the screen on/off intent is ordered, they will not be sent right now.
        SystemClock.sleep(2_000);
    }

    /**
     * Simulated for idle, and then perform idle maintenance now.
     */
    private void triggerIdle(boolean idle) throws Exception {
        if (idle) {
            executeAndLogShellCommand("cmd deviceidle force-idle light");
        } else {
            executeAndLogShellCommand("cmd deviceidle unforce");
        }
        // Wait a moment to let that happen before proceeding.
        SystemClock.sleep(2_000);
    }

    /**
     * Return true if the given supplier says it's true
     */
    private boolean waitUntilTrue(long maxWait, Supplier<Boolean> supplier) throws Exception {
        final long deadLine = SystemClock.uptimeMillis() + maxWait;
        boolean result = false;
        do {
            Thread.sleep(500);
        } while (!(result = supplier.get()) && SystemClock.uptimeMillis() < deadLine);
        return result;
    }
}
