/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.sharesheet.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.ActivityManager;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.LabeledIntent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Point;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.service.chooser.ChooserTarget;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * TODO: Add JavaDoc
 */
@RunWith(AndroidJUnit4.class)
public class CtsSharesheetDeviceTest {

    public static final String TAG = CtsSharesheetDeviceTest.class.getSimpleName();

    private static final int WAIT_AND_ASSERT_FOUND_TIMEOUT_MS = 5000;
    private static final int WAIT_AND_ASSERT_NOT_FOUND_TIMEOUT_MS = 2500;
    private static final int WAIT_FOR_IDLE_TIMEOUT_MS = 5000;

    private static final int MAX_EXTRA_INITIAL_INTENTS_SHOWN = 2;
    private static final int MAX_EXTRA_CHOOSER_TARGETS_SHOWN = 2;

    private static final String ACTION_INTENT_SENDER_FIRED_ON_CLICK =
            "android.sharesheet.cts.ACTION_INTENT_SENDER_FIRED_ON_CLICK";

    static final String CTS_DATA_TYPE = "test/cts"; // Special CTS mime type
    static final String CATEGORY_CTS_TEST = "CATEGORY_CTS_TEST";

    private Context mContext;
    private Instrumentation mInstrumentation;
    private UiAutomation mAutomation;
    public UiDevice mDevice;

    private String mPkg, mExcludePkg, mActivityLabelTesterPkg, mIntentFilterLabelTesterPkg;
    private String mSharesheetPkg;

    private ActivityManager mActivityManager;
    private ShortcutManager mShortcutManager;

    private String mAppLabel,
            mActivityTesterAppLabel, mActivityTesterActivityLabel,
            mIntentFilterTesterAppLabel, mIntentFilterTesterActivityLabel,
            mIntentFilterTesterIntentFilterLabel,
            mBlacklistLabel,
            mChooserTargetServiceLabel, mSharingShortcutLabel, mExtraChooserTargetsLabelBase,
            mExtraInitialIntentsLabelBase, mPreviewTitle, mPreviewText;

    private Set<ComponentName> mTargetsToExclude;

    private boolean mMeetsResolutionRequirements;

    /**
     * To validate Sharesheet API and API behavior works as intended UI test sare required. It is
     * impossible to know the how the Sharesheet UI will be modified by end partners so these tests
     * attempt to assume use the minimum needed assumptions to make the tests work.
     *
     * We cannot assume a scrolling direction or starting point because of potential UI variations.
     * Because of limits of the UiAutomator pipeline only content visible on screen can be tested.
     * These two constraints mean that all automated Sharesheet tests must be for content we
     * reasonably expect to be visible after the sheet is opened without any direct interaction.
     *
     * Extra care is taken to ensure tested content is reasonably visible by:
     * - Splitting tests across multiple Sharesheet calls
     * - Excluding all packages not relevant to the test
     * - Assuming a max of three targets per row of apps
     */

    @Before
    public void init() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        mPkg = mContext.getPackageName();
        mExcludePkg = mPkg + ".packages.excludetester";
        mActivityLabelTesterPkg = mPkg + ".packages.activitylabeltester";
        mIntentFilterLabelTesterPkg = mPkg + ".packages.intentfilterlabeltester";

        mDevice = UiDevice.getInstance(mInstrumentation);
        mAutomation = mInstrumentation.getUiAutomation();

        // The the device resolution is too low skip the unneeded init
        mMeetsResolutionRequirements = meetsResolutionRequirements();
        if (!mMeetsResolutionRequirements) return;

        mActivityManager = mContext.getSystemService(ActivityManager.class);
        mShortcutManager = mContext.getSystemService(ShortcutManager.class);
        PackageManager pm = mContext.getPackageManager();
        assertNotNull(mActivityManager);
        assertNotNull(mShortcutManager);
        assertNotNull(pm);

