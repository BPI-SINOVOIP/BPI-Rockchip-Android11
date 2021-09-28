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

package com.android.tv.data;

import static com.google.common.truth.Truth.assertThat;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.database.SQLException;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;
import android.util.Pair;

import androidx.tvprovider.media.tv.PreviewProgram;
import androidx.tvprovider.media.tv.TvContractCompat;

import com.android.tv.data.api.Channel;
import com.android.tv.data.api.Program;
import com.android.tv.dvr.data.RecordedProgram;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowLog;

import java.util.List;

/** Tests for {@link PreviewDataManager}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class PreviewDataManagerTest {
    private static final long FAKE_PREVIEW_CHANNEL_ID = 2002;
    private static final long FAKE_PROGRAM_ID = 1001;
    private static final String FAKE_PROGRAM_TITLE = "test program";
    private static final String FAKE_PROGRAM_POSTER_URI = "http://fake.uri/poster.jpg";
    private static final long FAKE_CHANNEL_ID = 1002;
    private static final String FAKE_CHANNEL_DISPLAY_NAME = "test channel";
    private static final String FAKE_INPUT_ID = "test input";

    private static class QueryExceptionProvider extends ContentProvider {
        @Override
        public boolean onCreate() {
            return false;
        }

        @Nullable
        @Override
        public Cursor query(
                @NonNull Uri uri,
                @Nullable String[] projection,
                @Nullable String selection,
                @Nullable String[] selectionArgs,
                @Nullable String sortOrder) {
            throw new SQLException("Testing " + uri);
        }

        @Nullable
        @Override
        public String getType(@NonNull Uri uri) {
            return null;
        }

        @Nullable
        @Override
        public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
            return null;
        }

        @Override
        public int delete(
                @NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
            return 0;
        }

        @Override
        public int update(
                @NonNull Uri uri,
                @Nullable ContentValues values,
                @Nullable String selection,
                @Nullable String[] selectionArgs) {
            return 0;
        }
    }

    @Test
    public void start() {
        PreviewDataManager previewDataManager =
                new PreviewDataManager(RuntimeEnvironment.application);
        assertThat(previewDataManager.isLoadFinished()).isFalse();
        previewDataManager.start();
        assertThat(previewDataManager.isLoadFinished()).isTrue();
    }

    @Test
    public void queryPreviewData_sqlexception() {
        ProviderInfo info = new ProviderInfo();
        info.authority = TvContractCompat.AUTHORITY;
        QueryExceptionProvider provider =
                Robolectric.buildContentProvider(QueryExceptionProvider.class).create(info).get();
        ShadowContentResolver.registerProviderInternal(TvContractCompat.AUTHORITY, provider);

        PreviewDataManager previewDataManager =
                new PreviewDataManager(RuntimeEnvironment.application);
        assertThat(previewDataManager.isLoadFinished()).isFalse();
        previewDataManager.start();
        List<ShadowLog.LogItem> logs = ShadowLog.getLogsForTag("PreviewDataManager");
        // The only warning should be the test one
        // NOTE: I am not using hamcrest matchers because of weird class loading issues
        // TODO: use truth
        for (ShadowLog.LogItem log : logs) {
            if (log.type == Log.WARN) {
                assertThat(log.msg).isEqualTo("Unable to get preview data");
                assertThat(log.throwable).isInstanceOf(SQLException.class);
                assertThat(log.throwable)
                        .hasMessageThat()
                        .isEqualTo("Testing content://android.media.tv/channel?preview=true");
            }
        }
    }

    @Test
    public void createPreviewProgram_fromProgram() {
        Program program =
                new ProgramImpl.Builder()
                        .setId(FAKE_PROGRAM_ID)
                        .setTitle(FAKE_PROGRAM_TITLE)
                        .setPosterArtUri(FAKE_PROGRAM_POSTER_URI)
                        .build();
        Channel channel =
                new ChannelImpl.Builder()
                        .setId(FAKE_CHANNEL_ID)
                        .setDisplayName(FAKE_CHANNEL_DISPLAY_NAME)
                        .setInputId(FAKE_INPUT_ID)
                        .build();

        PreviewProgram previewProgram =
                PreviewDataManager.PreviewDataUtils.createPreviewProgramFromContent(
                        PreviewProgramContent.createFromProgram(
                                FAKE_PREVIEW_CHANNEL_ID, program, channel),
                        0);

        assertThat(previewProgram.getChannelId()).isEqualTo(FAKE_PREVIEW_CHANNEL_ID);
        assertThat(previewProgram.getType())
                .isEqualTo(TvContractCompat.PreviewPrograms.TYPE_CHANNEL);
        assertThat(previewProgram.isLive()).isTrue();
        assertThat(previewProgram.getTitle()).isEqualTo(FAKE_PROGRAM_TITLE);
        assertThat(previewProgram.getDescription()).isEqualTo(FAKE_CHANNEL_DISPLAY_NAME);
        assertThat(previewProgram.getPosterArtUri().toString()).isEqualTo(FAKE_PROGRAM_POSTER_URI);
        assertThat(previewProgram.getIntentUri()).isEqualTo(channel.getUri());
        assertThat(previewProgram.getPreviewVideoUri())
                .isEqualTo(
                        PreviewDataManager.PreviewDataUtils.addQueryParamToUri(
                                channel.getUri(),
                                Pair.create(PreviewProgramContent.PARAM_INPUT, FAKE_INPUT_ID)));
        assertThat(previewProgram.getInternalProviderId())
                .isEqualTo(Long.toString(FAKE_PROGRAM_ID));
        assertThat(previewProgram.getContentId()).isEqualTo(channel.getUri().toString());
    }

    @Test
    public void createPreviewProgram_fromRecordedProgram() {
        RecordedProgram program =
                RecordedProgram.builder()
                        .setId(FAKE_PROGRAM_ID)
                        .setTitle(FAKE_PROGRAM_TITLE)
                        .setPosterArtUri(FAKE_PROGRAM_POSTER_URI)
                        .setInputId(FAKE_INPUT_ID)
                        .build();
        Uri recordedProgramUri = TvContractCompat.buildRecordedProgramUri(FAKE_PROGRAM_ID);

        PreviewProgram previewProgram =
                PreviewDataManager.PreviewDataUtils.createPreviewProgramFromContent(
                        PreviewProgramContent.createFromRecordedProgram(
                                FAKE_PREVIEW_CHANNEL_ID, program, null),
                        0);

        assertThat(previewProgram.getChannelId()).isEqualTo(FAKE_PREVIEW_CHANNEL_ID);
        assertThat(previewProgram.getType()).isEqualTo(TvContractCompat.PreviewPrograms.TYPE_CLIP);
        assertThat(previewProgram.isLive()).isFalse();
        assertThat(previewProgram.getTitle()).isEqualTo(FAKE_PROGRAM_TITLE);
        assertThat(previewProgram.getDescription()).isEmpty();
        assertThat(previewProgram.getPosterArtUri().toString()).isEqualTo(FAKE_PROGRAM_POSTER_URI);
        assertThat(previewProgram.getIntentUri()).isEqualTo(recordedProgramUri);
        assertThat(previewProgram.getPreviewVideoUri())
                .isEqualTo(
                        PreviewDataManager.PreviewDataUtils.addQueryParamToUri(
                                recordedProgramUri,
                                Pair.create(PreviewProgramContent.PARAM_INPUT, FAKE_INPUT_ID)));
        assertThat(previewProgram.getContentId()).isEqualTo(recordedProgramUri.toString());
    }
}
