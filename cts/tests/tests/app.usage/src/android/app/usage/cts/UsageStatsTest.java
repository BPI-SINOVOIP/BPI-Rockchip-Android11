/**
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

package android.app.usage.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.app.KeyguardManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.usage.cts.ITestReceiver;
import android.app.usage.EventStats;
import android.app.usage.UsageEvents;
import android.app.usage.UsageEvents.Event;
import android.app.usage.UsageStats;
import android.app.usage.UsageStatsManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.Parcel;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import android.provider.Settings;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.text.format.DateUtils;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseLongArray;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.AppStandbyUtils;
import com.android.compatibility.common.util.BatteryUtils;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.text.MessageFormat;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

/**
 * Test the UsageStats API. It is difficult to test the entire surface area
 * of the API, as a lot of the testing depends on what data is already present
 * on the device and for how long that data has been aggregating.
 *
 * These tests perform simple checks that each interval is of the correct duration,
 * and that events do appear in the event log.
 *
 * Tests to add that are difficult to add now:
 * - Invoking a device configuration change and then watching for it in the event log.
 * - Changing the system time and verifying that all data has been correctly shifted
 *   along with the new time.
 * - Proper eviction of old data.
 */
@RunWith(AndroidJUnit4.class)
public class UsageStatsTest {
    private static final boolean DEBUG = false;
    private static final String TAG = "UsageStatsTest";

    private static final String APPOPS_SET_SHELL_COMMAND = "appops set {0} " +
            AppOpsManager.OPSTR_GET_USAGE_STATS + " {1}";

    private static final String GET_SHELL_COMMAND = "settings get global ";

    private static final String SET_SHELL_COMMAND = "settings put global ";

    private static final String DELETE_SHELL_COMMAND = "settings delete global ";

