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

package com.android.car.settings.security;

import android.content.Context;
import android.os.CountDownTimer;
import android.os.SystemClock;

import androidx.annotation.Nullable;

import com.android.car.settings.R;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.widget.LockPatternUtils;

/** Common lockout handling code. */
public class ConfirmLockLockoutHelper {

    private static ConfirmLockLockoutHelper sInstance;

    private final Context mContext;
    private final int mUserId;
    private final LockPatternUtils mLockPatternUtils;
    private ConfirmLockUIController mUiController;
    private CountDownTimer mCountDownTimer;

    /** Return an instance of {@link ConfirmLockLockoutHelper}. */
    public static ConfirmLockLockoutHelper getInstance(Context context, int userId) {
        if (sInstance == null) {
            sInstance = new ConfirmLockLockoutHelper(context, userId,
                    new LockPatternUtils(context));
        }
        return sInstance;
    }

    @VisibleForTesting
    ConfirmLockLockoutHelper(Context context, int userId,
            LockPatternUtils lockPatternUtils) {
        mContext = context;
        mUserId = userId;
        mLockPatternUtils = lockPatternUtils;
    }

    /** Sets the UI controller. */
    public void setConfirmLockUIController(ConfirmLockUIController uiController) {
        mUiController = uiController;
    }

    /** Gets the lock pattern utils used by this helper. */
    public LockPatternUtils getLockPatternUtils() {
        return mLockPatternUtils;
    }

    /** Handles when the lock check is completed but returns a timeout. */
    public void onCheckCompletedWithTimeout(int timeoutMs) {
        if (timeoutMs <= 0) {
            return;
        }

        long deadline = mLockPatternUtils.setLockoutAttemptDeadline(mUserId, timeoutMs);
        handleAttemptLockout(deadline);
    }

    /** To be called when the UI is resumed to reset the timeout countdown if necessary. */
    public void onResumeUI() {
        if (isLockedOut()) {
            handleAttemptLockout(mLockPatternUtils.getLockoutAttemptDeadline(mUserId));
        } else {
            mUiController.refreshUI(isLockedOut());
        }
    }

    /** To be called when the UI is paused to cancel the ongoing countdown timer. */
    public void onPauseUI() {
        if (mCountDownTimer != null) {
            mCountDownTimer.cancel();
        }
    }

    @VisibleForTesting
    @Nullable
    CountDownTimer getCountDownTimer() {
        return mCountDownTimer;
    }

    private void handleAttemptLockout(long deadline) {
        long elapsedRealtime = SystemClock.elapsedRealtime();
        mUiController.refreshUI(isLockedOut());
        mCountDownTimer = newCountDownTimer(deadline - elapsedRealtime).start();
    }

    private boolean isLockedOut() {
        return mLockPatternUtils.getLockoutAttemptDeadline(mUserId) != 0;
    }

    private CountDownTimer newCountDownTimer(long countDownMillis) {
        return new CountDownTimer(countDownMillis,
                LockPatternUtils.FAILED_ATTEMPT_COUNTDOWN_INTERVAL_MS) {
            @Override
            public void onTick(long millisUntilFinished) {
                int secondsCountdown = (int) (millisUntilFinished / 1000);
                mUiController.setErrorText(
                        mContext.getString(
                                R.string.lockpattern_too_many_failed_confirmation_attempts,
                                secondsCountdown));
            }

            @Override
            public void onFinish() {
                mUiController.refreshUI(/* isLockedOut= */ false);
                mUiController.setErrorText("");
            }
        };
    }

    /** Interface for controlling the associated lock UI. */
    public interface ConfirmLockUIController {
        /** Sets the error text with the given string. */
        void setErrorText(String text);
        /** Refreshes the UI based on the locked out state. */
        void refreshUI(boolean isLockedOut);
    }
}
