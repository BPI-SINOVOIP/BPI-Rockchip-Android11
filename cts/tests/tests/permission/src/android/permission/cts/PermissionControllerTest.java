/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.permission.cts;

import static android.Manifest.permission.ACCESS_BACKGROUND_LOCATION;
import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.BODY_SENSORS;
import static android.Manifest.permission.READ_CALENDAR;
import static android.Manifest.permission.READ_CONTACTS;
import static android.Manifest.permission.WRITE_CALENDAR;
import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_FOREGROUND;
import static android.app.AppOpsManager.permissionToOp;
import static android.content.pm.PackageManager.PERMISSION_DENIED;
import static android.permission.PermissionControllerManager.COUNT_ONLY_WHEN_GRANTED;
import static android.permission.PermissionControllerManager.REASON_INSTALLER_POLICY_VIOLATION;
import static android.permission.PermissionControllerManager.REASON_MALWARE;
import static android.permission.cts.PermissionUtils.grantPermission;
import static android.permission.cts.PermissionUtils.isGranted;
import static android.permission.cts.PermissionUtils.isPermissionGranted;

import static com.android.compatibility.common.util.SystemUtil.callWithShellPermissionIdentity;
import static com.android.compatibility.common.util.SystemUtil.eventually;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static com.google.common.truth.Truth.assertThat;

import static java.util.Collections.singletonList;

import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PermissionGroupInfo;
import android.permission.PermissionControllerManager;
import android.permission.RuntimePermissionPresentationInfo;
import android.platform.test.annotations.AppModeFull;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test {@link PermissionControllerManager}
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "Instant apps cannot talk to permission controller")
public class PermissionControllerTest {
    private static final String APK =
            "/data/local/tmp/cts/permissions/CtsAppThatAccessesLocationOnCommand.apk";
    private static final String APP = "android.permission.cts.appthataccesseslocation";
    private static final String APK2 =
            "/data/local/tmp/cts/permissions/"
                    + "CtsAppThatRequestsCalendarContactsBodySensorCustomPermission.apk";
    private static final String APP2 = "android.permission.cts.appthatrequestcustompermission";
    private static final String CUSTOM_PERMISSION =
            "android.permission.cts.appthatrequestcustompermission.TEST_PERMISSION";

    private static final UiAutomation sUiAutomation =
            InstrumentationRegistry.getInstrumentation().getUiAutomation();
    private static final Context sContext = InstrumentationRegistry.getTargetContext();
    private static final PermissionControllerManager sController =
            sContext.getSystemService(PermissionControllerManager.class);

    @Before
    @After
    public void resetAppState() {
        runWithShellPermissionIdentity(() -> {
            sUiAutomation.grantRuntimePermission(APP, ACCESS_FINE_LOCATION);
            sUiAutomation.grantRuntimePermission(APP, ACCESS_BACKGROUND_LOCATION);
            setAppOp(APP, ACCESS_FINE_LOCATION, MODE_ALLOWED);
        });
    }

    @BeforeClass
    public static void installApp() {
        runShellCommand("pm install -r -g " + APK);
        runShellCommand("pm install -r " + APK2);
    }

    @AfterClass
    public static void uninstallApp() {
        runShellCommand("pm uninstall " + APP);
        runShellCommand("pm uninstall " + APP2);
    }

    private @NonNull Map<String, List<String>> revokePermissions(
            @NonNull Map<String, List<String>> request, boolean doDryRun, int reason,
            @NonNull Executor executor) throws Exception {
        AtomicReference<Map<String, List<String>>> result = new AtomicReference<>();

        sController.revokeRuntimePermissions(request, doDryRun, reason, executor,
                new PermissionControllerManager.OnRevokeRuntimePermissionsCallback() {
                    @Override
                    public void onRevokeRuntimePermissions(@NonNull Map<String, List<String>> r) {
                        synchronized (result) {
                            result.set(r);
                            result.notifyAll();
                        }
                    }
                });

        synchronized (result) {
            while (result.get() == null) {
                result.wait();
            }
        }

        return result.get();
    }

