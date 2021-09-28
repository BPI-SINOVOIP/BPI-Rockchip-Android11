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
 * limitations under the License
 */

package android.voicerecognition.cts;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.Manifest;
import android.app.compat.CompatChanges;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Process;
import android.os.SystemClock;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class RecognitionServiceMicIndicatorTest {

    private final String TAG = "RecognitionServiceMicIndicatorTest";
    // same as Settings.Secure.VOICE_RECOGNITION_SERVICE
    private final String VOICE_RECOGNITION_SERVICE = "voice_recognition_service";
    // same as Settings.Secure.VOICE_INTERACTION_SERVICE
    private final String VOICE_INTERACTION_SERVICE = "voice_interaction_service";
    // Th notification privacy indicator
    private final String PRIVACY_CHIP_PACLAGE_NAME = "com.android.systemui";
    private final String PRIVACY_CHIP_ID = "privacy_chip";
    // The cts app label
    private final String APP_LABEL = "CtsVoiceRecognitionTestCases";
    // A simple test voice recognition service implementation
    private final String CTS_VOICE_RECOGNITION_SERVICE =
            "android.recognitionservice.service/android.recognitionservice.service"
                    + ".CtsVoiceRecognitionService";
    private final String INDICATORS_FLAG = "camera_mic_icons_enabled";
    private final long INDICATOR_DISMISS_TIMEOUT = 5000L;
    private final long UI_WAIT_TIMEOUT = 1000L;
    private final long PERMISSION_INDICATORS_NOT_PRESENT = 162547999L;

    private UiDevice mUiDevice;
    private SpeechRecognitionActivity mActivity;
    private Context mContext;
    private String mOriginalVoiceRecognizer;
    private String mCameraLabel;
    private boolean mOriginalIndicatorsEnabledState;
    private boolean mTestRunnung;

    @Rule
    public ActivityTestRule<SpeechRecognitionActivity> mActivityTestRule =
            new ActivityTestRule<>(SpeechRecognitionActivity.class);

    @Before
    public void setup() {
        // If the change Id is not present, then isChangeEnabled will return true. To bypass this,
        // the change is set to "false" if present.
        assumeFalse("feature not present on this device", runWithShellPermissionIdentity(
                () -> CompatChanges.isChangeEnabled(PERMISSION_INDICATORS_NOT_PRESENT,
                        Process.SYSTEM_UID)));
        final PackageManager pm = InstrumentationRegistry.getTargetContext().getPackageManager();
        boolean hasTvFeature = pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK);
        assumeFalse("Not run in the tv device", hasTvFeature);
        mTestRunnung = true;
        prepareDevice();
        mContext = InstrumentationRegistry.getTargetContext();
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mActivity = mActivityTestRule.getActivity();

        try {
            mCameraLabel = pm.getPermissionGroupInfo(Manifest.permission_group.CAMERA, 0).loadLabel(
                    pm).toString();
        } catch (PackageManager.NameNotFoundException e) {
        }
        // get original indicator enable state
        runWithShellPermissionIdentity(() -> {
            mOriginalIndicatorsEnabledState =
                    DeviceConfig.getBoolean(DeviceConfig.NAMESPACE_PRIVACY, INDICATORS_FLAG, false);
        });
        // get original voice services
        mOriginalVoiceRecognizer = Settings.Secure.getString(
                mContext.getContentResolver(), VOICE_RECOGNITION_SERVICE);
        // QPR is default disabled, we need to enable it
        setIndicatorsEnabledStateIfNeeded(/* shouldBeEnabled */ true);
    }

    @After
    public void teardown() {
        if (!mTestRunnung) {
            return;
        }
        // press back to close the dialog
        mUiDevice.pressBack();
        // restore to original voice services
        setCurrentRecognizer(mOriginalVoiceRecognizer);
        // restore to original indicator enable state
        setIndicatorsEnabledStateIfNeeded(mOriginalIndicatorsEnabledState);
    }

    private void prepareDevice() {
        // Unlock screen.
        runShellCommand("input keyevent KEYCODE_WAKEUP");
        // Dismiss keyguard, in case it's set as "Swipe to unlock".
        runShellCommand("wm dismiss-keyguard");
    }

    private void setIndicatorsEnabledStateIfNeeded(Boolean shouldBeEnabled) {
        runWithShellPermissionIdentity(() -> {
            final boolean currentlyEnabled = DeviceConfig.getBoolean(DeviceConfig.NAMESPACE_PRIVACY,
                    INDICATORS_FLAG, false);
            if (currentlyEnabled != shouldBeEnabled) {
                DeviceConfig.setProperty(DeviceConfig.NAMESPACE_PRIVACY, INDICATORS_FLAG,
                        shouldBeEnabled.toString(), false);
            }
        });
    }

    private void setCurrentRecognizer(String recognizer) {
        runWithShellPermissionIdentity(
                () -> Settings.Secure.putString(mContext.getContentResolver(),
                        VOICE_RECOGNITION_SERVICE, recognizer));
        mUiDevice.waitForIdle();
    }

    private boolean hasPreInstalledRecognizer(String packageName) {
        Log.v(TAG, "hasPreInstalledRecognizer package=" + packageName);
        try {
            final PackageManager pm = mContext.getPackageManager();
            final ApplicationInfo info = pm.getApplicationInfo(packageName, 0);
            return ((info.flags & ApplicationInfo.FLAG_SYSTEM) != 0);
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private static String getComponentPackageNameFromString(String from) {
        ComponentName componentName = from != null ? ComponentName.unflattenFromString(from) : null;
        return componentName != null ? componentName.getPackageName() : "";
    }

    @Test
    public void testNonTrustedRecognitionServiceCannotBlameCallingApp() throws Throwable {
        // This is a workaound solution for R QPR. We treat trusted if the current voice recognizer
        // is also a preinstalled app. This is a untrusted case.
        setCurrentRecognizer(CTS_VOICE_RECOGNITION_SERVICE);

        // verify that the untrusted app cannot blame the calling app mic access
        testVoiceRecognitionServiceBlameCallingApp(/* trustVoiceService */ false);
    }

    @Test
    public void testTrustedRecognitionServiceCanBlameCallingApp() throws Throwable {
        // This is a workaound solution for R QPR. We treat trusted if the current voice recognizer
        // is also a preinstalled app. This is a trusted case.
        boolean hasPreInstalledRecognizer = hasPreInstalledRecognizer(
                getComponentPackageNameFromString(mOriginalVoiceRecognizer));
        assumeTrue("No preinstalled recognizer.", hasPreInstalledRecognizer);

        // verify that the trusted app can blame the calling app mic access
        testVoiceRecognitionServiceBlameCallingApp(/* trustVoiceService */ true);
    }

    private void testVoiceRecognitionServiceBlameCallingApp(boolean trustVoiceService)
            throws Throwable {
        // Start SpeechRecognition
        mActivity.startListening();

        assertPrivacyChipAndIndicatorsPresent(trustVoiceService);
    }

    private void assertPrivacyChipAndIndicatorsPresent(boolean trustVoiceService) {
        // Open notification and verify the privacy indicator is shown
        mUiDevice.openNotification();
        SystemClock.sleep(UI_WAIT_TIMEOUT);

        final UiObject2 privacyChip =
                mUiDevice.findObject(By.res(PRIVACY_CHIP_PACLAGE_NAME, PRIVACY_CHIP_ID));
        assertWithMessage("Can not find mic indicator").that(privacyChip).isNotNull();

        // Click the privacy indicator and verify the calling app name display status in the dialog.
        privacyChip.click();
        SystemClock.sleep(UI_WAIT_TIMEOUT);

        final UiObject2 recognitionCallingAppLabel = mUiDevice.findObject(By.text(APP_LABEL));
        if (trustVoiceService) {
            // Check trust recognizer can blame calling app mic permission
            assertWithMessage(
                    "Trusted voice recognition service can blame the calling app name " + APP_LABEL
                            + ", but does not find it.").that(
                    recognitionCallingAppLabel).isNotNull();
            assertThat(recognitionCallingAppLabel.getText()).isEqualTo(APP_LABEL);

            // Check trust recognizer cannot blame non-mic permission
            final UiObject2 cemaraLabel = mUiDevice.findObject(By.text(mCameraLabel));
            assertWithMessage("Trusted voice recognition service cannot blame non-mic permission")
                    .that(cemaraLabel).isNull();
        } else {
            assertWithMessage(
                    "Untrusted voice recognition service cannot blame the calling app name "
                            + APP_LABEL).that(recognitionCallingAppLabel).isNull();
        }
        // Wait for the privacy indicator to disappear to avoid the test becoming flaky.
        SystemClock.sleep(INDICATOR_DISMISS_TIMEOUT);
    }
}
