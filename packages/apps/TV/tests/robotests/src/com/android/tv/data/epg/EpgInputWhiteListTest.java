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

package com.android.tv.data.epg;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.common.flags.impl.DefaultCloudEpgFlags;
import com.android.tv.common.flags.impl.DefaultLegacyFlags;
import com.android.tv.features.TvFeatures;
import com.android.tv.testing.constants.ConfigConstants;

import com.android.tv.common.flags.proto.TypedFeatures.StringListParam;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link EpgInputWhiteList}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class EpgInputWhiteListTest {

    private EpgInputWhiteList mWhiteList;
    private DefaultCloudEpgFlags mCloudEpgFlags;
    private DefaultLegacyFlags mLegacyFlags;

    @Before
    public void setup() {
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.enableForTest();
        mCloudEpgFlags = new DefaultCloudEpgFlags();
        mLegacyFlags = DefaultLegacyFlags.DEFAULT;
        mWhiteList = new EpgInputWhiteList(mCloudEpgFlags, mLegacyFlags);
    }

    @After
    public void after() {
        TvFeatures.CLOUD_EPG_FOR_3RD_PARTY.resetForTests();
    }

    @Test
    public void isInputWhiteListed_noRemoteConfig() {
        assertThat(mWhiteList.isInputWhiteListed("com.example/.Foo")).isFalse();
    }

    @Test
    public void isInputWhiteListed_noMatch() {
        mCloudEpgFlags.setThirdPartyEpgInput(asStringListParam("com.example/.Bar"));
        assertThat(mWhiteList.isInputWhiteListed("com.example/.Foo")).isFalse();
    }

    @Test
    public void isInputWhiteListed_match() {
        mCloudEpgFlags.setThirdPartyEpgInput(asStringListParam("com.example/.Foo"));
        assertThat(mWhiteList.isInputWhiteListed("com.example/.Foo")).isTrue();
    }

    @Test
    public void isInputWhiteListed_matchWithTwo() {
        mCloudEpgFlags.setThirdPartyEpgInput(
                asStringListParam("com.example/.Foo", "com.example/.Bar"));
        assertThat(mWhiteList.isInputWhiteListed("com.example/.Foo")).isTrue();
    }

    @Test
    public void isPackageWhiteListed_noRemoteConfig() {
        assertThat(mWhiteList.isPackageWhiteListed("com.example")).isFalse();
    }

    @Test
    public void isPackageWhiteListed_noMatch() {
        mCloudEpgFlags.setThirdPartyEpgInput(asStringListParam("com.example/.Bar"));
        assertThat(mWhiteList.isPackageWhiteListed("com.other")).isFalse();
    }

    @Test
    public void isPackageWhiteListed_match() {
        mCloudEpgFlags.setThirdPartyEpgInput(asStringListParam("com.example/.Foo"));
        assertThat(mWhiteList.isPackageWhiteListed("com.example")).isTrue();
    }

    @Test
    public void isPackageWhiteListed_matchWithTwo() {
        mCloudEpgFlags.setThirdPartyEpgInput(
                asStringListParam("com.example/.Foo", "com.example/.Bar"));
        assertThat(mWhiteList.isPackageWhiteListed("com.example")).isTrue();
    }

    @Test
    public void isPackageWhiteListed_matchBadInput() {
        mCloudEpgFlags.setThirdPartyEpgInput(asStringListParam("com.example.Foo"));
        assertThat(mWhiteList.isPackageWhiteListed("com.example")).isFalse();
    }

    @Test
    public void isPackageWhiteListed_tunerInput() {
        EpgInputWhiteList whiteList =
                new EpgInputWhiteList(new DefaultCloudEpgFlags(), DefaultLegacyFlags.DEFAULT);
        assertThat(
                        whiteList.isInputWhiteListed(
                                "com.google.android.tv/.tuner.tvinput.TunerTvInputService"))
                .isTrue();
    }

    private static StringListParam asStringListParam(String... args) {
        List<String> list = Arrays.asList(args);
        return StringListParam.newBuilder().addAllElement(list).build();
    }
}
