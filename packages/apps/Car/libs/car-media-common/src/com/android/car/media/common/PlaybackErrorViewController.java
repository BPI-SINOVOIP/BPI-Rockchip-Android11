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
import android.car.drivingstate.CarUxRestrictions;
import android.content.res.Resources;
import android.util.Log;
import android.view.View;

import androidx.annotation.Nullable;

import com.android.car.apps.common.UxrButton;
import com.android.car.apps.common.UxrTextView;
import com.android.car.apps.common.util.ViewUtils;


/** Simple controller for the error message and the error button. */
public class PlaybackErrorViewController {

    private static final String TAG = "PlaybckErrViewCtrlr";

    // mErrorMessageView is defined explicitly as a UxrTextView instead of a TextView to
    // provide clarity as it may be misleading to assume that mErrorMessageView extends all TextView
    // methods. In addition, it increases discoverability of runtime issues that may occur.
    private final UxrTextView mErrorMessageView;
    private final UxrButton mErrorButton;

    private final int mFadeDuration;

    public PlaybackErrorViewController(View content) {
        mErrorMessageView = content.findViewById(R.id.error_message);
        mErrorButton = content.findViewById(R.id.error_button);

        Resources res = content.getContext().getResources();
        mFadeDuration = res.getInteger(R.integer.new_album_art_fade_in_duration);
    }

    /** Animates away the error views. */
    public void hideError() {
        ViewUtils.hideViewAnimated(mErrorMessageView, mFadeDuration);
        ViewUtils.hideViewAnimated(mErrorButton, mFadeDuration);
    }

    /** Hides the error views without animation. */
    public void hideErrorNoAnim() {
        ViewUtils.hideViewAnimated(mErrorMessageView, 0);
        ViewUtils.hideViewAnimated(mErrorButton, 0);
    }

    /** Sets the error message and optionally the error button. */
    public void setError(String message, @Nullable String label,
            @Nullable PendingIntent pendingIntent, boolean isDistractionOptimized) {
        mErrorMessageView.setText(message);
        ViewUtils.showViewAnimated(mErrorMessageView, mFadeDuration);

        // Only show the error button if the error is actionable.
        if (label != null && pendingIntent != null) {
            mErrorButton.setText(label);

            mErrorButton.setUxRestrictions(isDistractionOptimized
                    ? CarUxRestrictions.UX_RESTRICTIONS_BASELINE
                    : CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP);

            mErrorButton.setOnClickListener(v -> {
                try {
                    pendingIntent.send();
                } catch (PendingIntent.CanceledException e) {
                    if (Log.isLoggable(TAG, Log.ERROR)) {
                        Log.e(TAG, "Pending intent canceled");
                    }
                }
            });
            ViewUtils.showViewAnimated(mErrorButton, mFadeDuration);
        } else {
            ViewUtils.hideViewAnimated(mErrorButton, mFadeDuration);
        }
    }
}
