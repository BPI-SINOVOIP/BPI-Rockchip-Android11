/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.assertDynamicShortcutCountExceeded;
import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.assertWith;
import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.list;
import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.retryUntil;
import static com.android.server.pm.shortcutmanagertest.ShortcutManagerTestUtils.setDefaultLauncher;

import android.test.suitebuilder.annotation.SmallTest;

import com.android.compatibility.common.util.CddTest;

@CddTest(requirement="3.8.1/C-4-1")
@SmallTest
public class ShortcutManagerMaxCountTest extends ShortcutManagerCtsTestsBase {
    /**
     * Basic tests: single app, single activity, no manifest shortcuts.
     */
    public void testNumDynamicShortcuts() {
        runWithCallerWithStrictMode(mPackageContext1, () -> {
            assertTrue(getManager().setDynamicShortcuts(list(makeShortcut("s1"))));

            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("s", 1, 15))));
            assertWith(getManager().getDynamicShortcuts())
                    .haveIds(makeIds("s", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();

            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("sx", 1, 15))));

            assertDynamicShortcutCountExceeded(() -> {
                getManager().setDynamicShortcuts(makeShortcuts(makeIds("sy", 1, 16)));
            });

            assertWith(getManager().getDynamicShortcuts())
                    .haveIds(makeIds("sx", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();

            assertDynamicShortcutCountExceeded(() -> {
                getManager().addDynamicShortcuts(list(
                        makeShortcut("sy1")));
            });
            assertWith(getManager().getDynamicShortcuts())
                    .haveIds(makeIds("sx", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();
            getManager().removeDynamicShortcuts(list("sx15"));
            assertTrue(getManager().addDynamicShortcuts(list(
                    makeShortcut("sy1"))));

            assertWith(getManager().getDynamicShortcuts())
                    .haveIds("sx1", "sx2", "sx3", "sx4", "sx5", "sx6", "sx7", "sx8", "sx9", "sx10",
                            "sx11", "sx12", "sx13", "sx14", "sy1")
                    .areAllDynamic()
                    .areAllEnabled();

            getManager().removeAllDynamicShortcuts();

            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("s", 1, 15))));
            assertWith(getManager().getDynamicShortcuts())
                    .haveIds(makeIds("s", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();
        });
    }

    /**
     * Manifest shortcuts are included in the count too.
     */
    public void testWithManifest() throws Exception {
        runWithCallerWithStrictMode(mPackageContext1, () -> {
            enableManifestActivity("Launcher_manifest_1", true);
            enableManifestActivity("Launcher_manifest_2", true);

            retryUntil(() -> getManager().getManifestShortcuts().size() == 3,
                    "Manifest shortcuts didn't show up");

        });

        runWithCallerWithStrictMode(mPackageContext1, () -> {
            assertWith(getManager().getManifestShortcuts())
                    .haveIds("ms1", "ms21", "ms22")
                    .areAllManifest()
                    .areAllEnabled()
                    .areAllNotPinned()

                    .selectByIds("ms1")
                    .forAllShortcuts(sa -> {
                        assertEquals(getActivity("Launcher_manifest_1"), sa.getActivity());
                    })

                    .revertToOriginalList()
                    .selectByIds("ms21", "ms22")
                    .forAllShortcuts(sa -> {
                        assertEquals(getActivity("Launcher_manifest_2"), sa.getActivity());
                    });

        });

        // Note since max counts is per activity, testNumDynamicShortcuts_single should just pass.
        testNumDynamicShortcuts();

        // Launcher_manifest_1 has one manifest, so can only add 14 dynamic shortcuts.
        runWithCallerWithStrictMode(mPackageContext1, () -> {
            setTargetActivityOverride("Launcher_manifest_1");

            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("s", 1, 14))));
            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher_manifest_1"))
                    .haveIds(makeIds("s", 1, 14))
                    .areAllEnabled();

            assertDynamicShortcutCountExceeded(() -> getManager().setDynamicShortcuts(
                    makeShortcuts(makeIds("sx", 1, 15))));
            // Not changed.
            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher_manifest_1"))
                    .haveIds(makeIds("s", 1, 14))
                    .areAllEnabled();
        });

        // Launcher_manifest_2 has two manifests, so can only add 13.
        runWithCallerWithStrictMode(mPackageContext1, () -> {
            setTargetActivityOverride("Launcher_manifest_2");

            assertTrue(getManager().addDynamicShortcuts(makeShortcuts(makeIds("s", 1, 13))));
            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher_manifest_2"))
                    .haveIds(makeIds("s", 1, 13))
                    .areAllEnabled();

            assertDynamicShortcutCountExceeded(() -> getManager().addDynamicShortcuts(list(
                    makeShortcut("sx1")
            )));
            // Not added.
            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher_manifest_2"))
                    .haveIds(makeIds("s", 1, 13))
                    .areAllEnabled();
        });
    }

    public void testChangeActivity() {
        runWithCallerWithStrictMode(mPackageContext1, () -> {
            setTargetActivityOverride("Launcher");
            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("s", 1, 15))));
            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher"))
                    .haveIds(makeIds("s", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();

            setTargetActivityOverride("Launcher2");
            assertTrue(getManager().addDynamicShortcuts(makeShortcuts(makeIds("sb", 1, 15))));

            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher"))
                    .haveIds(makeIds("s", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled()

                    .revertToOriginalList()
                    .selectByActivity(getActivity("Launcher2"))
                    .haveIds(makeIds("sb", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();

            // Moving one from L1 to L2 is not allowed.
            assertDynamicShortcutCountExceeded(() -> getManager().updateShortcuts(list(
                    makeShortcut("s1", getActivity("Launcher2"))
            )));

            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher"))
                    .haveIds(makeIds("s", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled()

                    .revertToOriginalList()
                    .selectByActivity(getActivity("Launcher2"))
                    .haveIds(makeIds("sb", 1, 15))
                    .areAllDynamic()
                    .areAllEnabled();

            // But swapping shortcuts will work.
            assertTrue(getManager().updateShortcuts(list(
                    makeShortcut("s1", getActivity("Launcher2")),
                    makeShortcut("sb1", getActivity("Launcher"))
            )));

            assertWith(getManager().getDynamicShortcuts())
                    .selectByActivity(getActivity("Launcher"))
                    .haveIds("sb1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
                            "s12", "s13", "s14", "s15")
                    .areAllDynamic()
                    .areAllEnabled()

                    .revertToOriginalList()
                    .selectByActivity(getActivity("Launcher2"))
                    .haveIds("s1", "sb2", "sb3", "sb4", "sb5", "sb6", "sb7", "sb8", "sb9", "sb10",
                            "sb11", "sb12", "sb13", "sb14", "sb15")
                    .areAllDynamic()
                    .areAllEnabled();
        });
    }

    public void testWithPinned() {
        runWithCallerWithStrictMode(mPackageContext1, () -> {
            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("s", 1, 15))));
        });

        setDefaultLauncher(getInstrumentation(), mLauncherContext1);

        runWithCallerWithStrictMode(mLauncherContext1, () -> {
            getLauncherApps().pinShortcuts(mPackageContext1.getPackageName(),
                    list(makeIds("s", 1, 15)),
                    getUserHandle());
        });

        runWithCallerWithStrictMode(mPackageContext1, () -> {
            assertTrue(getManager().setDynamicShortcuts(makeShortcuts(makeIds("sb", 1, 15))));

            assertWith(getManager().getDynamicShortcuts())
                    .haveIds(makeIds("sb", 1, 15))
                    .areAllEnabled()
                    .areAllNotPinned();

            assertWith(getManager().getPinnedShortcuts())
                    .haveIds(makeIds("s", 1, 15))
                    .areAllEnabled()
                    .areAllNotDynamic();
        });
    }
}
