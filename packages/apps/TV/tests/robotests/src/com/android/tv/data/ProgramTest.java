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

import static android.media.tv.TvContract.Programs.Genres.COMEDY;
import static android.media.tv.TvContract.Programs.Genres.FAMILY_KIDS;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.media.tv.TvContentRating;
import android.media.tv.TvContract.Programs.Genres;
import android.os.Parcel;

import com.android.tv.data.api.Program;
import com.android.tv.data.api.Program.CriticScore;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;

import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link ProgramImpl}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class ProgramTest {
    private static final int NOT_FOUND_GENRE = 987;

    private static final int FAMILY_GENRE_ID = GenreItems.getId(FAMILY_KIDS);

    private static final int COMEDY_GENRE_ID = GenreItems.getId(COMEDY);

    @Test
    public void testBuild() {
        Program program = new ProgramImpl.Builder().build();
        assertWithMessage("isValid").that(program.isValid()).isFalse();
    }

    @Test
    public void testNoGenres() {
        Program program = new ProgramImpl.Builder().setCanonicalGenres("").build();
        assertNullCanonicalGenres(program);
        assertHasGenre(program, NOT_FOUND_GENRE, false);
        assertHasGenre(program, FAMILY_GENRE_ID, false);
        assertHasGenre(program, COMEDY_GENRE_ID, false);
        assertHasGenre(program, GenreItems.ID_ALL_CHANNELS, true);
    }

    @Test
    public void testFamilyGenre() {
        Program program = new ProgramImpl.Builder().setCanonicalGenres(FAMILY_KIDS).build();
        assertCanonicalGenres(program, FAMILY_KIDS);
        assertHasGenre(program, NOT_FOUND_GENRE, false);
        assertHasGenre(program, FAMILY_GENRE_ID, true);
        assertHasGenre(program, COMEDY_GENRE_ID, false);
        assertHasGenre(program, GenreItems.ID_ALL_CHANNELS, true);
    }

    @Test
    public void testFamilyComedyGenre() {
        Program program =
                new ProgramImpl.Builder().setCanonicalGenres(FAMILY_KIDS + ", " + COMEDY).build();
        assertCanonicalGenres(program, FAMILY_KIDS, COMEDY);
        assertHasGenre(program, NOT_FOUND_GENRE, false);
        assertHasGenre(program, FAMILY_GENRE_ID, true);
        assertHasGenre(program, COMEDY_GENRE_ID, true);
        assertHasGenre(program, GenreItems.ID_ALL_CHANNELS, true);
    }

    @Test
    public void testOtherGenre() {
        Program program = new ProgramImpl.Builder().setCanonicalGenres("other").build();
        assertCanonicalGenres(program);
        assertHasGenre(program, NOT_FOUND_GENRE, false);
        assertHasGenre(program, FAMILY_GENRE_ID, false);
        assertHasGenre(program, COMEDY_GENRE_ID, false);
        assertHasGenre(program, GenreItems.ID_ALL_CHANNELS, true);
    }

    @Test
    public void testParcelable() {
        List<CriticScore> criticScores = new ArrayList<>();
        criticScores.add(new CriticScore("1", "2", "3"));
        criticScores.add(new CriticScore("4", "5", "6"));
        ImmutableList<TvContentRating> ratings =
                ImmutableList.of(
                        TvContentRating.unflattenFromString("1/2/3"),
                        TvContentRating.unflattenFromString("4/5/6"));
        ProgramImpl p =
                new ProgramImpl.Builder()
                        .setId(1)
                        .setPackageName("2")
                        .setChannelId(3)
                        .setTitle("4")
                        .setSeriesId("5")
                        .setEpisodeTitle("6")
                        .setSeasonNumber("7")
                        .setSeasonTitle("8")
                        .setEpisodeNumber("9")
                        .setStartTimeUtcMillis(10)
                        .setEndTimeUtcMillis(11)
                        .setDescription("12")
                        .setLongDescription("12-long")
                        .setVideoWidth(13)
                        .setVideoHeight(14)
                        .setCriticScores(criticScores)
                        .setPosterArtUri("15")
                        .setThumbnailUri("16")
                        .setCanonicalGenres(Genres.encode(Genres.SPORTS, Genres.SHOPPING))
                        .setContentRatings(ratings)
                        .setRecordingProhibited(true)
                        .build();
        Parcel p1 = Parcel.obtain();
        Parcel p2 = Parcel.obtain();
        try {
            p.writeToParcel(p1, 0);
            byte[] bytes = p1.marshall();
            p2.unmarshall(bytes, 0, bytes.length);
            p2.setDataPosition(0);
            ProgramImpl r2 = ProgramImpl.fromParcel(p2);
            assertThat(r2).isEqualTo(p);
        } finally {
            p1.recycle();
            p2.recycle();
        }
    }

    @Test
    public void testParcelableWithCriticScore() {
        ProgramImpl program =
                new ProgramImpl.Builder()
                        .setTitle("MyTitle")
                        .addCriticScore(
                                new CriticScore(
                                        "default source", "5/10", "http://testurl/testimage.jpg"))
                        .build();
        Parcel parcel = Parcel.obtain();
        program.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        Program programFromParcel = ProgramImpl.CREATOR.createFromParcel(parcel);

        assertThat(programFromParcel.getCriticScores()).isNotNull();
        assertThat(programFromParcel.getCriticScores().get(0).source).isEqualTo("default source");
        assertThat(programFromParcel.getCriticScores().get(0).score).isEqualTo("5/10");
        assertThat(programFromParcel.getCriticScores().get(0).logoUrl)
                .isEqualTo("http://testurl/testimage.jpg");
    }

    @Test
    public void getEpisodeContentDescription_blank() {
        Program program = new ProgramImpl.Builder().build();
        assertThat(program.getEpisodeContentDescription(RuntimeEnvironment.application)).isNull();
    }

    @Test
    public void getEpisodeContentDescription_seasonEpisodeAndTitle() {
        Program program =
                new ProgramImpl.Builder()
                        .setSeasonNumber("1")
                        .setEpisodeNumber("2")
                        .setEpisodeTitle("The second one")
                        .build();
        assertThat(program.getEpisodeContentDescription(RuntimeEnvironment.application))
                .isEqualTo("Season 1 Episode 2 The second one");
    }

    @Test
    public void getEpisodeContentDescription_EpisodeAndTitle() {
        Program program =
                new ProgramImpl.Builder()
                        .setEpisodeNumber("2")
                        .setEpisodeTitle("The second one")
                        .build();
        assertThat(program.getEpisodeContentDescription(RuntimeEnvironment.application))
                .isEqualTo("Episode 2 The second one");
    }

    @Test
    public void getEpisodeContentDescription_seasonEpisode() {
        Program program =
                new ProgramImpl.Builder().setSeasonNumber("1").setEpisodeNumber("2").build();
        assertThat(program.getEpisodeContentDescription(RuntimeEnvironment.application))
                .isEqualTo("Season 1 Episode 2 ");
    }

    @Test
    public void getEpisodeContentDescription_EpisodeTitle() {
        Program program = new ProgramImpl.Builder().setEpisodeTitle("The second one").build();
        assertThat(program.getEpisodeContentDescription(RuntimeEnvironment.application))
                .isEqualTo("The second one");
    }

    @Test
    public void getEpisodeDisplayTitle_blank() {
        Program program = new ProgramImpl.Builder().build();
        assertThat(program.getEpisodeDisplayTitle(RuntimeEnvironment.application)).isNull();
    }

    @Test
    public void getEpisodeDisplayTitle_seasonEpisodeAndTitle() {
        Program program =
                new ProgramImpl.Builder()
                        .setSeasonNumber("1")
                        .setEpisodeNumber("2")
                        .setEpisodeTitle("The second one")
                        .build();
        assertThat(program.getEpisodeDisplayTitle(RuntimeEnvironment.application))
                .isEqualTo("S1: Ep. 2 The second one");
    }

    @Test
    public void getEpisodeDisplayTitle_EpisodeTitle() {
        Program program =
                new ProgramImpl.Builder()
                        .setEpisodeNumber("2")
                        .setEpisodeTitle("The second one")
                        .build();
        assertThat(program.getEpisodeDisplayTitle(RuntimeEnvironment.application))
                .isEqualTo("Ep. 2 The second one");
    }

    @Test
    public void getEpisodeDisplayTitle_seasonEpisode() {
        Program program =
                new ProgramImpl.Builder().setSeasonNumber("1").setEpisodeNumber("2").build();
        assertThat(program.getEpisodeDisplayTitle(RuntimeEnvironment.application))
                .isEqualTo("S1: Ep. 2 ");
    }

    @Test
    public void getEpisodeDisplayTitle_episode() {
        Program program = new ProgramImpl.Builder().setEpisodeTitle("The second one").build();
        assertThat(program.getEpisodeDisplayTitle(RuntimeEnvironment.application))
                .isEqualTo("The second one");
    }

    private static void assertNullCanonicalGenres(Program program) {
        String[] actual = program.getCanonicalGenres();
        assertWithMessage("Expected null canonical genres but was " + Arrays.toString(actual))
                .that(actual)
                .isNull();
    }

    private static void assertCanonicalGenres(Program program, String... expected) {
        assertWithMessage("canonical genres")
                .that(Arrays.asList(program.getCanonicalGenres()))
                .isEqualTo(Arrays.asList(expected));
    }

    private static void assertHasGenre(Program program, int genreId, boolean expected) {
        assertWithMessage("hasGenre(" + genreId + ")")
                .that(program.hasGenre(genreId))
                .isEqualTo(expected);
    }
}
