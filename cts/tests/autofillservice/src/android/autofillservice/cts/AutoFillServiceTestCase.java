/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.autofillservice.cts;

import static android.autofillservice.cts.Helper.getContext;
import static android.autofillservice.cts.InstrumentedAutoFillService.SERVICE_NAME;
import static android.content.Context.CLIPBOARD_SERVICE;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.app.PendingIntent;
import android.autofillservice.cts.InstrumentedAutoFillService.Replier;
import android.autofillservice.cts.augmented.AugmentedAuthActivity;
import android.autofillservice.cts.inline.InlineUiBot;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.service.autofill.InlinePresentation;
import android.util.Log;
import android.view.autofill.AutofillManager;
import android.widget.RemoteViews;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.compatibility.common.util.DeviceConfigStateChangerRule;
import com.android.compatibility.common.util.RequiredFeatureRule;
import com.android.compatibility.common.util.RetryRule;
import com.android.compatibility.common.util.SafeCleanerRule;
import com.android.compatibility.common.util.SettingsStateKeeperRule;
import com.android.compatibility.common.util.TestNameUtils;
import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSessionRule;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;

/**
 * Placeholder for the base class for all integration tests:
 *
 * <ul>
 *   <li>{@link AutoActivityLaunch}
 *   <li>{@link ManualActivityLaunch}
 * </ul>
 *
 * <p>These classes provide the common infrastructure such as:
 *
 * <ul>
 *   <li>Preserving the autofill service settings.
 *   <li>Cleaning up test state.
 *   <li>Wrapping the test under autofill-specific test rules.
 *   <li>Launching the activity used by the test.
 * </ul>
 */
public final class AutoFillServiceTestCase {

    /**
     * Base class for all test cases that use an {@link AutofillActivityTestRule} to
     * launch the activity.
     */
    // Must be public because of @ClassRule
    public abstract static class AutoActivityLaunch<A extends AbstractAutoFillActivity>
            extends BaseTestCase {

        /**
         * Returns if inline suggestion is enabled.
         */
        protected boolean isInlineMode() {
            return false;
        }

        protected static UiBot getInlineUiBot() {
            return sDefaultUiBot2;
        }

        protected static UiBot getDropdownUiBot() {
            return sDefaultUiBot;
        }

        @ClassRule
        public static final SettingsStateKeeperRule sPublicServiceSettingsKeeper =
                sTheRealServiceSettingsKeeper;

        protected AutoActivityLaunch() {
            super(sDefaultUiBot);
        }
        protected AutoActivityLaunch(UiBot uiBot) {
            super(uiBot);
        }

        @Override
        protected TestRule getMainTestRule() {
            return getActivityRule();
        }

        /**
         * Gets the rule to launch the main activity for this test.
         *
         * <p><b>Note: </b>the rule must be either lazily generated or a static singleton, otherwise
         * this method could return {@code null} when the rule chain that uses it is constructed.
         *
         */
        protected abstract @NonNull AutofillActivityTestRule<A> getActivityRule();

        protected @NonNull A launchActivity(@NonNull Intent intent) {
            return getActivityRule().launchActivity(intent);
        }

        protected @NonNull A getActivity() {
            return getActivityRule().getActivity();
        }
    }

    /**
     * Base class for all test cases that don't require an {@link AutofillActivityTestRule}.
     */
    // Must be public because of @ClassRule
    public abstract static class ManualActivityLaunch extends BaseTestCase {

        @ClassRule
        public static final SettingsStateKeeperRule sPublicServiceSettingsKeeper =
                sTheRealServiceSettingsKeeper;

        protected ManualActivityLaunch() {
            this(sDefaultUiBot);
        }

        protected ManualActivityLaunch(@NonNull UiBot uiBot) {
            super(uiBot);
        }

        @Override
        protected TestRule getMainTestRule() {
            // TODO: create a NoOpTestRule on common code
            return new TestRule() {

                @Override
                public Statement apply(Statement base, Description description) {
                    // Returns a no-op statements
                    return new Statement() {
                        @Override
                        public void evaluate() throws Throwable {
                            base.evaluate();
                        }
                    };
                }
            };
        }

        protected SimpleSaveActivity startSimpleSaveActivity() throws Exception {
            final Intent intent = new Intent(mContext, SimpleSaveActivity.class)
                    .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(intent);
            mUiBot.assertShownByRelativeId(SimpleSaveActivity.ID_LABEL);
            return SimpleSaveActivity.getInstance();
        }

        protected PreSimpleSaveActivity startPreSimpleSaveActivity() throws Exception {
            final Intent intent = new Intent(mContext, PreSimpleSaveActivity.class)
                    .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(intent);
            mUiBot.assertShownByRelativeId(PreSimpleSaveActivity.ID_PRE_LABEL);
            return PreSimpleSaveActivity.getInstance();
        }
    }