    private @NonNull Map<String, List<String>> revokePermissions(
            @NonNull Map<String, List<String>> request, boolean doDryRun, boolean adoptShell)
            throws Exception {
        if (adoptShell) {
            Map<String, List<String>> revokeRet =
                    callWithShellPermissionIdentity(() -> revokePermissions(
                            request, doDryRun, REASON_MALWARE, sContext.getMainExecutor()));
            return revokeRet;
        }
        return revokePermissions(request, doDryRun, REASON_MALWARE, sContext.getMainExecutor());
    }

    private @NonNull Map<String, List<String>> revokePermissions(
            @NonNull Map<String, List<String>> request, boolean doDryRun) throws Exception {
        return revokePermissions(request, doDryRun, true);
    }

    private void setAppOp(@NonNull String pkg, @NonNull String perm, int mode) throws Exception {
        sContext.getSystemService(AppOpsManager.class).setUidMode(permissionToOp(perm),
                sContext.getPackageManager().getPackageUid(pkg, 0), mode);
    }

    private Map<String, List<String>> buildRevokeRequest(@NonNull String app,
            @NonNull String permission) {
        return Collections.singletonMap(app, singletonList(permission));
    }

    private void assertRuntimePermissionLabelsAreValid(List<String> runtimePermissions,
            List<RuntimePermissionPresentationInfo> permissionInfos, int expectedRuntimeGranted,
            String app) throws Exception {
        int numRuntimeGranted = 0;
        for (String permission : runtimePermissions) {
            if (isPermissionGranted(app, permission)) {
                numRuntimeGranted++;
            }
        }
        assertThat(numRuntimeGranted).isEqualTo(expectedRuntimeGranted);

        ArrayList<CharSequence> maybeStandardPermissionLabels = new ArrayList<>();
        ArrayList<CharSequence> nonStandardPermissionLabels = new ArrayList<>();
        for (PermissionGroupInfo permGroup : sContext.getPackageManager().getAllPermissionGroups(
                0)) {
            CharSequence permissionGroupLabel = permGroup.loadLabel(sContext.getPackageManager());
            if (permGroup.packageName.equals("android")) {
                maybeStandardPermissionLabels.add(permissionGroupLabel);
            } else {
                nonStandardPermissionLabels.add(permissionGroupLabel);
            }
        }

        int numInfosGranted = 0;

        for (RuntimePermissionPresentationInfo permissionInfo : permissionInfos) {
            CharSequence permissionGroupLabel = permissionInfo.getLabel();

            // PermissionInfo should be included in exactly one of existing (possibly) standard
            // or nonstandard permission groups
            if (permissionInfo.isStandard()) {
                assertThat(maybeStandardPermissionLabels).contains(permissionGroupLabel);
            } else {
                assertThat(nonStandardPermissionLabels).contains(permissionGroupLabel);
            }
            if (permissionInfo.isGranted()) {
                numInfosGranted++;
            }
        }

        // Each permissionInfo represents one or more runtime permissions, but we don't have a
        // mapping, so we check that we have at least as many runtimePermissions as permissionInfos
        assertThat(numRuntimeGranted).isAtLeast(numInfosGranted);
    }

