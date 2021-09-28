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

package android.appenumeration.cts;

import static android.appenumeration.cts.Constants.ACTION_GET_INSTALLED_PACKAGES;
import static android.appenumeration.cts.Constants.ACTION_GET_PACKAGE_INFO;
import static android.appenumeration.cts.Constants.ACTION_JUST_FINISH;
import static android.appenumeration.cts.Constants.ACTION_MANIFEST_ACTIVITY;
import static android.appenumeration.cts.Constants.ACTION_MANIFEST_PROVIDER;
import static android.appenumeration.cts.Constants.ACTION_MANIFEST_SERVICE;
import static android.appenumeration.cts.Constants.ACTION_QUERY_ACTIVITIES;
import static android.appenumeration.cts.Constants.ACTION_QUERY_PROVIDERS;
import static android.appenumeration.cts.Constants.ACTION_QUERY_RESOLVER;
import static android.appenumeration.cts.Constants.ACTION_QUERY_SERVICES;
import static android.appenumeration.cts.Constants.ACTION_START_DIRECTLY;
import static android.appenumeration.cts.Constants.ACTION_START_FOR_RESULT;
import static android.appenumeration.cts.Constants.ACTIVITY_CLASS_DUMMY_ACTIVITY;
import static android.appenumeration.cts.Constants.ACTIVITY_CLASS_TEST;
import static android.appenumeration.cts.Constants.EXTRA_DATA;
import static android.appenumeration.cts.Constants.EXTRA_ERROR;
import static android.appenumeration.cts.Constants.EXTRA_FLAGS;
import static android.appenumeration.cts.Constants.EXTRA_REMOTE_CALLBACK;
import static android.appenumeration.cts.Constants.QUERIES_ACTIVITY_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_PERM;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_PROVIDER;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_Q;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_RECEIVES_PERM_URI;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_RECEIVES_URI;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_SEES_INSTALLER;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_SEES_INSTALLER_APK;
import static android.appenumeration.cts.Constants.QUERIES_NOTHING_SHARED_USER;
import static android.appenumeration.cts.Constants.QUERIES_PACKAGE;
import static android.appenumeration.cts.Constants.QUERIES_PROVIDER_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_PROVIDER_AUTH;
import static android.appenumeration.cts.Constants.QUERIES_SERVICE_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_UNEXPORTED_ACTIVITY_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_UNEXPORTED_PROVIDER_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_UNEXPORTED_PROVIDER_AUTH;
import static android.appenumeration.cts.Constants.QUERIES_UNEXPORTED_SERVICE_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_ACTION;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_BROWSABLE;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_BROWSER;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_CONTACTS;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_EDITOR;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_SHARE;
import static android.appenumeration.cts.Constants.QUERIES_WILDCARD_WEB;
import static android.appenumeration.cts.Constants.TARGET_BROWSER;
import static android.appenumeration.cts.Constants.TARGET_BROWSER_WILDCARD;
import static android.appenumeration.cts.Constants.TARGET_CONTACTS;
import static android.appenumeration.cts.Constants.TARGET_EDITOR;
import static android.appenumeration.cts.Constants.TARGET_FILTERS;
import static android.appenumeration.cts.Constants.TARGET_FILTERS_APK;
import static android.appenumeration.cts.Constants.TARGET_FORCEQUERYABLE;
import static android.appenumeration.cts.Constants.TARGET_FORCEQUERYABLE_NORMAL;
import static android.appenumeration.cts.Constants.TARGET_NO_API;
import static android.appenumeration.cts.Constants.TARGET_SHARE;
import static android.appenumeration.cts.Constants.TARGET_SHARED_USER;
import static android.appenumeration.cts.Constants.TARGET_WEB;
import static android.content.pm.PackageManager.MATCH_SYSTEM_ONLY;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Parcelable;
import android.os.RemoteCallback;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.SystemUtil;

import org.hamcrest.core.IsNull;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.Objects;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

@RunWith(AndroidJUnit4.class)
public class AppEnumerationTests {
    private static Handler sResponseHandler;
    private static HandlerThread sResponseThread;

    private static boolean sGlobalFeatureEnabled;

    @Rule
    public TestName name = new TestName();

