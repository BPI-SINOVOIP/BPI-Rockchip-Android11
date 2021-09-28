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
package com.android.tv.util;

import static com.android.tv.util.TvTrackInfoUtils.getBestTrackInfo;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertEquals;

import android.media.tv.TvTrackInfo;
import android.os.Build.VERSION_CODES;
import android.os.LocaleList;

import com.android.tv.testing.ComparatorTester;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/** Tests for {@link com.android.tv.util.TvTrackInfoUtils}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(minSdk = ConfigConstants.MIN_SDK, maxSdk = ConfigConstants.MAX_SDK)
public class TvTrackInfoUtilsTest {

    /** Tests for {@link TvTrackInfoUtils#getBestTrackInfo}. */
    private static final String UN_MATCHED_ID = "no matching ID";

    private final TvTrackInfo info1En1 = createTvTrackInfo("track_1", "en", 1);

    private final TvTrackInfo info2En5 = createTvTrackInfo("track_2", "en", 5);

    private final TvTrackInfo info3Fr8 = createTvTrackInfo("track_3", "fr", 8);

    private final TvTrackInfo info4Null2 = createTvTrackInfo("track_4", null, 2);

    private final TvTrackInfo info5Null6 = createTvTrackInfo("track_5", null, 6);

    private TvTrackInfo createTvTrackInfo(String trackId, String language, int audioChannelCount) {
        return new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, trackId)
                .setLanguage(language)
                .setAudioChannelCount(audioChannelCount)
                .build();
    }

    private final List<TvTrackInfo> allTracks =
            Arrays.asList(info1En1, info2En5, info3Fr8, info4Null2, info5Null6);
    private final List<TvTrackInfo> nullLanguageTracks = Arrays.asList(info4Null2, info5Null6);

    @Test
    public void testGetBestTrackInfo_empty() {
        TvTrackInfo result = getBestTrackInfo(Collections.emptyList(), UN_MATCHED_ID, "en", 1);
        assertWithMessage("best track ").that(result).isEqualTo(null);
    }

    @Test
    public void testGetBestTrackInfo_exactMatch() {
        TvTrackInfo result = getBestTrackInfo(allTracks, "track_1", "en", 1);
        assertWithMessage("best track ").that(result).isEqualTo(info1En1);
    }

    @Test
    public void testGetBestTrackInfo_langAndChannelCountMatch() {
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, "en", 5);
        assertWithMessage("best track ").that(result).isEqualTo(info2En5);
    }

    @Test
    public void testGetBestTrackInfo_languageOnlyMatch() {
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, "fr", 1);
        assertWithMessage("best track ").that(result).isEqualTo(info3Fr8);
    }

    @Test
    @Config(minSdk = ConfigConstants.MIN_SDK, maxSdk = VERSION_CODES.M)
    public void testGetBestTrackInfo_channelCountOnlyMatchWithNullLanguage_23() {
        Locale localPreference = Locale.forLanguageTag("es");
        Locale.setDefault(localPreference);
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, null, 8);
        assertWithMessage("best track ").that(result).isEqualTo(info3Fr8);
    }

    @Test
    @Config(minSdk = VERSION_CODES.N, maxSdk = ConfigConstants.MAX_SDK)
    public void testGetBestTrackInfo_channelCountOnlyMatchWithNullLanguage() {
        // Setting LoacaleList to a language which is not in the test set.
        LocaleList localPreferenceList = LocaleList.forLanguageTags("es");
        LocaleList.setDefault(localPreferenceList);
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, null, 8);
        assertWithMessage("best track ").that(result).isEqualTo(info3Fr8);
    }

    @Test
    public void testGetBestTrackInfo_noMatches() {
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, "kr", 1);
        assertWithMessage("best track ").that(result).isEqualTo(info1En1);
    }

    @Test
    @Config(minSdk = ConfigConstants.MIN_SDK, maxSdk = VERSION_CODES.M)
    public void testGetBestTrackInfo_noMatchesWithNullLanguage_23() {
        Locale localPreference = Locale.forLanguageTag("es");
        Locale.setDefault(localPreference);
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, null, 0);
        assertWithMessage("best track ").that(result).isEqualTo(info3Fr8);
    }

    @Test
    @Config(minSdk = VERSION_CODES.N, maxSdk = ConfigConstants.MAX_SDK)
    public void testGetBestTrackInfo_noMatchesWithNullLanguage() {
        // Setting LoacaleList to a language which is not in the test set.
        LocaleList localPreferenceList = LocaleList.forLanguageTags("es");
        LocaleList.setDefault(localPreferenceList);
        TvTrackInfo result = getBestTrackInfo(allTracks, UN_MATCHED_ID, null, 0);
        assertWithMessage("best track ").that(result).isEqualTo(info3Fr8);
    }

    @Test
    public void testGetBestTrackInfo_channelCountAndIdMatch() {
        TvTrackInfo result = getBestTrackInfo(nullLanguageTracks, "track_5", null, 6);
        assertWithMessage("best track ").that(result).isEqualTo(info5Null6);
    }

    @Test
    public void testComparator() {
        List<String> languages = Arrays.asList("en", "spa", "hi");
        Comparator<TvTrackInfo> comparator =
                TvTrackInfoUtils.createComparator("track_1", languages, 1);
        new ComparatorTester(comparator)
                .permitInconsistencyWithEquals()
                // lang not match
                .addEqualityGroup(
                        createTvTrackInfo("track_1", "kr", 2),
                        createTvTrackInfo("track_1", "ja", 2),
                        createTvTrackInfo("track_1", "ch", 2))
                // lang not match, better count
                .addEqualityGroup(
                        createTvTrackInfo("track_1", "kr", 3),
                        createTvTrackInfo("track_1", "ch", 3))
                // lang not match, count match
                .addEqualityGroup(
                        createTvTrackInfo("track_1", "ch", 1),
                        createTvTrackInfo("track_1", "ja", 1))
                // lang match in order of increasing priority
                .addEqualityGroup(createTvTrackInfo("track_1", "hi", 3))
                .addEqualityGroup(createTvTrackInfo("track_2", "hi", 7))
                .addEqualityGroup(createTvTrackInfo("track_1", "hi", 1))
                .addEqualityGroup(createTvTrackInfo("track_1", "spa", 5))
                .addEqualityGroup(createTvTrackInfo("track_2", "spa", 1))
                .addEqualityGroup(createTvTrackInfo("track_1", "spa", 1))
                .addEqualityGroup(createTvTrackInfo("track_2", "en", 3))
                .addEqualityGroup(
                        createTvTrackInfo("track_1", "en", 5),
                        createTvTrackInfo("track_2", "en", 5))
                // best lang match and count match
                .addEqualityGroup(
                        createTvTrackInfo("track_2", "en", 1),
                        createTvTrackInfo("track_3", "en", 1))
                // all match
                .addEqualityGroup(
                        createTvTrackInfo("track_1", "en", 1),
                        createTvTrackInfo("track_1", "en", 1))
                .testCompare();
    }

    /** Tests for {@link TvTrackInfoUtils#needToShowSampleRate}. */
    private final TvTrackInfo info6En1 = createTvTrackInfo("track_6", "en", 1);

    private final TvTrackInfo info7En0 = createTvTrackInfo("track_7", "en", 0);

    private final TvTrackInfo info8En0 = createTvTrackInfo("track_8", "en", 0);

    private List<TvTrackInfo> trackList;

    @Test
    public void needToShowSampleRate_false() {
        trackList = Arrays.asList(info1En1, info2En5);
        assertEquals(
                false,
                TvTrackInfoUtils.needToShowSampleRate(RuntimeEnvironment.application, trackList));
    }

    @Test
    public void needToShowSampleRate_sameLanguageAndChannelCount() {
        trackList = Arrays.asList(info1En1, info6En1);
        assertEquals(
                true,
                TvTrackInfoUtils.needToShowSampleRate(RuntimeEnvironment.application, trackList));
    }

    @Test
    public void needToShowSampleRate_sameLanguageNoChannelCount() {
        trackList = Arrays.asList(info7En0, info8En0);
        assertEquals(
                true,
                TvTrackInfoUtils.needToShowSampleRate(RuntimeEnvironment.application, trackList));
    }

    /** Tests for {@link TvTrackInfoUtils#getMultiAudioString}. */
    private static final String TRACK_ID = "test_track_id";

    private static final int AUDIO_SAMPLE_RATE = 48000;

    @Test
    public void testAudioTrackLanguage() {
        assertEquals(
                "Korean",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("kor"), false));
        assertEquals(
                "English",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng"), false));
        assertEquals(
                "Unknown language",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo(null), false));
        assertEquals(
                "Unknown language",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo(""), false));
        assertEquals(
                "abc",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("abc"), false));
    }

    @Test
    public void testAudioTrackCount() {
        assertEquals(
                "English",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", -1), false));
        assertEquals(
                "English",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 0), false));
        assertEquals(
                "English (mono)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 1), false));
        assertEquals(
                "English (stereo)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 2), false));
        assertEquals(
                "English (3 channels)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 3), false));
        assertEquals(
                "English (4 channels)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 4), false));
        assertEquals(
                "English (5 channels)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 5), false));
        assertEquals(
                "English (5.1 surround)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 6), false));
        assertEquals(
                "English (7 channels)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 7), false));
        assertEquals(
                "English (7.1 surround)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("eng", 8), false));
    }

    @Test
    public void testShowSampleRate() {
        assertEquals(
                "Korean (48kHz)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("kor", 0), true));
        assertEquals(
                "Korean (7.1 surround, 48kHz)",
                TvTrackInfoUtils.getMultiAudioString(
                        RuntimeEnvironment.application, createAudioTrackInfo("kor", 8), true));
    }

    private static TvTrackInfo createAudioTrackInfo(String language) {
        return createAudioTrackInfo(language, 0);
    }

    private static TvTrackInfo createAudioTrackInfo(String language, int channelCount) {
        return new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, TRACK_ID)
                .setLanguage(language)
                .setAudioChannelCount(channelCount)
                .setAudioSampleRate(AUDIO_SAMPLE_RATE)
                .build();
    }

    /** Tests for {@link TvTrackInfoUtils#toString */
    @Test
    public void toString_audioWithDetails() {
        assertEquals(
                "TvTrackInfo{type=Audio, id=track_1, language=en, "
                        + "description=test, audioChannelCount=1, audioSampleRate=5}",
                TvTrackInfoUtils.toString(
                        new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, "track_1")
                                .setLanguage("en")
                                .setAudioChannelCount(1)
                                .setDescription("test")
                                .setAudioSampleRate(5)
                                .build()));
    }

    @Test
    public void toString_audioWithDefaults() {
        assertEquals(
                "TvTrackInfo{type=Audio, id=track_2, language=null, "
                        + "description=null, audioChannelCount=0, audioSampleRate=0}",
                TvTrackInfoUtils.toString(
                        new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, "track_2").build()));
    }

    @Test
    public void toString_videoWithDetails() {
        assertEquals(
                "TvTrackInfo{type=Video, id=track_3, language=en, description=test, videoWidth=1,"
                        + " videoHeight=1, videoFrameRate=1.0, videoPixelAspectRatio=2.0}",
                TvTrackInfoUtils.toString(
                        new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "track_3")
                                .setLanguage("en")
                                .setDescription("test")
                                .setVideoWidth(1)
                                .setVideoHeight(1)
                                .setVideoFrameRate(1)
                                .setVideoPixelAspectRatio(2)
                                .build()));
    }

    @Test
    public void toString_videoWithDefaults() {
        assertEquals(
                "TvTrackInfo{type=Video, id=track_4, language=null, description=null, videoWidth=0,"
                        + " videoHeight=0, videoFrameRate=0.0, videoPixelAspectRatio=1.0}",
                TvTrackInfoUtils.toString(
                        new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "track_4").build()));
    }

    @Test
    public void toString_subtitleWithDetails() {
        assertEquals(
                "TvTrackInfo{type=Subtitle, id=track_5, language=en, description=test}",
                TvTrackInfoUtils.toString(
                        new TvTrackInfo.Builder(TvTrackInfo.TYPE_SUBTITLE, "track_5")
                                .setLanguage("en")
                                .setDescription("test")
                                .build()));
    }

    @Test
    public void toString_subtitleWithDefaults() {
        assertEquals(
                "TvTrackInfo{type=Subtitle, id=track_6, language=null, description=null}",
                TvTrackInfoUtils.toString(
                        new TvTrackInfo.Builder(TvTrackInfo.TYPE_SUBTITLE, "track_6").build()));
    }
}
