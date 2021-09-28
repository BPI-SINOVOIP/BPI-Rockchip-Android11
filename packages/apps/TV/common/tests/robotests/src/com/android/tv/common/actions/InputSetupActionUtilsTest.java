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
 * limitations under the License
 */
package com.android.tv.common.actions;

import static androidx.test.ext.truth.os.BundleSubject.assertThat;

import static com.google.common.truth.Truth.assertThat;

import android.content.Intent;
import android.os.Bundle;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link InputSetupActionUtils} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class InputSetupActionUtilsTest {

    @Test
    public void hasInputSetupAction_launchInputSetup() {
        Intent intent = new Intent("com.android.tv.action.LAUNCH_INPUT_SETUP");
        assertThat(InputSetupActionUtils.hasInputSetupAction(intent)).isTrue();
    }

    @Test
    public void hasInputSetupAction_googleLaunchInputSetup() {
        Intent intent = new Intent("com.google.android.tv.action.LAUNCH_INPUT_SETUP");
        assertThat(InputSetupActionUtils.hasInputSetupAction(intent)).isTrue();
    }

    @Test
    public void hasInputSetupAction_bad() {
        Intent intent = new Intent("com.example.action.LAUNCH_INPUT_SETUP");
        assertThat(InputSetupActionUtils.hasInputSetupAction(intent)).isFalse();
    }

    @Test
    public void getExtraActivityAfter_null() {
        Intent intent = new Intent();
        assertThat(InputSetupActionUtils.getExtraActivityAfter(intent)).isNull();
    }

    @Test
    public void getExtraActivityAfter_activityAfter() {
        Intent intent = new Intent();
        Intent after = new Intent("after");
        intent.putExtra("com.android.tv.intent.extra.ACTIVITY_AFTER_COMPLETION", after);
        assertThat(InputSetupActionUtils.getExtraActivityAfter(intent)).isEqualTo(after);
    }

    @Test
    public void getExtraActivityAfter_googleActivityAfter() {
        Intent intent = new Intent();
        Intent after = new Intent("google_setup");
        intent.putExtra("com.google.android.tv.intent.extra.ACTIVITY_AFTER_COMPLETION", after);
        assertThat(InputSetupActionUtils.getExtraActivityAfter(intent)).isEqualTo(after);
    }

    @Test
    public void getExtraSetupIntent_null() {
        Intent intent = new Intent();
        assertThat(InputSetupActionUtils.getExtraSetupIntent(intent)).isNull();
    }

    @Test
    public void getExtraSetupIntent_setupIntent() {
        Intent intent = new Intent();
        Intent setup = new Intent("setup");
        intent.putExtra("com.android.tv.extra.SETUP_INTENT", setup);
        assertThat(InputSetupActionUtils.getExtraSetupIntent(intent)).isEqualTo(setup);
    }

    @Test
    public void getExtraSetupIntent_googleSetupIntent() {
        Intent intent = new Intent();
        Intent setup = new Intent("google_setup");
        intent.putExtra("com.google.android.tv.extra.SETUP_INTENT", setup);
        assertThat(InputSetupActionUtils.getExtraSetupIntent(intent)).isEqualTo(setup);
    }

    @Test
    public void removeSetupIntent_empty() {
        Bundle extras = new Bundle();
        Bundle expected = new Bundle();
        InputSetupActionUtils.removeSetupIntent(extras);
        assertThat(extras.toString()).isEqualTo(expected.toString());
    }

    @Test
    public void removeSetupIntent_other() {
        Bundle extras = createTestBundle();
        Bundle expected = createTestBundle();
        InputSetupActionUtils.removeSetupIntent(extras);
        assertThat(extras.toString()).isEqualTo(expected.toString());
    }

    @Test
    public void removeSetupIntent_setup() {
        Bundle extras = createTestBundle();
        Intent setup = new Intent("setup");
        extras.putParcelable("com.android.tv.extra.SETUP_INTENT", setup);
        InputSetupActionUtils.removeSetupIntent(extras);
        assertThat(extras).doesNotContainKey("com.android.tv.extra.SETUP_INTENT");
    }

    @Test
    public void removeSetupIntent_googleSetup() {
        Bundle extras = createTestBundle();
        Intent googleSetup = new Intent("googleSetup");
        extras.putParcelable("com.google.android.tv.extra.SETUP_INTENT", googleSetup);
        InputSetupActionUtils.removeSetupIntent(extras);
        assertThat(extras).doesNotContainKey("com.google.android.tv.extra.SETUP_INTENT");
    }

    @Test
    public void removeSetupIntent_bothSetups() {
        Bundle extras = createTestBundle();
        Intent setup = new Intent("setup");
        extras.putParcelable("com.android.tv.extra.SETUP_INTENT", setup);
        Intent googleSetup = new Intent("googleSetup");
        extras.putParcelable("com.google.android.tv.extra.SETUP_INTENT", googleSetup);
        InputSetupActionUtils.removeSetupIntent(extras);
        assertThat(extras).doesNotContainKey("com.android.tv.extra.SETUP_INTENT");
        assertThat(extras).doesNotContainKey("com.google.android.tv.extra.SETUP_INTENT");
    }

    private static Bundle createTestBundle() {
        Bundle extras = new Bundle();
        extras.putInt("other", 1);
        return extras;
    }
}
