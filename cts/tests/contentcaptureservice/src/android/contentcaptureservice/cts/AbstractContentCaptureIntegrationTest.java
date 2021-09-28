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
package android.contentcaptureservice.cts;

import static android.content.Context.CONTENT_CAPTURE_MANAGER_SERVICE;
import static android.contentcaptureservice.cts.Helper.MY_PACKAGE;
import static android.contentcaptureservice.cts.Helper.resetService;
import static android.contentcaptureservice.cts.Helper.sContext;
import static android.contentcaptureservice.cts.Helper.setService;
import static android.contentcaptureservice.cts.Helper.toSet;
import static android.provider.Settings.Secure.CONTENT_CAPTURE_ENABLED;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.content.ComponentName;
import android.content.ContentCaptureOptions;
import android.contentcaptureservice.cts.CtsContentCaptureService.ServiceWatcher;
import android.provider.DeviceConfig;
import android.util.Log;
import android.util.Pair;
import android.view.contentcapture.ContentCaptureManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.compatibility.common.util.DeviceConfigStateChangerRule;
import com.android.compatibility.common.util.DeviceConfigStateManager;
import com.android.compatibility.common.util.RequiredServiceRule;
import com.android.compatibility.common.util.SafeCleanerRule;
import com.android.compatibility.common.util.SettingsStateChangerRule;
import com.android.compatibility.common.util.SettingsUtils;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;

import java.util.Set;

/**
 * Base class for all (or most :-) integration tests in this CTS suite.
 */
@RunWith(AndroidJUnit4.class)
public abstract class AbstractContentCaptureIntegrationTest {

    private static final String TAG = AbstractContentCaptureIntegrationTest.class.getSimpleName();

    protected static final DeviceConfigStateManager sKillSwitchManager =
            new DeviceConfigStateManager(sContext, DeviceConfig.NAMESPACE_CONTENT_CAPTURE,
            ContentCaptureManager.DEVICE_CONFIG_PROPERTY_SERVICE_EXPLICITLY_ENABLED);

    protected final String mTag = getClass().getSimpleName();

    private final RequiredServiceRule mRequiredServiceRule =
            new RequiredServiceRule(CONTENT_CAPTURE_MANAGER_SERVICE);

    private final DeviceConfigStateChangerRule mVerboseLoggingRule =
            new DeviceConfigStateChangerRule(
            sContext, DeviceConfig.NAMESPACE_CONTENT_CAPTURE,
            ContentCaptureManager.DEVICE_CONFIG_PROPERTY_LOGGING_LEVEL,
            Integer.toString(ContentCaptureManager.LOGGING_LEVEL_VERBOSE));

    private final ContentCaptureLoggingTestRule mLoggingRule = new ContentCaptureLoggingTestRule();

    /**
     * Watcher set on {@link #enableService()} and used to wait until it's gone after the test
     * finishes.
     */
    private ServiceWatcher mServiceWatcher;

    protected final SafeCleanerRule mSafeCleanerRule = new SafeCleanerRule()
            .setDumper(mLoggingRule)
            .add(() -> {
                return CtsContentCaptureService.getExceptions();
            });

