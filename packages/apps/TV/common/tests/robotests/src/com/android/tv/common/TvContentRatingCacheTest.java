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

package com.android.tv.common;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.content.ComponentCallbacks2;
import android.media.tv.TvContentRating;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.constants.TvContentRatingConstants;

import com.google.common.collect.ImmutableList;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Test for {@link TvContentRatingCache}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class TvContentRatingCacheTest {
    /** US_TV_MA and US_TV_Y7 in order */
    public static final String MA_AND_Y7 =
            TvContentRatingConstants.STRING_US_TV_MA
                    + ","
                    + TvContentRatingConstants.STRING_US_TV_Y7_US_TV_FV;

    /** US_TV_MA and US_TV_Y7 not in order */
    public static final String Y7_AND_MA =
            TvContentRatingConstants.STRING_US_TV_Y7_US_TV_FV
                    + ","
                    + TvContentRatingConstants.STRING_US_TV_MA;

    final TvContentRatingCache mCache = TvContentRatingCache.getInstance();

    @Before
    public void setUp() {
        mCache.performTrimMemory(ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
    }

    @After
    public void tearDown() {
        mCache.performTrimMemory(ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
    }

    @Test
    public void testGetRatings_US_TV_MA() {
        ImmutableList<TvContentRating> result =
                mCache.getRatings(TvContentRatingConstants.STRING_US_TV_MA);
        assertThat(result).contains(TvContentRatingConstants.CONTENT_RATING_US_TV_MA);
    }

    @Test
    public void testGetRatings_US_TV_MA_same() {
        ImmutableList<TvContentRating> first =
                mCache.getRatings(TvContentRatingConstants.STRING_US_TV_MA);
        ImmutableList<TvContentRating> second =
                mCache.getRatings(TvContentRatingConstants.STRING_US_TV_MA);
        assertThat(first).isSameInstanceAs(second);
    }

    @Test
    public void testGetRatings_US_TV_MA_diffAfterClear() {
        ImmutableList<TvContentRating> first =
                mCache.getRatings(TvContentRatingConstants.STRING_US_TV_MA);
        mCache.performTrimMemory(ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
        ImmutableList<TvContentRating> second =
                mCache.getRatings(TvContentRatingConstants.STRING_US_TV_MA);
        assertThat(first).isNotSameInstanceAs(second);
    }

    @Test
    public void testGetRatings_TWO_orderDoesNotMatter() {
        ImmutableList<TvContentRating> first = mCache.getRatings(MA_AND_Y7);
        ImmutableList<TvContentRating> second = mCache.getRatings(Y7_AND_MA);
        assertThat(first).isSameInstanceAs(second);
    }

    @Test
    public void testContentRatingsToString_null() {
        String result = TvContentRatingCache.contentRatingsToString(null);
        assertWithMessage("ratings string").that(result).isNull();
    }

    @Test
    public void testContentRatingsToString_none() {
        String result = TvContentRatingCache.contentRatingsToString(ImmutableList.of());
        assertWithMessage("ratings string").that(result).isEmpty();
    }

    @Test
    public void testContentRatingsToString_one() {
        String result =
                TvContentRatingCache.contentRatingsToString(
                        ImmutableList.of(TvContentRatingConstants.CONTENT_RATING_US_TV_MA));
        assertWithMessage("ratings string")
                .that(result)
                .isEqualTo(TvContentRatingConstants.STRING_US_TV_MA);
    }

    @Test
    public void testContentRatingsToString_twoInOrder() {
        String result =
                TvContentRatingCache.contentRatingsToString(
                        ImmutableList.of(
                                TvContentRatingConstants.CONTENT_RATING_US_TV_MA,
                                TvContentRatingConstants.CONTENT_RATING_US_TV_Y7_US_TV_FV));
        assertWithMessage("ratings string").that(result).isEqualTo(MA_AND_Y7);
    }

    @Test
    public void testContentRatingsToString_twoNotInOrder() {
        String result =
                TvContentRatingCache.contentRatingsToString(
                        ImmutableList.of(
                                TvContentRatingConstants.CONTENT_RATING_US_TV_Y7_US_TV_FV,
                                TvContentRatingConstants.CONTENT_RATING_US_TV_MA));
        assertWithMessage("ratings string").that(result).isEqualTo(MA_AND_Y7);
    }

    @Test
    public void testContentRatingsToString_double() {
        String result =
                TvContentRatingCache.contentRatingsToString(
                        ImmutableList.of(
                                TvContentRatingConstants.CONTENT_RATING_US_TV_MA,
                                TvContentRatingConstants.CONTENT_RATING_US_TV_MA));
        assertWithMessage("ratings string")
                .that(result)
                .isEqualTo(TvContentRatingConstants.STRING_US_TV_MA);
    }

    @Test
    public void testStringToContentRatings_null() {
        assertThat(TvContentRatingCache.stringToContentRatings(null)).isEmpty();
    }

    @Test
    public void testStringToContentRatings_none() {
        assertThat(TvContentRatingCache.stringToContentRatings("")).isEmpty();
    }

    @Test
    public void testStringToContentRatings_bad() {
        assertThat(TvContentRatingCache.stringToContentRatings("bad")).isEmpty();
    }

    @Test
    public void testStringToContentRatings_oneGoodOneBad() {
        ImmutableList<TvContentRating> results =
                TvContentRatingCache.stringToContentRatings(
                        TvContentRatingConstants.STRING_US_TV_Y7_US_TV_FV + ",bad");
        assertWithMessage("ratings")
                .that(results)
                .containsExactly(TvContentRatingConstants.CONTENT_RATING_US_TV_Y7_US_TV_FV);
    }

    @Test
    public void testStringToContentRatings_one() {
        ImmutableList<TvContentRating> results =
                TvContentRatingCache.stringToContentRatings(
                        TvContentRatingConstants.STRING_US_TV_Y7_US_TV_FV);
        assertWithMessage("ratings")
                .that(results)
                .containsExactly(TvContentRatingConstants.CONTENT_RATING_US_TV_Y7_US_TV_FV);
    }

    @Test
    public void testStringToContentRatings_twoNotInOrder() {
        ImmutableList<TvContentRating> results =
                TvContentRatingCache.stringToContentRatings(Y7_AND_MA);
        assertWithMessage("ratings")
                .that(results)
                .containsExactly(
                        TvContentRatingConstants.CONTENT_RATING_US_TV_MA,
                        TvContentRatingConstants.CONTENT_RATING_US_TV_Y7_US_TV_FV);
    }

    @Test
    public void testStringToContentRatings_twoInOrder() {
        ImmutableList<TvContentRating> results =
                TvContentRatingCache.stringToContentRatings(MA_AND_Y7);
        assertWithMessage("ratings")
                .that(results)
                .containsExactly(
                        TvContentRatingConstants.CONTENT_RATING_US_TV_MA,
                        TvContentRatingConstants.CONTENT_RATING_US_TV_Y7_US_TV_FV);
    }

    @Test
    public void testStringToContentRatings_double() {
        ImmutableList<TvContentRating> results =
                TvContentRatingCache.stringToContentRatings(
                        TvContentRatingConstants.STRING_US_TV_MA
                                + ","
                                + TvContentRatingConstants.STRING_US_TV_MA);
        assertWithMessage("ratings")
                .that(results)
                .containsExactly((TvContentRatingConstants.CONTENT_RATING_US_TV_MA));

        assertThat(results).containsExactly(TvContentRatingConstants.CONTENT_RATING_US_TV_MA);
    }
}
