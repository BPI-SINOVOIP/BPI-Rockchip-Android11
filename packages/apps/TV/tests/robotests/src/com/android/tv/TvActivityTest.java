/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv;

import static androidx.test.ext.truth.content.IntentSubject.assertThat;

import android.content.Intent;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.util.Utils;

import com.google.common.truth.Truth;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

/** Test for {@link TvActivity}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class TvActivityTest {

    @Test
    public void testLifeCycle() {
        TvActivity activity = Robolectric.setupActivity(TvActivity.class);
        Truth.assertThat(activity.isFinishing()).isTrue();

        Intent nextStartedActivity = ShadowApplication.getInstance().getNextStartedActivity();
        assertThat(nextStartedActivity).hasComponentClass(MainActivity.class);
        assertThat(nextStartedActivity).extras().bool(Utils.EXTRA_KEY_FROM_LAUNCHER).isTrue();
    }
}
