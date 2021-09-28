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

package android.media.tv.cts;

import static com.google.common.truth.Truth.assertAbout;

import android.annotation.Nullable;
import android.media.tv.TvTrackInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Parcelable;

import androidx.annotation.RequiresApi;
import androidx.test.ext.truth.os.BundleSubject;
import androidx.test.ext.truth.os.ParcelableSubject;

import com.google.common.truth.FailureMetadata;
import com.google.common.truth.SimpleSubjectBuilder;
import com.google.common.truth.Subject;

/** Test {@link Subject} for {@link TvTrackInfo}. */
public final class TvTrackInfoSubject extends Subject<TvTrackInfoSubject, TvTrackInfo> {
    // TODO(b/153190774): Move to androidx.test.ext.truth

    @Nullable
    private final TvTrackInfo actual;

    private TvTrackInfoSubject(FailureMetadata metadata, @Nullable TvTrackInfo actual) {
        super(metadata, actual);
        this.actual = actual;
    }

    public static TvTrackInfoSubject assertThat(@Nullable TvTrackInfo actual) {
        return assertAbout(tvTrackInfos()).that(actual);
    }

    public static Factory<TvTrackInfoSubject, TvTrackInfo> tvTrackInfos() {
        return TvTrackInfoSubject::new;
    }

    public void hasAudioChannelCount(int id) {
        check("getAudioChannelCount()").that(actual.getAudioChannelCount()).isEqualTo(id);
    }

    public void hasAudioSampleRate(int rate) {
        check("getAudioSampleRate()").that(actual.getAudioSampleRate()).isEqualTo(rate);
    }

    public void hasContentDescription(int content) {
        check("describeContents()").that(actual.describeContents()).isEqualTo(content);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    public void hasEncoding(String encoding) {
        check("getEncoding()").that(actual.getEncoding()).isEqualTo(encoding);
    }

    public void hasId(String id) {
        check("getId()").that(actual.getId()).isEqualTo(id);
    }

    public void hasLanguage(String language) {
        check("getLanguage()").that(actual.getLanguage()).isEqualTo(language);
    }

    public void hasType(int type) {
        check("getType()").that(actual.getType()).isEqualTo(type);
    }

    public void hasVideoActiveFormatDescription(byte format) {
        check("getVideoActiveFormatDescription()").that(
                actual.getVideoActiveFormatDescription()).isEqualTo(format);
    }
    public void hasVideoHeight(int height) {
        check("getVideoHeight()").that(actual.getVideoHeight()).isEqualTo(height);
    }

    public void hasVideoFrameRate(float rate) {
        check("getVideoFrameRate()").that(actual.getVideoFrameRate()).isEqualTo(rate);
    }

    public void hasVideoPixelAspectRatio(float ratio) {
        check("getVideoPixelAspectRatio()").that(actual.getVideoPixelAspectRatio()).isEqualTo(
                ratio);
    }

    public void hasVideoWidth(int width) {
        check("getVideoWidth()").that(actual.getVideoWidth()).isEqualTo(width);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    public void isAudioDescription(boolean enabled) {
        check("isAudioDescription()").that(actual.isAudioDescription()).isEqualTo(enabled);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    public void isEncrypted(boolean enabled) {
        check("isEncrypted()").that(actual.isEncrypted()).isEqualTo(enabled);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    public void isHardOfHearing(boolean enabled) {
        check("isHardOfHearing()").that(actual.isHardOfHearing()).isEqualTo(enabled);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    public void isSpokenSubtitle(boolean enabled) {
        check("isSpokenSubtitle()").that(actual.isSpokenSubtitle()).isEqualTo(enabled);
    }

    public BundleSubject extra() {
        SimpleSubjectBuilder<? extends BundleSubject, Bundle> bundleSubjectBuilder = check(
                "getExtra()").about(BundleSubject.bundles());
        return bundleSubjectBuilder.that(actual.getExtra());
    }

    public void recreatesEqual(Parcelable.Creator<TvTrackInfo> creator) {
        SimpleSubjectBuilder<? extends ParcelableSubject, Parcelable> parcelableSubjectBuilder =
                check("recreatesEqual()").about(ParcelableSubject.parcelables());
        parcelableSubjectBuilder.that(actual).recreatesEqual(creator);
    }
}
