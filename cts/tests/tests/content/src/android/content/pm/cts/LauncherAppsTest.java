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

package android.content.pm.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.app.usage.UsageStatsManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.os.Process;
import android.os.UserHandle;
import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Duration;
import java.util.ArrayList;
import java.util.concurrent.TimeUnit;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

/** Some tests in this class are ignored until b/126946674 is fixed. */
@RunWith(AndroidJUnit4.class)
public class LauncherAppsTest {

    private Context mContext;
    private LauncherApps mLauncherApps;
    private UsageStatsManager mUsageStatsManager;
    private ComponentName mDefaultHome;
    private ComponentName mTestHome;

    private static final String PACKAGE_NAME = "android.content.cts";
    private static final String FULL_CLASS_NAME = "android.content.pm.cts.LauncherMockActivity";
    private static final ComponentName FULL_COMPONENT_NAME = new ComponentName(
            PACKAGE_NAME, FULL_CLASS_NAME);

    private static final String FULL_DISABLED_CLASS_NAME =
            "android.content.pm.cts.MockActivity_Disabled";
    private static final ComponentName FULL_DISABLED_COMPONENT_NAME = new ComponentName(
            PACKAGE_NAME, FULL_DISABLED_CLASS_NAME);

    private static final int DEFAULT_OBSERVER_ID = 0;
    private static final String SETTINGS_PACKAGE = "com.android.settings";
    private static final String[] SETTINGS_PACKAGE_GROUP = new String[] {SETTINGS_PACKAGE};
    private static final int DEFAULT_TIME_LIMIT = 1;
    private static final UserHandle USER_HANDLE = Process.myUserHandle();

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mLauncherApps = (LauncherApps) mContext.getSystemService(Context.LAUNCHER_APPS_SERVICE);
        mUsageStatsManager = (UsageStatsManager) mContext.getSystemService(
                Context.USAGE_STATS_SERVICE);

        mDefaultHome = mContext.getPackageManager().getHomeActivities(new ArrayList<>());
        mTestHome = new ComponentName(PACKAGE_NAME, FULL_CLASS_NAME);
        setHomeActivity(mTestHome);
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_isNull() {
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        assertNull(limit); // An observer was never registered
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_isNotNull() {
        registerDefaultObserver();
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        assertNotNull(limit);
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_isNullOnUnregister() {
        registerDefaultObserver();
        unregisterObserver(DEFAULT_OBSERVER_ID);
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        assertNull("An unregistered observer was returned.", limit);
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_getTotalUsageLimit() {
        registerDefaultObserver();
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        assertEquals("Total usage limit not equal to the limit registered.",
                TimeUnit.MINUTES.toMillis(DEFAULT_TIME_LIMIT), limit.getTotalUsageLimit());
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_getTotalUsageRemaining() {
        registerDefaultObserver();
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        assertEquals("Usage remaining not equal to the total limit with no usage.",
                limit.getTotalUsageLimit(), limit.getUsageRemaining());
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_smallestLimitReturned() {
        registerDefaultObserver();
        registerObserver(1, Duration.ofMinutes(5), Duration.ofMinutes(0));
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        try {
            assertEquals("Smallest usage limit not returned when multiple limits exist.",
                    TimeUnit.MINUTES.toMillis(DEFAULT_TIME_LIMIT), limit.getTotalUsageLimit());
        } finally {
            unregisterObserver(1);
        }
    }

    @Test
    @Ignore("Can be enabled only after b/126946674 is fixed")
    @AppModeFull(reason = "Need special permission")
    public void testGetAppUsageLimit_zeroUsageRemaining() {
        registerObserver(DEFAULT_OBSERVER_ID, Duration.ofMinutes(1), Duration.ofMinutes(1));
        final LauncherApps.AppUsageLimit limit = mLauncherApps.getAppUsageLimit(
                SETTINGS_PACKAGE, USER_HANDLE);
        assertNotNull("An observer with an exhaused time limit was not registered.", limit);
        assertEquals("Usage remaining expected to be 0.", 0, limit.getUsageRemaining());
    }


    @Test
    @AppModeFull(reason = "Need special permission")
    public void testIsActivityEnabled() {
        assertTrue(mLauncherApps.isActivityEnabled(FULL_COMPONENT_NAME, USER_HANDLE));
        assertFalse(mLauncherApps.isActivityEnabled(FULL_DISABLED_COMPONENT_NAME, USER_HANDLE));

        mContext.getPackageManager().setComponentEnabledSetting(
                FULL_COMPONENT_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP);
        mContext.getPackageManager().setComponentEnabledSetting(
                FULL_DISABLED_COMPONENT_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
                PackageManager.DONT_KILL_APP);

        assertFalse(mLauncherApps.isActivityEnabled(FULL_COMPONENT_NAME, USER_HANDLE));
        assertFalse(mLauncherApps.isActivityEnabled(FULL_DISABLED_COMPONENT_NAME, USER_HANDLE));

        mContext.getPackageManager().setComponentEnabledSetting(
                FULL_COMPONENT_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP);
        mContext.getPackageManager().setComponentEnabledSetting(
                FULL_DISABLED_COMPONENT_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                PackageManager.DONT_KILL_APP);

        assertTrue(mLauncherApps.isActivityEnabled(FULL_COMPONENT_NAME, USER_HANDLE));
        assertTrue(mLauncherApps.isActivityEnabled(FULL_DISABLED_COMPONENT_NAME, USER_HANDLE));

        mContext.getPackageManager().setComponentEnabledSetting(
                FULL_COMPONENT_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                PackageManager.DONT_KILL_APP);
        mContext.getPackageManager().setComponentEnabledSetting(
                FULL_DISABLED_COMPONENT_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                PackageManager.DONT_KILL_APP);

        assertTrue(mLauncherApps.isActivityEnabled(FULL_COMPONENT_NAME, USER_HANDLE));
        assertFalse(mLauncherApps.isActivityEnabled(FULL_DISABLED_COMPONENT_NAME, USER_HANDLE));
    }

    @After
    public void tearDown() throws Exception {
        unregisterObserver(DEFAULT_OBSERVER_ID);
        if (mDefaultHome != null) {
            setHomeActivity(mDefaultHome);
        }
    }

    private void registerDefaultObserver() {
        registerObserver(DEFAULT_OBSERVER_ID, Duration.ofMinutes(DEFAULT_TIME_LIMIT),
                Duration.ofMinutes(0));
    }

    private void registerObserver(int observerId, Duration timeLimit, Duration timeUsed) {
        SystemUtil.runWithShellPermissionIdentity(() ->
                mUsageStatsManager.registerAppUsageLimitObserver(
                        observerId, SETTINGS_PACKAGE_GROUP, timeLimit, timeUsed,
                        PendingIntent.getActivity(mContext, -1, new Intent(), 0)));
    }

    private void unregisterObserver(int observerId) {
        SystemUtil.runWithShellPermissionIdentity(() ->
                mUsageStatsManager.unregisterAppUsageLimitObserver(observerId));
    }

    private void setHomeActivity(ComponentName component) throws Exception {
        SystemUtil.runShellCommand("cmd package set-home-activity --user "
                + USER_HANDLE.getIdentifier() + " " + component.flattenToString());
    }
}
