/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.security;

import android.os.Bundle;

import androidx.fragment.app.Fragment;

import com.android.car.settings.common.Logger;
import com.android.internal.widget.LockPatternChecker;
import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockPatternView;
import com.android.internal.widget.LockscreenCredential;

import java.util.List;

/**
 * An invisible retained worker fragment to track the AsyncTask that checks the entered lock
 * credential (pattern/pin/password).
 */
public class CheckLockWorker extends Fragment implements LockPatternChecker.OnCheckCallback {

    private static final Logger LOG = new Logger(CheckLockWorker.class);

    private boolean mHasPendingResult;
    private boolean mLockMatched;
    private int mThrottleTimeoutMs;
    private boolean mCheckInProgress;
    private Listener mListener;
    private LockPatternUtils mLockPatternUtils;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRetainInstance(true);
        mLockPatternUtils = new LockPatternUtils(getContext());
    }

    @Override
    public void onChecked(boolean matched, int throttleTimeoutMs) {
        mCheckInProgress = false;

        if (mListener == null) {
            mHasPendingResult = true;
            mLockMatched = matched;
            mThrottleTimeoutMs = throttleTimeoutMs;
        } else {
            mListener.onCheckCompleted(matched, throttleTimeoutMs);
        }
    }

    /**
     * Sets the listener for callback when lock check is completed.
     */
    public void setListener(Listener listener) {
        mListener = listener;
        if (mListener != null && mHasPendingResult) {
            mHasPendingResult = false;
            mListener.onCheckCompleted(mLockMatched, mThrottleTimeoutMs);
        }
    }

    /** Returns whether a lock check is in progress. */
    public final boolean isCheckInProgress() {
        return mCheckInProgress;
    }

    /**
     * Checks lock pattern asynchronously. To receive callback when check is completed,
     * implement {@link Listener} and call {@link #setListener(Listener)}.
     */
    public final void checkPattern(int userId, List<LockPatternView.Cell> pattern) {
        if (mCheckInProgress) {
            LOG.w("Check pattern request issued while one is still running");
            return;
        }

        mCheckInProgress = true;
        LockPatternChecker.checkCredential(mLockPatternUtils,
                LockscreenCredential.createPattern(pattern), userId, this);
    }

    /**
     * Checks lock PIN/password asynchronously.  To receive callback when check is completed,
     * implement {@link Listener} and call {@link #setListener(Listener)}.
     */
    public final void checkPinPassword(int userId, LockscreenCredential password) {
        if (mCheckInProgress) {
            LOG.w("Check pin/password request issued while one is still running");
            return;
        }
        if (password == null || password.isNone()) {
            LOG.w("Empty pin/password LockscreenCredential");
            return;
        }
        mCheckInProgress = true;
        LockPatternChecker.checkCredential(mLockPatternUtils, password, userId, this);
    }

    /**
     * Callback when lock check is completed.
     */
    interface Listener {
        /**
         * @param matched Whether the entered password matches the stored record.
         * @param timeoutMs The remaining amount of time that the user is locked out from
         *                  retrying the password challenge.
         */
        void onCheckCompleted(boolean matched, int timeoutMs);
    }
}