    @RunWith(AndroidJUnit4.class)
    // Must be public because of @ClassRule
    public abstract static class BaseTestCase {

        private static final String TAG = "AutoFillServiceTestCase";

        protected static final Replier sReplier = InstrumentedAutoFillService.getReplier();

        protected static final Context sContext = getInstrumentation().getTargetContext();

        // Hack because JUnit requires that @ClassRule instance belong to a public class.
        protected static final SettingsStateKeeperRule sTheRealServiceSettingsKeeper =
                new SettingsStateKeeperRule(sContext, Settings.Secure.AUTOFILL_SERVICE) {
            @Override
            protected void preEvaluate(Description description) {
                TestNameUtils.setCurrentTestClass(description.getClassName());
            }

            @Override
            protected void postEvaluate(Description description) {
                TestNameUtils.setCurrentTestClass(null);
            }
        };

        public static final MockImeSessionRule sMockImeSessionRule = new MockImeSessionRule(
                InstrumentationRegistry.getTargetContext(),
                InstrumentationRegistry.getInstrumentation().getUiAutomation(),
                new ImeSettings.Builder().setInlineSuggestionsEnabled(true)
                        .setInlineSuggestionViewContentDesc(InlineUiBot.SUGGESTION_STRIP_DESC));

        protected static final RequiredFeatureRule sRequiredFeatureRule =
                new RequiredFeatureRule(PackageManager.FEATURE_AUTOFILL);

        private final AutofillTestWatcher mTestWatcher = new AutofillTestWatcher();

        private final RetryRule mRetryRule =
                new RetryRule(getNumberRetries(), () -> {
                    // Between testing and retries, clean all launched activities to avoid
                    // exception:
                    //     Could not launch intent Intent { ... } within 45 seconds.
                    mTestWatcher.cleanAllActivities();
                });

        private final AutofillLoggingTestRule mLoggingRule = new AutofillLoggingTestRule(TAG);

        protected final SafeCleanerRule mSafeCleanerRule = new SafeCleanerRule()
                .setDumper(mLoggingRule)
                .run(() -> sReplier.assertNoUnhandledFillRequests())
                .run(() -> sReplier.assertNoUnhandledSaveRequests())
                .add(() -> { return sReplier.getExceptions(); });

        @Rule
        public final RuleChain mLookAllTheseRules = RuleChain
                //
                // requiredFeatureRule should be first so the test can be skipped right away
                .outerRule(getRequiredFeaturesRule())
                //
                // mTestWatcher should always be one the first rules, as it defines the name of the
                // test being ran and finishes dangling activities at the end
                .around(mTestWatcher)
                //
                // sMockImeSessionRule make sure MockImeSession.create() is used to launch mock IME
                .around(sMockImeSessionRule)
                //
                // mLoggingRule wraps the test but doesn't interfere with it
                .around(mLoggingRule)
                //
                // mSafeCleanerRule will catch errors
                .around(mSafeCleanerRule)
                //
                // mRetryRule should be closest to the main test as possible
                .around(mRetryRule)
                //
                // Augmented Autofill should be disabled by default
                .around(new DeviceConfigStateChangerRule(sContext, DeviceConfig.NAMESPACE_AUTOFILL,
                        AutofillManager.DEVICE_CONFIG_AUTOFILL_SMART_SUGGESTION_SUPPORTED_MODES,
                        Integer.toString(getSmartSuggestionMode())))
                //
                // Finally, let subclasses add their own rules (like ActivityTestRule)
                .around(getMainTestRule());


        protected final Context mContext = sContext;
        protected final String mPackageName;
        protected final UiBot mUiBot;

        private BaseTestCase(@NonNull UiBot uiBot) {
            mPackageName = mContext.getPackageName();
            mUiBot = uiBot;
            mUiBot.reset();
        }

        protected int getSmartSuggestionMode() {
            return AutofillManager.FLAG_SMART_SUGGESTION_OFF;
        }

        /**
         * Gets how many times a test should be retried.
         *
         * @return {@code 1} by default, unless overridden by subclasses or by a global settings
         * named {@code CLASS_NAME + #getNumberRetries} or
         * {@code CtsAutoFillServiceTestCases#getNumberRetries} (the former having a higher
         * priority).
         */
        protected int getNumberRetries() {
            final String localProp = getClass().getName() + "#getNumberRetries";
            final Integer localValue = getNumberRetries(localProp);
            if (localValue != null) return localValue.intValue();

            final String globalProp = "CtsAutoFillServiceTestCases#getNumberRetries";
            final Integer globalValue = getNumberRetries(globalProp);
            if (globalValue != null) return globalValue.intValue();

            return 1;
        }