    @BeforeClass
    public static void setup() {
        final String deviceConfigResponse =
                SystemUtil.runShellCommand(
                        "device_config get package_manager_service "
                                + "package_query_filtering_enabled")
                        .trim();
        if ("null".equalsIgnoreCase(deviceConfigResponse) || deviceConfigResponse.isEmpty()) {
            sGlobalFeatureEnabled = true;
        } else {
            sGlobalFeatureEnabled = Boolean.parseBoolean(deviceConfigResponse);
        }
        System.out.println("Feature enabled: " + sGlobalFeatureEnabled);
        if (!sGlobalFeatureEnabled) return;

        sResponseThread = new HandlerThread("response");
        sResponseThread.start();
        sResponseHandler = new Handler(sResponseThread.getLooper());
    }

    @AfterClass
    public static void tearDown() {
        if (!sGlobalFeatureEnabled) return;
        sResponseThread.quit();
    }

    @Test
    public void systemPackagesQueryable_notEnabled() throws Exception {
        final Resources resources = Resources.getSystem();
        assertFalse(
                "config_forceSystemPackagesQueryable must not be true.",
                resources.getBoolean(resources.getIdentifier(
                        "config_forceSystemPackagesQueryable", "bool", "android")));

        // now let's assert that that the actual set of system apps is limited
        assertThat("Not all system apps should be visible.",
                getInstalledPackages(QUERIES_NOTHING_PERM, MATCH_SYSTEM_ONLY).length,
                greaterThan(getInstalledPackages(QUERIES_NOTHING, MATCH_SYSTEM_ONLY).length));
    }

    @Test
    public void all_canSeeForceQueryable() throws Exception {
        assertVisible(QUERIES_NOTHING, TARGET_FORCEQUERYABLE);
        assertVisible(QUERIES_ACTIVITY_ACTION, TARGET_FORCEQUERYABLE);
        assertVisible(QUERIES_SERVICE_ACTION, TARGET_FORCEQUERYABLE);
        assertVisible(QUERIES_PROVIDER_AUTH, TARGET_FORCEQUERYABLE);
        assertVisible(QUERIES_PACKAGE, TARGET_FORCEQUERYABLE);
    }

