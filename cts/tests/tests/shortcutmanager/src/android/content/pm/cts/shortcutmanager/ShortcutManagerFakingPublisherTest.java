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
package android.content.pm.cts.shortcutmanager;

import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.assertExpectException;
import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.list;
import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.runCommand;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ShortcutInfo;
import android.platform.test.annotations.SecurityTest;
import android.test.suitebuilder.annotation.SmallTest;

import androidx.test.InstrumentationRegistry;

/**
 * CTS for b/109824443.
 */
@SmallTest
@SecurityTest
public class ShortcutManagerFakingPublisherTest extends ShortcutManagerCtsTestsBase {
    private static final String ANOTHER_PACKAGE =
            "android.content.pm.cts.shortcutmanager.packages.package4";

    private static final ComponentName ANOTHER_HOME_ACTIVITY = new ComponentName(
            ANOTHER_PACKAGE, "android.content.pm.cts.shortcutmanager.packages.HomeActivity");

    private static final String INVALID_ID =
            "[ShortcutManagerFakingPublisherTest.shortcut_that_should_not_be_created]";

    @Override
    protected String getOverrideConfig() {
        return "reset_interval_sec=999999,"
                + "max_updates_per_interval=999999,"
                + "max_shortcuts=10"
                + "max_icon_dimension_dp=96,"
                + "max_icon_dimension_dp_lowram=96,"
                + "icon_format=PNG,"
                + "icon_quality=100";
    }

    public void testSpoofingPublisher() {
        final Context myContext = getTestContext();
        final Context anotherContext;
        try {
            anotherContext = getTestContext().createPackageContext(ANOTHER_PACKAGE, 0);
        } catch (NameNotFoundException e) {
            fail("Unable to create package context for " + ANOTHER_PACKAGE);
            return;
        }
        final ShortcutInfo invalid = new ShortcutInfo.Builder(anotherContext, INVALID_ID)
                .setShortLabel(INVALID_ID)
                .setIntent(new Intent(Intent.ACTION_VIEW))
                .setActivity(ANOTHER_HOME_ACTIVITY)
                .build();

        // Check set.
        runWithCaller(mPackageContext1, () -> {
            getManager().removeAllDynamicShortcuts();

            assertShortcutPackageMismatch("setDynamicShortcuts1", mPackageContext1, () -> {
                getManager().setDynamicShortcuts(list(
                        invalid));
            });
            assertInvalidShortcutNotCreated();
            assertShortcutPackageMismatch("setDynamicShortcuts2A", mPackageContext1, () -> {
                getManager().setDynamicShortcuts(list(
                        invalid,
                        makeShortcut("s1", "title1")));
            });
            assertInvalidShortcutNotCreated();
            assertShortcutPackageMismatch("setDynamicShortcuts2B", mPackageContext1, () -> {
                getManager().setDynamicShortcuts(list(
                        makeShortcut("s1", "title1"),
                        invalid));
            });
            assertInvalidShortcutNotCreated();
        });

        // Check add.
        runWithCaller(mPackageContext1, () -> {
            getManager().removeAllDynamicShortcuts();

            assertShortcutPackageMismatch("addDynamicShortcuts1", mPackageContext1, () -> {
                getManager().addDynamicShortcuts(list(
                        invalid));
            });
            assertInvalidShortcutNotCreated();
            assertShortcutPackageMismatch("addDynamicShortcuts2A", mPackageContext1, () -> {
                getManager().addDynamicShortcuts(list(
                        invalid,
                        makeShortcut("s1", "title1")));
            });
            assertInvalidShortcutNotCreated();
            assertShortcutPackageMismatch("addDynamicShortcuts2B", mPackageContext1, () -> {
                getManager().addDynamicShortcuts(list(
                        makeShortcut("s1", "title1"),
                        invalid));
            });
            assertInvalidShortcutNotCreated();
        });

        // Check update.
        runWithCaller(mPackageContext1, () -> {
            getManager().removeAllDynamicShortcuts();

            assertShortcutPackageMismatch("updateShortcuts1", mPackageContext1, () -> {
                getManager().updateShortcuts(list(
                        invalid));
            });
            assertInvalidShortcutNotCreated();
            assertShortcutPackageMismatch("updateShortcuts2A", mPackageContext1, () -> {
                getManager().updateShortcuts(list(
                        invalid,
                        makeShortcut("s1", "title1")));
            });
            assertInvalidShortcutNotCreated();
            assertShortcutPackageMismatch("updateShortcuts2B", mPackageContext1, () -> {
                getManager().updateShortcuts(list(
                        makeShortcut("s1", "title1"),
                        invalid));
            });
            assertInvalidShortcutNotCreated();
        });

        // requestPin (API26 and above)
        runWithCaller(mPackageContext1, () -> {
            getManager().removeAllDynamicShortcuts();

            assertShortcutPackageMismatch("requestPinShortcut", mPackageContext1, () -> {
                getManager().requestPinShortcut(invalid, null);
            });
            assertInvalidShortcutNotCreated();
        });

        // createShortcutResultIntent (API26 and above)
        runWithCaller(mPackageContext1, () -> {
            getManager().removeAllDynamicShortcuts();

            assertShortcutPackageMismatch("createShortcutResultIntent", mPackageContext1, () -> {
                getManager().createShortcutResultIntent(invalid);
            });
            assertInvalidShortcutNotCreated();
        });
    }

    private void assertInvalidShortcutNotCreated() {
        for (String s : runCommand(InstrumentationRegistry.getInstrumentation(),
                "dumpsys shortcut")) {
            assertFalse("dumpsys shortcut contained invalid ID", s.contains(INVALID_ID));
        }
    }

    private void assertShortcutPackageMismatch(String method, Context callerContext, Runnable r) {
        assertExpectException(
                "Caller=" + callerContext.getPackageName() + ", method=" + method,
                SecurityException.class, "Shortcut package name mismatch",
                () -> runWithCaller(callerContext, () -> r.run())
        );
    }
}
