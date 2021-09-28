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

package android.permission2.cts;

import static android.Manifest.permission.READ_EXTERNAL_STORAGE;
import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.OPSTR_LEGACY_STORAGE;
import static android.permission.cts.PermissionUtils.isGranted;
import static android.permission2.cts.RestrictedStoragePermissionSharedUidTest.StorageState.DENIED;
import static android.permission2.cts.RestrictedStoragePermissionSharedUidTest.StorageState.ISOLATED;
import static android.permission2.cts.RestrictedStoragePermissionSharedUidTest.StorageState.NON_ISOLATED;

import static com.android.compatibility.common.util.SystemUtil.eventually;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static com.google.common.truth.Truth.assertThat;

import static java.lang.Integer.min;

import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameter;
import org.junit.runners.Parameterized.Parameters;

import java.util.ArrayList;

@AppModeFull(reason = "Instant apps cannot access other app's properties")
@RunWith(Parameterized.class)
public class RestrictedStoragePermissionSharedUidTest {
    private static final String LOG_TAG =
            RestrictedStoragePermissionSharedUidTest.class.getSimpleName();

    public enum StorageState {
        /** The app has non-isolated storage */
        NON_ISOLATED,

        /** The app has isolated storage */
        ISOLATED,

        /** The read-external-storage permission cannot be granted */
        DENIED
    }

    /**
     * An app that is tested
     */
    private static class TestApp {
        private static @NonNull Context sContext =
                InstrumentationRegistry.getInstrumentation().getContext();
        private static @NonNull AppOpsManager sAppOpsManager =
                sContext.getSystemService(AppOpsManager.class);
        private static @NonNull PackageManager sPackageManager = sContext.getPackageManager();

        private final String mApk;
        private final String mPkg;

        public final boolean isRestricted;
        public final boolean hasRequestedLegacyExternalStorage;

        TestApp(@NonNull String apk, @NonNull String pkg, boolean isRestricted,
                @NonNull boolean hasRequestedLegacyExternalStorage) {
            mApk = apk;
            mPkg = pkg;

            this.isRestricted = isRestricted;
            this.hasRequestedLegacyExternalStorage = hasRequestedLegacyExternalStorage;
        }

        /**
         * Assert that the read-external-storage permission was granted or not granted.
         *
         * @param expectGranted {@code true} if the permission is expected to be granted
         */
        void assertStoragePermGranted(boolean expectGranted) {
            eventually(() -> assertThat(isGranted(mPkg, READ_EXTERNAL_STORAGE)).named(
                    this + " read storage granted").isEqualTo(expectGranted));
        }

        /**
         * Assert that the app has non-isolated storage
         *
         * @param expectGranted {@code true} if the app is expected to have non-isolated storage
         */
        void assertHasNotIsolatedStorage(boolean expectHasNotIsolatedStorage) {
            eventually(() -> runWithShellPermissionIdentity(() -> {
                int uid = sContext.getPackageManager().getPackageUid(mPkg, 0);
                if (expectHasNotIsolatedStorage) {
                    assertThat(sAppOpsManager.unsafeCheckOpRawNoThrow(OPSTR_LEGACY_STORAGE, uid,
                            mPkg)).named(this + " legacy storage mode").isEqualTo(MODE_ALLOWED);
                } else {
                    assertThat(sAppOpsManager.unsafeCheckOpRawNoThrow(OPSTR_LEGACY_STORAGE, uid,
                            mPkg)).named(this + " legacy storage mode").isNotEqualTo(MODE_ALLOWED);
                }
            }));
        }

        int getTargetSDK() throws Exception {
            return sPackageManager.getApplicationInfo(mPkg, 0).targetSdkVersion;
        }

        void install() {
            if (isRestricted) {
                runShellCommand("pm install -g --force-queryable --restrict-permissions " + mApk);
            } else {
                runShellCommand("pm install -g --force-queryable " + mApk);
            }
        }

        void uninstall() {
            runShellCommand("pm uninstall " + mPkg);
        }

        @Override
        public String toString() {
            return mPkg.substring(PKG_PREFIX.length());
        }
    }

