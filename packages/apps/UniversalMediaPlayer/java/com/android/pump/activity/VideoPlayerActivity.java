/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.pump.activity;

import android.content.ContentUris;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.media.AudioAttributesCompat;
import androidx.media2.common.SessionPlayer;
import androidx.media2.common.UriMediaItem;
import androidx.media2.player.MediaPlayer;
import androidx.media2.widget.VideoView;

import com.android.pump.R;
import com.android.pump.db.Video;
import com.android.pump.util.Clog;
import com.android.pump.util.IntentUtils;

@UiThread
public class VideoPlayerActivity extends AppCompatActivity {
    private static final String TAG = Clog.tag(VideoPlayerActivity.class);
    private static final String SAVED_POSITION_KEY = "SavedPosition";

    private VideoView mVideoView;
    private MediaPlayer mMediaPlayer;
    private long mSavedPosition = SessionPlayer.UNKNOWN_TIME;

    public static void start(@NonNull Context context, @NonNull Video video) {
        // TODO(b/123703220) Find a better URI (video.getUri()?)
        Uri uri = ContentUris.withAppendedId(MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
                video.getId());
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        intent.setDataAndTypeAndNormalize(uri, video.getMimeType());
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        IntentUtils.startExternalActivity(context, intent);
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video_player);
        mVideoView = findViewById(R.id.video_view);

        if (savedInstanceState != null) {
            mSavedPosition = savedInstanceState.getLong(SAVED_POSITION_KEY,
                    SessionPlayer.UNKNOWN_TIME);
        }

        mMediaPlayer = new MediaPlayer(VideoPlayerActivity.this);
        AudioAttributesCompat audioAttributes = new AudioAttributesCompat.Builder()
                .setUsage(AudioAttributesCompat.USAGE_MEDIA)
                .setContentType(AudioAttributesCompat.CONTENT_TYPE_MOVIE).build();

        mMediaPlayer.setAudioAttributes(audioAttributes);
        mVideoView.setPlayer(mMediaPlayer);

        handleIntent();
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        outState.putLong(SAVED_POSITION_KEY, mMediaPlayer.getCurrentPosition());
        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onNewIntent(@Nullable Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);

        handleIntent();
    }

    private void handleIntent() {
        Intent intent = getIntent();
        Uri uri = intent.getData();
        if (uri == null) {
            Clog.e(TAG, "The intent has no uri. Finishing activity...");
            finish();
            return;
        }
        UriMediaItem mediaItem = new UriMediaItem.Builder(uri).build();
        mMediaPlayer.setMediaItem(mediaItem)
                .addListener(new Runnable() {
                    @Override
                    public void run() {
                        if (mSavedPosition != SessionPlayer.UNKNOWN_TIME) {
                            mMediaPlayer.seekTo(mSavedPosition);
                            mSavedPosition = SessionPlayer.UNKNOWN_TIME;
                        }
                        mMediaPlayer.play();
                    }
                }, ContextCompat.getMainExecutor(this));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        try {
            if (mMediaPlayer != null) {
                mMediaPlayer.close();
            }
        } catch (Exception e) { }
    }
}