        private Integer getNumberRetries(String prop) {
            final String value = Settings.Global.getString(sContext.getContentResolver(), prop);
            if (value != null) {
                Log.i(TAG, "getNumberRetries(): overriding to " + value + " because of '" + prop
                        + "' global setting");
                try {
                    return Integer.parseInt(value);
                } catch (Exception e) {
                    Log.w(TAG, "error parsing property '" + prop + "'='" + value + "'", e);
                }
            }
            return null;
        }

        /**
         * Gets a rule that defines which features must be present for this test to run.
         *
         * <p>By default it returns a rule that requires {@link PackageManager#FEATURE_AUTOFILL},
         * but subclass can override to be more specific.
         */
        @NonNull
        protected TestRule getRequiredFeaturesRule() {
            return sRequiredFeatureRule;
        }

        /**
         * Gets the test-specific {@link Rule @Rule}.
         *
         * <p>Sub-class <b>MUST</b> override this method instead of annotation their own rules,
         * so the order is preserved.
         *
         */
        @NonNull
        protected abstract TestRule getMainTestRule();

        @BeforeClass
        public static void disableDefaultAugmentedService() {
            Log.v(TAG, "@BeforeClass: disableDefaultAugmentedService()");
            Helper.setDefaultAugmentedAutofillServiceEnabled(false);
        }

        @AfterClass
        public static void enableDefaultAugmentedService() {
            Log.v(TAG, "@AfterClass: enableDefaultAugmentedService()");
            Helper.setDefaultAugmentedAutofillServiceEnabled(true);
        }

        @Before
        public void prepareDevice() throws Exception {
            Log.v(TAG, "@Before: prepareDevice()");

            // Unlock screen.
            runShellCommand("input keyevent KEYCODE_WAKEUP");

            // Dismiss keyguard, in case it's set as "Swipe to unlock".
            runShellCommand("wm dismiss-keyguard");

            // Collapse notifications.
            runShellCommand("cmd statusbar collapse");

            // Set orientation as portrait, otherwise some tests might fail due to elements not
            // fitting in, IME orientation, etc...
            mUiBot.setScreenOrientation(UiBot.PORTRAIT);

            // Wait until device is idle to avoid flakiness
            mUiBot.waitForIdle();

            // Clear Clipboard
            // TODO(b/117768051): remove try/catch once fixed
            try {
                ((ClipboardManager) mContext.getSystemService(CLIPBOARD_SERVICE))
                    .clearPrimaryClip();
            } catch (Exception e) {
                Log.e(TAG, "Ignoring exception clearing clipboard", e);
            }
        }

        @Before
        public void preTestCleanup() {
            Log.v(TAG, "@Before: preTestCleanup()");

            prepareServicePreTest();

            InstrumentedAutoFillService.resetStaticState();
            AuthenticationActivity.resetStaticState();
            AugmentedAuthActivity.resetStaticState();
            sReplier.reset();
        }

        /**
         * Prepares the service before each test - by default, disables it
         */
        protected void prepareServicePreTest() {
            Log.v(TAG, "prepareServicePreTest(): calling disableService()");
            disableService();
        }

        /**
         * Enables the {@link InstrumentedAutoFillService} for autofill for the current user.
         */
        protected void enableService() {
            Helper.enableAutofillService(getContext(), SERVICE_NAME);
        }

        /**
         * Disables the {@link InstrumentedAutoFillService} for autofill for the current user.
         */
        protected void disableService() {
            Helper.disableAutofillService(getContext());
        }

        /**
         * Asserts that the {@link InstrumentedAutoFillService} is enabled for the default user.
         */
        protected void assertServiceEnabled() {
            Helper.assertAutofillServiceStatus(SERVICE_NAME, true);
        }

        /**
         * Asserts that the {@link InstrumentedAutoFillService} is disabled for the default user.
         */
        protected void assertServiceDisabled() {
            Helper.assertAutofillServiceStatus(SERVICE_NAME, false);
        }

        protected RemoteViews createPresentation(String message) {
            return Helper.createPresentation(message);
        }

        protected RemoteViews createPresentationWithCancel(String message) {
            final RemoteViews presentation = new RemoteViews(getContext()
                    .getPackageName(), R.layout.list_item_cancel);
            presentation.setTextViewText(R.id.text1, message);
            return presentation;
        }

        protected InlinePresentation createInlinePresentation(String message) {
            return Helper.createInlinePresentation(message);
        }

        protected InlinePresentation createInlinePresentation(String message,
                                                              PendingIntent attribution) {
            return Helper.createInlinePresentation(message, attribution);
        }

        @NonNull
        protected AutofillManager getAutofillManager() {
            return mContext.getSystemService(AutofillManager.class);
        }
    }

    protected static final UiBot sDefaultUiBot = new UiBot();
    protected static final UiBot sDefaultUiBot2 = new InlineUiBot();

    private AutoFillServiceTestCase() {
        throw new UnsupportedOperationException("Contain static stuff only");
    }
}