    private final TestRule mServiceDisablerRule = (base, description) -> {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    base.evaluate();
                } finally {
                    Log.v(mTag, "@mServiceDisablerRule: safelyDisableService()");
                    safelyDisableService();
                }
            }
        };
    };

    private void safelyDisableService() {
        try {
            resetService();

            if (mServiceWatcher != null) {
                mServiceWatcher.waitOnDestroy();
            }
        } catch (Throwable t) {
            Log.e(TAG, "error disabling service", t);
        }
    }

    private final DeviceConfigStateChangerRule mKillSwitchKillerRule =
            new DeviceConfigStateChangerRule(sKillSwitchManager, "true");

    private final SettingsStateChangerRule mFeatureEnablerRule = new SettingsStateChangerRule(
            sContext, CONTENT_CAPTURE_ENABLED, "1");

    @Rule
    public final RuleChain mLookAllTheseRules = RuleChain
            // mRequiredServiceRule should be first so the test can be skipped right away
            .outerRule(mRequiredServiceRule)

            // service must be disable at the last step, otherwise it's contents are not dump in
            // case of error
            .around(mServiceDisablerRule)

            // log everything
            .around(mVerboseLoggingRule)

            // enable it as soon as possible, as it have to wait for the listener
            .around(mKillSwitchKillerRule)
            .around(mFeatureEnablerRule)

            // mLoggingRule wraps the test but doesn't interfere with it
            .around(mLoggingRule)

            // mSafeCleanerRule will catch errors
            .around(mSafeCleanerRule)

            // Finally, let subclasses set their own rule
            .around(getMainTestRule());

    /**
     * Hack to make sure ContentCapture is available for the CTS test package.
     *
     * <p>It must be set here because when the application starts it queries the server, at which
     * point our service is not set yet.
     */
    // TODO: remove this hack if we ever split the CTS module in multiple APKs
    @BeforeClass
    public static void whitelistSelf() {
        final ContentCaptureOptions options = ContentCaptureOptions.forWhitelistingItself();
        Log.v(TAG, "@BeforeClass: whitelistSelf(): options=" + options);
        sContext.getApplicationContext().setContentCaptureOptions(options);
    }

    @AfterClass
    public static void unWhitelistSelf() {
        Log.v(TAG, "@afterClass: unWhitelistSelf()");
        sContext.getApplicationContext().setContentCaptureOptions(null);
    }

    @BeforeClass
    public static void disableDefaultService() {
        Log.v(TAG, "@BeforeClass: disableDefaultService()");
        Helper.setDefaultServiceEnabled(false);
    }

    @AfterClass
    public static void enableDefaultService() {
        Log.v(TAG, "@AfterClass: enableDefaultService()");
        Helper.setDefaultServiceEnabled(true);
    }

    @Before
    public void prepareDevice() throws Exception {
        Log.v(mTag, "@Before: prepareDevice()");

        // Unlock screen.
        runShellCommand("input keyevent KEYCODE_WAKEUP");

        // Dismiss keyguard, in case it's set as "Swipe to unlock".
        runShellCommand("wm dismiss-keyguard");

        // Collapse notifications.
        runShellCommand("cmd statusbar collapse");
    }

    @Before
    public void clearState() {
        Log.v(mTag, "@Before: clearState()");
        CtsContentCaptureService.resetStaticState();
    }

    @After
    public void clearServiceWatcher() {
        Log.v(mTag, "@After: clearServiceWatcher()");
        CtsContentCaptureService.clearServiceWatcher();
    }

    @Nullable
    public static void setFeatureEnabledBySettings(@Nullable boolean enabled) {
        SettingsUtils.syncSet(sContext, CONTENT_CAPTURE_ENABLED, enabled ? "1" : "0");
    }

    /**
     * Sets {@link CtsContentCaptureService} as the service for the current user and waits until
     * its created, then whitelist the CTS test package.
     */
    public CtsContentCaptureService enableService() throws InterruptedException {
        return enableService(toSet(MY_PACKAGE), /* whitelistedComponents= */ null);
    }

    public CtsContentCaptureService enableService(@Nullable Set<String> whitelistedPackages,
            @Nullable Set<ComponentName> whitelistedComponents) throws InterruptedException {
        return enableService(new Pair<>(whitelistedPackages, whitelistedComponents));
    }

    public CtsContentCaptureService enableService(
            @Nullable Pair<Set<String>, Set<ComponentName>> whitelist) throws InterruptedException {
        if (mServiceWatcher != null) {
            throw new IllegalStateException("There Can Be Only One!");
        }
        mServiceWatcher = CtsContentCaptureService.setServiceWatcher();
        setService(CtsContentCaptureService.SERVICE_NAME);

        mServiceWatcher.whitelist(whitelist);

        return mServiceWatcher.waitOnCreate();
    }

    /**
     * Gets the test-specific {@link Rule}.
     */
    @NonNull
    protected abstract TestRule getMainTestRule();
}