        // Load in string to match against
        mBlacklistLabel = mContext.getString(R.string.test_blacklist_label);
        mAppLabel = mContext.getString(R.string.test_app_label);
        mActivityTesterAppLabel = mContext.getString(R.string.test_activity_label_app);
        mActivityTesterActivityLabel = mContext.getString(R.string.test_activity_label_activity);
        mIntentFilterTesterAppLabel = mContext.getString(R.string.test_intent_filter_label_app);
        mIntentFilterTesterActivityLabel =
                mContext.getString(R.string.test_intent_filter_label_activity);
        mIntentFilterTesterIntentFilterLabel =
                mContext.getString(R.string.test_intent_filter_label_intentfilter);
        mChooserTargetServiceLabel = mContext.getString(R.string.test_chooser_target_service_label);
        mSharingShortcutLabel = mContext.getString(R.string.test_sharing_shortcut_label);
        mExtraChooserTargetsLabelBase = mContext.getString(R.string.test_extra_chooser_targets_label);
        mExtraInitialIntentsLabelBase = mContext.getString(R.string.test_extra_initial_intents_label);
        mPreviewTitle = mContext.getString(R.string.test_preview_title);
        mPreviewText = mContext.getString(R.string.test_preview_text);

        // We want to only show targets in the sheet put forth by the CTS test. In order to do that
        // a special type is used but this doesn't prevent apps registered against */* from showing.
        // To hide */* targets, search for all matching targets and exclude them. Requires
        // permission android.permission.QUERY_ALL_PACKAGES.
        List<ResolveInfo> matchingTargets = mContext.getPackageManager().queryIntentActivities(
                createMatchingIntent(),
                PackageManager.MATCH_DEFAULT_ONLY | PackageManager.GET_META_DATA
        );

        mTargetsToExclude = matchingTargets.stream()
                .map(ri -> {
                    return new ComponentName(ri.activityInfo.packageName, ri.activityInfo.name);
                })
                .filter(cn -> {
                    // Exclude our own test targets
                    String pkg = cn.getPackageName();
                    boolean isInternalPkg  = pkg.equals(mPkg) ||
                            pkg.equals(mActivityLabelTesterPkg) ||
                            pkg.equals(mIntentFilterLabelTesterPkg);

                    return !isInternalPkg;
                })
                .collect(Collectors.toSet());

        // We need to know the package used by the system Sharesheet so we can properly
        // wait for the UI to load. Do this by resolving which activity consumes the share intent.
        // There must be a system Sharesheet or fail, otherwise fetch its the package.
        Intent shareIntent = createShareIntent(false, 0, 0);
        ResolveInfo shareRi = pm.resolveActivity(shareIntent, PackageManager.MATCH_DEFAULT_ONLY);

        assertNotNull(shareRi);
        assertNotNull(shareRi.activityInfo);

        mSharesheetPkg = shareRi.activityInfo.packageName;
        assertNotNull(mSharesheetPkg);

