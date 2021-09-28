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

import android.app.admin.DevicePolicyManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.UserHandle;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockscreenCredential;

/** Business logic for the no lock preference. */
public class NoLockPreferenceController extends LockTypeBasePreferenceController {

    private static final int[] ALLOWED_PASSWORD_QUALITIES =
            new int[]{DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED};

    @VisibleForTesting
    final ConfirmationDialogFragment.ConfirmListener mConfirmListener = arguments -> {
        int userId = UserHandle.myUserId();
        new LockPatternUtils(getContext()).setLockCredential(
                LockscreenCredential.createNone(), getCurrentPassword(), userId);
        getFragmentController().goBack();
    };

    public NoLockPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }


    /**
     * If the dialog to confirm removal of lock was open previously, make sure the listener is
     * restored.
     */
    @Override
    protected void onCreateInternal() {
        super.onCreateInternal();
        ConfirmationDialogFragment dialog =
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        ConfirmationDialogFragment.TAG);
        ConfirmationDialogFragment.resetListeners(
                dialog,
                mConfirmListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);
    }

    @Override
    protected boolean handlePreferenceClicked(Preference preference) {
        if (isCurrentLock()) {
            // No need to show dialog to confirm remove screen lock if screen lock is already
            // removed, which is true if this controller is the current lock.
            getFragmentController().goBack();
        } else {
            getFragmentController().showDialog(
                getConfirmRemoveScreenLockDialogFragment(),
                ConfirmationDialogFragment.TAG);
        }
        return true;
    }

    @Override
    protected Fragment fragmentToOpen() {
        // Selecting this preference does not open a new fragment. Instead it opens a dialog to
        // confirm the removal of the existing lock screen.
        return null;
    }

    @Override
    protected int[] allowedPasswordQualities() {
        return ALLOWED_PASSWORD_QUALITIES;
    }

    private ConfirmationDialogFragment getConfirmRemoveScreenLockDialogFragment() {
        return new ConfirmationDialogFragment.Builder(getContext())
                .setTitle(R.string.remove_screen_lock_title)
                .setMessage(R.string.remove_screen_lock_message)
                .setPositiveButton(R.string.remove_button, mConfirmListener)
                .setNegativeButton(android.R.string.cancel, /* rejectListener= */ null)
                .build();
    }
}
