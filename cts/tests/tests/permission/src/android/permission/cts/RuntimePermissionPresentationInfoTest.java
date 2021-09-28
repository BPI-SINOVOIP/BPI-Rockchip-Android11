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

package android.permission.cts;

import static com.google.common.truth.Truth.assertThat;

import android.permission.RuntimePermissionPresentationInfo;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link RuntimePermissionPresentationInfoTest}
 */
@RunWith(AndroidJUnit4.class)
public class RuntimePermissionPresentationInfoTest {
    @Test
    public void runtimePermissionLabelSet() {
        assertThat(new RuntimePermissionPresentationInfo("test", true,
                true).getLabel()).isEqualTo("test");
    }

    @Test(expected = NullPointerException.class)
    public void runtimePermissionLabelSetNull() {
        RuntimePermissionPresentationInfo info = new RuntimePermissionPresentationInfo(null, true,
                true);
    }

    @Test
    public void runtimePermissionGrantedCanBeTrue() {
        assertThat(new RuntimePermissionPresentationInfo("", true, true).isGranted()).isTrue();
    }

    @Test
    public void runtimePermissionGrantedCanBeFalse() {
        assertThat(new RuntimePermissionPresentationInfo("", false, true).isGranted()).isFalse();
    }

    @Test
    public void runtimePermissionStandardCanBeTrue() {
        assertThat(new RuntimePermissionPresentationInfo("", true, true).isStandard()).isTrue();
    }

    @Test
    public void runtimePermissionStandardCanBeFalse() {
        assertThat(new RuntimePermissionPresentationInfo("", true, false).isStandard()).isFalse();
    }
}
