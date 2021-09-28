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

package android.voiceinteraction.cts;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.content.Context;
import android.provider.Settings;
import android.voiceinteraction.common.Utils;

import com.android.compatibility.common.util.RequiredFeatureRule;
import com.android.compatibility.common.util.SettingsStateChangerRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.runner.RunWith;

import androidx.test.ext.junit.runners.AndroidJUnit4;

/**
 * Base class for all test cases
 */
@RunWith(AndroidJUnit4.class)
abstract class AbstractVoiceInteractionTestCase {

    // TODO: use PackageManager's / make it @TestApi
    protected static final String FEATURE_VOICE_RECOGNIZERS = "android.software.voice_recognizers";

    protected final Context mContext = getInstrumentation().getTargetContext();

    @Rule
    public final RequiredFeatureRule mRequiredFeatureRule = new RequiredFeatureRule(
            FEATURE_VOICE_RECOGNIZERS);

    @Rule
    public final SettingsStateChangerRule mServiceSetterRule = new SettingsStateChangerRule(
            mContext, Settings.Secure.VOICE_INTERACTION_SERVICE, Utils.SERVICE_NAME);

    @Before
    public void prepareDevice() throws Exception {
        // Unlock screen.
        runShellCommand("input keyevent KEYCODE_WAKEUP");

        // Dismiss keyguard, in case it's set as "Swipe to unlock".
        runShellCommand("wm dismiss-keyguard");
    }
}
