/**
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package android.accessibilityservice.cts;

import static android.content.Context.AUDIO_SERVICE;

import static org.junit.Assert.assertEquals;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.app.Instrumentation;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.Presubmit;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Verify that accessibility services can control the accessibility volume.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityVolumeTest {
    Instrumentation mInstrumentation;
    AudioManager mAudioManager;
    // If a platform collects all volumes into one, these tests aren't relevant
    boolean mSingleVolume;
    // If a11y volume is stuck at a single value, don't run the tests
    boolean mFixedA11yVolume;

    private InstrumentedAccessibilityServiceTestRule<InstrumentedAccessibilityService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    InstrumentedAccessibilityService.class, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    @Before
    public void setUp() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mAudioManager =
                (AudioManager) mInstrumentation.getContext().getSystemService(AUDIO_SERVICE);
        // TVs and fixed volume devices have a single volume
        PackageManager pm = mInstrumentation.getContext().getPackageManager();
        mSingleVolume = (pm != null) && (pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
                || pm.hasSystemFeature(PackageManager.FEATURE_TELEVISION))
                || mAudioManager.isVolumeFixed();
        final int MIN = mAudioManager.getStreamMinVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int MAX = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_ACCESSIBILITY);
        mFixedA11yVolume = (MIN == MAX);
    }

    @Test
    @Presubmit
    public void testChangeAccessibilityVolume_outsideValidAccessibilityService_shouldFail() {
        if (mSingleVolume || mFixedA11yVolume) {
            return;
        }
        final int startingVolume = mAudioManager.getStreamVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int MIN = mAudioManager.getStreamMinVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int MAX = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int otherVolume = (startingVolume == MIN) ? MAX : MIN;
        mAudioManager.setStreamVolume(AudioManager.STREAM_ACCESSIBILITY, otherVolume, 0);
        assertEquals("Non accessibility service should not be able to change accessibility volume",
                startingVolume, mAudioManager.getStreamVolume(AudioManager.STREAM_ACCESSIBILITY));
    }

    @Test
    @AppModeFull
    public void testChangeAccessibilityVolume_inAccessibilityService_shouldWork() {
        if (mSingleVolume || mFixedA11yVolume) {
            return;
        }
        final int startingVolume = mAudioManager.getStreamVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int MIN = mAudioManager.getStreamMinVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int MAX = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_ACCESSIBILITY);
        final int otherVolume = (startingVolume == MIN) ? MAX : MIN;
        final InstrumentedAccessibilityService service = mServiceRule.enableService();

        service.runOnServiceSync(() -> mAudioManager.setStreamVolume(
                AudioManager.STREAM_ACCESSIBILITY, otherVolume, 0));
        assertEquals("Accessibility service should be able to change accessibility volume",
                otherVolume, mAudioManager.getStreamVolume(AudioManager.STREAM_ACCESSIBILITY));
        service.runOnServiceSync(() -> mAudioManager.setStreamVolume(
                AudioManager.STREAM_ACCESSIBILITY, startingVolume, 0));
    }
}
