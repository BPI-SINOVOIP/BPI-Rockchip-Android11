/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.tv.tuner.exoplayer2;

import android.content.Context;
import android.media.PlaybackParams;
import android.net.Uri;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.view.Surface;

import com.android.tv.common.SoftPreconditions;
import com.android.tv.tuner.data.Cea708Data;
import com.android.tv.tuner.data.Cea708Data.CaptionEvent;
import com.android.tv.tuner.data.Cea708Parser;
import com.android.tv.tuner.data.TunerChannel;
import com.android.tv.tuner.source.TsDataSource;
import com.android.tv.tuner.source.TsDataSourceManager;
import com.android.tv.tuner.ts.EventDetector;
import com.android.tv.tuner.tvinput.debug.TunerDebug;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.ExoPlaybackException;
import com.google.android.exoplayer2.ExoPlayer;
import com.google.android.exoplayer2.ExoPlayerFactory;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.Player;
import com.google.android.exoplayer2.SimpleExoPlayer;
import com.google.android.exoplayer2.Timeline;
import com.google.android.exoplayer2.audio.AudioListener;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.ProgressiveMediaSource;
import com.google.android.exoplayer2.source.TrackGroup;
import com.google.android.exoplayer2.source.TrackGroupArray;
import com.google.android.exoplayer2.text.Cue;
import com.google.android.exoplayer2.text.TextOutput;
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import com.google.android.exoplayer2.trackselection.MappingTrackSelector.MappedTrackInfo;
import com.google.android.exoplayer2.trackselection.TrackSelection;
import com.google.android.exoplayer2.trackselection.TrackSelectionArray;
import com.google.android.exoplayer2.video.VideoListener;
import com.google.android.exoplayer2.video.VideoRendererEventListener;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** MPEG-2 TS stream player implementation using ExoPlayer2. */
public class MpegTsPlayerV2
        implements Player.EventListener,
                           VideoListener,
                           AudioListener,
                           TextOutput,
                           VideoRendererEventListener {

    /** Interface definition for a callback to be notified of changes in player state. */
    public interface Callback {
        /**
         * Called when player state changes.
         *
         * @param playbackState notifies the updated player state.
         */
        void onStateChanged(@PlayerState int playbackState);

        /** Called when player has ended with an error. */
        void onError(Exception e);

        /** Called when size of input video to the player changes. */
        void onVideoSizeChanged(int width, int height, float pixelWidthHeightRatio);

        /** Called when player rendered its first frame. */
        void onRenderedFirstFrame();

        /** Called when audio stream is unplayable. */
        void onAudioUnplayable();

        /** Called when player drops some frames. */
        void onSmoothTrickplayForceStopped();
    }

    /** Interface definition for a callback to be notified of changes on video display. */
    public interface VideoEventListener {
        /** Notifies the caption event. */
        void onEmitCaptionEvent(CaptionEvent event);

        /** Notifies clearing up whole closed caption event. */
        void onClearCaptionEvent();

        /** Notifies the discovered caption service number. */
        void onDiscoverCaptionServiceNumber(int serviceNumber);

    }

    public static final int MIN_BUFFER_MS = 0;
    public static final int MIN_REBUFFER_MS = 500;

    @IntDef({TRACK_TYPE_VIDEO, TRACK_TYPE_AUDIO, TRACK_TYPE_TEXT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TrackType {}

    public static final int TRACK_TYPE_VIDEO = 0;
    public static final int TRACK_TYPE_AUDIO = 1;
    public static final int TRACK_TYPE_TEXT = 2;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({STATE_IDLE, STATE_BUFFERING, STATE_READY, STATE_ENDED})
    public @interface PlayerState {}

    public static final int STATE_IDLE = Player.STATE_IDLE;
    public static final int STATE_BUFFERING = Player.STATE_BUFFERING;
    public static final int STATE_READY = Player.STATE_READY;
    public static final int STATE_ENDED = Player.STATE_ENDED;

    private static final float MAX_SMOOTH_TRICKPLAY_SPEED = 9.0f;
    private static final float MIN_SMOOTH_TRICKPLAY_SPEED = 0.1f;

    private int mCaptionServiceNumber = Cea708Data.EMPTY_SERVICE_NUMBER;

    private final Context mContext;
    private final SimpleExoPlayer mPlayer;
    private final DefaultTrackSelector mTrackSelector;
    private final TsDataSourceManager mSourceManager;

    private DefaultTrackSelector.Parameters mTrackSelectorParameters;
    private TrackGroupArray mLastSeenTrackGroupArray;
    private Callback mCallback;
    private TsDataSource mDataSource;
    private VideoEventListener mVideoEventListener;
    private boolean mTrickplayRunning;

    /**
     * Creates MPEG2-TS stream player.
     *
     * @param context       the application context
     * @param sourceManager the manager for {@link TsDataSource}
     * @param callback      callback for playback state changes
     */
    public MpegTsPlayerV2(Context context, TsDataSourceManager sourceManager, Callback callback) {
        mContext = context;
        mTrackSelectorParameters = new DefaultTrackSelector.ParametersBuilder().build();
        mTrackSelector = new DefaultTrackSelector();
        mTrackSelector.setParameters(mTrackSelectorParameters);
        mLastSeenTrackGroupArray = null;
        mPlayer = ExoPlayerFactory.newSimpleInstance(context, mTrackSelector);
        mPlayer.addListener(this);
        mPlayer.addVideoListener(this);
        mPlayer.addAudioListener(this);
        mPlayer.addTextOutput(this);
        mSourceManager = sourceManager;
        mCallback = callback;
    }

    /**
     * Sets the video event listener.
     *
     * @param videoEventListener the listener for video events
     */
    public void setVideoEventListener(VideoEventListener videoEventListener) {
        mVideoEventListener = videoEventListener;
    }

    /**
     * Sets the closed caption service number.
     *
     * @param captionServiceNumber the service number of CEA-708 closed caption
     */
    public void setCaptionServiceNumber(int captionServiceNumber) {
        mCaptionServiceNumber = captionServiceNumber;
        if (captionServiceNumber == Cea708Data.EMPTY_SERVICE_NUMBER) return;
        MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
        if (mappedTrackInfo != null) {
            int rendererCount = mappedTrackInfo.getRendererCount();
            for (int rendererIndex = 0; rendererIndex < rendererCount; rendererIndex++) {
                if (mappedTrackInfo.getRendererType(rendererIndex) == C.TRACK_TYPE_TEXT) {
                    TrackGroupArray trackGroupArray = mappedTrackInfo.getTrackGroups(rendererIndex);
                    for (int i = 0; i < trackGroupArray.length; i++) {
                        int readServiceNumber =
                                trackGroupArray.get(i).getFormat(0).accessibilityChannel;
                        int serviceNumber =
                                readServiceNumber == Format.NO_VALUE ? 1 : readServiceNumber;
                        if (serviceNumber == captionServiceNumber) {
                            setSelectedTrack(TRACK_TYPE_TEXT, i);
                        }
                    }
                }
            }
        }
    }

    /**
     * Invoked each time there is a change in the {@link Cue}s to be rendered
     *
     * @param cues The {@link Cue}s to be rendered, or an empty list if no cues are to be rendered.
     */
    @Override
    public void onCues(List<Cue> cues) {
        mVideoEventListener.onEmitCaptionEvent(
                new CaptionEvent(
                        Cea708Parser.CAPTION_EMIT_TYPE_COMMAND_DFX,
                        new Cea708Data.CaptionWindow(
                                /* id= */ 0,
                                /* visible= */ true,
                                /* rowlock= */ false,
                                /* columnLock= */ false,
                                /* priority= */ 3,
                                /* relativePositioning= */ true,
                                /* anchorVertical= */ 0,
                                /* anchorHorizontal= */ 0,
                                /* anchorId= */ 0,
                                /* rowCount= */ 0,
                                /* columnCount= */ 0,
                                /* penStyle= */ 0,
                                /* windowStyle= */ 2)));
        mVideoEventListener.onEmitCaptionEvent(
                new CaptionEvent(Cea708Parser.CAPTION_EMIT_TYPE_BUFFER,
                        cues));
    }

    /**
     * Sets the surface for the player.
     *
     * @param surface the {@link Surface} to render video
     */
    public void setSurface(Surface surface) {
        mPlayer.setVideoSurface(surface);
    }

    /**
     * Creates renderers and {@link TsDataSource} and initializes player.
     *
     * @return true when everything is created and initialized well, false otherwise
     */
    public boolean prepare(TunerChannel channel, EventDetector.EventListener eventListener) {
        TsDataSource source = null;
        if (channel != null) {
            source = mSourceManager.createDataSource(mContext, channel, eventListener);
            if (source == null) {
                return false;
            }
        }
        mDataSource = source;
        MediaSource mediaSource =
                new ProgressiveMediaSource.Factory(() -> mDataSource).createMediaSource(Uri.EMPTY);
        mPlayer.prepare(mediaSource, true, false);
        return true;
    }


    /** Returns {@link TsDataSource} which provides MPEG2-TS stream. */
    public TsDataSource getDataSource() {
        return mDataSource;
    }

    /**
     * Sets the player state to pause or play.
     *
     * @param playWhenReady sets the player state to being ready to play when {@code true}, sets the
     *                      player state to being paused when {@code false}
     */
    public void setPlayWhenReady(boolean playWhenReady) {
        mPlayer.setPlayWhenReady(playWhenReady);
        stopSmoothTrickplay(false);
    }

    /** Returns true, if trickplay is supported. */
    public boolean supportSmoothTrickPlay(float playbackSpeed) {
        return playbackSpeed > MIN_SMOOTH_TRICKPLAY_SPEED
                       && playbackSpeed < MAX_SMOOTH_TRICKPLAY_SPEED;
    }

    /**
     * Starts trickplay. It'll be reset, if {@link #seekTo} or {@link #setPlayWhenReady} is called.
     */
    public void startSmoothTrickplay(PlaybackParams playbackParams) {
        SoftPreconditions.checkState(supportSmoothTrickPlay(playbackParams.getSpeed()));
        mPlayer.setPlayWhenReady(true);
        mTrickplayRunning = true;
    }

    private void stopSmoothTrickplay(boolean calledBySeek) {
        if (mTrickplayRunning) {
            mTrickplayRunning = false;
        }
    }

    /**
     * Seeks to the specified position of the current playback.
     *
     * @param positionMs the specified position in milli seconds.
     */
    public void seekTo(long positionMs) {
        mPlayer.seekTo(positionMs);
        stopSmoothTrickplay(true);
    }

    /** Releases the player. */
    public void release() {
        if (mDataSource != null) {
            mDataSource = null;
        }
        mCallback = null;
        mPlayer.release();
    }

    /** Returns the current status of the player. */
    public int getPlaybackState() {
        return mPlayer.getPlaybackState();
    }

    /** Returns {@code true} when the player is prepared to play, {@code false} otherwise. */
    public boolean isPrepared() {
        int state = getPlaybackState();
        return state == ExoPlayer.STATE_READY || state == ExoPlayer.STATE_BUFFERING;
    }

    /** Returns {@code true} when the player is being ready to play, {@code false} otherwise. */
    public boolean isPlaying() {
        int state = getPlaybackState();
        return (state == ExoPlayer.STATE_READY || state == ExoPlayer.STATE_BUFFERING)
                       && mPlayer.getPlayWhenReady();
    }

    /** Returns {@code true} when the player is buffering, {@code false} otherwise. */
    public boolean isBuffering() {
        return getPlaybackState() == ExoPlayer.STATE_BUFFERING;
    }

    /** Returns the current position of the playback in milli seconds. */
    public long getCurrentPosition() {
        return mPlayer.getCurrentPosition();
    }

    /**
     * Sets the volume of the audio.
     *
     * @param volume see also
     *               {@link com.google.android.exoplayer2.Player.AudioComponent#setVolume(float)}
     */
    public void setVolume(float volume) {
        mPlayer.setVolume(volume);
    }

    /**
     * Enables or disables audio and closed caption.
     *
     * @param enable enables the audio and closed caption when {@code true}, disables otherwise.
     */
    public void setAudioTrackAndClosedCaption(boolean enable) {}

    @Override
    public void onTimelineChanged(
            Timeline timeline,
            @Nullable Object manifest,
            @Player.TimelineChangeReason int reason) {}

    @Override
    public void onTracksChanged(TrackGroupArray trackGroups, TrackSelectionArray trackSelections) {
        if (trackGroups != mLastSeenTrackGroupArray) {
            mLastSeenTrackGroupArray = trackGroups;
        }
        if (mVideoEventListener != null) {
            MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
            if (mappedTrackInfo != null) {
                int rendererCount = mappedTrackInfo.getRendererCount();
                for (int rendererIndex = 0; rendererIndex < rendererCount; rendererIndex++) {
                    if (mappedTrackInfo.getRendererType(rendererIndex) == C.TRACK_TYPE_TEXT) {
                        TrackGroupArray trackGroupArray =
                                mappedTrackInfo.getTrackGroups(rendererIndex);
                        for (int i = 0; i < trackGroupArray.length; i++) {
                            int serviceNumber =
                                    trackGroupArray.get(i).getFormat(0).accessibilityChannel;
                            mVideoEventListener.onDiscoverCaptionServiceNumber(
                                    serviceNumber == Format.NO_VALUE ? 1 : serviceNumber);
                        }
                    }
                }
            }
        }
    }

    /**
     * Checks the stream for the renderer of required track type.
     *
     * @param trackType Returns {@code true} if the player has any renderer for track type
     * {@trackType}, {@code false} otherwise.
     */
    private boolean hasRendererType(int trackType) {
        MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
        if (mappedTrackInfo != null) {
            int rendererCount = mappedTrackInfo.getRendererCount();
            for (int rendererIndex = 0; rendererIndex < rendererCount; rendererIndex++) {
                if (mappedTrackInfo.getRendererType(rendererIndex) == trackType) {
                    return true;
                }
            }
        }
        return false;
    }

    /** Returns {@code true} if the player has any video track, {@code false} otherwise. */
    public boolean hasVideo() {
        return hasRendererType(C.TRACK_TYPE_VIDEO);
    }

    /** Returns {@code true} if the player has any audio track, {@code false} otherwise. */
    public boolean hasAudio() {
        return hasRendererType(C.TRACK_TYPE_AUDIO);
    }

    /** Returns the number of tracks exposed by the specified renderer. */
    public int getTrackCount(int rendererIndex) {
        MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
        if (mappedTrackInfo != null) {
            TrackGroupArray trackGroupArray = mappedTrackInfo.getTrackGroups(rendererIndex);
            return trackGroupArray.length;
        }
        return 0;
    }

    /** Selects a track for the specified renderer. */
    public void setSelectedTrack(int rendererIndex, int trackIndex) {
        MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
        if (trackIndex >= getTrackCount(rendererIndex)) {
            return;
        }
        TrackGroupArray trackGroupArray = mappedTrackInfo.getTrackGroups(rendererIndex);
        mTrackSelectorParameters = mTrackSelector.getParameters();
        DefaultTrackSelector.SelectionOverride override =
                new DefaultTrackSelector.SelectionOverride(trackIndex, 0);
        DefaultTrackSelector.ParametersBuilder builder =
                mTrackSelectorParameters.buildUpon()
                        .clearSelectionOverrides(rendererIndex)
                        .setRendererDisabled(rendererIndex, false)
                        .setSelectionOverride(rendererIndex, trackGroupArray, override);
        mTrackSelector.setParameters(builder);
    }

    /**
     * Returns the index of the currently selected track for the specified renderer.
     *
     * @param rendererIndex The index of the renderer.
     * @return The selected track. A negative value or a value greater than or equal to the
     * renderer's track count indicates that the renderer is disabled.
     */
    public int getSelectedTrack(int rendererIndex) {
        TrackSelection trackSelection = mPlayer.getCurrentTrackSelections().get(rendererIndex);
        MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
        TrackGroupArray trackGroupArray;
        if (trackSelection != null && mappedTrackInfo != null) {
            trackGroupArray = mappedTrackInfo.getTrackGroups(rendererIndex);
            return trackGroupArray.indexOf(trackSelection.getTrackGroup());
        }
        return C.INDEX_UNSET;
    }

    /**
     * Returns the format of a track.
     *
     * @param rendererIndex The index of the renderer.
     * @param trackIndex    The index of the track.
     * @return The format of the track.
     */
    public Format getTrackFormat(int rendererIndex, int trackIndex) {
        MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
        if (mappedTrackInfo != null) {
            TrackGroupArray trackGroupArray = mappedTrackInfo.getTrackGroups(rendererIndex);
            TrackGroup trackGroup = trackGroupArray.get(trackIndex);
            return trackGroup.getFormat(0);
        }
        return null;
    }

    @Override
    public void onPlayerStateChanged(boolean playWhenReady, @PlayerState int state) {
        if (mCallback == null) {
            return;
        }
        mCallback.onStateChanged(state);
        if (state == STATE_READY && hasVideo() && playWhenReady) {
            Format format = mPlayer.getVideoFormat();
            mCallback.onVideoSizeChanged(format.width, format.height, format.pixelWidthHeightRatio);
        }
    }

    @Override
    public void onPlayerError(ExoPlaybackException exception) {
        if (mCallback != null) {
            mCallback.onError(exception);
        }
    }

    @Override
    public void onVideoSizeChanged(
            int width, int height, int unappliedRotationDegrees, float pixelWidthHeightRatio) {
        if (mCallback != null) {
            mCallback.onVideoSizeChanged(width, height, pixelWidthHeightRatio);
        }
    }

    @Override
    public void onSurfaceSizeChanged(int width, int height) {}

    @Override
    public void onRenderedFirstFrame() {
        if (mCallback != null) {
            mCallback.onRenderedFirstFrame();
        }
    }

    @Override
    public void onDroppedFrames(int count, long elapsed) {
        TunerDebug.notifyVideoFrameDrop(count, elapsed);
        if (mTrickplayRunning && mCallback != null) {
            mCallback.onSmoothTrickplayForceStopped();
        }
    }
}