        // Finally ensure the device is awake
        mDevice.wakeUp();
    }

    /**
     * To test all features the Sharesheet will need to be opened and closed a few times. To keep
     * total run time low, jam as many tests are possible into each visible test portion.
     */
    @Test
    public void bulkTest1() {
        if (!mMeetsResolutionRequirements) return; // Skip test if resolution is too low
        try {
            launchSharesheet(createShareIntent(false /* do not test preview */,
                    0 /* do not test EIIs */,
                    0 /* do not test ECTs */));

            doesExcludeComponents();
            showsApplicationLabel();
            showsAppAndActivityLabel();
            showsAppAndIntentFilterLabel();
            isChooserTargetServiceDirectShareEnabled();

            // Must be run last, partial completion closes the Sharesheet
            firesIntentSenderWithExtraChosenComponent();

        } catch (Exception e) {
            // No-op
        } finally {
            // The Sharesheet may or may not be open depending on test success, close it if it is
            closeSharesheetIfNeeded();
        }
    }

    @Test
    public void bulkTest2() {
        if (!mMeetsResolutionRequirements) return; // Skip test if resolution is too low
        try {
            addShortcuts(1);
            launchSharesheet(createShareIntent(false /* do not test preview */,
                    MAX_EXTRA_INITIAL_INTENTS_SHOWN + 1 /* test EIIs at 1 above cap */,
                    MAX_EXTRA_CHOOSER_TARGETS_SHOWN + 1 /* test ECTs at 1 above cap */));
            // Note: EII and ECT cap is not tested here

            showsExtraInitialIntents();
            showsExtraChooserTargets();
            isSharingShortcutDirectShareEnabled();

        } catch (Exception e) {
            // No-op
        } finally {
            closeSharesheet();
            clearShortcuts();
        }
    }

    /**
     * Testing content preview must be isolated into its own test because in AOSP on small devices
     * the preview can push app and direct share content offscreen because of its height.
     */
    @Test
    public void contentPreviewTest() {
        if (!mMeetsResolutionRequirements) return; // Skip test if resolution is too low
        try {
            launchSharesheet(createShareIntent(true /* test content preview */,
                    0 /* do not test EIIs */,
                    0 /* do not test ECTs */));
            showsContentPreviewTitle();
            showsContentPreviewText();
        } catch (Exception e) {
            // No-op
        } finally {
            closeSharesheet();
        }
    }

    /*
    Test methods
     */

    /**
     * Tests API compliance for Intent.EXTRA_EXCLUDE_COMPONENTS. This test is necessary for other
     * tests to run as expected.
     *
     * Requires content loaded with permission: android.permission.QUERY_ALL_PACKAGES
     */
    public void doesExcludeComponents() {
        // The excluded component should not be found on screen
        waitAndAssertNoTextContains(mBlacklistLabel);
    }

    /**
     * Tests API behavior compliance for security to always show application label
     */
    public void showsApplicationLabel() {
        // For each app target the providing app's application manifest label should be shown
        waitAndAssertTextContains(mAppLabel);
    }

    /**
     * Tests API behavior compliance to show application and activity label when available
     */
    public void showsAppAndActivityLabel() {
        waitAndAssertTextContains(mActivityTesterAppLabel);
        waitAndAssertTextContains(mActivityTesterActivityLabel);
    }

    /**
     * Tests API behavior compliance to show application and intent filter label when available
     */
    public void showsAppAndIntentFilterLabel() {
        // NOTE: it is not necessary to show any set Activity label if an IntentFilter label is set
        waitAndAssertTextContains(mIntentFilterTesterAppLabel);
        waitAndAssertTextContains(mIntentFilterTesterIntentFilterLabel);
    }

    /**
     * Tests API compliance for Intent.EXTRA_INITIAL_INTENTS
     */
    public void showsExtraInitialIntents() {
        // Should show extra initial intents but must limit them, can't test limit here
        waitAndAssertTextContains(mExtraInitialIntentsLabelBase);
    }

    /**
     * Tests API compliance for Intent.EXTRA_CHOOSER_TARGETS
     */
    public void showsExtraChooserTargets() {
        // Should show chooser targets but must limit them, can't test limit here
        if (mActivityManager.isLowRamDevice()) {
            // The direct share row and EXTRA_CHOOSER_TARGETS should be hidden on low-ram devices
            waitAndAssertNoTextContains(mExtraChooserTargetsLabelBase);
        } else {
            waitAndAssertTextContains(mExtraChooserTargetsLabelBase);
        }
    }

    /**
     * Tests API behavior compliance for Intent.EXTRA_TITLE
     */
    public void showsContentPreviewTitle() {
        waitAndAssertTextContains(mPreviewTitle);
    }

    /**
     * Tests API behavior compliance for Intent.EXTRA_TEXT
     */
    public void showsContentPreviewText() {
        waitAndAssertTextContains(mPreviewText);
    }

    /**
     * Tests API compliance for Intent.EXTRA_CHOSEN_COMPONENT_INTENT_SENDER and related APIs
     * UI assumption: target labels are clickable, clicking opens target
     */
    public void firesIntentSenderWithExtraChosenComponent() throws Exception {
        // To receive the extra chosen component a target must be clicked. Clicking the target
        // will close the Sharesheet. Run this last in any sequence of tests.

        // First find the target to click. This will fail if the showsApplicationLabel() test fails.
        UiObject2 shareTarget = findTextContains(mAppLabel);
        assertNotNull(shareTarget);

        ComponentName clickedComponent = new ComponentName(mContext,
                CtsSharesheetDeviceActivity.class);

        final CountDownLatch latch = new CountDownLatch(1);
        final Intent[] response = {null}; // Must be final so use an array

        // Listen for the PendingIntent broadcast on click
        BroadcastReceiver br = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                response[0] = intent;
                latch.countDown();
            }
        };
        mContext.registerReceiver(br, new IntentFilter(ACTION_INTENT_SENDER_FIRED_ON_CLICK));

        // Start the event sequence and wait for results
        shareTarget.click();

        // The latch may fail for a number of reasons but we still need to unregister the
        // BroadcastReceiver, so capture and rethrow any errors.
        Exception delayedException = null;
        try {
            latch.await(1000, TimeUnit.MILLISECONDS);
        } catch (Exception e) {
            delayedException = e;
        } finally {
            mContext.unregisterReceiver(br);
        }
        if (delayedException != null) throw delayedException;

        // Finally validate the received Intent
        validateChosenComponentIntent(response[0], clickedComponent);
    }

    private void validateChosenComponentIntent(Intent intent, ComponentName matchingComponent) {
        assertNotNull(intent);

        assertTrue(intent.hasExtra(Intent.EXTRA_CHOSEN_COMPONENT));
        Object extra = intent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT);
        assertNotNull(extra);

        assertTrue(extra instanceof ComponentName);
        ComponentName component = (ComponentName) extra;

        assertEquals(component, matchingComponent);
    }

    /**
     * Tests API behavior compliance for ChooserTargetService
     */
    public void isChooserTargetServiceDirectShareEnabled() {
        // ChooserTargets can take time to load. To account for this:
        // * All non-test ChooserTargetServices shouldn't be loaded because of blacklist
        // * waitAndAssert operations have lengthy timeout periods
        // * Last time to run in suite so prior operations reduce wait time

        if (mActivityManager.isLowRamDevice()) {
            // Ensure direct share is disabled on low ram devices
            waitAndAssertNoTextContains(mChooserTargetServiceLabel);
        } else {
            // Ensure direct share is enabled
            waitAndAssertTextContains(mChooserTargetServiceLabel);
        }
    }

    /**
     * Tests API behavior compliance for Sharing Shortcuts
     */
    public void isSharingShortcutDirectShareEnabled() {
        if (mActivityManager.isLowRamDevice()) {
            // Ensure direct share is disabled on low ram devices
            waitAndAssertNoTextContains(mSharingShortcutLabel);
        } else {
            // Ensure direct share is enabled
            waitAndAssertTextContains(mSharingShortcutLabel);
        }
    }

    /*
    Setup methods
     */

    /**
     * Included CTS tests can fail for resolutions that are too small. This is because
     * the tests check for visibility of UI elements that are hidden below certain resolutions.
     * Ensure that the device under test has the min necessary screen height in dp. Tests do not
     * fail at any width at or above the CDD minimum of 320dp.
     *
     * Tests have different failure heights:
     * bulkTest1, bulkTest2 fail below ~680 dp in height
     * contentPreviewTest fails below ~640dp in height
     *
     * For safety, check against screen height some buffer on the worst case: 700dp. Most new
     * consumer devices have a height above this.
     *
     * @return if min resolution requirements are met
     */
    private boolean meetsResolutionRequirements() {
        final Point displaySizeDp = mDevice.getDisplaySizeDp();
        return displaySizeDp.y >= 700; // dp
    }

    public void addShortcuts(int size) {
        mShortcutManager.addDynamicShortcuts(createShortcuts(size));
    }

    public void clearShortcuts() {
        mShortcutManager.removeAllDynamicShortcuts();
    }

    private List<ShortcutInfo> createShortcuts(int size) {
        List<ShortcutInfo> ret = new ArrayList<>();
        for (int i=0; i<size; i++) {
            ret.add(createShortcut(""+i));
        }
        return ret;
    }

    private ShortcutInfo createShortcut(String id) {
        HashSet<String> categories = new HashSet<>();
        categories.add(CATEGORY_CTS_TEST);

        return new ShortcutInfo.Builder(mContext, id)
                .setShortLabel(mSharingShortcutLabel)
                .setIcon(Icon.createWithResource(mContext, R.drawable.black_64x64))
                .setCategories(categories)
                .setIntent(new Intent(Intent.ACTION_DEFAULT)) /* an Intent with an action must be set */
                .build();
    }

    private void launchSharesheet(Intent shareIntent) {
        mContext.startActivity(shareIntent);
        waitAndAssertPkgVisible(mSharesheetPkg);
        waitForIdle();
    }

    private void closeSharesheetIfNeeded() {
        if (isSharesheetVisible()) closeSharesheet();
    }

    private void closeSharesheet() {
        mDevice.pressBack();
        waitAndAssertPkgNotVisible(mSharesheetPkg);
        waitForIdle();
    }

    private boolean isSharesheetVisible() {
        // This method intentionally does not wait, looks to see if visible on method call
        return mDevice.findObject(By.pkg(mSharesheetPkg).depth(0)) != null;
    }

    private Intent createMatchingIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType(CTS_DATA_TYPE);
        return intent;
    }

    private Intent createShareIntent(boolean contentPreview,
            int numExtraInitialIntents,
            int numExtraChooserTargets) {

        Intent intent = createMatchingIntent();

        if (contentPreview) {
            intent.putExtra(Intent.EXTRA_TITLE, mPreviewTitle);
            intent.putExtra(Intent.EXTRA_TEXT, mPreviewText);
        }

        PendingIntent pi = PendingIntent.getBroadcast(
                mContext,
                9384 /* number not relevant */ ,
                new Intent(ACTION_INTENT_SENDER_FIRED_ON_CLICK),
                PendingIntent.FLAG_UPDATE_CURRENT);

        Intent shareIntent = Intent.createChooser(intent, null, pi.getIntentSender());

        // Intent.EXTRA_EXCLUDE_COMPONENTS is used to ensure only test targets appear
        List<ComponentName> list = new ArrayList<>(mTargetsToExclude);
        list.add(new ComponentName(mPkg, mPkg + ".BlacklistTestActivity"));
        shareIntent.putExtra(Intent.EXTRA_EXCLUDE_COMPONENTS,
                list.toArray(new ComponentName[0]));

        if (numExtraInitialIntents > 0) {
            Intent[] eiis = new Intent[numExtraInitialIntents];
            for (int i = 0; i < eiis.length; i++) {
                Intent eii = new Intent();
                eii.setComponent(new ComponentName(mPkg,
                        mPkg + ".ExtraInitialIntentTestActivity"));

                LabeledIntent labeledEii = new LabeledIntent(eii, mPkg,
                        getExtraInitialIntentsLabel(i),
                        0 /* provide no icon */);

                eiis[i] = labeledEii;
            }

            shareIntent.putExtra(Intent.EXTRA_INITIAL_INTENTS, eiis);
        }

        if (numExtraChooserTargets > 0) {
            ChooserTarget[] ects = new ChooserTarget[numExtraChooserTargets];
            for (int i = 0; i < ects.length; i++) {
                ects[i] = new ChooserTarget(
                        getExtraChooserTargetLabel(i),
                        Icon.createWithResource(mContext, R.drawable.black_64x64),
                        1f,
                        new ComponentName(mPkg, mPkg + ".CtsSharesheetDeviceActivity"),
                        new Bundle());
            }

            shareIntent.putExtra(Intent.EXTRA_CHOOSER_TARGETS, ects);
        }

        // Ensure the sheet will launch directly from the test
        shareIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        return shareIntent;
    }

    private String getExtraChooserTargetLabel(int position) {
        return mExtraChooserTargetsLabelBase + " " + position;
    }

    private String getExtraInitialIntentsLabel(int position) {
        return mExtraInitialIntentsLabelBase + " " + position;
    }

    /*
    UI testing methods
     */

    private void waitForIdle() {
        mDevice.waitForIdle(WAIT_FOR_IDLE_TIMEOUT_MS);
    }

    private void waitAndAssertPkgVisible(String pkg) {
        waitAndAssertFound(By.pkg(pkg).depth(0));
    }

    private void waitAndAssertPkgNotVisible(String pkg) {
        waitAndAssertNotFound(By.pkg(pkg));
    }

    private void waitAndAssertTextContains(String containsText) {
        waitAndAssertFound(By.textContains(containsText));
    }

    private void waitAndAssertNoTextContains(String containsText) {
        waitAndAssertNotFound(By.textContains(containsText));
    }

    /**
     * waitAndAssertFound will wait until UI defined by the selector is found. If it's never found,
     * this will wait for the duration of the full timeout. Take care to call this method after
     * reasonable steps are taken to ensure fast completion.
     */
    private void waitAndAssertFound(BySelector selector) {
        assertNotNull(mDevice.wait(Until.findObject(selector), WAIT_AND_ASSERT_FOUND_TIMEOUT_MS));
    }

    /**
     * waitAndAssertNotFound waits for any visible UI to be hidden, validates that it's indeed gone
     * without waiting more and returns. This means if the UI wasn't visible to start with the
     * method will return without no timeout. Take care to call this method only once there's reason
     * to think the UI is in the right state for testing.
     */
    private void waitAndAssertNotFound(BySelector selector) {
        mDevice.wait(Until.gone(selector), WAIT_AND_ASSERT_NOT_FOUND_TIMEOUT_MS);
        assertNull(mDevice.findObject(selector));
    }

    /**
     * findTextContains uses logic similar to waitAndAssertFound to locate UI objects that contain
     * the provided String.
     * @param containsText the String to search for, note this is not an exact match only contains
     * @return UiObject2 that can be used, for example, to execute a click
     */
    private UiObject2 findTextContains(String containsText) {
        return mDevice.wait(Until.findObject(By.textContains(containsText)),
                WAIT_AND_ASSERT_FOUND_TIMEOUT_MS);
    }
}
