/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.settings.system;

import static android.os.UserHandle.USER_SYSTEM;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.os.UserManager;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.internal.widget.ILockSettings;
import com.android.internal.widget.LockPatternUtils;
import com.android.internal.widget.LockscreenCredential;
import com.android.tv.settings.dialog.PinDialogFragment;
import com.android.tv.settings.users.RestrictedProfilePinDialogFragment;
import com.android.tv.settings.users.RestrictedProfilePinService;

import java.util.Objects;

/**
 * Triggered instead of the home screen when user-selected home app isn't encryption aware.
 */
public class FallbackHome extends Activity implements PinDialogFragment.ResultListener {

    private static final String TAG = "FallbackHome";

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            maybeFinish();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        registerReceiver(mReceiver, new IntentFilter(Intent.ACTION_USER_UNLOCKED));

        maybeStartPinDialog();
        maybeFinish();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(mReceiver);
    }

    private BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            maybeFinish();
        }
    };

    void maybeFinish() {
        if (isUserUnlocked()) {
            final Intent homeIntent = new Intent(Intent.ACTION_MAIN)
                    .addCategory(Intent.CATEGORY_HOME);
            final ResolveInfo homeInfo = getPackageManager().resolveActivity(homeIntent, 0);
            if (Objects.equals(getPackageName(), homeInfo.activityInfo.packageName)) {
                Log.d(TAG, "User unlocked but no home; let's hope someone enables one soon?");
                mHandler.sendEmptyMessageDelayed(0, 500);
            } else {
                Log.d(TAG, "User unlocked and real home found; let's go!");
                finish();
            }
        }
    }

    /**
     * If we have file-based encryption and a restricted profile we must request PIN entry on boot.
     *
     * Unlike a normal password, the restricted profile PIN is set on USER_OWNER in order to
     * prevent switching out. Under FBE this means that the underlying USER_SYSTEM will remain
     * encrypted and in RUNNING_LOCKED state. In order for various system functions to work
     * we will need to decrypt first.
     */
    @VisibleForTesting
    void maybeStartPinDialog() {
        if (isUserUnlocked() || !hasLockscreenSecurity(getUserId()) || !isFileEncryptionEnabled()) {
            return;
        }

        unlockDevice();
    }

    private void unlockDevice() {
        LockscreenCredential pin = getPinFromSharedPreferences();

        if (pin == null || pin.isNone()) {
            showPinDialogToUnlockDevice();
        } else {
            // Give LockSettings the pin. This unlocks the device.
            unlockDeviceWithPin(pin);
        }
    }

    @VisibleForTesting
    void unlockDeviceWithPin(LockscreenCredential pin) {
        try {
            getLockSettings().checkCredential(pin, USER_SYSTEM, null);
        } catch (RemoteException e) {
            e.printStackTrace();
        }
    }

    @VisibleForTesting
    void showPinDialogToUnlockDevice() {
        // Prompt the user with a dialog to dial the pin and unlock the device.
        RestrictedProfilePinDialogFragment restrictedProfilePinDialogFragment =
                RestrictedProfilePinDialogFragment.newInstance(
                        PinDialogFragment.PIN_DIALOG_TYPE_ENTER_PIN);
        restrictedProfilePinDialogFragment.show(getFragmentManager(),
                PinDialogFragment.DIALOG_TAG);
    }

    @VisibleForTesting
    LockscreenCredential getPinFromSharedPreferences() {
        SharedPreferences sp = getSharedPreferences(RestrictedProfilePinService.PIN_STORE_NAME,
                Context.MODE_PRIVATE);
        return LockscreenCredential.createPinOrNone(
                sp.getString(RestrictedProfilePinService.PIN_STORE_NAME, null));
    }

    @VisibleForTesting
    boolean isUserUnlocked() {
        return getSystemService(UserManager.class).isUserUnlocked();
    }

    @VisibleForTesting
    boolean hasLockscreenSecurity(final int userId) {
        final LockPatternUtils lpu = new LockPatternUtils(this);
        return lpu.isLockPasswordEnabled(userId) || lpu.isLockPatternEnabled(userId);
    }

    @VisibleForTesting
    boolean isFileEncryptionEnabled() {
        return LockPatternUtils.isFileEncryptionEnabled();
    }

    @VisibleForTesting
    ILockSettings getLockSettings() {
        return new LockPatternUtils(this).getLockSettings();
    }

    @Override
    public void pinFragmentDone(int requestCode, boolean success) {
        maybeStartPinDialog();
        maybeFinish();
    }
}
