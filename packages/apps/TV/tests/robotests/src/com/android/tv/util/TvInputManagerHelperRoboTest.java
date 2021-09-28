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
 * limitations under the License
 */

package com.android.tv.util;

import static com.google.common.truth.Truth.assertThat;

import android.media.tv.TvInputInfo;
import android.media.tv.TvInputManager;

import com.android.tv.common.flags.impl.DefaultLegacyFlags;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/**
 * Tests for {@link TvInputManagerHelper}.
 *
 * <p>This test is named ...RoboTest because there is already a test named <code>
 * TvInputManagerHelperTest</code>
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class TvInputManagerHelperRoboTest {

    @Test
    public void getInputState_null() {
        TvInputInfo tvinputInfo = null;
        TvInputManagerHelper tvInputManagerHelper =
                new TvInputManagerHelper(
                        RuntimeEnvironment.application, DefaultLegacyFlags.DEFAULT);
        assertThat(TvInputManager.INPUT_STATE_DISCONNECTED)
                .isSameInstanceAs(tvInputManagerHelper.getInputState(tvinputInfo));
    }
}
