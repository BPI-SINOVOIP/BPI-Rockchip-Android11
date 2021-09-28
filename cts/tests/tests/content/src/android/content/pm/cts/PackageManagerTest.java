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

package android.content.pm.cts;

import static android.content.pm.ApplicationInfo.FLAG_HAS_CODE;
import static android.content.pm.ApplicationInfo.FLAG_INSTALLED;
import static android.content.pm.ApplicationInfo.FLAG_SYSTEM;
import static android.content.pm.PackageManager.GET_ACTIVITIES;
import static android.content.pm.PackageManager.GET_META_DATA;
import static android.content.pm.PackageManager.GET_PERMISSIONS;
import static android.content.pm.PackageManager.GET_PROVIDERS;
import static android.content.pm.PackageManager.GET_RECEIVERS;
import static android.content.pm.PackageManager.GET_SERVICES;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.cts.R;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageItemInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PermissionGroupInfo;
import android.content.pm.PermissionInfo;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.platform.test.annotations.AppModeFull;
import android.text.TextUtils;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Collectors;

/**
 * This test is based on the declarations in AndroidManifest.xml. We create mock declarations
 * in AndroidManifest.xml just for test of PackageManager, and there are no corresponding parts
 * of these declarations in test project.
 */
@AppModeFull // TODO(Instant) Figure out which APIs should work.
@RunWith(AndroidJUnit4.class)
public class PackageManagerTest {
    private static final String TAG = "PackageManagerTest";

    private PackageManager mPackageManager;
    private static final String PACKAGE_NAME = "android.content.cts";
    private static final String CONTENT_PKG_NAME = "android.content.cts";
    private static final String APPLICATION_NAME = "android.content.cts.MockApplication";
    private static final String ACTIVITY_ACTION_NAME = "android.intent.action.PMTEST";
    private static final String MAIN_ACTION_NAME = "android.intent.action.MAIN";
    private static final String SERVICE_ACTION_NAME =
                                "android.content.pm.cts.activity.PMTEST_SERVICE";
    private static final String GRANTED_PERMISSION_NAME = "android.permission.INTERNET";
    private static final String NOT_GRANTED_PERMISSION_NAME = "android.permission.HARDWARE_TEST";
    private static final String ACTIVITY_NAME = "android.content.pm.cts.TestPmActivity";
    private static final String SERVICE_NAME = "android.content.pm.cts.TestPmService";
    private static final String RECEIVER_NAME = "android.content.pm.cts.PmTestReceiver";
    private static final String INSTRUMENT_NAME = "android.content.pm.cts.TestPmInstrumentation";
    private static final String CALL_ABROAD_PERMISSION_NAME =
            "android.content.cts.CALL_ABROAD_PERMISSION";
    private static final String PROVIDER_NAME = "android.content.cts.MockContentProvider";
    private static final String PERMISSIONGROUP_NAME = "android.permission-group.COST_MONEY";
    private static final String PERMISSION_TREE_ROOT =
            "android.content.cts.permission.TEST_DYNAMIC";
    // Number of activities/activity-alias in AndroidManifest
    private static final int NUM_OF_ACTIVITIES_IN_MANIFEST = 12;

    private static final String SHIM_APEX_PACKAGE_NAME = "com.android.apex.cts.shim";

    @Before
    public void setup() throws Exception {
        mPackageManager = InstrumentationRegistry.getContext().getPackageManager();
    }

    @Test
    public void testQuery() throws NameNotFoundException {
        // Test query Intent Activity related methods

        Intent activityIntent = new Intent(ACTIVITY_ACTION_NAME);
        String cmpActivityName = "android.content.pm.cts.TestPmCompare";
        // List with different activities and the filter doesn't work,
        List<ResolveInfo> listWithDiff = mPackageManager.queryIntentActivityOptions(
                new ComponentName(PACKAGE_NAME, cmpActivityName), null, activityIntent, 0);
        checkActivityInfoName(ACTIVITY_NAME, listWithDiff);

        // List with the same activities to make filter work
        List<ResolveInfo> listInSame = mPackageManager.queryIntentActivityOptions(
                new ComponentName(PACKAGE_NAME, ACTIVITY_NAME), null, activityIntent, 0);
        assertEquals(0, listInSame.size());

        // Test queryIntentActivities
        List<ResolveInfo> intentActivities =
                mPackageManager.queryIntentActivities(activityIntent, 0);
        assertTrue(intentActivities.size() > 0);
        checkActivityInfoName(ACTIVITY_NAME, intentActivities);

        // End of Test query Intent Activity related methods

        // Test queryInstrumentation
        String targetPackage = "android";
        List<InstrumentationInfo> instrumentations = mPackageManager.queryInstrumentation(
                targetPackage, 0);
        checkInstrumentationInfoName(INSTRUMENT_NAME, instrumentations);

        // Test queryIntentServices
        Intent serviceIntent = new Intent(SERVICE_ACTION_NAME);
        List<ResolveInfo> services = mPackageManager.queryIntentServices(serviceIntent,
                PackageManager.GET_INTENT_FILTERS);
        checkServiceInfoName(SERVICE_NAME, services);

        // Test queryBroadcastReceivers
        String receiverActionName = "android.content.pm.cts.PackageManagerTest.PMTEST_RECEIVER";
        Intent broadcastIntent = new Intent(receiverActionName);
        List<ResolveInfo> broadcastReceivers = new ArrayList<ResolveInfo>();
        broadcastReceivers = mPackageManager.queryBroadcastReceivers(broadcastIntent, 0);
        checkActivityInfoName(RECEIVER_NAME, broadcastReceivers);

        // Test queryPermissionsByGroup, queryContentProviders
        String testPermissionsGroup = "android.permission-group.COST_MONEY";
        List<PermissionInfo> permissions = mPackageManager.queryPermissionsByGroup(
                testPermissionsGroup, PackageManager.GET_META_DATA);
        checkPermissionInfoName(CALL_ABROAD_PERMISSION_NAME, permissions);

        ApplicationInfo appInfo = mPackageManager.getApplicationInfo(PACKAGE_NAME, 0);
        List<ProviderInfo> providers = mPackageManager.queryContentProviders(PACKAGE_NAME,
                appInfo.uid, 0);
        checkProviderInfoName(PROVIDER_NAME, providers);
    }

