/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tv.settings.users;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import com.android.tv.settings.dialog.PinDialogFragment;

import java.util.function.Consumer;

/**
 * Fragment that handles the PIN dialog for Restricted Profile actions.
 */
public class RestrictedProfilePinDialogFragment extends PinDialogFragment {
    private static final String TAG = RestrictedProfilePinDialogFragment.class.getSimpleName();

    private static final  String SHARED_PREFERENCE_NAME = "RestrictedProfilePinDialogFragment";
    private static final String PREF_DISABLE_PIN_UNTIL = "disable_pin_until";

    private RestrictedProfilePinStorage mRestrictedProfilePinStorage;

    private HandlerThread mBackgroundThread;
    private Handler mBackgroundThreadHandler;
    private Handler mUiThreadHandler;

    public static RestrictedProfilePinDialogFragment newInstance(@PinDialogType int type) {
        RestrictedProfilePinDialogFragment fragment = new RestrictedProfilePinDialogFragment();
        final Bundle b = new Bundle(1);
        b.putInt(PinDialogFragment.ARG_TYPE, type);
        fragment.setArguments(b);
        return fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        mBackgroundThread = new HandlerThread(TAG);
        mBackgroundThread.start();
        mBackgroundThreadHandler = new Handler(mBackgroundThread.getLooper());
        mUiThreadHandler = new Handler(Looper.getMainLooper());

        mBackgroundThreadHandler.post(() -> {
            mRestrictedProfilePinStorage = RestrictedProfilePinStorage.newInstance(getContext());
            mRestrictedProfilePinStorage.bind();
        });
    }

    @Override
    public void onDetach() {
        mRestrictedProfilePinStorage.unbind();
        mRestrictedProfilePinStorage = null;
        mUiThreadHandler = null;
        mBackgroundThreadHandler = null;
        mBackgroundThread.quitSafely();
        mBackgroundThread = null;
        super.onDetach();
    }

    /**
     * Returns the time until we should disable the PIN dialog (because the user input wrong
     * PINs repeatedly).
     */
    public static long getDisablePinUntil(Context context) {
        return context.getSharedPreferences(SHARED_PREFERENCE_NAME, Context.MODE_PRIVATE)
                      .getLong(PREF_DISABLE_PIN_UNTIL, 0);
    }

    /**
     * Saves the time until we should disable the PIN dialog (because the user input wrong PINs
     * repeatedly).
     */
    public static void setDisablePinUntil(Context context, long timeMillis) {
        context.getSharedPreferences(SHARED_PREFERENCE_NAME, Context.MODE_PRIVATE)
                .edit()
                .putLong(PREF_DISABLE_PIN_UNTIL, timeMillis)
                .apply();
    }

    @Override
    public long getPinDisabledUntil() {
        return getDisablePinUntil(getActivity());
    }

    @Override
    public void setPinDisabledUntil(long retryDisableTimeout) {
        setDisablePinUntil(getActivity(), retryDisableTimeout);
    }

    @Override
    public void setPin(String pin, String originalPin, Consumer<Boolean> consumer) {
        // Set pin on the background thread and consume the result on the UI thread.
        mBackgroundThreadHandler.post(() -> {
            mRestrictedProfilePinStorage.setPin(pin, originalPin);
            mUiThreadHandler.post(() -> consumer.accept(true));
        });
    }

    @Override
    public void deletePin(String oldPin, Consumer<Boolean> consumer) {
        // Delete the pin on the background thread and consume the result on the UI thread.
        mBackgroundThreadHandler.post(() -> {
            mRestrictedProfilePinStorage.deletePin(oldPin);
            mUiThreadHandler.post(() -> consumer.accept(true));
        });
    }

    @Override
    public void isPinCorrect(String pin, Consumer<Boolean> consumer) {
        // Check if the pin is correct and consume the result on the UI thread.
        mBackgroundThreadHandler.post(() -> {
            boolean isPinCorrect = mRestrictedProfilePinStorage.isPinCorrect(pin);
            mUiThreadHandler.post(() -> consumer.accept(isPinCorrect));
        });
    }

    @Override
    public void isPinSet(Consumer<Boolean> consumer) {
        // Check if the pin is set and consume the result on the UI thread.
        mBackgroundThreadHandler.post(() -> {
            boolean isPinSet = mRestrictedProfilePinStorage.isPinSet();
            mUiThreadHandler.post(() -> consumer.accept(isPinSet));
        });
    }
}
