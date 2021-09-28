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
 * limitations under the License
 */
package com.android.tv.common.dev;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Test for {@link DeveloperPreference}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class DeveloperPreferenceTest {

    @Test
    public void createBoolean_default_true() {
        DeveloperPreference<Boolean> devPref = DeveloperPreference.create("test", true);
        assertThat(devPref.get(RuntimeEnvironment.systemContext)).isTrue();
        DeveloperPreference.getPreferences(RuntimeEnvironment.systemContext)
                .edit()
                .putBoolean("test", false)
                .apply();
        assertThat(devPref.get(RuntimeEnvironment.systemContext)).isFalse();
        devPref.clear(RuntimeEnvironment.systemContext);
    }

    @Test
    public void create_integer_default_one() {
        DeveloperPreference<Integer> devPref = DeveloperPreference.create("test", 1);
        assertThat(devPref.get(RuntimeEnvironment.systemContext)).isEqualTo(1);
        DeveloperPreference.getPreferences(RuntimeEnvironment.systemContext)
                .edit()
                .putInt("test", 2)
                .apply();
        assertThat(devPref.get(RuntimeEnvironment.systemContext)).isEqualTo(2);
        devPref.clear(RuntimeEnvironment.systemContext);
    }
}