    private static final String TEST_APP_PKG = "android.app.usage.cts.test1";
    private static final String TEST_APP_CLASS = "android.app.usage.cts.test1.SomeActivity";
    private static final String TEST_APP_CLASS_LOCUS
            = "android.app.usage.cts.test1.SomeActivityWithLocus";
    private static final String TEST_APP_CLASS_SERVICE
            = "android.app.usage.cts.test1.TestService";

    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);
    private static final long MINUTE = TimeUnit.MINUTES.toMillis(1);
    private static final long DAY = TimeUnit.DAYS.toMillis(1);
    private static final long WEEK = 7 * DAY;
    private static final long MONTH = 30 * DAY;
    private static final long YEAR = 365 * DAY;
    private static final long TIME_DIFF_THRESHOLD = 200;
    private static final String CHANNEL_ID = "my_channel";

    private static final long TIMEOUT_BINDER_SERVICE_SEC = 2;

    private Context mContext;
    private UiDevice mUiDevice;
    private ActivityManager mAm;
    private UsageStatsManager mUsageStatsManager;
    private KeyguardManager mKeyguardManager;
    private String mTargetPackage;
    private String mCachedUsageSourceSetting;
    private String mCachedEnableRestrictedBucketSetting;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mAm = mContext.getSystemService(ActivityManager.class);
        mUsageStatsManager = (UsageStatsManager) mContext.getSystemService(
                Context.USAGE_STATS_SERVICE);
        mKeyguardManager = mContext.getSystemService(KeyguardManager.class);
        mTargetPackage = mContext.getPackageName();

        assumeTrue("App Standby not enabled on device", AppStandbyUtils.isAppStandbyEnabled());
        setAppOpsMode("allow");
        mCachedUsageSourceSetting = getSetting(Settings.Global.APP_TIME_LIMIT_USAGE_SOURCE);
        mCachedEnableRestrictedBucketSetting = getSetting(Settings.Global.ENABLE_RESTRICTED_BUCKET);
    }

    @After
    public void cleanUp() throws Exception {
        if (mCachedUsageSourceSetting != null &&
                !mCachedUsageSourceSetting.equals(
                    getSetting(Settings.Global.APP_TIME_LIMIT_USAGE_SOURCE))) {
            setUsageSourceSetting(mCachedUsageSourceSetting);
        }
        setSetting(Settings.Global.ENABLE_RESTRICTED_BUCKET, mCachedEnableRestrictedBucketSetting);
        // Force stop test package to avoid any running test code from carrying over to the next run
        SystemUtil.runWithShellPermissionIdentity(() -> mAm.forceStopPackage(TEST_APP_PKG));
        mUiDevice.pressHome();
    }

    private static void assertLessThan(long left, long right) {
        assertTrue("Expected " + left + " to be less than " + right, left < right);
    }

    private static void assertLessThanOrEqual(long left, long right) {
        assertTrue("Expected " + left + " to be less than " + right, left <= right);
    }

    private void setAppOpsMode(String mode) throws Exception {
        executeShellCmd(MessageFormat.format(APPOPS_SET_SHELL_COMMAND, mTargetPackage, mode));
    }

    private String getSetting(String name) throws Exception {
        return executeShellCmd(GET_SHELL_COMMAND + name);
    }

    private void setSetting(String name, String setting) throws Exception {
        if (setting == null || setting.equals("null")) {
            executeShellCmd(DELETE_SHELL_COMMAND + name);
        } else {
            executeShellCmd(SET_SHELL_COMMAND + name + " " + setting);
        }
    }

    private void setUsageSourceSetting(String value) throws Exception {
        setSetting(Settings.Global.APP_TIME_LIMIT_USAGE_SOURCE, value);
        mUsageStatsManager.forceUsageSourceSettingRead();
    }

    private void launchSubActivity(Class<? extends Activity> clazz) {
        final Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(mTargetPackage, clazz.getName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        mUiDevice.wait(Until.hasObject(By.clazz(clazz)), TIMEOUT);
    }

    private void launchTestActivity(String pkgName, String className) {
        Intent intent = new Intent();
        intent.setClassName(pkgName, className);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        mUiDevice.wait(Until.hasObject(By.clazz(pkgName, className)), TIMEOUT);
    }

    private void launchSubActivities(Class<? extends Activity>[] activityClasses) {
        for (Class<? extends Activity> clazz : activityClasses) {
            launchSubActivity(clazz);
        }
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testOrderedActivityLaunchSequenceInEventLog() throws Exception {
        @SuppressWarnings("unchecked")
        Class<? extends Activity>[] activitySequence = new Class[] {
                Activities.ActivityOne.class,
                Activities.ActivityTwo.class,
                Activities.ActivityThree.class,
        };
        mUiDevice.wakeUp();

        final long startTime = System.currentTimeMillis();
        // Launch the series of Activities.
        launchSubActivities(activitySequence);
        final long endTime = System.currentTimeMillis();
        UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        // Only look at events belongs to mTargetPackage.
        ArrayList<UsageEvents.Event> eventList = new ArrayList<>();
        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            if (mTargetPackage.equals(event.getPackageName())) {
                eventList.add(event);
            }
        }

        final int activityCount = activitySequence.length;
        for (int i = 0; i < activityCount; i++) {
            String className = activitySequence[i].getName();
            ArrayList<UsageEvents.Event> activityEvents = new ArrayList<>();
            final int size = eventList.size();
            for (int j = 0; j < size; j++) {
                Event evt = eventList.get(j);
                if (className.equals(evt.getClassName())) {
                    activityEvents.add(evt);
                }
            }
            // We expect 3 events per Activity launched (ACTIVITY_RESUMED + ACTIVITY_PAUSED
            // + ACTIVITY_STOPPED) except for the last Activity, which only has
            // ACTIVITY_RESUMED event.
            if (i < activityCount - 1) {
                assertEquals(3, activityEvents.size());
                assertEquals(Event.ACTIVITY_RESUMED, activityEvents.get(0).getEventType());
                assertEquals(Event.ACTIVITY_PAUSED, activityEvents.get(1).getEventType());
                assertEquals(Event.ACTIVITY_STOPPED, activityEvents.get(2).getEventType());
            } else {
                // The last activity
                assertEquals(1, activityEvents.size());
                assertEquals(Event.ACTIVITY_RESUMED, activityEvents.get(0).getEventType());
            }
        }
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testActivityOnBackButton() throws Exception {
        testActivityOnButton(mUiDevice::pressBack);
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testActivityOnHomeButton() throws Exception {
        testActivityOnButton(mUiDevice::pressHome);
    }

    private void testActivityOnButton(Runnable pressButton) throws Exception {
        mUiDevice.wakeUp();
        final long startTime = System.currentTimeMillis();
        final Class clazz = Activities.ActivityOne.class;
        launchSubActivity(clazz);
        pressButton.run();
        Thread.sleep(1000);
        final long endTime = System.currentTimeMillis();
        UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        ArrayList<UsageEvents.Event> eventList = new ArrayList<>();
        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            if (mTargetPackage.equals(event.getPackageName())
                && clazz.getName().equals(event.getClassName())) {
                eventList.add(event);
            }
        }
        assertEquals(3, eventList.size());
        assertEquals(Event.ACTIVITY_RESUMED, eventList.get(0).getEventType());
        assertEquals(Event.ACTIVITY_PAUSED, eventList.get(1).getEventType());
        assertEquals(Event.ACTIVITY_STOPPED, eventList.get(2).getEventType());
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testAppLaunchCount() throws Exception {
        long endTime = System.currentTimeMillis();
        long startTime = endTime - DateUtils.DAY_IN_MILLIS;
        Map<String,UsageStats> events = mUsageStatsManager.queryAndAggregateUsageStats(
                startTime, endTime);
        UsageStats stats = events.get(mTargetPackage);
        int startingCount = stats.getAppLaunchCount();
        launchSubActivity(Activities.ActivityOne.class);
        launchSubActivity(Activities.ActivityTwo.class);
        endTime = System.currentTimeMillis();
        events = mUsageStatsManager.queryAndAggregateUsageStats(
                startTime, endTime);
        stats = events.get(mTargetPackage);
        assertEquals(startingCount + 1, stats.getAppLaunchCount());
        mUiDevice.pressHome();
        launchSubActivity(Activities.ActivityOne.class);
        launchSubActivity(Activities.ActivityTwo.class);
        launchSubActivity(Activities.ActivityThree.class);
        endTime = System.currentTimeMillis();
        events = mUsageStatsManager.queryAndAggregateUsageStats(
                startTime, endTime);
        stats = events.get(mTargetPackage);
        assertEquals(startingCount + 2, stats.getAppLaunchCount());
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testStandbyBucketChangeLog() throws Exception {
        final long startTime = System.currentTimeMillis();
        setStandByBucket(mTargetPackage, "rare");

        final long endTime = System.currentTimeMillis();
        UsageEvents events = mUsageStatsManager.queryEvents(startTime - 1_000, endTime + 1_000);

        boolean found = false;
        // Check all the events.
        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            if (event.getEventType() == UsageEvents.Event.STANDBY_BUCKET_CHANGED) {
                found |= event.getAppStandbyBucket() == UsageStatsManager.STANDBY_BUCKET_RARE;
            }
        }

        assertTrue(found);
    }

    @Test
    public void testGetAppStandbyBuckets() throws Exception {
        final boolean origValue = AppStandbyUtils.isAppStandbyEnabledAtRuntime();
        AppStandbyUtils.setAppStandbyEnabledAtRuntime(true);
        try {
            assumeTrue("Skip GetAppStandby test: app standby is disabled.",
                    AppStandbyUtils.isAppStandbyEnabled());

            setStandByBucket(mTargetPackage, "rare");
            Map<String, Integer> bucketMap = mUsageStatsManager.getAppStandbyBuckets();
            assertTrue("No bucket data returned", bucketMap.size() > 0);
            final int bucket = bucketMap.getOrDefault(mTargetPackage, -1);
            assertEquals("Incorrect bucket returned for " + mTargetPackage, bucket,
                    UsageStatsManager.STANDBY_BUCKET_RARE);
        } finally {
            AppStandbyUtils.setAppStandbyEnabledAtRuntime(origValue);
        }
    }

    @Test
    public void testGetAppStandbyBucket() throws Exception {
        // App should be at least active, since it's running instrumentation tests
        assertLessThanOrEqual(UsageStatsManager.STANDBY_BUCKET_ACTIVE,
                mUsageStatsManager.getAppStandbyBucket());
    }

    @Test
    public void testQueryEventsForSelf() throws Exception {
        setAppOpsMode("ignore"); // To ensure permission is not required
        // Time drifts of 2s are expected inside usage stats
        final long start = System.currentTimeMillis() - 2_000;
        setStandByBucket(mTargetPackage, "rare");
        Thread.sleep(100);
        setStandByBucket(mTargetPackage, "working_set");
        Thread.sleep(100);
        final long end = System.currentTimeMillis() + 2_000;
        final UsageEvents events = mUsageStatsManager.queryEventsForSelf(start, end);
        long rareTimeStamp = end + 1; // Initializing as rareTimeStamp > workingTimeStamp
        long workingTimeStamp = start - 1;
        int numEvents = 0;
        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            numEvents++;
            assertEquals("Event for a different package", mTargetPackage, event.getPackageName());
            if (event.getEventType() == Event.STANDBY_BUCKET_CHANGED) {
                if (event.getAppStandbyBucket() == UsageStatsManager.STANDBY_BUCKET_RARE) {
                    rareTimeStamp = event.getTimeStamp();
                }
                else if (event.getAppStandbyBucket() == UsageStatsManager
                        .STANDBY_BUCKET_WORKING_SET) {
                    workingTimeStamp = event.getTimeStamp();
                }
            }
        }
        assertTrue("Only " + numEvents + " events returned", numEvents >= 2);
        assertLessThan(rareTimeStamp, workingTimeStamp);
    }

    /**
     * We can't run this test because we are unable to change the system time.
     * It would be nice to add a shell command or other to allow the shell user
     * to set the time, thereby allowing this test to set the time using the UIAutomator.
     */
    @Ignore
    @Test
    public void ignore_testStatsAreShiftedInTimeWhenSystemTimeChanges() throws Exception {
        launchSubActivity(Activities.ActivityOne.class);
        launchSubActivity(Activities.ActivityThree.class);

        long endTime = System.currentTimeMillis();
        long startTime = endTime - MINUTE;
        Map<String, UsageStats> statsMap = mUsageStatsManager.queryAndAggregateUsageStats(startTime,
                endTime);
        assertFalse(statsMap.isEmpty());
        assertTrue(statsMap.containsKey(mTargetPackage));
        final UsageStats before = statsMap.get(mTargetPackage);

        SystemClock.setCurrentTimeMillis(System.currentTimeMillis() - (DAY / 2));
        try {
            endTime = System.currentTimeMillis();
            startTime = endTime - MINUTE;
            statsMap = mUsageStatsManager.queryAndAggregateUsageStats(startTime, endTime);
            assertFalse(statsMap.isEmpty());
            assertTrue(statsMap.containsKey(mTargetPackage));
            final UsageStats after = statsMap.get(mTargetPackage);
            assertEquals(before.getPackageName(), after.getPackageName());

            long diff = before.getFirstTimeStamp() - after.getFirstTimeStamp();
            assertLessThan(Math.abs(diff - (DAY / 2)), TIME_DIFF_THRESHOLD);

            assertEquals(before.getLastTimeStamp() - before.getFirstTimeStamp(),
                    after.getLastTimeStamp() - after.getFirstTimeStamp());
            assertEquals(before.getLastTimeUsed() - before.getFirstTimeStamp(),
                    after.getLastTimeUsed() - after.getFirstTimeStamp());
            assertEquals(before.getTotalTimeInForeground(), after.getTotalTimeInForeground());
        } finally {
            SystemClock.setCurrentTimeMillis(System.currentTimeMillis() + (DAY / 2));
        }
    }

    @Test
    public void testUsageEventsParceling() throws Exception {
        final long startTime = System.currentTimeMillis() - MINUTE;

        // Ensure some data is in the UsageStats log.
        @SuppressWarnings("unchecked")
        Class<? extends Activity>[] activityClasses = new Class[] {
                Activities.ActivityTwo.class,
                Activities.ActivityOne.class,
                Activities.ActivityThree.class,
        };
        launchSubActivities(activityClasses);

        final long endTime = System.currentTimeMillis();
        UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);
        assertTrue(events.getNextEvent(new UsageEvents.Event()));

        Parcel p = Parcel.obtain();
        p.setDataPosition(0);
        events.writeToParcel(p, 0);
        p.setDataPosition(0);

        UsageEvents reparceledEvents = UsageEvents.CREATOR.createFromParcel(p);

        UsageEvents.Event e1 = new UsageEvents.Event();
        UsageEvents.Event e2 = new UsageEvents.Event();
        while (events.hasNextEvent() && reparceledEvents.hasNextEvent()) {
            events.getNextEvent(e1);
            reparceledEvents.getNextEvent(e2);
            assertEquals(e1.getPackageName(), e2.getPackageName());
            assertEquals(e1.getClassName(), e2.getClassName());
            assertEquals(e1.getConfiguration(), e2.getConfiguration());
            assertEquals(e1.getEventType(), e2.getEventType());
            assertEquals(e1.getTimeStamp(), e2.getTimeStamp());
        }

        assertEquals(events.hasNextEvent(), reparceledEvents.hasNextEvent());
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testPackageUsageStatsIntervals() throws Exception {
        final long beforeTime = System.currentTimeMillis();

        // Launch an Activity.
        launchSubActivity(Activities.ActivityFour.class);
        launchSubActivity(Activities.ActivityThree.class);

        final long endTime = System.currentTimeMillis();

        final SparseLongArray intervalLengths = new SparseLongArray();
        intervalLengths.put(UsageStatsManager.INTERVAL_DAILY, DAY);
        intervalLengths.put(UsageStatsManager.INTERVAL_WEEKLY, WEEK);
        intervalLengths.put(UsageStatsManager.INTERVAL_MONTHLY, MONTH);
        intervalLengths.put(UsageStatsManager.INTERVAL_YEARLY, YEAR);

        final int intervalCount = intervalLengths.size();
        for (int i = 0; i < intervalCount; i++) {
            final int intervalType = intervalLengths.keyAt(i);
            final long intervalDuration = intervalLengths.valueAt(i);
            final long startTime = endTime - (2 * intervalDuration);
            final List<UsageStats> statsList = mUsageStatsManager.queryUsageStats(intervalType,
                    startTime, endTime);
            assertFalse(statsList.isEmpty());

            boolean foundPackage = false;
            for (UsageStats stats : statsList) {
                // Verify that each period is a day long.
                assertLessThanOrEqual(stats.getLastTimeStamp() - stats.getFirstTimeStamp(),
                        intervalDuration);
                if (stats.getPackageName().equals(mTargetPackage) &&
                        stats.getLastTimeUsed() >= beforeTime - TIME_DIFF_THRESHOLD) {
                    foundPackage = true;
                }
            }

            assertTrue("Did not find package " + mTargetPackage + " in interval " + intervalType,
                    foundPackage);
        }
    }

    @Test
    public void testNoAccessSilentlyFails() throws Exception {
        final long startTime = System.currentTimeMillis() - MINUTE;

        launchSubActivity(android.app.usage.cts.Activities.ActivityOne.class);
        launchSubActivity(android.app.usage.cts.Activities.ActivityThree.class);

        final long endTime = System.currentTimeMillis();
        List<UsageStats> stats = mUsageStatsManager.queryUsageStats(UsageStatsManager.INTERVAL_BEST,
                startTime, endTime);
        assertFalse(stats.isEmpty());

        // We set the mode to ignore because our package has the PACKAGE_USAGE_STATS permission,
        // and default would allow in this case.
        setAppOpsMode("ignore");

        stats = mUsageStatsManager.queryUsageStats(UsageStatsManager.INTERVAL_BEST,
                startTime, endTime);
        assertTrue(stats.isEmpty());
    }

    private void generateAndSendNotification() throws Exception {
        final NotificationManager mNotificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        final NotificationChannel mChannel = new NotificationChannel(CHANNEL_ID, "Channel",
                NotificationManager.IMPORTANCE_DEFAULT);
        // Configure the notification channel.
        mChannel.setDescription("Test channel");
        mNotificationManager.createNotificationChannel(mChannel);
        final Notification.Builder mBuilder =
                new Notification.Builder(mContext, CHANNEL_ID)
                        .setSmallIcon(R.drawable.ic_notification)
                        .setContentTitle("My notification")
                        .setContentText("Hello World!");
        final PendingIntent pi = PendingIntent.getActivity(mContext, 1,
                new Intent(Settings.ACTION_SETTINGS), 0);
        mBuilder.setContentIntent(pi);
        mNotificationManager.notify(1, mBuilder.build());
        Thread.sleep(500);
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testNotificationSeen() throws Exception {
        final long startTime = System.currentTimeMillis();

        // Skip the test for wearable devices, televisions and automotives; neither has
        // a notification shade, as notifications are shown via a different path than phones
        assumeFalse("Test cannot run on a watch- notification shade is not shown",
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WATCH));
        assumeFalse("Test cannot run on a television- notifications are not shown",
                mContext.getPackageManager().hasSystemFeature(
                        PackageManager.FEATURE_LEANBACK_ONLY));
        assumeFalse("Test cannot run on an automotive - notification shade is not shown",
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE));

        generateAndSendNotification();

        long endTime = System.currentTimeMillis();
        UsageEvents events = queryEventsAsShell(startTime, endTime);
        boolean found = false;
        Event event = new Event();
        while (events.hasNextEvent()) {
            events.getNextEvent(event);
            if (event.getEventType() == Event.NOTIFICATION_SEEN) {
                found = true;
            }
        }
        assertFalse(found);
        // Pull down shade
        mUiDevice.openNotification();
        outer:
        for (int i = 0; i < 5; i++) {
            Thread.sleep(500);
            endTime = System.currentTimeMillis();
            events = queryEventsAsShell(startTime, endTime);
            found = false;
            while (events.hasNextEvent()) {
                events.getNextEvent(event);
                if (event.getEventType() == Event.NOTIFICATION_SEEN) {
                    found = true;
                    break outer;
                }
            }
        }
        assertTrue(found);
        mUiDevice.pressBack();
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testNotificationInterruptionEventsObfuscation() throws Exception {
        final long startTime = System.currentTimeMillis();

        // Skip the test for wearable devices and televisions; neither has a notification shade.
        assumeFalse("Test cannot run on a watch- notification shade is not shown",
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WATCH));
        assumeFalse("Test cannot run on a television- notifications are not shown",
                mContext.getPackageManager().hasSystemFeature(
                        PackageManager.FEATURE_LEANBACK_ONLY));

        generateAndSendNotification();
        final long endTime = System.currentTimeMillis();

        final UsageEvents obfuscatedEvents = mUsageStatsManager.queryEvents(startTime, endTime);
        final UsageEvents unobfuscatedEvents = queryEventsAsShell(startTime, endTime);
        verifyNotificationInterruptionEvent(obfuscatedEvents, true);
        verifyNotificationInterruptionEvent(unobfuscatedEvents, false);
    }

    private void verifyNotificationInterruptionEvent(UsageEvents events, boolean obfuscated) {
        boolean found = false;
        Event event = new Event();
        while (events.hasNextEvent()) {
            events.getNextEvent(event);
            if (event.getEventType() == Event.NOTIFICATION_INTERRUPTION) {
                found = true;
                break;
            }
        }
        assertTrue(found);
        if (obfuscated) {
            assertEquals("Notification channel id was not obfuscated.",
                    UsageEvents.OBFUSCATED_NOTIFICATION_CHANNEL_ID, event.mNotificationChannelId);
        } else {
            assertEquals("Failed to verify notification channel id.",
                    CHANNEL_ID, event.mNotificationChannelId);
        }
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testUserUnlockedEventExists() throws Exception {
        final UsageEvents events = mUsageStatsManager.queryEvents(0, System.currentTimeMillis());
        while (events.hasNextEvent()) {
            final Event event = new Event();
            events.getNextEvent(event);
            if (event.mEventType == Event.USER_UNLOCKED) {
                return;
            }
        }
        fail("Couldn't find a user unlocked event.");
    }

    // TODO(148887416): get this test to work for instant apps
    @AppModeFull(reason = "Test APK Activity not found when installed as an instant app")
    @Test
    public void testUserForceIntoRestricted() throws Exception {
        setSetting(Settings.Global.ENABLE_RESTRICTED_BUCKET, "1");

        launchSubActivity(TaskRootActivity.class);
        assertEquals("Activity launch didn't bring app up to ACTIVE bucket",
                UsageStatsManager.STANDBY_BUCKET_ACTIVE,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

        // User force shouldn't have to deal with the timeout.
        setStandByBucket(mTargetPackage, "restricted");
        assertEquals("User was unable to force an ACTIVE app down into RESTRICTED bucket",
                UsageStatsManager.STANDBY_BUCKET_RESTRICTED,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

    }

    // TODO(148887416): get this test to work for instant apps
    @AppModeFull(reason = "Test APK Activity not found when installed as an instant app")
    @Test
    public void testUserForceIntoRestricted_BucketDisabled() throws Exception {
        setSetting(Settings.Global.ENABLE_RESTRICTED_BUCKET, "0");

        launchSubActivity(TaskRootActivity.class);
        assertEquals("Activity launch didn't bring app up to ACTIVE bucket",
                UsageStatsManager.STANDBY_BUCKET_ACTIVE,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

        // User force shouldn't have to deal with the timeout.
        setStandByBucket(mTargetPackage, "restricted");
        assertNotEquals("User was able to force into RESTRICTED bucket when bucket disabled",
                UsageStatsManager.STANDBY_BUCKET_RESTRICTED,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

    }

    // TODO(148887416): get this test to work for instant apps
    @AppModeFull(reason = "Test APK Activity not found when installed as an instant app")
    @Test
    public void testUserLaunchRemovesFromRestricted() throws Exception {
        setSetting(Settings.Global.ENABLE_RESTRICTED_BUCKET, "1");

        setStandByBucket(mTargetPackage, "restricted");
        assertEquals("User was unable to force an app into RESTRICTED bucket",
                UsageStatsManager.STANDBY_BUCKET_RESTRICTED,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

        launchSubActivity(TaskRootActivity.class);
        assertEquals("Activity launch didn't bring RESTRICTED app into ACTIVE bucket",
                UsageStatsManager.STANDBY_BUCKET_ACTIVE,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));
    }

    /** Confirm the default value of {@link Settings.Global.ENABLE_RESTRICTED_BUCKET}. */
    // TODO(148887416): get this test to work for instant apps
    @AppModeFull(reason = "Test APK Activity not found when installed as an instant app")
    @Test
    public void testDefaultEnableRestrictedBucketOff() throws Exception {
        setSetting(Settings.Global.ENABLE_RESTRICTED_BUCKET, null);

        launchSubActivity(TaskRootActivity.class);
        assertEquals("Activity launch didn't bring app up to ACTIVE bucket",
                UsageStatsManager.STANDBY_BUCKET_ACTIVE,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

        // User force shouldn't have to deal with the timeout.
        setStandByBucket(mTargetPackage, "restricted");
        assertNotEquals("User was able to force into RESTRICTED bucket when bucket disabled",
                UsageStatsManager.STANDBY_BUCKET_RESTRICTED,
                mUsageStatsManager.getAppStandbyBucket(mTargetPackage));

    }

    // TODO(148887416): get this test to work for instant apps
    @AppModeFull(reason = "Test APK Activity not found when installed as an instant app")
    @Test
    public void testIsAppInactive() throws Exception {
        setStandByBucket(mTargetPackage, "rare");

        try {
            BatteryUtils.runDumpsysBatteryUnplug();

            waitUntil(() -> mUsageStatsManager.isAppInactive(mTargetPackage), true);
            assertFalse(
                    "App without PACKAGE_USAGE_STATS permission should always receive false for "
                            + "isAppInactive",
                    isAppInactiveAsPermissionlessApp(mTargetPackage));

            launchSubActivity(Activities.ActivityOne.class);

            waitUntil(() -> mUsageStatsManager.isAppInactive(mTargetPackage), false);
            assertFalse(
                    "App without PACKAGE_USAGE_STATS permission should always receive false for "
                            + "isAppInactive",
                    isAppInactiveAsPermissionlessApp(mTargetPackage));

            mUiDevice.pressHome();
            setStandByBucket(TEST_APP_PKG, "rare");
            // Querying for self does not require the PACKAGE_USAGE_STATS
            waitUntil(() -> mUsageStatsManager.isAppInactive(TEST_APP_PKG), true);
            assertTrue(
                    "App without PACKAGE_USAGE_STATS permission should be able to call "
                            + "isAppInactive for itself",
                    isAppInactiveAsPermissionlessApp(TEST_APP_PKG));

            launchTestActivity(TEST_APP_PKG, TEST_APP_CLASS);

            waitUntil(() -> mUsageStatsManager.isAppInactive(TEST_APP_PKG), false);
            assertFalse(
                    "App without PACKAGE_USAGE_STATS permission should be able to call "
                            + "isAppInactive for itself",
                    isAppInactiveAsPermissionlessApp(TEST_APP_PKG));

        } finally {
            BatteryUtils.runDumpsysBatteryReset();
        }
    }

    // TODO(148887416): get this test to work for instant apps
    @AppModeFull(reason = "Test APK Activity not found when installed as an instant app")
    @Test
    public void testIsAppInactive_Charging() throws Exception {
        setStandByBucket(TEST_APP_PKG, "rare");

        try {
            BatteryUtils.runDumpsysBatteryUnplug();
            // Plug/unplug change takes a while to propagate inside the system.
            waitUntil(() -> mUsageStatsManager.isAppInactive(TEST_APP_PKG), true);

            BatteryUtils.runDumpsysBatterySetPluggedIn(true);
            BatteryUtils.runDumpsysBatterySetLevel(100);
            // Plug/unplug change takes a while to propagate inside the system.
            waitUntil(() -> mUsageStatsManager.isAppInactive(TEST_APP_PKG), false);
        } finally {
            BatteryUtils.runDumpsysBatteryReset();
        }
    }

    private static final int[] INTERACTIVE_EVENTS = new int[] {
            Event.SCREEN_INTERACTIVE,
            Event.SCREEN_NON_INTERACTIVE
    };

    private static final int[] KEYGUARD_EVENTS = new int[] {
            Event.KEYGUARD_SHOWN,
            Event.KEYGUARD_HIDDEN
    };

    private static final int[] ALL_EVENTS = new int[] {
            Event.SCREEN_INTERACTIVE,
            Event.SCREEN_NON_INTERACTIVE,
            Event.KEYGUARD_SHOWN,
            Event.KEYGUARD_HIDDEN
    };

    private long getEvents(int[] whichEvents, long startTime, List<Event> out) {
        final long endTime = System.currentTimeMillis();
        if (DEBUG) Log.i(TAG, "Looking for events " + Arrays.toString(whichEvents)
                + " between " + startTime + " and " + endTime);
        UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        long latestTime = 0;

        // Find events.
        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            final int ev = event.getEventType();
            for (int which : whichEvents) {
                if (ev == which) {
                    if (out != null) {
                        out.add(event);
                    }
                    if (DEBUG) Log.i(TAG, "Next event type " + event.getEventType()
                            + " time=" + event.getTimeStamp());
                    if (latestTime < event.getTimeStamp()) {
                        latestTime = event.getTimeStamp();
                    }
                    break;
                }
            }
        }

        return latestTime;
    }

    private ArrayList<Event> waitForEventCount(int[] whichEvents, long startTime, int count) {
        final ArrayList<Event> events = new ArrayList<>();
        final long endTime = SystemClock.uptimeMillis() + 2000;
        do {
            events.clear();
            getEvents(whichEvents, startTime, events);
            if (events.size() == count) {
                return events;
            }
            if (events.size() > count) {
                fail("Found too many events: got " + events.size() + ", expected " + count);
                return events;
            }
            SystemClock.sleep(10);
        } while (SystemClock.uptimeMillis() < endTime);

        fail("Timed out waiting for " + count + " events, only reached " + events.size());
        return events;
    }

    private void waitUntil(BooleanSupplier condition, boolean expected) throws Exception {
        final long sleepTimeMs = 500;
        final int count = 10;
        for (int i = 0; i < count; ++i) {
            if (condition.getAsBoolean() == expected) {
                return;
            }
            Thread.sleep(sleepTimeMs);
        }
        fail("Condition wasn't satisfied after " + (sleepTimeMs * count) + "ms");
    }

    static class AggrEventData {
        final String label;
        int count;
        long duration;
        long lastEventTime;

        AggrEventData(String label) {
            this.label = label;
        }
    }

    static class AggrAllEventsData {
        final AggrEventData interactive = new AggrEventData("Interactive");
        final AggrEventData nonInteractive = new AggrEventData("Non-interactive");
        final AggrEventData keyguardShown = new AggrEventData("Keyguard shown");
        final AggrEventData keyguardHidden = new AggrEventData("Keyguard hidden");
    }

    private SparseArray<AggrAllEventsData> getAggrEventData() {
        final long endTime = System.currentTimeMillis();

        final SparseLongArray intervalLengths = new SparseLongArray();
        intervalLengths.put(UsageStatsManager.INTERVAL_DAILY, DAY);
        intervalLengths.put(UsageStatsManager.INTERVAL_WEEKLY, WEEK);
        intervalLengths.put(UsageStatsManager.INTERVAL_MONTHLY, MONTH);
        intervalLengths.put(UsageStatsManager.INTERVAL_YEARLY, YEAR);

        final SparseArray<AggrAllEventsData> allAggr = new SparseArray<>();

        final int intervalCount = intervalLengths.size();
        for (int i = 0; i < intervalCount; i++) {
            final int intervalType = intervalLengths.keyAt(i);
            final long intervalDuration = intervalLengths.valueAt(i);
            final long startTime = endTime - (2 * intervalDuration);
            List<EventStats> statsList = mUsageStatsManager.queryEventStats(intervalType,
                    startTime, endTime);
            assertFalse(statsList.isEmpty());

            final AggrAllEventsData aggr = new AggrAllEventsData();
            allAggr.put(intervalType, aggr);

            boolean foundEvent = false;
            for (EventStats stats : statsList) {
                // Verify that each period is a day long.
                //assertLessThanOrEqual(stats.getLastTimeStamp() - stats.getFirstTimeStamp(),
                //        intervalDuration);
                AggrEventData data = null;
                switch (stats.getEventType()) {
                    case Event.SCREEN_INTERACTIVE:
                        data = aggr.interactive;
                        break;
                    case Event.SCREEN_NON_INTERACTIVE:
                        data = aggr.nonInteractive;
                        break;
                    case Event.KEYGUARD_HIDDEN:
                        data = aggr.keyguardHidden;
                        break;
                    case Event.KEYGUARD_SHOWN:
                        data = aggr.keyguardShown;
                        break;
                }
                if (data != null) {
                    foundEvent = true;
                    data.count += stats.getCount();
                    data.duration += stats.getTotalTime();
                    if (data.lastEventTime < stats.getLastEventTime()) {
                        data.lastEventTime = stats.getLastEventTime();
                    }
                }
            }

            assertTrue("Did not find event data in interval " + intervalType,
                    foundEvent);
        }

        return allAggr;
    }

    private void verifyCount(int oldCount, int newCount, boolean larger, String label,
            int interval) {
        if (larger) {
            if (newCount <= oldCount) {
                fail(label + " count newer " + newCount
                        + " expected to be larger than older " + oldCount
                        + " @ interval " + interval);
            }
        } else {
            if (newCount != oldCount) {
                fail(label + " count newer " + newCount
                        + " expected to be same as older " + oldCount
                        + " @ interval " + interval);
            }
        }
    }

    private void verifyDuration(long oldDur, long newDur, boolean larger, String label,
            int interval) {
        if (larger) {
            if (newDur <= oldDur) {
                fail(label + " duration newer " + newDur
                        + " expected to be larger than older " + oldDur
                        + " @ interval " + interval);
            }
        } else {
            if (newDur != oldDur) {
                fail(label + " duration newer " + newDur
                        + " expected to be same as older " + oldDur
                        + " @ interval " + interval);
            }
        }
    }

    private void verifyAggrEventData(AggrEventData older, AggrEventData newer,
            boolean countLarger, boolean durationLarger, int interval) {
        verifyCount(older.count, newer.count, countLarger, older.label, interval);
        verifyDuration(older.duration, newer.duration, durationLarger, older.label, interval);
    }

    private void verifyAggrInteractiveEventData(SparseArray<AggrAllEventsData> older,
            SparseArray<AggrAllEventsData> newer, boolean interactiveLarger,
            boolean nonInteractiveLarger) {
        for (int i = 0; i < older.size(); i++) {
            AggrAllEventsData o = older.valueAt(i);
            AggrAllEventsData n = newer.valueAt(i);
            // When we are told something is larger, that means we have transitioned
            // *out* of that state -- so the duration of that state is expected to
            // increase, but the count should stay the same (and the count of the state
            // we transition to is increased).
            final int interval = older.keyAt(i);
            verifyAggrEventData(o.interactive, n.interactive, nonInteractiveLarger,
                    interactiveLarger, interval);
            verifyAggrEventData(o.nonInteractive, n.nonInteractive, interactiveLarger,
                    nonInteractiveLarger, interval);
        }
    }

    private void verifyAggrKeyguardEventData(SparseArray<AggrAllEventsData> older,
            SparseArray<AggrAllEventsData> newer, boolean hiddenLarger,
            boolean shownLarger) {
        for (int i = 0; i < older.size(); i++) {
            AggrAllEventsData o = older.valueAt(i);
            AggrAllEventsData n = newer.valueAt(i);
            // When we are told something is larger, that means we have transitioned
            // *out* of that state -- so the duration of that state is expected to
            // increase, but the count should stay the same (and the count of the state
            // we transition to is increased).
            final int interval = older.keyAt(i);
            verifyAggrEventData(o.keyguardHidden, n.keyguardHidden, shownLarger,
                    hiddenLarger, interval);
            verifyAggrEventData(o.keyguardShown, n.keyguardShown, hiddenLarger,
                    shownLarger, interval);
        }
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testInteractiveEvents() throws Exception {
        // We need to start out with the screen on.
        mUiDevice.wakeUp();
        dismissKeyguard(); // also want to start out with the keyguard dismissed.

        try {
            ArrayList<Event> events;

            // Determine time to start looking for events.
            final long startTime = getEvents(ALL_EVENTS, 0, null) + 1;
            SparseArray<AggrAllEventsData> baseAggr = getAggrEventData();

            // First test -- put device to sleep and make sure we see this event.
            mUiDevice.sleep();

            // Do we have one event, going in to non-interactive mode?
            events = waitForEventCount(INTERACTIVE_EVENTS, startTime, 1);
            assertEquals(Event.SCREEN_NON_INTERACTIVE, events.get(0).getEventType());
            SparseArray<AggrAllEventsData> offAggr = getAggrEventData();
            verifyAggrInteractiveEventData(baseAggr, offAggr, true, false);

            // Next test -- turn screen on and make sure we have a second event.
            // XXX need to wait a bit so we don't accidentally trigger double-power
            // to launch camera.  (SHOULD FIX HOW WE WAKEUP / SLEEP TO NOT USE POWER KEY)
            SystemClock.sleep(500);
            mUiDevice.wakeUp();
            events = waitForEventCount(INTERACTIVE_EVENTS, startTime, 2);
            assertEquals(Event.SCREEN_NON_INTERACTIVE, events.get(0).getEventType());
            assertEquals(Event.SCREEN_INTERACTIVE, events.get(1).getEventType());
            SparseArray<AggrAllEventsData> onAggr = getAggrEventData();
            verifyAggrInteractiveEventData(offAggr, onAggr, false, true);

            // If the device is doing a lock screen, verify that we are also seeing the
            // appropriate keyguard behavior.  We don't know the timing from when the screen
            // will go off until the keyguard is shown, so we will do this all after turning
            // the screen back on (at which point it must be shown).
            // XXX CTS seems to be preventing the keyguard from showing, so this path is
            // never being tested.
            if (mKeyguardManager.isKeyguardLocked()) {
                events = waitForEventCount(KEYGUARD_EVENTS, startTime, 1);
                assertEquals(Event.KEYGUARD_SHOWN, events.get(0).getEventType());
                SparseArray<AggrAllEventsData> shownAggr = getAggrEventData();
                verifyAggrKeyguardEventData(offAggr, shownAggr, true, false);

                // Now dismiss the keyguard and verify the resulting events.
                executeShellCmd("wm dismiss-keyguard");
                events = waitForEventCount(KEYGUARD_EVENTS, startTime, 2);
                assertEquals(Event.KEYGUARD_SHOWN, events.get(0).getEventType());
                assertEquals(Event.KEYGUARD_HIDDEN, events.get(1).getEventType());
                SparseArray<AggrAllEventsData> hiddenAggr = getAggrEventData();
                verifyAggrKeyguardEventData(shownAggr, hiddenAggr, false, true);
            }

        } finally {
            // Dismiss keyguard to get device back in its normal state.
            mUiDevice.wakeUp();
            executeShellCmd("wm dismiss-keyguard");
        }
    }

    @Test
    public void testIgnoreNonexistentPackage() throws Exception {
        final String fakePackageName = "android.fake.package.name";
        final int defaultValue = -1;

        setStandByBucket(fakePackageName, "rare");
        // Verify the above does not add a new entry to the App Standby bucket map
        Map<String, Integer> bucketMap = mUsageStatsManager.getAppStandbyBuckets();
        int bucket = bucketMap.getOrDefault(fakePackageName, defaultValue);
        assertFalse("Meaningful bucket value " + bucket + " returned for " + fakePackageName
                + " after set-standby-bucket", bucket > 0);

        executeShellCmd("am get-standby-bucket " + fakePackageName);
        // Verify the above does not add a new entry to the App Standby bucket map
        bucketMap = mUsageStatsManager.getAppStandbyBuckets();
        bucket = bucketMap.getOrDefault(fakePackageName, defaultValue);
        assertFalse("Meaningful bucket value " + bucket + " returned for " + fakePackageName
                + " after get-standby-bucket", bucket > 0);
    }

    @Test
    public void testObserveUsagePermissionForRegisterObserver() {
        final int observerId = 0;
        final String[] packages = new String[] {"com.android.settings"};

        try {
            mUsageStatsManager.registerAppUsageObserver(observerId, packages,
                    1, java.util.concurrent.TimeUnit.HOURS, null);
            fail("Expected SecurityException for an app not holding OBSERVE_APP_USAGE permission.");
        } catch (SecurityException e) {
            // Exception expected
        }

        try {
            mUsageStatsManager.registerUsageSessionObserver(observerId, packages,
                    Duration.ofHours(1), Duration.ofSeconds(10), null, null);
            fail("Expected SecurityException for an app not holding OBSERVE_APP_USAGE permission.");
        } catch (SecurityException e) {
            // Exception expected
        }

        try {
            mUsageStatsManager.registerAppUsageLimitObserver(observerId, packages,
                    Duration.ofHours(1), Duration.ofHours(0), null);
            fail("Expected SecurityException for an app not holding OBSERVE_APP_USAGE permission.");
        } catch (SecurityException e) {
            // Exception expected
        }
    }

    @Test
    public void testObserveUsagePermissionForUnregisterObserver() {
        final int observerId = 0;

        try {
            mUsageStatsManager.unregisterAppUsageObserver(observerId);
            fail("Expected SecurityException for an app not holding OBSERVE_APP_USAGE permission.");
        } catch (SecurityException e) {
            // Exception expected
        }

        try {
            mUsageStatsManager.unregisterUsageSessionObserver(observerId);
            fail("Expected SecurityException for an app not holding OBSERVE_APP_USAGE permission.");
        } catch (SecurityException e) {
            // Exception expected
        }

        try {
            mUsageStatsManager.unregisterAppUsageLimitObserver(observerId);
            fail("Expected SecurityException for an app not holding OBSERVE_APP_USAGE permission.");
        } catch (SecurityException e) {
            // Exception expected
        }
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testForegroundService() throws Exception {
        // This test start a foreground service then stop it. The event list should have one
        // FOREGROUND_SERVICE_START and one FOREGROUND_SERVICE_STOP event.
        final long startTime = System.currentTimeMillis();
        mContext.startService(new Intent(mContext, TestService.class));
        mUiDevice.wait(Until.hasObject(By.clazz(TestService.class)), TIMEOUT);
        final long sleepTime = 500;
        SystemClock.sleep(sleepTime);
        mContext.stopService(new Intent(mContext, TestService.class));
        mUiDevice.wait(Until.gone(By.clazz(TestService.class)), TIMEOUT);
        final long endTime = System.currentTimeMillis();
        UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        int numStarts = 0;
        int numStops = 0;
        int startIdx = -1;
        int stopIdx = -1;
        int i = 0;
        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            if (mTargetPackage.equals(event.getPackageName())
                    || TestService.class.getName().equals(event.getClassName())) {
                if (event.getEventType() == Event.FOREGROUND_SERVICE_START) {
                    numStarts++;
                    startIdx = i;
                } else if (event.getEventType() == Event.FOREGROUND_SERVICE_STOP) {
                    numStops++;
                    stopIdx = i;
                }
                i++;
            }
        }
        // One FOREGROUND_SERVICE_START event followed by one FOREGROUND_SERVICE_STOP event.
        assertEquals(numStarts, 1);
        assertEquals(numStops, 1);
        assertLessThan(startIdx, stopIdx);

        final Map<String, UsageStats> map = mUsageStatsManager.queryAndAggregateUsageStats(
            startTime, endTime);
        final UsageStats stats = map.get(mTargetPackage);
        assertNotNull(stats);
        final long lastTimeUsed = stats.getLastTimeForegroundServiceUsed();
        // lastTimeUsed should be falling between startTime and endTime.
        assertLessThan(startTime, lastTimeUsed);
        assertLessThan(lastTimeUsed, endTime);
        final long totalTimeUsed = stats.getTotalTimeForegroundServiceUsed();
        // because we slept for 500 milliseconds earlier, we know the totalTimeUsed must be more
        // more than 500 milliseconds.
        assertLessThan(sleepTime, totalTimeUsed);
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testTaskRootEventField() throws Exception {
        mUiDevice.wakeUp();
        dismissKeyguard(); // also want to start out with the keyguard dismissed.

        final long startTime = System.currentTimeMillis();
        launchSubActivity(TaskRootActivity.class);
        final long endTime = System.currentTimeMillis();
        UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        while (events.hasNextEvent()) {
            UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            if (TaskRootActivity.TEST_APP_PKG.equals(event.getPackageName())
                    && TaskRootActivity.TEST_APP_CLASS.equals(event.getClassName())) {
                assertEquals(mTargetPackage, event.getTaskRootPackageName());
                assertEquals(TaskRootActivity.class.getCanonicalName(),
                        event.getTaskRootClassName());
                return;
            }
        }
        fail("Did not find nested activity name in usage events");
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testUsageSourceAttribution() throws Exception {
        mUiDevice.wakeUp();
        dismissKeyguard(); // also want to start out with the keyguard dismissed.
        mUiDevice.pressHome();

        setUsageSourceSetting(Integer.toString(UsageStatsManager.USAGE_SOURCE_CURRENT_ACTIVITY));
        launchSubActivity(TaskRootActivity.class);
        // Usage should be attributed to the test app package
        assertAppOrTokenUsed(TaskRootActivity.TEST_APP_PKG, true);

        SystemUtil.runWithShellPermissionIdentity(() -> mAm.forceStopPackage(TEST_APP_PKG));

        setUsageSourceSetting(Integer.toString(UsageStatsManager.USAGE_SOURCE_TASK_ROOT_ACTIVITY));
        launchSubActivity(TaskRootActivity.class);
        // Usage should be attributed to this package
        assertAppOrTokenUsed(mTargetPackage, true);
    }

    @AppModeInstant
    @Test
    public void testInstantAppUsageEventsObfuscated() throws Exception {
        @SuppressWarnings("unchecked")
        final Class<? extends Activity>[] activitySequence = new Class[] {
                Activities.ActivityOne.class,
                Activities.ActivityTwo.class,
                Activities.ActivityThree.class,
        };
        mUiDevice.wakeUp();
        mUiDevice.pressHome();

        final long startTime = System.currentTimeMillis();
        // Launch the series of Activities.
        launchSubActivities(activitySequence);
        SystemClock.sleep(250);

        final long endTime = System.currentTimeMillis();
        final UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        int resumes = 0;
        int pauses = 0;
        int stops = 0;

        // Only look at events belongs to mTargetPackage.
        while (events.hasNextEvent()) {
            final UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));
            // There should be no events with this packages name
            assertNotEquals("Instant app package name found in usage event list",
                    mTargetPackage, event.getPackageName());

            // Look for the obfuscated instant app string instead
            if(UsageEvents.INSTANT_APP_PACKAGE_NAME.equals(event.getPackageName())) {
                switch (event.mEventType) {
                    case Event.ACTIVITY_RESUMED:
                        resumes++;
                        break;
                    case Event.ACTIVITY_PAUSED:
                        pauses++;
                        break;
                    case Event.ACTIVITY_STOPPED:
                        stops++;
                        break;
                }
            }
        }
        assertEquals("Unexpected number of activity resumes", 3, resumes);
        assertEquals("Unexpected number of activity pauses", 2, pauses);
        assertEquals("Unexpected number of activity stops", 2, stops);
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testSuddenDestroy() throws Exception {
        mUiDevice.wakeUp();
        dismissKeyguard(); // also want to start out with the keyguard dismissed.
        mUiDevice.pressHome();

        final long startTime = System.currentTimeMillis();

        Intent intent = new Intent();
        intent.setClassName(TEST_APP_PKG, TEST_APP_CLASS);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        mUiDevice.wait(Until.hasObject(By.clazz(TEST_APP_PKG, TEST_APP_CLASS)), TIMEOUT);
        SystemClock.sleep(500);

        // Destroy the activity
        SystemUtil.runWithShellPermissionIdentity(() -> mAm.forceStopPackage(TEST_APP_PKG));
        mUiDevice.wait(Until.gone(By.clazz(TEST_APP_PKG, TEST_APP_CLASS)), TIMEOUT);
        SystemClock.sleep(500);

        final long endTime = System.currentTimeMillis();
        final UsageEvents events = mUsageStatsManager.queryEvents(startTime, endTime);

        int resumes = 0;
        int stops = 0;

        while (events.hasNextEvent()) {
            final UsageEvents.Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));

            if(TEST_APP_PKG.equals(event.getPackageName())) {
                switch (event.mEventType) {
                    case Event.ACTIVITY_RESUMED:
                        assertNotNull("ACTIVITY_RESUMED event Task Root should not be null",
                                event.getTaskRootPackageName());
                        resumes++;
                        break;
                    case Event.ACTIVITY_STOPPED:
                        assertNotNull("ACTIVITY_STOPPED event Task Root should not be null",
                                event.getTaskRootPackageName());
                        stops++;
                        break;
                }
            }
        }
        assertEquals("Unexpected number of activity resumes", 1, resumes);
        assertEquals("Unexpected number of activity stops", 1, stops);
    }

    @AppModeFull(reason = "No usage events access in instant apps")
    @Test
    public void testLocusIdEventsVisibility() throws Exception {
        final long startTime = System.currentTimeMillis();
        startAndDestroyActivityWithLocus();
        final long endTime = System.currentTimeMillis();

        final UsageEvents restrictedEvents = mUsageStatsManager.queryEvents(startTime, endTime);
        final UsageEvents allEvents = queryEventsAsShell(startTime, endTime);
        verifyLocusIdEventVisibility(restrictedEvents, false);
        verifyLocusIdEventVisibility(allEvents, true);
    }

    private void startAndDestroyActivityWithLocus() {
        Intent intent = new Intent();
        intent.setClassName(TEST_APP_PKG, TEST_APP_CLASS_LOCUS);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        mUiDevice.wait(Until.hasObject(By.clazz(TEST_APP_PKG, TEST_APP_CLASS_LOCUS)), TIMEOUT);
        SystemClock.sleep(500);

        // Destroy the activity
        SystemUtil.runWithShellPermissionIdentity(() -> mAm.forceStopPackage(TEST_APP_PKG));
        mUiDevice.wait(Until.gone(By.clazz(TEST_APP_PKG, TEST_APP_CLASS_LOCUS)), TIMEOUT);
        SystemClock.sleep(500);
    }

    private void verifyLocusIdEventVisibility(UsageEvents events, boolean hasPermission) {
        int locuses = 0;
        while (events.hasNextEvent()) {
            final Event event = new UsageEvents.Event();
            assertTrue(events.getNextEvent(event));

            if (TEST_APP_PKG.equals(event.getPackageName())
                    && event.mEventType == Event.LOCUS_ID_SET) {
                locuses++;
            }
        }

        if (hasPermission) {
            assertEquals("LOCUS_ID_SET events were not visible.", 2, locuses);
        } else {
            assertEquals("LOCUS_ID_SET events were visible.", 0, locuses);
        }
    }

    /**
     * Assert on an app or token's usage state.
     * @param entity name of the app or token
     * @param expected expected usage state, true for in use, false for not in use
     */
    private void assertAppOrTokenUsed(String entity, boolean expected) throws IOException {
        final String activeUsages = executeShellCmd("dumpsys usagestats apptimelimit actives");
        final String[] actives = activeUsages.split("\n");
        boolean found = false;

        for (String active : actives) {
            if (active.equals(entity)) {
                found = true;
                break;
            }
        }
        if (expected) {
            assertTrue(entity + " not found in list of active activities and tokens\n"
                    + activeUsages, found);
        } else {
            assertFalse(entity + " found in list of active activities and tokens\n"
                    + activeUsages, found);
        }
    }

    private void dismissKeyguard() throws Exception {
        if (mKeyguardManager.isKeyguardLocked()) {
            final long startTime = getEvents(KEYGUARD_EVENTS, 0, null) + 1;
            executeShellCmd("wm dismiss-keyguard");
            final ArrayList<Event> events = waitForEventCount(KEYGUARD_EVENTS, startTime, 1);
            assertEquals(Event.KEYGUARD_HIDDEN, events.get(0).getEventType());
            SystemClock.sleep(500);
        }
    }

    private void setStandByBucket(String packageName, String bucket) throws IOException {
        executeShellCmd("am set-standby-bucket " + packageName + " " + bucket);
    }

    private String executeShellCmd(String command) throws IOException {
        return mUiDevice.executeShellCommand(command);
    }

    private UsageEvents queryEventsAsShell(long start, long end) {
        return SystemUtil.runWithShellPermissionIdentity(() ->
                mUsageStatsManager.queryEvents(start, end));
    }

    private ITestReceiver bindToTestService() throws Exception {
        final TestServiceConnection connection = new TestServiceConnection();
        final Intent intent = new Intent().setComponent(
                new ComponentName(TEST_APP_PKG, TEST_APP_CLASS_SERVICE));
        mContext.bindService(intent, connection, Context.BIND_AUTO_CREATE);
        return ITestReceiver.Stub.asInterface(connection.getService());
    }

    private class TestServiceConnection implements ServiceConnection {
        private BlockingQueue<IBinder> mBlockingQueue = new LinkedBlockingQueue<>();

        public void onServiceConnected(ComponentName componentName, IBinder service) {
            mBlockingQueue.offer(service);
        }

        public void onServiceDisconnected(ComponentName componentName) {
        }

        public IBinder getService() throws Exception {
            final IBinder service = mBlockingQueue.poll(TIMEOUT_BINDER_SERVICE_SEC,
                    TimeUnit.SECONDS);
            return service;
        }
    }

    private boolean isAppInactiveAsPermissionlessApp(String pkg) throws Exception {
        final ITestReceiver testService = bindToTestService();
        return testService.isAppInactive(pkg);
    }
}