    private void checkActivityInfoName(String expectedName, List<ResolveInfo> resolves) {
        // Flag for checking if the name is contained in list array.
        boolean isContained = false;
        Iterator<ResolveInfo> infoIterator = resolves.iterator();
        String current;
        while (infoIterator.hasNext()) {
            current = infoIterator.next().activityInfo.name;
            if (current.equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }

    private void checkServiceInfoName(String expectedName, List<ResolveInfo> resolves) {
        boolean isContained = false;
        Iterator<ResolveInfo> infoIterator = resolves.iterator();
        String current;
        while (infoIterator.hasNext()) {
            current = infoIterator.next().serviceInfo.name;
            if (current.equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }

    private void checkPermissionInfoName(String expectedName, List<PermissionInfo> permissions) {
        List<String> names = new ArrayList<String>();
        for (PermissionInfo permission : permissions) {
            names.add(permission.name);
        }
        boolean isContained = names.contains(expectedName);
        assertTrue("Permission " + expectedName + " not present in " + names, isContained);
    }

    private void checkProviderInfoName(String expectedName, List<ProviderInfo> providers) {
        boolean isContained = false;
        Iterator<ProviderInfo> infoIterator = providers.iterator();
        String current;
        while (infoIterator.hasNext()) {
            current = infoIterator.next().name;
            if (current.equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }

    private void checkInstrumentationInfoName(String expectedName,
            List<InstrumentationInfo> instrumentations) {
        boolean isContained = false;
        Iterator<InstrumentationInfo> infoIterator = instrumentations.iterator();
        String current;
        while (infoIterator.hasNext()) {
            current = infoIterator.next().name;
            if (current.equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }

    @Test
    public void testGetInfo() throws NameNotFoundException {
        // Test getApplicationInfo, getText
        ApplicationInfo appInfo = mPackageManager.getApplicationInfo(PACKAGE_NAME, 0);
        int discriptionRes = R.string.hello_android;
        String expectedDisciptionRes = "Hello, Android!";
        CharSequence appText = mPackageManager.getText(PACKAGE_NAME, discriptionRes, appInfo);
        assertEquals(expectedDisciptionRes, appText);
        ComponentName activityName = new ComponentName(PACKAGE_NAME, ACTIVITY_NAME);
        ComponentName serviceName = new ComponentName(PACKAGE_NAME, SERVICE_NAME);
        ComponentName receiverName = new ComponentName(PACKAGE_NAME, RECEIVER_NAME);
        ComponentName instrName = new ComponentName(PACKAGE_NAME, INSTRUMENT_NAME);

        // Test getPackageInfo
        PackageInfo packageInfo = mPackageManager.getPackageInfo(PACKAGE_NAME,
                PackageManager.GET_INSTRUMENTATION);
        assertEquals(PACKAGE_NAME, packageInfo.packageName);

        // Test getApplicationInfo, getApplicationLabel
        String appLabel = "Android TestCase";
        assertEquals(appLabel, mPackageManager.getApplicationLabel(appInfo));
        assertEquals(PACKAGE_NAME, appInfo.processName);

        // Test getServiceInfo
        assertEquals(SERVICE_NAME, mPackageManager.getServiceInfo(serviceName,
                PackageManager.GET_META_DATA).name);

        // Test getReceiverInfo
        assertEquals(RECEIVER_NAME, mPackageManager.getReceiverInfo(receiverName, 0).name);

        // Test getPackageArchiveInfo
        final String apkRoute = InstrumentationRegistry.getContext().getPackageCodePath();
        final String apkName = InstrumentationRegistry.getContext().getPackageName();
        assertEquals(apkName, mPackageManager.getPackageArchiveInfo(apkRoute, 0).packageName);

        // Test getPackagesForUid, getNameForUid
        checkPackagesNameForUid(PACKAGE_NAME, mPackageManager.getPackagesForUid(appInfo.uid));
        assertEquals(PACKAGE_NAME, mPackageManager.getNameForUid(appInfo.uid));

        // Test getActivityInfo
        assertEquals(ACTIVITY_NAME, mPackageManager.getActivityInfo(activityName, 0).name);

        // Test getPackageGids
        assertTrue(mPackageManager.getPackageGids(PACKAGE_NAME).length > 0);

        // Test getPermissionInfo
        assertEquals(GRANTED_PERMISSION_NAME,
                mPackageManager.getPermissionInfo(GRANTED_PERMISSION_NAME, 0).name);

        // Test getPermissionGroupInfo
        assertEquals(PERMISSIONGROUP_NAME, mPackageManager.getPermissionGroupInfo(
                PERMISSIONGROUP_NAME, 0).name);

        // Test getAllPermissionGroups
        List<PermissionGroupInfo> permissionGroups = mPackageManager.getAllPermissionGroups(0);
        checkPermissionGroupInfoName(PERMISSIONGROUP_NAME, permissionGroups);

        // Test getInstalledApplications
        assertTrue(mPackageManager.getInstalledApplications(PackageManager.GET_META_DATA).size() > 0);

        // Test getInstalledPacakge
        assertTrue(mPackageManager.getInstalledPackages(0).size() > 0);

        // Test getInstrumentationInfo
        assertEquals(INSTRUMENT_NAME, mPackageManager.getInstrumentationInfo(instrName, 0).name);

        // Test getSystemSharedLibraryNames, in javadoc, String array and null
        // are all OK as return value.
        mPackageManager.getSystemSharedLibraryNames();

        // Test getLaunchIntentForPackage, Intent of activity
        // android.content.pm.cts.TestPmCompare is set to match the condition
        // to make sure the return of this method is not null.
        assertEquals(MAIN_ACTION_NAME, mPackageManager.getLaunchIntentForPackage(PACKAGE_NAME)
                .getAction());

        // Test isSafeMode. Because the test case will not run in safe mode, so
        // the return will be false.
        assertFalse(mPackageManager.isSafeMode());
    }

    private void checkPackagesNameForUid(String expectedName, String[] uid) {
        boolean isContained = false;
        for (int i = 0; i < uid.length; i++) {
            if (uid[i].equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }

    private void checkPermissionGroupInfoName(String expectedName,
            List<PermissionGroupInfo> permissionGroups) {
        boolean isContained = false;
        Iterator<PermissionGroupInfo> infoIterator = permissionGroups.iterator();
        String current;
        while (infoIterator.hasNext()) {
            current = infoIterator.next().name;
            if (current.equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }


    /**
     * Simple test for {@link PackageManager#getPreferredActivities(List, List, String)} that tests
     * calling it has no effect. The method is essentially a no-op because no preferred activities
     * can be added.
     * @see PackageManager#addPreferredActivity(IntentFilter, int, ComponentName[], ComponentName)
     */
    @Test
    public void testGetPreferredActivities() {
        assertNoPreferredActivities();
    }

    /**
     * Helper method to test that {@link PackageManager#getPreferredActivities(List, List, String)}
     * returns empty lists.
     */
    private void assertNoPreferredActivities() {
        List<ComponentName> outActivities = new ArrayList<ComponentName>();
        List<IntentFilter> outFilters = new ArrayList<IntentFilter>();
        mPackageManager.getPreferredActivities(outFilters, outActivities, PACKAGE_NAME);
        assertEquals(0, outActivities.size());
        assertEquals(0, outFilters.size());
    }

    /**
     * Test that calling {@link PackageManager#addPreferredActivity(IntentFilter, int,
     * ComponentName[], ComponentName)} throws a {@link SecurityException}.
     * <p/>
     * The method is protected by the {@link android.permission.SET_PREFERRED_APPLICATIONS}
     * signature permission. Even though this app declares that permission, it still should not be
     * able to call this method because it is not signed with the platform certificate.
     */
    @Test
    public void testAddPreferredActivity() {
        IntentFilter intentFilter = new IntentFilter(ACTIVITY_ACTION_NAME);
        ComponentName[] componentName = {new ComponentName(PACKAGE_NAME, ACTIVITY_NAME)};
        try {
            mPackageManager.addPreferredActivity(intentFilter, IntentFilter.MATCH_CATEGORY_HOST,
                    componentName, componentName[0]);
            fail("addPreferredActivity unexpectedly succeeded");
        } catch (SecurityException e) {
            // expected
        }
        assertNoPreferredActivities();
    }

    /**
     * Test that calling {@link PackageManager#clearPackagePreferredActivities(String)} has no
     * effect.
     */
    @Test
    public void testClearPackagePreferredActivities() {
        // just ensure no unexpected exceptions are thrown, nothing else to do
        mPackageManager.clearPackagePreferredActivities(PACKAGE_NAME);
    }

    private void checkComponentName(String expectedName, List<ComponentName> componentNames) {
        boolean isContained = false;
        Iterator<ComponentName> nameIterator = componentNames.iterator();
        String current;
        while (nameIterator.hasNext()) {
            current = nameIterator.next().getClassName();
            if (current.equals(expectedName)) {
                isContained = true;
                break;
            }
        }
        assertTrue(isContained);
    }

    private void checkIntentFilterAction(String expectedName, List<IntentFilter> intentFilters) {
        boolean isContained = false;
        Iterator<IntentFilter> filterIterator = intentFilters.iterator();
        IntentFilter currentFilter;
        String currentAction;
        while (filterIterator.hasNext()) {
            currentFilter = filterIterator.next();
            for (int i = 0; i < currentFilter.countActions(); i++) {
                currentAction = currentFilter.getAction(i);
                if (currentAction.equals(expectedName)) {
                    isContained = true;
                    break;
                }
            }
        }
        assertTrue(isContained);
    }

    @Test
    public void testAccessEnabledSetting() {
        mPackageManager.setApplicationEnabledSetting(PACKAGE_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        assertEquals(PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                mPackageManager.getApplicationEnabledSetting(PACKAGE_NAME));

        ComponentName componentName = new ComponentName(PACKAGE_NAME, ACTIVITY_NAME);
        mPackageManager.setComponentEnabledSetting(componentName,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
        assertEquals(PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
                mPackageManager.getComponentEnabledSetting(componentName));
    }

    @Test
    public void testGetApplicationEnabledSetting_notFound() {
        try {
            mPackageManager.getApplicationEnabledSetting("this.package.does.not.exist");
            fail("Exception expected");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testGetIcon() throws NameNotFoundException {
        assertNotNull(mPackageManager.getApplicationIcon(PACKAGE_NAME));
        assertNotNull(mPackageManager.getApplicationIcon(mPackageManager.getApplicationInfo(
                PACKAGE_NAME, 0)));
        assertNotNull(mPackageManager
                .getActivityIcon(new ComponentName(PACKAGE_NAME, ACTIVITY_NAME)));
        assertNotNull(mPackageManager.getActivityIcon(new Intent(MAIN_ACTION_NAME)));

        assertNotNull(mPackageManager.getDefaultActivityIcon());
        assertTrue(mPackageManager.isDefaultApplicationIcon(
                mPackageManager.getDefaultActivityIcon()));
        assertTrue(mPackageManager.isDefaultApplicationIcon(mPackageManager.getDefaultActivityIcon()
                .getConstantState().newDrawable()));

        assertFalse(mPackageManager.isDefaultApplicationIcon(mPackageManager.getActivityIcon(
                new ComponentName(PACKAGE_NAME, ACTIVITY_NAME))));

        // getDrawable is called by ComponentInfo.loadIcon() which called by getActivityIcon()
        // method of PackageMaganer. Here is just assurance for its functionality.
        int iconRes = R.drawable.start;
        ApplicationInfo appInfo = mPackageManager.getApplicationInfo(PACKAGE_NAME, 0);
        assertNotNull(mPackageManager.getDrawable(PACKAGE_NAME, iconRes, appInfo));
    }

    @Test
    public void testCheckSignaturesMatch_byPackageName() {
        // Compare the signature of this package to another package installed by this test suite
        // (see AndroidTest.xml). Their signatures must match.
        assertEquals(PackageManager.SIGNATURE_MATCH, mPackageManager.checkSignatures(PACKAGE_NAME,
                "com.android.cts.stub"));
        // This package's signature should match its own signature.
        assertEquals(PackageManager.SIGNATURE_MATCH, mPackageManager.checkSignatures(PACKAGE_NAME,
                PACKAGE_NAME));
    }

    @Test
    public void testCheckSignaturesMatch_byUid() throws NameNotFoundException {
        // Compare the signature of this package to another package installed by this test suite
        // (see AndroidTest.xml). Their signatures must match.
        int uid1 = mPackageManager.getPackageInfo(PACKAGE_NAME, 0).applicationInfo.uid;
        int uid2 = mPackageManager.getPackageInfo("com.android.cts.stub", 0).applicationInfo.uid;
        assertEquals(PackageManager.SIGNATURE_MATCH, mPackageManager.checkSignatures(uid1, uid2));

        // A UID's signature should match its own signature.
        assertEquals(PackageManager.SIGNATURE_MATCH, mPackageManager.checkSignatures(uid1, uid1));
    }

    @Test
    public void testCheckSignaturesNoMatch_byPackageName() {
        // This test package's signature shouldn't match the system's signature.
        assertEquals(PackageManager.SIGNATURE_NO_MATCH, mPackageManager.checkSignatures(
                PACKAGE_NAME, "android"));
    }

    @Test
    public void testCheckSignaturesNoMatch_byUid() throws NameNotFoundException {
        // This test package's signature shouldn't match the system's signature.
        int uid1 = mPackageManager.getPackageInfo(PACKAGE_NAME, 0).applicationInfo.uid;
        int uid2 = mPackageManager.getPackageInfo("android", 0).applicationInfo.uid;
        assertEquals(PackageManager.SIGNATURE_NO_MATCH,
                mPackageManager.checkSignatures(uid1, uid2));
    }

    @Test
    public void testCheckSignaturesUnknownPackage() {
        assertEquals(PackageManager.SIGNATURE_UNKNOWN_PACKAGE, mPackageManager.checkSignatures(
                PACKAGE_NAME, "this.package.does.not.exist"));
    }

    @Test
    public void testCheckPermissionGranted() {
        assertEquals(PackageManager.PERMISSION_GRANTED,
                mPackageManager.checkPermission(GRANTED_PERMISSION_NAME, PACKAGE_NAME));
    }

    @Test
    public void testCheckPermissionNotGranted() {
        assertEquals(PackageManager.PERMISSION_DENIED,
                mPackageManager.checkPermission(NOT_GRANTED_PERMISSION_NAME, PACKAGE_NAME));
    }

    @Test
    public void testResolveMethods() {
        // Test resolveActivity
        Intent intent = new Intent(ACTIVITY_ACTION_NAME);
        intent.setComponent(new ComponentName(PACKAGE_NAME, ACTIVITY_NAME));
        assertEquals(ACTIVITY_NAME, mPackageManager.resolveActivity(intent,
                PackageManager.MATCH_DEFAULT_ONLY).activityInfo.name);

        // Test resolveService
        intent = new Intent(SERVICE_ACTION_NAME);
        intent.setComponent(new ComponentName(PACKAGE_NAME, SERVICE_NAME));
        ResolveInfo resolveInfo = mPackageManager.resolveService(intent,
                PackageManager.GET_INTENT_FILTERS);
        assertEquals(SERVICE_NAME, resolveInfo.serviceInfo.name);

        // Test resolveContentProvider
        String providerAuthorities = "ctstest";
        assertEquals(PROVIDER_NAME,
                mPackageManager.resolveContentProvider(providerAuthorities, 0).name);
    }

    @Test
    public void testGetResources() throws NameNotFoundException {
        ComponentName componentName = new ComponentName(PACKAGE_NAME, ACTIVITY_NAME);
        int resourceId = R.xml.pm_test;
        String xmlName = "android.content.cts:xml/pm_test";
        ApplicationInfo appInfo = mPackageManager.getApplicationInfo(PACKAGE_NAME, 0);
        assertNotNull(mPackageManager.getXml(PACKAGE_NAME, resourceId, appInfo));
        assertEquals(xmlName, mPackageManager.getResourcesForActivity(componentName)
                .getResourceName(resourceId));
        assertEquals(xmlName, mPackageManager.getResourcesForApplication(appInfo).getResourceName(
                resourceId));
        assertEquals(xmlName, mPackageManager.getResourcesForApplication(PACKAGE_NAME)
                .getResourceName(resourceId));
    }

    @Test
    public void testGetPackageArchiveInfo() throws Exception {
        final String apkPath = InstrumentationRegistry.getContext().getPackageCodePath();
        final String apkName = InstrumentationRegistry.getContext().getPackageName();

        final int flags = PackageManager.GET_SIGNATURES;

        final PackageInfo pkgInfo = mPackageManager.getPackageArchiveInfo(apkPath, flags);

        assertEquals("getPackageArchiveInfo should return the correct package name",
                apkName, pkgInfo.packageName);

        assertNotNull("Signatures should have been collected when GET_SIGNATURES flag specified",
                pkgInfo.signatures);
    }

    @Test
    public void testGetNamesForUids_null() throws Exception {
        assertNull(mPackageManager.getNamesForUids(null));
    }

    @Test
    public void testGetNamesForUids_empty() throws Exception {
        assertNull(mPackageManager.getNamesForUids(new int[0]));
    }

    @Test
    public void testGetNamesForUids_valid() throws Exception {
        final int shimId =
                mPackageManager.getApplicationInfo("com.android.cts.ctsshim", 0 /*flags*/).uid;
        final int[] uids = new int[] {
                1000,
                Integer.MAX_VALUE,
                shimId,
        };
        final String[] result;
        result = mPackageManager.getNamesForUids(uids);
        assertNotNull(result);
        assertEquals(3, result.length);
        assertEquals("shared:android.uid.system", result[0]);
        assertEquals(null, result[1]);
        assertEquals("com.android.cts.ctsshim", result[2]);
    }

    @Test
    public void testGetPackageUid() throws NameNotFoundException {
        int userId = InstrumentationRegistry.getContext().getUserId();
        int expectedUid = UserHandle.getUid(userId, 1000);

        assertEquals(expectedUid, mPackageManager.getPackageUid("android", 0));

        int uid = mPackageManager.getApplicationInfo("com.android.cts.ctsshim", 0 /*flags*/).uid;
        assertEquals(uid, mPackageManager.getPackageUid("com.android.cts.ctsshim", 0));
    }

    @Test
    public void testGetPackageInfo() throws NameNotFoundException {
        PackageInfo pkgInfo = mPackageManager.getPackageInfo(PACKAGE_NAME, GET_META_DATA
                | GET_PERMISSIONS | GET_ACTIVITIES | GET_PROVIDERS | GET_SERVICES | GET_RECEIVERS);
        assertTestPackageInfo(pkgInfo);
    }

    @Test
    public void testGetPackageInfo_notFound() {
        try {
            mPackageManager.getPackageInfo("this.package.does.not.exist", 0);
            fail("Exception expected");
        } catch (NameNotFoundException expected) {
        }
    }

    @Test
    public void testGetInstalledPackages() throws Exception {
        List<PackageInfo> pkgs = mPackageManager.getInstalledPackages(GET_META_DATA
                | GET_PERMISSIONS | GET_ACTIVITIES | GET_PROVIDERS | GET_SERVICES | GET_RECEIVERS);

        PackageInfo pkgInfo = findPackageOrFail(pkgs, PACKAGE_NAME);
        assertTestPackageInfo(pkgInfo);
    }

    /**
     * Asserts that the pkgInfo object correctly describes the {@link #PACKAGE_NAME} package.
     */
    private void assertTestPackageInfo(PackageInfo pkgInfo) {
        // Check metadata
        ApplicationInfo appInfo = pkgInfo.applicationInfo;
        assertEquals(APPLICATION_NAME, appInfo.name);
        assertEquals("Android TestCase", appInfo.loadLabel(mPackageManager));
        assertEquals(PACKAGE_NAME, appInfo.packageName);
        assertTrue(appInfo.enabled);
        // The process name defaults to the package name when not set.
        assertEquals(PACKAGE_NAME, appInfo.processName);
        assertEquals(0, appInfo.flags & FLAG_SYSTEM);
        assertEquals(FLAG_INSTALLED, appInfo.flags & FLAG_INSTALLED);
        assertEquals(FLAG_HAS_CODE, appInfo.flags & FLAG_HAS_CODE);

        // Check required permissions
        List<String> requestedPermissions = Arrays.asList(pkgInfo.requestedPermissions);
        assertThat(requestedPermissions).containsAllOf(
                "android.permission.MANAGE_ACCOUNTS",
                "android.permission.ACCESS_NETWORK_STATE",
                "android.content.cts.permission.TEST_GRANTED");

        // Check declared permissions
        PermissionInfo declaredPermission = (PermissionInfo) findPackageItemOrFail(
                pkgInfo.permissions, CALL_ABROAD_PERMISSION_NAME);
        assertEquals("Call abroad", declaredPermission.loadLabel(mPackageManager));
        assertEquals(PERMISSIONGROUP_NAME, declaredPermission.group);
        assertEquals(PermissionInfo.PROTECTION_NORMAL, declaredPermission.protectionLevel);

        // Check if number of activities in PackageInfo matches number of activities in manifest,
        // to make sure no synthesized activities not in the manifest are returned.
        assertEquals("Number of activities in manifest != Number of activities in PackageInfo",
                NUM_OF_ACTIVITIES_IN_MANIFEST, pkgInfo.activities.length);
        // Check activities
        ActivityInfo activity = findPackageItemOrFail(pkgInfo.activities, ACTIVITY_NAME);
        assertTrue(activity.enabled);
        assertTrue(activity.exported); // Has intent filters - export by default.
        assertEquals(PACKAGE_NAME, activity.taskAffinity);
        assertEquals(ActivityInfo.LAUNCH_SINGLE_TOP, activity.launchMode);

        // Check services
        ServiceInfo service = findPackageItemOrFail(pkgInfo.services, SERVICE_NAME);
        assertTrue(service.enabled);
        assertTrue(service.exported); // Has intent filters - export by default.
        assertEquals(PACKAGE_NAME, service.packageName);
        assertEquals(CALL_ABROAD_PERMISSION_NAME, service.permission);

        // Check ContentProviders
        ProviderInfo provider = findPackageItemOrFail(pkgInfo.providers, PROVIDER_NAME);
        assertTrue(provider.enabled);
        assertFalse(provider.exported); // Don't export by default.
        assertEquals(PACKAGE_NAME, provider.packageName);
        assertEquals("ctstest", provider.authority);

        // Check Receivers
        ActivityInfo receiver = findPackageItemOrFail(pkgInfo.receivers, RECEIVER_NAME);
        assertTrue(receiver.enabled);
        assertTrue(receiver.exported); // Has intent filters - export by default.
        assertEquals(PACKAGE_NAME, receiver.packageName);
    }

    // Tests that other packages can be queried.
    @Test
    public void testGetInstalledPackages_OtherPackages() throws Exception {
        List<PackageInfo> pkgInfos = mPackageManager.getInstalledPackages(0);

        // Check a normal package.
        PackageInfo pkgInfo = findPackageOrFail(pkgInfos, "com.android.cts.stub"); // A test package
        assertEquals(0, pkgInfo.applicationInfo.flags & FLAG_SYSTEM);

        // Check a system package.
        pkgInfo = findPackageOrFail(pkgInfos, "android");
        assertEquals(FLAG_SYSTEM, pkgInfo.applicationInfo.flags & FLAG_SYSTEM);
    }

    @Test
    public void testGetInstalledApplications() throws Exception {
        List<ApplicationInfo> apps = mPackageManager.getInstalledApplications(GET_META_DATA);

        ApplicationInfo app = findPackageItemOrFail(
                apps.toArray(new ApplicationInfo[] {}), APPLICATION_NAME);

        assertEquals(APPLICATION_NAME, app.name);
        assertEquals("Android TestCase", app.loadLabel(mPackageManager));
        assertEquals(PACKAGE_NAME, app.packageName);
        assertTrue(app.enabled);
        // The process name defaults to the package name when not set.
        assertEquals(PACKAGE_NAME, app.processName);
    }

    private PackageInfo findPackageOrFail(List<PackageInfo> pkgInfos, String pkgName) {
        for (PackageInfo pkgInfo : pkgInfos) {
            if (pkgName.equals(pkgInfo.packageName)) {
                return pkgInfo;
            }
        }
        fail("Package not found with name " + pkgName);
        return null;
    }

    private <T extends PackageItemInfo> T findPackageItemOrFail(T[] items, String name) {
        for (T item : items) {
            if (name.equals(item.name)) {
                return item;
            }
        }
        fail("Package item not found with name " + name);
        return null;
    }

    @Test
    public void testGetPackagesHoldingPermissions() {
        List<PackageInfo> pkgInfos = mPackageManager.getPackagesHoldingPermissions(
                new String[] { GRANTED_PERMISSION_NAME }, 0);
        findPackageOrFail(pkgInfos, PACKAGE_NAME);

        pkgInfos = mPackageManager.getPackagesHoldingPermissions(
                new String[] { NOT_GRANTED_PERMISSION_NAME }, 0);
        for (PackageInfo pkgInfo : pkgInfos) {
            if (PACKAGE_NAME.equals(pkgInfo.packageName)) {
                fail("Must not return package " + PACKAGE_NAME);
            }
        }
    }

    @Test
    public void testGetPermissionInfo() throws NameNotFoundException {
        // Check a normal permission.
        String permissionName = "android.permission.INTERNET";
        PermissionInfo permissionInfo = mPackageManager.getPermissionInfo(permissionName, 0);
        assertEquals(permissionName, permissionInfo.name);
        assertEquals(PermissionInfo.PROTECTION_NORMAL, permissionInfo.getProtection());

        // Check a dangerous (runtime) permission.
        permissionName = "android.permission.RECORD_AUDIO";
        permissionInfo = mPackageManager.getPermissionInfo(permissionName, 0);
        assertEquals(permissionName, permissionInfo.name);
        assertEquals(PermissionInfo.PROTECTION_DANGEROUS, permissionInfo.getProtection());
        assertNotNull(permissionInfo.group);

        // Check a signature permission.
        permissionName = "android.permission.MODIFY_PHONE_STATE";
        permissionInfo = mPackageManager.getPermissionInfo(permissionName, 0);
        assertEquals(permissionName, permissionInfo.name);
        assertEquals(PermissionInfo.PROTECTION_SIGNATURE, permissionInfo.getProtection());

        // Check a special access (appop) permission.
        permissionName = "android.permission.SYSTEM_ALERT_WINDOW";
        permissionInfo = mPackageManager.getPermissionInfo(permissionName, 0);
        assertEquals(permissionName, permissionInfo.name);
        assertEquals(PermissionInfo.PROTECTION_SIGNATURE, permissionInfo.getProtection());
        assertEquals(PermissionInfo.PROTECTION_FLAG_APPOP,
                permissionInfo.getProtectionFlags() & PermissionInfo.PROTECTION_FLAG_APPOP);
    }

    @Test
    public void testGetPermissionInfo_notFound() {
        try {
            mPackageManager.getPermissionInfo("android.permission.nonexistent.permission", 0);
            fail("Exception expected");
        } catch (NameNotFoundException expected) {
        }
    }

    @Test
    public void testGetPermissionGroupInfo() throws NameNotFoundException {
        PermissionGroupInfo groupInfo = mPackageManager.getPermissionGroupInfo(
                PERMISSIONGROUP_NAME, 0);
        assertEquals(PERMISSIONGROUP_NAME, groupInfo.name);
        assertEquals(PACKAGE_NAME, groupInfo.packageName);
        assertFalse(TextUtils.isEmpty(groupInfo.loadDescription(mPackageManager)));
    }

    @Test
    public void testGetPermissionGroupInfo_notFound() throws NameNotFoundException {
        try {
            mPackageManager.getPermissionGroupInfo("this.group.does.not.exist", 0);
            fail("Exception expected");
        } catch (NameNotFoundException expected) {
        }
    }

    @Test
    public void testAddPermission_cantAddOutsideRoot() {
        PermissionInfo permissionInfo = new PermissionInfo();
        permissionInfo.name = "some.other.permission.tree.some-permission";
        permissionInfo.nonLocalizedLabel = "Some Permission";
        permissionInfo.protectionLevel = PermissionInfo.PROTECTION_NORMAL;
        // Remove first
        try {
            mPackageManager.removePermission(permissionInfo.name);
        } catch (SecurityException se) {
        }
        try {
            mPackageManager.addPermission(permissionInfo);
            fail("Must not add permission outside the permission tree defined in the manifest.");
        } catch (SecurityException expected) {
        }
    }

    @Test
    public void testAddPermission() throws NameNotFoundException {
        PermissionInfo permissionInfo = new PermissionInfo();
        permissionInfo.name = PERMISSION_TREE_ROOT + ".some-permission";
        permissionInfo.protectionLevel = PermissionInfo.PROTECTION_NORMAL;
        permissionInfo.nonLocalizedLabel = "Some Permission";
        // Remove first
        try {
            mPackageManager.removePermission(permissionInfo.name);
        } catch (SecurityException se) {
        }
        mPackageManager.addPermission(permissionInfo);
        PermissionInfo savedInfo = mPackageManager.getPermissionInfo(permissionInfo.name, 0);
        assertEquals(PACKAGE_NAME, savedInfo.packageName);
        assertEquals(PermissionInfo.PROTECTION_NORMAL, savedInfo.protectionLevel);
    }

    @Test
    public void testGetPackageInfo_ApexSupported_ApexPackage_MatchesApex() throws Exception {
        // This really should be a assumeTrue(isUpdatingApexSupported()), but JUnit3 doesn't support
        // assumptions framework.
        // TODO: change to assumeTrue after migrating tests to JUnit4.
        if (!isUpdatingApexSupported()) {
            Log.i(TAG, "Device doesn't support updating APEX");
            return;
        }
        PackageInfo packageInfo = mPackageManager.getPackageInfo(SHIM_APEX_PACKAGE_NAME,
                PackageManager.MATCH_APEX | PackageManager.MATCH_FACTORY_ONLY);
        assertShimApexInfoIsCorrect(packageInfo);
    }

    @Test
    public void testGetPackageInfo_ApexSupported_ApexPackage_DoesNotMatchApex() {
        // This really should be a assumeTrue(isUpdatingApexSupported()), but JUnit3 doesn't support
        // assumptions framework.
        // TODO: change to assumeTrue after migrating tests to JUnit4.
        if (!isUpdatingApexSupported()) {
            Log.i(TAG, "Device doesn't support updating APEX");
            return;
        }
        try {
            mPackageManager.getPackageInfo(SHIM_APEX_PACKAGE_NAME, 0 /* flags */);
            fail("NameNotFoundException expected");
        } catch (NameNotFoundException expected) {
        }
    }

    @Test
    public void testGetPackageInfo_ApexNotSupported_ApexPackage_MatchesApex() {
        if (isUpdatingApexSupported()) {
            Log.i(TAG, "Device supports updating APEX");
            return;
        }
        try {
            mPackageManager.getPackageInfo(SHIM_APEX_PACKAGE_NAME, PackageManager.MATCH_APEX);
            fail("NameNotFoundException expected");
        } catch (NameNotFoundException expected) {
        }
    }

    @Test
    public void testGetPackageInfo_ApexNotSupported_ApexPackage_DoesNotMatchApex() {
        if (isUpdatingApexSupported()) {
            Log.i(TAG, "Device supports updating APEX");
            return;
        }
        try {
            mPackageManager.getPackageInfo(SHIM_APEX_PACKAGE_NAME, 0);
            fail("NameNotFoundException expected");
        } catch (NameNotFoundException expected) {
        }
    }

    @Test
    public void testGetInstalledPackages_ApexSupported_MatchesApex() {
        if (!isUpdatingApexSupported()) {
            Log.i(TAG, "Device doesn't support updating APEX");
            return;
        }
        List<PackageInfo> installedPackages = mPackageManager.getInstalledPackages(
                PackageManager.MATCH_APEX | PackageManager.MATCH_FACTORY_ONLY);
        List<PackageInfo> shimApex = installedPackages.stream().filter(
                packageInfo -> packageInfo.packageName.equals(SHIM_APEX_PACKAGE_NAME)).collect(
                Collectors.toList());
        assertWithMessage("More than one shim apex found").that(shimApex).hasSize(1);
        assertShimApexInfoIsCorrect(shimApex.get(0));
    }

    @Test
    public void testGetInstalledPackages_ApexSupported_DoesNotMatchApex() {
        if (!isUpdatingApexSupported()) {
            Log.i(TAG, "Device doesn't support updating APEX");
            return;
        }
        List<PackageInfo> installedPackages = mPackageManager.getInstalledPackages(0);
        List<PackageInfo> shimApex = installedPackages.stream().filter(
                packageInfo -> packageInfo.packageName.equals(SHIM_APEX_PACKAGE_NAME)).collect(
                Collectors.toList());
        assertWithMessage("Shim apex wasn't supposed to be found").that(shimApex).isEmpty();
    }

    @Test
    public void testGetInstalledPackages_ApexNotSupported_MatchesApex() {
        if (isUpdatingApexSupported()) {
            Log.i(TAG, "Device supports updating APEX");
            return;
        }
        List<PackageInfo> installedPackages = mPackageManager.getInstalledPackages(
                PackageManager.MATCH_APEX);
        List<PackageInfo> shimApex = installedPackages.stream().filter(
                packageInfo -> packageInfo.packageName.equals(SHIM_APEX_PACKAGE_NAME)).collect(
                Collectors.toList());
        assertWithMessage("Shim apex wasn't supposed to be found").that(shimApex).isEmpty();
    }

    @Test
    public void testGetInstalledPackages_ApexNotSupported_DoesNotMatchApex() {
        if (isUpdatingApexSupported()) {
            Log.i(TAG, "Device supports updating APEX");
            return;
        }
        List<PackageInfo> installedPackages = mPackageManager.getInstalledPackages(0);
        List<PackageInfo> shimApex = installedPackages.stream().filter(
                packageInfo -> packageInfo.packageName.equals(SHIM_APEX_PACKAGE_NAME)).collect(
                Collectors.toList());
        assertWithMessage("Shim apex wasn't supposed to be found").that(shimApex).isEmpty();
    }

    @Test
    public void testGetApplicationInfo_ApexSupported_MatchesApex() throws Exception {
        assumeTrue("Device doesn't support updating APEX", isUpdatingApexSupported());

        ApplicationInfo ai = mPackageManager.getApplicationInfo(
                SHIM_APEX_PACKAGE_NAME, PackageManager.MATCH_APEX);
        assertThat(ai.sourceDir).isEqualTo("/system/apex/com.android.apex.cts.shim.apex");
        assertThat(ai.publicSourceDir).isEqualTo(ai.sourceDir);
        assertThat(ai.flags & ApplicationInfo.FLAG_SYSTEM).isEqualTo(ApplicationInfo.FLAG_SYSTEM);
        assertThat(ai.flags & ApplicationInfo.FLAG_INSTALLED)
                .isEqualTo(ApplicationInfo.FLAG_INSTALLED);
    }

    private boolean isUpdatingApexSupported() {
        return SystemProperties.getBoolean("ro.apex.updatable", false);
    }

    private static void assertShimApexInfoIsCorrect(PackageInfo packageInfo) {
        assertThat(packageInfo.packageName).isEqualTo(SHIM_APEX_PACKAGE_NAME);
        assertThat(packageInfo.getLongVersionCode()).isEqualTo(1);
        assertThat(packageInfo.isApex).isTrue();
        assertThat(packageInfo.applicationInfo.sourceDir).isEqualTo(
                "/system/apex/com.android.apex.cts.shim.apex");
        assertThat(packageInfo.applicationInfo.publicSourceDir)
                .isEqualTo(packageInfo.applicationInfo.sourceDir);
        // Verify that legacy mechanism for handling signatures is supported.
        Signature[] pastSigningCertificates =
                packageInfo.signingInfo.getSigningCertificateHistory();
        assertThat(packageInfo.signatures)
                .asList().containsExactly((Object[]) pastSigningCertificates);
    }
}
