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

package com.android.tv.data;

import static com.google.common.truth.Truth.assertThat;

import android.media.tv.TvContract.Programs.Genres;
import android.os.Build;

import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Tests for {@link GenreItems}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class GenreItemTest {
    private static final String INVALID_GENRE = "INVALID GENRE";

    @Test
    public void testGetLabels() {
        // Checks if no exception is thrown.
        GenreItems.getLabels(RuntimeEnvironment.application);
    }

    @Test
    public void testGetCanonicalGenre() {
        int count = GenreItems.getGenreCount();
        assertThat(GenreItems.getCanonicalGenre(GenreItems.ID_ALL_CHANNELS)).isNull();
        for (int i = 1; i < count; ++i) {
            assertThat(GenreItems.getCanonicalGenre(i)).isNotNull();
        }
    }

    @Test
    public void testGetId_base() {
        int count = GenreItems.getGenreCount();
        assertThat(GenreItems.getId(null)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
        assertThat(GenreItems.getId(INVALID_GENRE)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
        assertInRange(GenreItems.getId(Genres.FAMILY_KIDS), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.SPORTS), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.SHOPPING), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.MOVIES), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.COMEDY), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.TRAVEL), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.DRAMA), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.EDUCATION), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.ANIMAL_WILDLIFE), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.NEWS), 1, count - 1);
        assertInRange(GenreItems.getId(Genres.GAMING), 1, count - 1);
    }

    @Test
    public void testGetId_lmp_mr1() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP_MR1) {
            assertThat(GenreItems.getId(Genres.ARTS)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
            assertThat(GenreItems.getId(Genres.ENTERTAINMENT))
                    .isEqualTo(GenreItems.ID_ALL_CHANNELS);
            assertThat(GenreItems.getId(Genres.LIFE_STYLE)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
            assertThat(GenreItems.getId(Genres.MUSIC)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
            assertThat(GenreItems.getId(Genres.PREMIER)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
            assertThat(GenreItems.getId(Genres.TECH_SCIENCE)).isEqualTo(GenreItems.ID_ALL_CHANNELS);
        } else {
            int count = GenreItems.getGenreCount();
            assertInRange(GenreItems.getId(Genres.ARTS), 1, count - 1);
            assertInRange(GenreItems.getId(Genres.ENTERTAINMENT), 1, count - 1);
            assertInRange(GenreItems.getId(Genres.LIFE_STYLE), 1, count - 1);
            assertInRange(GenreItems.getId(Genres.MUSIC), 1, count - 1);
            assertInRange(GenreItems.getId(Genres.PREMIER), 1, count - 1);
            assertInRange(GenreItems.getId(Genres.TECH_SCIENCE), 1, count - 1);
        }
    }

    private void assertInRange(int value, int lower, int upper) {
        assertThat(value >= lower && value <= upper).isTrue();
    }
}
