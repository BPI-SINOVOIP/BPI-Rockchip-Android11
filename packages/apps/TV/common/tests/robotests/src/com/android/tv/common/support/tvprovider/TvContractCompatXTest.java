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

package com.android.tv.common.support.tvprovider;

import static com.google.common.truth.Truth.assertThat;

import android.net.Uri;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Test for {@link TvContractCompatX}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class TvContractCompatXTest {

    @Test
    public void buildChannelUri() {
        assertThat(TvContractCompatX.buildChannelUri("com.example", "foo"))
                .isEqualTo(
                        Uri.parse(
                                "content://android.media.tv/channel?"
                                        + "package=com.example&internal_provider_id=foo"));
    }

    @Test
    public void buildProgramsUriForChannel() {
        assertThat(TvContractCompatX.buildProgramsUriForChannel("com.example", "foo"))
                .isEqualTo(
                        Uri.parse(
                                "content://android.media.tv/program?"
                                        + "package=com.example&internal_provider_id=foo"));
    }

    @Test
    public void buildProgramsUriForChannel_period() {
        assertThat(TvContractCompatX.buildProgramsUriForChannel("com.example", "foo", 1234L, 5467L))
                .isEqualTo(
                        Uri.parse(
                                "content://android.media.tv/program?"
                                        + "package=com.example&internal_provider_id=foo"
                                        + "&start_time=1234&end_time=5467"));
    }

    @Test
    public void buildProgramsUri_period() {
        assertThat(TvContractCompatX.buildProgramsUri(1234L, 5467L))
                .isEqualTo(
                        Uri.parse(
                                "content://android.media.tv/program?"
                                        + "start_time=1234&end_time=5467"));
    }
}