    /**
     * Placeholder for "no app". The properties are chosen that when combined with another app, the
     * other app always decides the resulting property,
     */
    private static class NoApp extends TestApp {
        NoApp() {
            super("", PKG_PREFIX + "(none)", true, false);
        }

        void assertStoragePermGranted(boolean ignored) {
            // empty
        }

        void assertHasNotIsolatedStorage(boolean ignored) {
            // empty
        }

        @Override
        int getTargetSDK() {
            return 10000;
        }

        @Override
        public void install() {
            // empty
        }

        @Override
        public void uninstall() {
            // empty
        }
    }

    private static final String APK_PATH = "/data/local/tmp/cts/permissions2/";
    private static final String PKG_PREFIX = "android.permission2.cts.legacystoragewithshareduid.";

    private static final TestApp[] TEST_APPS = new TestApp[]{
            new TestApp(APK_PATH + "CtsLegacyStorageNotIsolatedWithSharedUid.apk",
                    PKG_PREFIX + "notisolated", false, true),
            new TestApp(APK_PATH + "CtsLegacyStorageIsolatedWithSharedUid.apk",
                    PKG_PREFIX + "isolated", false, false),
            new TestApp(APK_PATH + "CtsLegacyStorageRestrictedWithSharedUid.apk",
                    PKG_PREFIX + "restricted", true, false),
            new TestApp(APK_PATH + "CtsLegacyStorageRestrictedSdk28WithSharedUid.apk",
                    PKG_PREFIX + "restrictedsdk28", true, true),
            new NoApp()};

    /**
     * First app to be tested. This is the first in an entry created by {@link
     * #getTestAppCombinations}
     */
    @Parameter(0)
    public @NonNull TestApp app1;

    /**
     * Second app to be tested. This is the second in an entry created by {@link
     * #getTestAppCombinations}
     */
    @Parameter(1)
    public @NonNull TestApp app2;

    /**
     * Run this test for all combination of two tests-apps out of {@link #TEST_APPS}. This includes
     * the {@link NoApp}, i.e. we also test a single test-app by itself.
     *
     * @return All combinations of two test-apps
     */
    @Parameters(name = "{0} and {1}")
    public static Iterable<Object[]> getTestAppCombinations() {
        ArrayList<Object[]> parameters = new ArrayList<>();

        for (int firstApp = 0; firstApp < TEST_APPS.length; firstApp++) {
            for (int secondApp = firstApp + 1; secondApp < TEST_APPS.length; secondApp++) {
                parameters.add(new Object[]{TEST_APPS[firstApp], TEST_APPS[secondApp]});
            }
        }

        return parameters;
    }

    @Test
    public void checkExceptedStorageStateForAppsSharingUid() throws Exception {
        app1.install();
        app2.install();

        int targetSDK = min(app1.getTargetSDK(), app2.getTargetSDK());
        boolean isRestricted = app1.isRestricted && app2.isRestricted;
        boolean hasRequestedLegacyExternalStorage =
                app1.hasRequestedLegacyExternalStorage || app2.hasRequestedLegacyExternalStorage;

        StorageState expectedState;
        if (isRestricted) {
            if (targetSDK < Build.VERSION_CODES.Q) {
                expectedState = DENIED;
            } else {
                expectedState = ISOLATED;
            }
        } else if (hasRequestedLegacyExternalStorage && targetSDK <= Build.VERSION_CODES.Q) {
            expectedState = NON_ISOLATED;
        } else {
            expectedState = ISOLATED;
        }

        Log.i(LOG_TAG, "Expected state=" + expectedState);

        app1.assertStoragePermGranted(expectedState != DENIED);
        app2.assertStoragePermGranted(expectedState != DENIED);

        if (expectedState != DENIED) {
            app1.assertHasNotIsolatedStorage(expectedState == NON_ISOLATED);
            app2.assertHasNotIsolatedStorage(expectedState == NON_ISOLATED);
        }
    }

    @After
    public void uninstallAllTestPackages() {
        app1.uninstall();
        app2.uninstall();
    }
}
