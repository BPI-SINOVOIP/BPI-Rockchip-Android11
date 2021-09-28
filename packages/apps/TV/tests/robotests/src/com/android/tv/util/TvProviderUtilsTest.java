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
 * limitations under the License.
 */

package com.android.tv.util;

import static com.google.common.truth.Truth.assertThat;

import android.content.pm.ProviderInfo;
import android.media.tv.TvContract;
import android.os.Bundle;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.fakes.FakeTvProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;

import java.util.Set;

/** Tests for {@link TvProviderUtils}. */
@RunWith(RobolectricTestRunner.class)
@Config(
        sdk = ConfigConstants.SDK,
        shadows = {ShadowContentResolver.class})
public class TvProviderUtilsTest {

    @Before
    public void setUp() {
        ProviderInfo info = new ProviderInfo();
        info.authority = TvContract.AUTHORITY;
        FakeTvProvider provider =
                Robolectric.buildContentProvider(FakeTvProviderForTesting.class).create(info).get();
        provider.onCreate();
        ShadowContentResolver.registerProviderInternal(TvContract.AUTHORITY, provider);
    }

    @Test
    public void testAddExtraColumnsToProjection() {
        String[] inputStrings = {"column_1", "column_2", "column_3"};
        assertThat(
                        TvProviderUtils.addExtraColumnsToProjection(
                                inputStrings, TvProviderUtils.EXTRA_PROGRAM_COLUMN_STATE))
                .asList()
                .containsExactly(
                        "column_1",
                        "column_2",
                        "column_3",
                        TvProviderUtils.EXTRA_PROGRAM_COLUMN_STATE)
                .inOrder();
    }

    @Test
    public void testAddExtraColumnsToProjection_extraColumnExists() {
        String[] inputStrings = {
            "column_1",
            "column_2",
            TvProviderUtils.EXTRA_PROGRAM_COLUMN_SERIES_ID,
            TvProviderUtils.EXTRA_PROGRAM_COLUMN_STATE,
            "column_3"
        };
        assertThat(
                        TvProviderUtils.addExtraColumnsToProjection(
                                inputStrings, TvProviderUtils.EXTRA_PROGRAM_COLUMN_STATE))
                .asList()
                .containsExactly(
                        "column_1",
                        "column_2",
                        TvProviderUtils.EXTRA_PROGRAM_COLUMN_SERIES_ID,
                        TvProviderUtils.EXTRA_PROGRAM_COLUMN_STATE,
                        "column_3")
                .inOrder();
    }

    @Test
    public void testGetExistingColumns_noException() {
        FakeTvProviderForTesting.mThrowException = false;
        FakeTvProviderForTesting.mBundleResult = new Bundle();
        FakeTvProviderForTesting.mBundleResult.putStringArray(
                TvContract.EXTRA_EXISTING_COLUMN_NAMES, new String[] {"column 1", "column 2"});
        Set<String> columns =
                TvProviderUtils.getExistingColumns(
                        RuntimeEnvironment.application, TvContract.Programs.CONTENT_URI);
        assertThat(columns).containsExactly("column 1", "column 2");
    }

    @Test
    public void testGetExistingColumns_throwsException() {
        FakeTvProviderForTesting.mThrowException = true;
        FakeTvProviderForTesting.mBundleResult = new Bundle();
        // should be no exception here
        Set<String> columns =
                TvProviderUtils.getExistingColumns(
                        RuntimeEnvironment.application, TvContract.Programs.CONTENT_URI);
        assertThat(columns).isEmpty();
    }

    private static class FakeTvProviderForTesting extends FakeTvProvider {
        private static Bundle mBundleResult;
        private static boolean mThrowException;

        @Override
        public Bundle call(String method, String arg, Bundle extras) {
            if (mThrowException) {
                throw new IllegalStateException();
            }
            return mBundleResult;
        }
    }
}