    @Test
    public void revokePermissionsDryRunSinglePermission() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);

        Map<String, List<String>> result = revokePermissions(request, true);

        assertThat(result.size()).isEqualTo(1);
        assertThat(result.get(APP)).isNotNull();
        assertThat(result.get(APP)).containsExactly(ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void revokePermissionsSinglePermission() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);

        revokePermissions(request, false);

        assertThat(sContext.getPackageManager().checkPermission(ACCESS_BACKGROUND_LOCATION,
                APP)).isEqualTo(PERMISSION_DENIED);
    }

    @Test
    public void revokePermissionsDoNotAlreadyRevokedPermission() throws Exception {
        // Properly revoke the permission
        runWithShellPermissionIdentity(() -> {
            sUiAutomation.revokeRuntimePermission(APP, ACCESS_BACKGROUND_LOCATION);
            setAppOp(APP, ACCESS_FINE_LOCATION, MODE_FOREGROUND);
        });

        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);
        Map<String, List<String>> result = revokePermissions(request, false);

        assertThat(result).isEmpty();
    }

    @Test
    public void revokePermissionsDryRunForegroundPermission() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_FINE_LOCATION);

        Map<String, List<String>> result = revokePermissions(request, true);

        assertThat(result.size()).isEqualTo(1);
        assertThat(result.get(APP)).isNotNull();
        assertThat(result.get(APP)).containsExactly(ACCESS_FINE_LOCATION,
                ACCESS_BACKGROUND_LOCATION, ACCESS_COARSE_LOCATION);
    }

    @Test
    public void revokePermissionsUnrequestedPermission() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, READ_CONTACTS);

        Map<String, List<String>> result = revokePermissions(request, false);

        assertThat(result).isEmpty();
    }

    @Test
    public void revokeFromUnknownPackage() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest("invalid.app", READ_CONTACTS);

        Map<String, List<String>> result = revokePermissions(request, false);

        assertThat(result).isEmpty();
    }

    @Test
    public void revokePermissionsFromUnknownPermission() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, "unknown.permission");

        Map<String, List<String>> result = revokePermissions(request, false);

        assertThat(result).isEmpty();
    }

    @Test
    public void revokePermissionsPolicyViolationFromWrongPackage() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_FINE_LOCATION);
        Map<String, List<String>> result = callWithShellPermissionIdentity(
                () -> revokePermissions(request,
                        false, REASON_INSTALLER_POLICY_VIOLATION, sContext.getMainExecutor()));
        assertThat(result).isEmpty();
    }

    @Test
    public void revokePermissionsWithExecutorForCallback() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);

        AtomicBoolean wasRunOnExecutor = new AtomicBoolean();
        runWithShellPermissionIdentity(() ->
                revokePermissions(request, true, REASON_MALWARE, command -> {
                    wasRunOnExecutor.set(true);
                    command.run();
                }));

        assertThat(wasRunOnExecutor.get()).isTrue();
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionsWithNullPkg() throws Exception {
        Map<String, List<String>> request = Collections.singletonMap(null,
                singletonList(ACCESS_FINE_LOCATION));

        revokePermissions(request, true);
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionsWithNullPermissions() throws Exception {
        Map<String, List<String>> request = Collections.singletonMap(APP, null);

        revokePermissions(request, true);
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionsWithNullPermission() throws Exception {
        Map<String, List<String>> request = Collections.singletonMap(APP,
                singletonList(null));

        revokePermissions(request, true);
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionsWithNullRequests() {
        sController.revokeRuntimePermissions(null, false, REASON_MALWARE,
                sContext.getMainExecutor(),
                new PermissionControllerManager.OnRevokeRuntimePermissionsCallback() {
                    @Override
                    public void onRevokeRuntimePermissions(
                            @NonNull Map<String, List<String>> revoked) {
                    }
                });
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionsWithNullCallback() {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);

        sController.revokeRuntimePermissions(request, false, REASON_MALWARE,
                sContext.getMainExecutor(), null);
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionsWithNullExecutor() {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);

        sController.revokeRuntimePermissions(request, false, REASON_MALWARE, null,
                new PermissionControllerManager.OnRevokeRuntimePermissionsCallback() {
                    @Override
                    public void onRevokeRuntimePermissions(
                            @NonNull Map<String, List<String>> revoked) {

                    }
                });
    }

    @Test(expected = SecurityException.class)
    public void revokePermissionsWithoutPermission() throws Exception {
        Map<String, List<String>> request = buildRevokeRequest(APP, ACCESS_BACKGROUND_LOCATION);

        // This will fail as the test-app does not have the required permission
        revokePermissions(request, true, false);
    }

    @Test
    public void getAppPermissionsForApp() throws Exception {
        CompletableFuture<List<RuntimePermissionPresentationInfo>> futurePermissionInfos =
                new CompletableFuture<>();

        List<String> runtimePermissions;
        List<RuntimePermissionPresentationInfo> permissionInfos;

        sUiAutomation.adoptShellPermissionIdentity();
        try {
            sController.getAppPermissions(APP, futurePermissionInfos::complete, null);
            runtimePermissions = PermissionUtils.getRuntimePermissions(APP);
            assertThat(runtimePermissions).isNotEmpty();
            permissionInfos = futurePermissionInfos.get();
        } finally {
            sUiAutomation.dropShellPermissionIdentity();
        }

        assertRuntimePermissionLabelsAreValid(runtimePermissions, permissionInfos, 3, APP);
    }

    @Test
    public void getAppPermissionsForCustomApp() throws Exception {
        CompletableFuture<List<RuntimePermissionPresentationInfo>> futurePermissionInfos =
                new CompletableFuture<>();

        // Grant all requested permissions except READ_CALENDAR
        sUiAutomation.grantRuntimePermission(APP2, CUSTOM_PERMISSION);
        PermissionUtils.grantPermission(APP2, BODY_SENSORS);
        PermissionUtils.grantPermission(APP2, READ_CONTACTS);
        PermissionUtils.grantPermission(APP2, WRITE_CALENDAR);

        List<String> runtimePermissions;
        List<RuntimePermissionPresentationInfo> permissionInfos;
        sUiAutomation.adoptShellPermissionIdentity();
        try {
            sController.getAppPermissions(APP2, futurePermissionInfos::complete, null);
            runtimePermissions = PermissionUtils.getRuntimePermissions(APP2);

            permissionInfos = futurePermissionInfos.get();
        } finally {
            sUiAutomation.dropShellPermissionIdentity();
        }

        assertThat(permissionInfos).isNotEmpty();
        assertThat(runtimePermissions.size()).isEqualTo(5);
        assertRuntimePermissionLabelsAreValid(runtimePermissions, permissionInfos, 4, APP2);
    }

    @Test
    public void revokePermissionAutomaticallyExtendsToWholeGroup() throws Exception {
        grantPermission(APP2, READ_CALENDAR);
        grantPermission(APP2, WRITE_CALENDAR);

        runWithShellPermissionIdentity(
                () -> {
                    sController.revokeRuntimePermission(APP2, READ_CALENDAR);

                    eventually(() -> {
                        assertThat(isGranted(APP2, READ_CALENDAR)).isEqualTo(false);
                        // revokePermission automatically extends the revocation to whole group
                        assertThat(isGranted(APP2, WRITE_CALENDAR)).isEqualTo(false);
                    });
                });
    }

    @Test
    public void revokePermissionCustom() throws Exception {
        sUiAutomation.grantRuntimePermission(APP2, CUSTOM_PERMISSION);

        runWithShellPermissionIdentity(
                () -> {
                    sController.revokeRuntimePermission(APP2, CUSTOM_PERMISSION);

                    eventually(() -> {
                        assertThat(isPermissionGranted(APP2, CUSTOM_PERMISSION)).isEqualTo(false);
                    });
                });
    }

    @Test
    public void revokePermissionWithInvalidPkg() throws Exception {
        // No return value, call is ignored
        runWithShellPermissionIdentity(
                () -> sController.revokeRuntimePermission("invalid.package", READ_CALENDAR));
    }

    @Test
    public void revokePermissionWithInvalidPermission() throws Exception {
        // No return value, call is ignored
        runWithShellPermissionIdentity(
                () -> sController.revokeRuntimePermission(APP2, "invalid.permission"));
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionWithNullPkg() throws Exception {
        sController.revokeRuntimePermission(null, READ_CALENDAR);
    }

    @Test(expected = NullPointerException.class)
    public void revokePermissionWithNullPermission() throws Exception {
        sController.revokeRuntimePermission(APP2, null);
    }

    // TODO: Add more tests for countPermissionAppsGranted when the method can be safely called
    //       multiple times in a row

    @Test
    public void countPermissionAppsGranted() {
        runWithShellPermissionIdentity(
                () -> {
                    CompletableFuture<Integer> numApps = new CompletableFuture<>();

                    sController.countPermissionApps(singletonList(ACCESS_FINE_LOCATION),
                            COUNT_ONLY_WHEN_GRANTED, numApps::complete, null);

                    // TODO: Better would be to count before, grant a permission, count again and
                    //       then compare before and after
                    assertThat(numApps.get()).isAtLeast(1);
                });
    }

    @Test(expected = NullPointerException.class)
    public void countPermissionAppsNullPermission() {
        sController.countPermissionApps(null, 0, (n) -> { }, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void countPermissionAppsInvalidFlags() {
        sController.countPermissionApps(singletonList(ACCESS_FINE_LOCATION), -1, (n) -> { }, null);
    }

    @Test(expected = NullPointerException.class)
    public void countPermissionAppsNullCallback() {
        sController.countPermissionApps(singletonList(ACCESS_FINE_LOCATION), 0, null, null);
    }
}
