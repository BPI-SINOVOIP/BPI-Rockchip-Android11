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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.contains;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.CountDownTimer;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.internal.runner.junit4.statement.UiThreadStatement;

import com.android.internal.widget.LockPatternUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class ConfirmLockLockoutHelperTest {
    private static final int TEST_USER = 100;

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private ConfirmLockLockoutHelper mConfirmLockLockoutHelper;

    @Mock
    private LockPatternUtils mLockPatternUtils;
    @Mock
    private ConfirmLockLockoutHelper.ConfirmLockUIController mUIController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mConfirmLockLockoutHelper = new ConfirmLockLockoutHelper(mContext, TEST_USER,
                mLockPatternUtils);
        mConfirmLockLockoutHelper.setConfirmLockUIController(mUIController);
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsZero_doesNothing() {
        runOnCheckCompletedWithTimeout(0);

        verify(mLockPatternUtils, never()).setLockoutAttemptDeadline(TEST_USER, 0);
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsZero_noTimer() {
        runOnCheckCompletedWithTimeout(0);

        assertThat(mConfirmLockLockoutHelper.getCountDownTimer()).isNull();
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsPositive_setsLockoutDeadline() {
        runOnCheckCompletedWithTimeout(1000);

        verify(mLockPatternUtils).setLockoutAttemptDeadline(TEST_USER, 1000);
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsPositive_refreshesUILocked() {
        when(mLockPatternUtils.getLockoutAttemptDeadline(TEST_USER)).thenReturn(1000L);
        runOnCheckCompletedWithTimeout(1000);

        verify(mUIController).refreshUI(true);
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsPositive_createsTimer() {
        runOnCheckCompletedWithTimeout(1000);

        assertThat(mConfirmLockLockoutHelper.getCountDownTimer()).isNotNull();
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsPositive_timerTickUpdatesErrorText() {
        runOnCheckCompletedWithTimeout(1000);

        reset(mUIController);
        CountDownTimer timer = mConfirmLockLockoutHelper.getCountDownTimer();
        timer.onTick(1000);

        verify(mUIController).setErrorText(contains("1"));
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsPositive_onFinishUpdatesErrorText() {
        runOnCheckCompletedWithTimeout(1000);

        reset(mUIController);
        CountDownTimer timer = mConfirmLockLockoutHelper.getCountDownTimer();
        timer.onFinish();

        verify(mUIController).setErrorText("");
    }

    @Test
    public void onCheckCompletedWithTimeout_timeoutIsPositive_onFinishUpdatesUINotLocked() {
        runOnCheckCompletedWithTimeout(1000);

        reset(mUIController);
        CountDownTimer timer = mConfirmLockLockoutHelper.getCountDownTimer();
        timer.onFinish();

        verify(mUIController).refreshUI(false);
    }

    private void runOnCheckCompletedWithTimeout(int timeout) {
        try {
            // Needs to be called on the UI thread due to the CountDownTimer.
            UiThreadStatement.runOnUiThread(() -> {
                mConfirmLockLockoutHelper.onCheckCompletedWithTimeout(timeout);
            });
        } catch (Throwable throwable) {
            throwable.printStackTrace();
        }
    }
}