    @Test
    public void all_cannotSeeForceQueryableInstalledNormally() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_FORCEQUERYABLE_NORMAL);
        assertNotVisible(QUERIES_ACTIVITY_ACTION, TARGET_FORCEQUERYABLE_NORMAL);
        assertNotVisible(QUERIES_SERVICE_ACTION, TARGET_FORCEQUERYABLE_NORMAL);
        assertNotVisible(QUERIES_PROVIDER_AUTH, TARGET_FORCEQUERYABLE_NORMAL);
        assertNotVisible(QUERIES_PACKAGE, TARGET_FORCEQUERYABLE_NORMAL);
    }

    @Test
    public void startExplicitly_canStartNonVisible() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_FILTERS);
        startExplicitIntentViaComponent(QUERIES_NOTHING, TARGET_FILTERS);
        startExplicitIntentViaPackageName(QUERIES_NOTHING, TARGET_FILTERS);
    }

    @Test
    public void startExplicitly_canStartVisible() throws Exception {
        assertVisible(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS);
        startExplicitIntentViaComponent(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS);
        startExplicitIntentViaPackageName(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS);
    }

    @Test
    public void startImplicitly_canStartNonVisible() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_FILTERS);
        startImplicitIntent(QUERIES_NOTHING);
    }

    @Test
    public void startActivityWithNoPermissionUri_canSeeProvider() throws Exception {
        assertNotVisible(QUERIES_NOTHING_RECEIVES_URI, QUERIES_NOTHING_PERM);

        // send with uri but no grant flags; shouldn't be visible
        startExplicitActivityWithIntent(QUERIES_NOTHING_PERM, QUERIES_NOTHING_RECEIVES_URI,
                new Intent(ACTION_JUST_FINISH)
                        .setData(Uri.parse("content://" + QUERIES_NOTHING_PERM + "/test")));
        assertNotVisible(QUERIES_NOTHING_RECEIVES_URI, QUERIES_NOTHING_PERM);

        // send again with uri bug grant flags now set; should be visible
        startExplicitActivityWithIntent(QUERIES_NOTHING_PERM, QUERIES_NOTHING_RECEIVES_URI,
                new Intent(ACTION_JUST_FINISH)
                        .setData(Uri.parse("content://" + QUERIES_NOTHING_PERM + "/test"))
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        assertVisible(QUERIES_NOTHING_RECEIVES_URI, QUERIES_NOTHING_PERM);
    }

    @Test
    public void startActivityWithUri_canSeePermissionProtectedProvider() throws Exception {
        assertNotVisible(QUERIES_NOTHING_RECEIVES_PERM_URI, QUERIES_NOTHING_PERM);

        // send with uri but no grant flags; shouldn't be visible
        startExplicitActivityWithIntent(QUERIES_NOTHING_PERM, QUERIES_NOTHING_RECEIVES_PERM_URI,
                new Intent(ACTION_JUST_FINISH)
                        .setData(Uri.parse("content://" + QUERIES_NOTHING_PERM + "2/test")));
        assertNotVisible(QUERIES_NOTHING_RECEIVES_PERM_URI, QUERIES_NOTHING_PERM);

        // send again with uri bug grant flags now set; should be visible
        startExplicitActivityWithIntent(QUERIES_NOTHING_PERM, QUERIES_NOTHING_RECEIVES_PERM_URI,
                new Intent(ACTION_JUST_FINISH)
                        .setData(Uri.parse("content://" + QUERIES_NOTHING_PERM + "2/test"))
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        assertVisible(QUERIES_NOTHING_RECEIVES_PERM_URI, QUERIES_NOTHING_PERM);
    }

    private void startExplicitActivityWithIntent(
            String sourcePackageName, String targetPackageName, Intent intent) throws Exception {
        sendCommandBlocking(sourcePackageName, targetPackageName,
                intent.setClassName(targetPackageName, ACTIVITY_CLASS_TEST),
                ACTION_START_DIRECTLY);
    }

    @Test
    public void queriesNothing_cannotSeeNonForceQueryable() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_NO_API);
        assertNotVisible(QUERIES_NOTHING, TARGET_FILTERS);
    }

    @Test
    public void queriesNothingTargetsQ_canSeeAll() throws Exception {
        assertVisible(QUERIES_NOTHING_Q, TARGET_FORCEQUERYABLE);
        assertVisible(QUERIES_NOTHING_Q, TARGET_NO_API);
        assertVisible(QUERIES_NOTHING_Q, TARGET_FILTERS);
    }

    @Test
    public void queriesNothingHasPermission_canSeeAll() throws Exception {
        assertVisible(QUERIES_NOTHING_PERM, TARGET_FORCEQUERYABLE);
        assertVisible(QUERIES_NOTHING_PERM, TARGET_NO_API);
        assertVisible(QUERIES_NOTHING_PERM, TARGET_FILTERS);
    }

    @Test
    public void queriesNothing_cannotSeeFilters() throws Exception {
        assertNotQueryable(QUERIES_NOTHING, TARGET_FILTERS,
                ACTION_MANIFEST_ACTIVITY, this::queryIntentActivities);
        assertNotQueryable(QUERIES_NOTHING, TARGET_FILTERS,
                ACTION_MANIFEST_SERVICE, this::queryIntentServices);
        assertNotQueryable(QUERIES_NOTHING, TARGET_FILTERS,
                ACTION_MANIFEST_PROVIDER, this::queryIntentProviders);
    }

    @Test
    public void queriesActivityAction_canSeeFilters() throws Exception {
        assertQueryable(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS,
                ACTION_MANIFEST_ACTIVITY, this::queryIntentActivities);
        assertQueryable(QUERIES_SERVICE_ACTION, TARGET_FILTERS,
                ACTION_MANIFEST_SERVICE, this::queryIntentServices);
        assertQueryable(QUERIES_PROVIDER_AUTH, TARGET_FILTERS,
                ACTION_MANIFEST_PROVIDER, this::queryIntentProviders);
        assertQueryable(QUERIES_PROVIDER_ACTION, TARGET_FILTERS,
                ACTION_MANIFEST_PROVIDER, this::queryIntentProviders);
    }

    @Test
    public void queriesNothingHasPermission_canSeeFilters() throws Exception {
        assertQueryable(QUERIES_NOTHING_PERM, TARGET_FILTERS,
                ACTION_MANIFEST_ACTIVITY, this::queryIntentActivities);
        assertQueryable(QUERIES_NOTHING_PERM, TARGET_FILTERS,
                ACTION_MANIFEST_SERVICE, this::queryIntentServices);
        assertQueryable(QUERIES_NOTHING_PERM, TARGET_FILTERS,
                ACTION_MANIFEST_PROVIDER, this::queryIntentProviders);
    }

    @Test
    public void queriesSomething_cannotSeeNoApi() throws Exception {
        assertNotVisible(QUERIES_ACTIVITY_ACTION, TARGET_NO_API);
        assertNotVisible(QUERIES_SERVICE_ACTION, TARGET_NO_API);
        assertNotVisible(QUERIES_PROVIDER_AUTH, TARGET_NO_API);
        assertNotVisible(QUERIES_PROVIDER_ACTION, TARGET_NO_API);
    }

    @Test
    public void queriesActivityAction_canSeeTarget() throws Exception {
        assertVisible(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesServiceAction_canSeeTarget() throws Exception {
        assertVisible(QUERIES_SERVICE_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesWildcardAction_canSeeTargets() throws Exception {
        assertVisible(QUERIES_WILDCARD_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesProviderAuthority_canSeeTarget() throws Exception {
        assertVisible(QUERIES_PROVIDER_AUTH, TARGET_FILTERS);
    }

    @Test
    public void queriesProviderAction_canSeeTarget() throws Exception {
        assertVisible(QUERIES_PROVIDER_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesActivityAction_cannotSeeUnexportedTarget() throws Exception {
        assertNotVisible(QUERIES_UNEXPORTED_ACTIVITY_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesServiceAction_cannotSeeUnexportedTarget() throws Exception {
        assertNotVisible(QUERIES_UNEXPORTED_SERVICE_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesProviderAuthority_cannotSeeUnexportedTarget() throws Exception {
        assertNotVisible(QUERIES_UNEXPORTED_PROVIDER_AUTH, TARGET_FILTERS);
    }

    @Test
    public void queriesProviderAction_cannotSeeUnexportedTarget() throws Exception {
        assertNotVisible(QUERIES_UNEXPORTED_PROVIDER_ACTION, TARGET_FILTERS);
    }

    @Test
    public void queriesPackage_canSeeTarget() throws Exception {
        assertVisible(QUERIES_PACKAGE, TARGET_NO_API);
    }

    @Test
    public void queriesNothing_canSeeInstaller() throws Exception {
        runShellCommand("pm uninstall " + QUERIES_NOTHING_SEES_INSTALLER);
        runShellCommand("pm install"
                + " -i " + TARGET_NO_API
                + " --pkg " + QUERIES_NOTHING_SEES_INSTALLER
                + " " + QUERIES_NOTHING_SEES_INSTALLER_APK);
        try {
            assertVisible(QUERIES_NOTHING_SEES_INSTALLER, TARGET_NO_API);
        } finally {
            runShellCommand("pm uninstall " + QUERIES_NOTHING_SEES_INSTALLER);
        }
    }


    @Test
    public void whenStarted_canSeeCaller() throws Exception {
        // let's first make sure that the target cannot see the caller.
        assertNotVisible(QUERIES_NOTHING, QUERIES_NOTHING_PERM);
        // now let's start the target and make sure that it can see the caller as part of that call
        PackageInfo packageInfo = startForResult(QUERIES_NOTHING_PERM, QUERIES_NOTHING);
        assertThat(packageInfo, IsNull.notNullValue());
        assertThat(packageInfo.packageName, is(QUERIES_NOTHING_PERM));
        // and finally let's re-run the last check to make sure that the target can still see the
        // caller
        assertVisible(QUERIES_NOTHING, QUERIES_NOTHING_PERM);
    }

    @Test
    public void whenStartedViaIntentSender_canSeeCaller() throws Exception {
        // let's first make sure that the target cannot see the caller.
        assertNotVisible(QUERIES_NOTHING, QUERIES_NOTHING_Q);
        // now let's start the target via pending intent and make sure that it can see the caller
        // as part of that call
        PackageInfo packageInfo = startSenderForResult(QUERIES_NOTHING_Q, QUERIES_NOTHING);
        assertThat(packageInfo, IsNull.notNullValue());
        assertThat(packageInfo.packageName, is(QUERIES_NOTHING_Q));
        // and finally let's re-run the last check to make sure that the target can still see the
        // caller
        assertVisible(QUERIES_NOTHING, QUERIES_NOTHING_Q);
    }


    @Test
    public void sharedUserMember_canSeeOtherMember() throws Exception {
        assertVisible(QUERIES_NOTHING_SHARED_USER, TARGET_SHARED_USER);
    }

    @Test
    public void queriesPackage_canSeeAllSharedUserMembers() throws Exception {
        // explicitly queries target via manifest
        assertVisible(QUERIES_PACKAGE, TARGET_SHARED_USER);
        // implicitly granted visibility to other member of shared user
        assertVisible(QUERIES_PACKAGE, QUERIES_NOTHING_SHARED_USER);
    }

    @Test
    public void queriesWildcardContacts() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_CONTACTS);
        assertVisible(QUERIES_WILDCARD_CONTACTS, TARGET_CONTACTS);
    }

    @Test
    public void queriesWildcardWeb() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_WEB);
        assertVisible(QUERIES_WILDCARD_BROWSABLE, TARGET_WEB);
        assertVisible(QUERIES_WILDCARD_WEB, TARGET_WEB);
    }

    @Test
    public void queriesWildcardBrowser() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_BROWSER);
        assertNotVisible(QUERIES_WILDCARD_BROWSER, TARGET_WEB);
        assertVisible(QUERIES_WILDCARD_BROWSER, TARGET_BROWSER);
        assertVisible(QUERIES_WILDCARD_BROWSER, TARGET_BROWSER_WILDCARD);
    }


    @Test
    public void queriesWildcardEditor() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_EDITOR);
        assertVisible(QUERIES_WILDCARD_EDITOR, TARGET_EDITOR);
    }

    @Test
    public void queriesWildcardShareSheet() throws Exception {
        assertNotVisible(QUERIES_NOTHING, TARGET_SHARE);
        assertVisible(QUERIES_WILDCARD_SHARE, TARGET_SHARE);
    }

    private void assertVisible(String sourcePackageName, String targetPackageName)
            throws Exception {
        if (!sGlobalFeatureEnabled) return;
        Assert.assertNotNull(sourcePackageName + " should be able to see " + targetPackageName,
                getPackageInfo(sourcePackageName, targetPackageName));
    }

    @Test
    public void broadcastAdded_notVisibleDoesNotReceive() throws Exception {
        final Result result = sendCommand(QUERIES_NOTHING, TARGET_FILTERS, null,
                Constants.ACTION_AWAIT_PACKAGE_ADDED);
        runShellCommand("pm install " + TARGET_FILTERS_APK);
        try {
            result.await();
            fail();
        } catch (MissingBroadcastException e) {
            // hooray
        }
    }

    @Test
    public void broadcastAdded_visibleReceives() throws Exception {
        final Result result = sendCommand(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS, null,
                Constants.ACTION_AWAIT_PACKAGE_ADDED);
        runShellCommand("pm install " + TARGET_FILTERS_APK);
        try {
            Assert.assertEquals(TARGET_FILTERS,
                    Uri.parse(result.await().getString(EXTRA_DATA)).getSchemeSpecificPart());
        } catch (MissingBroadcastException e) {
            fail();
        }
    }

    @Test
    public void broadcastRemoved_notVisibleDoesNotReceive() throws Exception {
        final Result result = sendCommand(QUERIES_NOTHING, TARGET_FILTERS, null,
                Constants.ACTION_AWAIT_PACKAGE_REMOVED);
        runShellCommand("pm install " + TARGET_FILTERS_APK);
        try {
            result.await();
            fail();
        } catch (MissingBroadcastException e) {
            // hooray
        }
    }

    @Test
    public void broadcastRemoved_visibleReceives() throws Exception {
        final Result result = sendCommand(QUERIES_ACTIVITY_ACTION, TARGET_FILTERS, null,
                Constants.ACTION_AWAIT_PACKAGE_REMOVED);
        runShellCommand("pm install " + TARGET_FILTERS_APK);
        try {
            Assert.assertEquals(TARGET_FILTERS,
                    Uri.parse(result.await().getString(EXTRA_DATA)).getSchemeSpecificPart());
        } catch (MissingBroadcastException e) {
            fail();
        }
    }

    @Test
    public void queriesResolver_grantsVisibilityToProvider() throws Exception {
        assertNotVisible(QUERIES_NOTHING_PROVIDER, QUERIES_NOTHING_PERM);

        String[] result = sendCommandBlocking(
                QUERIES_NOTHING_PERM, QUERIES_NOTHING_PROVIDER, null, ACTION_QUERY_RESOLVER)
                .getStringArray(Intent.EXTRA_RETURN_RESULT);
        Arrays.sort(result);
        assertThat(QUERIES_NOTHING_PERM + " not visible to " + QUERIES_NOTHING_PROVIDER
                        + " during resolver interaction",
                Arrays.binarySearch(result, QUERIES_NOTHING_PERM),
                greaterThanOrEqualTo(0));

        assertVisible(QUERIES_NOTHING_PROVIDER, QUERIES_NOTHING_PERM);
    }

    private void assertNotVisible(String sourcePackageName, String targetPackageName)
            throws Exception {
        if (!sGlobalFeatureEnabled) return;
        try {
            getPackageInfo(sourcePackageName, targetPackageName);
            fail(sourcePackageName + " should not be able to see " + targetPackageName);
        } catch (PackageManager.NameNotFoundException ignored) {
        }
    }

    interface ThrowingBiFunction<T, U, R> {
        R apply(T arg1, U arg2) throws Exception;
    }

    private void assertNotQueryable(String sourcePackageName, String targetPackageName,
            String intentAction, ThrowingBiFunction<String, Intent, String[]> commandMethod)
            throws Exception {
        if (!sGlobalFeatureEnabled) return;
        Intent intent = new Intent(intentAction);
        String[] queryablePackageNames = commandMethod.apply(sourcePackageName, intent);
        for (String packageName : queryablePackageNames) {
            if (packageName.contentEquals(targetPackageName)) {
                fail(sourcePackageName + " should not be able to query " + targetPackageName +
                        " via " + intentAction);
            }
        }
    }

    private void assertQueryable(String sourcePackageName, String targetPackageName,
            String intentAction, ThrowingBiFunction<String, Intent, String[]> commandMethod)
            throws Exception {
        if (!sGlobalFeatureEnabled) return;
        Intent intent = new Intent(intentAction);
        String[] queryablePackageNames = commandMethod.apply(sourcePackageName, intent);
        for (String packageName : queryablePackageNames) {
            if (packageName.contentEquals(targetPackageName)) {
                return;
            }
        }
        fail(sourcePackageName + " should be able to query " + targetPackageName + " via "
                + intentAction);
    }

    private PackageInfo getPackageInfo(String sourcePackageName, String targetPackageName)
            throws Exception {
        Bundle response = sendCommandBlocking(sourcePackageName, targetPackageName,
                null /*queryIntent*/, ACTION_GET_PACKAGE_INFO);
        return response.getParcelable(Intent.EXTRA_RETURN_RESULT);
    }

    private PackageInfo startForResult(String sourcePackageName, String targetPackageName)
            throws Exception {
        Bundle response = sendCommandBlocking(sourcePackageName, targetPackageName,
                null /*queryIntent*/, ACTION_START_FOR_RESULT);
        return response.getParcelable(Intent.EXTRA_RETURN_RESULT);
    }

    private PackageInfo startSenderForResult(String sourcePackageName, String targetPackageName)
            throws Exception {
        PendingIntent pendingIntent = PendingIntent.getActivity(
                InstrumentationRegistry.getInstrumentation().getContext(), 100,
                new Intent("android.appenumeration.cts.action.SEND_RESULT").setComponent(
                        new ComponentName(targetPackageName,
                                "android.appenumeration.cts.query.TestActivity")),
                PendingIntent.FLAG_ONE_SHOT);

        Bundle response = sendCommandBlocking(sourcePackageName, targetPackageName,
                pendingIntent /*queryIntent*/, Constants.ACTION_START_SENDER_FOR_RESULT);
        return response.getParcelable(Intent.EXTRA_RETURN_RESULT);
    }


    private String[] queryIntentActivities(String sourcePackageName, Intent queryIntent)
            throws Exception {
        Bundle response =
                sendCommandBlocking(sourcePackageName, null, queryIntent, ACTION_QUERY_ACTIVITIES);
        return response.getStringArray(Intent.EXTRA_RETURN_RESULT);
    }

    private String[] queryIntentServices(String sourcePackageName, Intent queryIntent)
            throws Exception {
        Bundle response = sendCommandBlocking(sourcePackageName, null, queryIntent,
                ACTION_QUERY_SERVICES);
        return response.getStringArray(Intent.EXTRA_RETURN_RESULT);
    }

    private String[] queryIntentProviders(String sourcePackageName, Intent queryIntent)
            throws Exception {
        Bundle response = sendCommandBlocking(sourcePackageName, null, queryIntent,
                ACTION_QUERY_PROVIDERS);
        return response.getStringArray(Intent.EXTRA_RETURN_RESULT);
    }

    private String[] getInstalledPackages(String sourcePackageNames, int flags) throws Exception {
        Bundle response = sendCommandBlocking(sourcePackageNames, null,
                new Intent().putExtra(EXTRA_FLAGS, flags), ACTION_GET_INSTALLED_PACKAGES);
        return response.getStringArray(Intent.EXTRA_RETURN_RESULT);
    }

    private void startExplicitIntentViaComponent(String sourcePackage, String targetPackage)
            throws Exception {
        sendCommandBlocking(sourcePackage, targetPackage,
                new Intent().setComponent(new ComponentName(targetPackage,
                        ACTIVITY_CLASS_DUMMY_ACTIVITY)),
                ACTION_START_DIRECTLY);
    }
    private void startExplicitIntentViaPackageName(String sourcePackage, String targetPackage)
            throws Exception {
        sendCommandBlocking(sourcePackage, targetPackage,
                new Intent().setPackage(targetPackage),
                ACTION_START_DIRECTLY);
    }

    private void startImplicitIntent(String sourcePackage) throws Exception {
        sendCommandBlocking(sourcePackage, TARGET_FILTERS, new Intent(ACTION_MANIFEST_ACTIVITY),
                ACTION_START_DIRECTLY);
    }

    interface Result {
        Bundle await() throws Exception;
    }

    private Result sendCommand(String sourcePackageName,
            @Nullable String targetPackageName,
            @Nullable Parcelable intentExtra, String action) {
        final Intent intent = new Intent(action)
                .setComponent(new ComponentName(sourcePackageName, ACTIVITY_CLASS_TEST))
                // data uri unique to each activity start to ensure actual launch and not just
                // redisplay
                .setData(Uri.parse("test://" + UUID.randomUUID().toString()))
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        if (targetPackageName != null) {
            intent.putExtra(Intent.EXTRA_PACKAGE_NAME, targetPackageName);
        }
        if (intentExtra != null) {
            if (intentExtra instanceof Intent) {
                intent.putExtra(Intent.EXTRA_INTENT, intentExtra);
            } else if (intentExtra instanceof PendingIntent) {
                intent.putExtra("pendingIntent", intentExtra);
            }
        }

        final ConditionVariable latch = new ConditionVariable();
        final AtomicReference<Bundle> resultReference = new AtomicReference<>();
        final RemoteCallback callback = new RemoteCallback(
                bundle -> {
                    resultReference.set(bundle);
                    latch.open();
                },
                sResponseHandler);
        intent.putExtra(EXTRA_REMOTE_CALLBACK, callback);
        InstrumentationRegistry.getInstrumentation().getContext().startActivity(intent);
        return () -> {
            if (!latch.block(TimeUnit.SECONDS.toMillis(10))) {
                throw new TimeoutException(
                        "Latch timed out while awiating a response from " + sourcePackageName);
            }
            final Bundle bundle = resultReference.get();
            if (bundle != null && bundle.containsKey(EXTRA_ERROR)) {
                throw (Exception) Objects.requireNonNull(bundle.getSerializable(EXTRA_ERROR));
            }
            return bundle;
        };
    }

    private Bundle sendCommandBlocking(String sourcePackageName, @Nullable String targetPackageName,
            @Nullable Parcelable intentExtra, String action)
            throws Exception {
        Result result = sendCommand(sourcePackageName, targetPackageName, intentExtra, action);
        return result.await();
    }

}
