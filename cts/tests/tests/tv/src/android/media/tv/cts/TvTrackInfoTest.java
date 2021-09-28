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

package android.media.tv.cts;

import static android.media.tv.cts.TvTrackInfoSubject.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.Context;
import android.media.tv.TvTrackInfo;
import android.os.Bundle;

import androidx.test.core.os.Parcelables;

import com.android.compatibility.common.util.RequiredServiceRule;

import org.junit.Rule;
import org.junit.Test;

/**
 * Test {@link android.media.tv.TvTrackInfo}.
 */
public class TvTrackInfoTest {

    @Rule
    public final RequiredServiceRule requiredServiceRule = new RequiredServiceRule(
            Context.TV_INPUT_SERVICE);


    @Test
    public void newAudioTrack_default() {
        final TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, "default")
                .build();
        assertThat(info).hasType(TvTrackInfo.TYPE_AUDIO);
        assertThat(info).hasId("default");
        assertThat(info).hasAudioChannelCount(0);
        assertThat(info).hasAudioSampleRate(0);
        assertThat(info).hasEncoding(null);
        assertThat(info).hasLanguage(null);
        assertThat(info).isAudioDescription(false);
        assertThat(info).isEncrypted(false);
        assertThat(info).isHardOfHearing(false);
        assertThat(info).isSpokenSubtitle(false);
        assertThat(info).extra().isNull();
        assertThat(info).hasContentDescription(0);
        assertThat(info).recreatesEqual(TvTrackInfo.CREATOR);
        TvTrackInfo copy = Parcelables.forceParcel(info, TvTrackInfo.CREATOR);
        assertThat(copy).extra().isNull();
    }

    @Test
    public void newAudioTrack_everything() {
        final Bundle bundle = new Bundle();
        final TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_AUDIO, "id_audio")
                .setAudioChannelCount(2)
                .setAudioSampleRate(48000)
                .setAudioDescription(true)
                .setEncoding("test_encoding")
                .setEncrypted(true)
                .setLanguage("eng")
                .setHardOfHearing(true)
                .setSpokenSubtitle(true)
                .setExtra(bundle)
                .build();
        assertThat(info).hasType(TvTrackInfo.TYPE_AUDIO);
        assertThat(info).hasId("id_audio");
        assertThat(info).hasAudioChannelCount(2);
        assertThat(info).hasAudioSampleRate(48000);
        assertThat(info).hasEncoding("test_encoding");
        assertThat(info).hasLanguage("eng");
        assertThat(info).isAudioDescription(true);
        assertThat(info).isEncrypted(true);
        assertThat(info).isHardOfHearing(true);
        assertThat(info).isSpokenSubtitle(true);
        assertThat(info).extra().isEmpty();
        assertThat(info).hasContentDescription(0);
        assertThat(info).recreatesEqual(TvTrackInfo.CREATOR);
        TvTrackInfo copy = Parcelables.forceParcel(info, TvTrackInfo.CREATOR);
        assertThat(copy).extra().isEmpty();
    }

    @Test
    public void newVideoTrack_default() {
        final TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "default")
                .build();
        assertThat(info).hasType(TvTrackInfo.TYPE_VIDEO);
        assertThat(info).hasId("default");
        assertThat(info).hasEncoding(null);
        assertThat(info).hasVideoWidth(0);
        assertThat(info).hasVideoHeight(0);
        assertThat(info).hasVideoFrameRate(0f);
        assertThat(info).hasVideoPixelAspectRatio(1.0f);
        assertThat(info).hasVideoActiveFormatDescription((byte) 0);
        assertThat(info).hasLanguage(null);
        assertThat(info).isEncrypted(false);
        assertThat(info).extra().isNull();
        assertThat(info).hasContentDescription(0);
        assertThat(info).recreatesEqual(TvTrackInfo.CREATOR);
        TvTrackInfo copy = Parcelables.forceParcel(info, TvTrackInfo.CREATOR);
        assertThat(copy).extra().isNull();
    }

    @Test
    public void newVideoTrack_everything() {
        final Bundle bundle = new Bundle();
        bundle.putBoolean("testTrue", true);
        final TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "id_video")
                .setEncoding("test_encoding")
                .setEncrypted(true)
                .setVideoWidth(1920)
                .setVideoHeight(1080)
                .setVideoFrameRate(29.97f)
                .setVideoPixelAspectRatio(1.0f)
                .setVideoActiveFormatDescription((byte) 8)
                .setLanguage("eng")
                .setExtra(bundle)
                .build();
        assertThat(info).hasType(TvTrackInfo.TYPE_VIDEO);
        assertThat(info).hasId("id_video");
        assertThat(info).hasEncoding("test_encoding");
        assertThat(info).hasVideoWidth(1920);
        assertThat(info).hasVideoHeight(1080);
        assertThat(info).hasVideoFrameRate(29.97f);
        assertThat(info).hasVideoPixelAspectRatio(1.0f);
        assertThat(info).hasVideoActiveFormatDescription((byte) 8);
        assertThat(info).hasLanguage("eng");
        assertThat(info).isEncrypted(true);
        assertThat(info).extra().bool("testTrue").isTrue();
        assertThat(info).hasContentDescription(0);
        assertThat(info).recreatesEqual(TvTrackInfo.CREATOR);
        TvTrackInfo copy = Parcelables.forceParcel(info, TvTrackInfo.CREATOR);
        assertThat(copy).extra().bool("testTrue").isTrue();
    }

    @Test
    public void newSubtitleTrack_default() {
        final Bundle bundle = new Bundle();
        bundle.putBoolean("testTrue", true);
        final TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_SUBTITLE, "default")
                .build();
        assertThat(info).hasType(TvTrackInfo.TYPE_SUBTITLE);
        assertThat(info).hasId("default");
        assertThat(info).hasEncoding(null);
        assertThat(info).hasLanguage(null);
        assertThat(info).isEncrypted(false);
        assertThat(info).isHardOfHearing(false);
        assertThat(info).extra().isNull();
        assertThat(info).hasContentDescription(0);
        assertThat(info).recreatesEqual(TvTrackInfo.CREATOR);
        TvTrackInfo copy = Parcelables.forceParcel(info, TvTrackInfo.CREATOR);
        assertThat(copy).extra().isNull();
    }

    @Test
    public void newSubtitleTrack_everything() {
        final Bundle bundle = new Bundle();
        bundle.putBoolean("testTrue", true);
        final TvTrackInfo info = new TvTrackInfo.Builder(TvTrackInfo.TYPE_SUBTITLE, "id_subtitle")
                .setLanguage("eng")
                .setEncoding("test_encoding")
                .setEncrypted(true)
                .setHardOfHearing(true)
                .setExtra(bundle)
                .build();
        assertThat(info).hasType(TvTrackInfo.TYPE_SUBTITLE);
        assertThat(info).hasId("id_subtitle");
        assertThat(info).hasEncoding("test_encoding");
        assertThat(info).hasLanguage("eng");
        assertThat(info).isEncrypted(true);
        assertThat(info).isHardOfHearing(true);
        assertThat(info).extra().bool("testTrue").isTrue();
        assertThat(info).hasContentDescription(0);
        assertThat(info).recreatesEqual(TvTrackInfo.CREATOR);
        TvTrackInfo copy = Parcelables.forceParcel(info, TvTrackInfo.CREATOR);
        assertThat(copy).extra().bool("testTrue").isTrue();
    }

    @Test
    public void setHardOfHearing_invalid() {
        assertThrows(
                IllegalStateException.class,
                () -> new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "invalid")
                        .setHardOfHearing(true)
        );
    }

    @Test
    public void setSpokenSubtitle() {
        assertThrows(
                IllegalStateException.class,
                () -> new TvTrackInfo.Builder(TvTrackInfo.TYPE_VIDEO, "invalid")
                        .setSpokenSubtitle(true)
        );
        assertThrows(
                IllegalStateException.class,
                () -> new TvTrackInfo.Builder(TvTrackInfo.TYPE_SUBTITLE, "invalid")
                        .setSpokenSubtitle(true)
        );
    }
}
