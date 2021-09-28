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

package com.android.car.media.common;

import android.app.PendingIntent;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.media.session.PlaybackStateCompat;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.media.common.playback.PlaybackViewModel.PlaybackStateWrapper;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Abstract class to factorize most of the error handling logic.
 */
public abstract class PlaybackErrorsHelper {

    private static final Map<Integer, Integer> ERROR_CODE_MESSAGES_MAP;

    static {
        Map<Integer, Integer> map = new HashMap<>();
        map.put(PlaybackStateCompat.ERROR_CODE_APP_ERROR, R.string.error_code_app_error);
        map.put(PlaybackStateCompat.ERROR_CODE_NOT_SUPPORTED, R.string.error_code_not_supported);
        map.put(PlaybackStateCompat.ERROR_CODE_AUTHENTICATION_EXPIRED,
                R.string.error_code_authentication_expired);
        map.put(PlaybackStateCompat.ERROR_CODE_PREMIUM_ACCOUNT_REQUIRED,
                R.string.error_code_premium_account_required);
        map.put(PlaybackStateCompat.ERROR_CODE_CONCURRENT_STREAM_LIMIT,
                R.string.error_code_concurrent_stream_limit);
        map.put(PlaybackStateCompat.ERROR_CODE_PARENTAL_CONTROL_RESTRICTED,
                R.string.error_code_parental_control_restricted);
        map.put(PlaybackStateCompat.ERROR_CODE_NOT_AVAILABLE_IN_REGION,
                R.string.error_code_not_available_in_region);
        map.put(PlaybackStateCompat.ERROR_CODE_CONTENT_ALREADY_PLAYING,
                R.string.error_code_content_already_playing);
        map.put(PlaybackStateCompat.ERROR_CODE_SKIP_LIMIT_REACHED,
                R.string.error_code_skip_limit_reached);
        map.put(PlaybackStateCompat.ERROR_CODE_ACTION_ABORTED, R.string.error_code_action_aborted);
        map.put(PlaybackStateCompat.ERROR_CODE_END_OF_QUEUE, R.string.error_code_end_of_queue);
        ERROR_CODE_MESSAGES_MAP = Collections.unmodifiableMap(map);
    }

    private final Context mContext;
    private PlaybackStateWrapper mCurrentPlaybackStateWrapper;

    public PlaybackErrorsHelper(Context context) {
        mContext = context;
    }

    protected abstract void handleNewPlaybackState(String displayedMessage, PendingIntent intent,
            String label);

    /**
     * Triggers updates of the error state.
     * Must be called when the children list of the root of the browse tree changes AND when
     * the playback state changes.
     */
    public void handlePlaybackState(@NonNull String tag, PlaybackStateWrapper state,
            boolean ignoreSameState) {
        if (Log.isLoggable(tag, Log.DEBUG)) {
            Log.d(tag,
                    "handlePlaybackState(); state change: " + (mCurrentPlaybackStateWrapper != null
                            ? mCurrentPlaybackStateWrapper.getState() : null) + " -> " + (
                            state != null ? state.getState() : null));
        }

        if (state == null) {
            mCurrentPlaybackStateWrapper = null;
            return;
        }

        String displayedMessage = getDisplayedMessage(mContext, state);
        if (Log.isLoggable(tag, Log.DEBUG)) {
            Log.d(tag, "Displayed error message: [" + displayedMessage + "]");
        }
        if (ignoreSameState && mCurrentPlaybackStateWrapper != null
                && mCurrentPlaybackStateWrapper.getState() == state.getState()
                && TextUtils.equals(displayedMessage,
                getDisplayedMessage(mContext, mCurrentPlaybackStateWrapper))) {
            if (Log.isLoggable(tag, Log.DEBUG)) {
                Log.d(tag, "Ignore same playback state.");
            }
            return;
        }

        mCurrentPlaybackStateWrapper = state;

        PendingIntent intent = getErrorResolutionIntent(state);
        String label = getErrorResolutionLabel(state);
        handleNewPlaybackState(displayedMessage, intent, label);
    }


    @Nullable
    private String getDisplayedMessage(Context ctx, @Nullable PlaybackStateWrapper state) {
        if (state == null) {
            return null;
        }
        if (!TextUtils.isEmpty(state.getErrorMessage())) {
            return state.getErrorMessage().toString();
        }
        // ERROR_CODE_UNKNOWN_ERROR means there is no error in PlaybackState.
        if (state.getErrorCode() != PlaybackStateCompat.ERROR_CODE_UNKNOWN_ERROR) {
            Integer messageId = ERROR_CODE_MESSAGES_MAP.get(state.getErrorCode());
            return messageId != null ? ctx.getString(messageId) : ctx.getString(
                    R.string.default_error_message);
        }
        if (state.getState() == PlaybackStateCompat.STATE_ERROR) {
            return ctx.getString(R.string.default_error_message);
        }
        return null;
    }

    @Nullable
    private PendingIntent getErrorResolutionIntent(@NonNull PlaybackStateWrapper state) {
        Bundle extras = state.getExtras();
        return extras == null ? null : extras.getParcelable(
                MediaConstants.ERROR_RESOLUTION_ACTION_INTENT);
    }

    @Nullable
    private String getErrorResolutionLabel(@NonNull PlaybackStateWrapper state) {
        Bundle extras = state.getExtras();
        return extras == null ? null : extras.getString(
                MediaConstants.ERROR_RESOLUTION_ACTION_LABEL);
    }

}
