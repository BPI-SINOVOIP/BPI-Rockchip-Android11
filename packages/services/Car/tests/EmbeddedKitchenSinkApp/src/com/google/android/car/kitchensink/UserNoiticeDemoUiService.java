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

package com.google.android.car.kitchensink;

import android.app.AlertDialog;
import android.app.Service;
import android.car.settings.CarSettings;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.RemoteException;
import android.provider.Settings;
import android.util.Log;
import android.view.WindowManager;

import com.android.internal.annotations.GuardedBy;

/**
 * Example service of implementing UserNoticeUI.
 * <p>IUserNotice and IUserNoticeUI are intentionally accessed / implemented without using the
 * generated code from aidl so that this can be done without accessing hidden API.
 */
public class UserNoiticeDemoUiService extends Service {

    private static final String TAG = UserNoiticeDemoUiService.class.getSimpleName();

    private static final String IUSER_NOTICE_BINDER_DESCRIPTOR = "android.car.user.IUserNotice";
    private static final int IUSER_NOTICE_TR_ON_DIALOG_DISMISSED =
            android.os.IBinder.FIRST_CALL_TRANSACTION;

    private static final String IUSER_NOTICE_UI_BINDER_DESCRIPTOR =
            "android.car.user.IUserNoticeUI";
    private static final int IUSER_NOTICE_UI_BINDER_TR_SET_CALLBACK =
            android.os.IBinder.FIRST_CALL_TRANSACTION;

    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    private final Object mLock = new Object();

    // Do not use IUserNoticeUI class intentionally to show how it can be
    // implemented without accessing the hidden API.
    private IBinder mIUserNoticeUiBinder = new Binder() {
        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            switch (code) {
                case IUSER_NOTICE_UI_BINDER_TR_SET_CALLBACK:
                    data.enforceInterface(IUSER_NOTICE_UI_BINDER_DESCRIPTOR);
                    IBinder binder = data.readStrongBinder();
                    onSetCallbackBinder(binder);
                    return true;
                default:
                    return super.onTransact(code, data, reply, flags);
            }
        }
    };

    @GuardedBy("mLock")
    private IBinder mIUserNoticeService;

    @GuardedBy("mLock")
    private AlertDialog mDialog;

    @Override
    public IBinder onBind(Intent intent) {
        return mIUserNoticeUiBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        stopDialog(true);
        return false;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopDialog(true);
    }

    private void onSetCallbackBinder(IBinder binder) {
        if (binder == null) {
            Log.wtf(TAG, "No binder set in onSetCallbackBinder call", new RuntimeException());
            return;
        }
        mMainHandler.post(() -> {
            synchronized (mLock) {
                mIUserNoticeService = binder;
            }
            startDialog();
        });
    }

    private void startDialog() {
        synchronized (mLock) {
            if (mDialog != null) {
                Log.wtf(TAG, "Dialog already created", new RuntimeException());
                return;
            }
            mDialog = createDialog();
            // Necessary permission is auto-granted by car service before starting this.
            mDialog.getWindow().setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
            mDialog.setCancelable(false);
        }
        mDialog.show();
    }

    private void stopDialog(boolean dismiss) {
        IBinder userNotice;
        AlertDialog dialog;
        synchronized (mLock) {
            userNotice = mIUserNoticeService;
            dialog = mDialog;
            mDialog = null;
            mIUserNoticeService = null;
        }
        if (userNotice != null) {
            sendOnDialogDismissedToCarService(userNotice);
        }
        if (dialog != null && dismiss) {
            dialog.dismiss();
        }
        stopSelf();
    }

    private void sendOnDialogDismissedToCarService(IBinder userNotice) {
        Parcel data = Parcel.obtain();
        data.writeInterfaceToken(IUSER_NOTICE_BINDER_DESCRIPTOR);
        try {
            userNotice.transact(IUSER_NOTICE_TR_ON_DIALOG_DISMISSED, data, null, 0);
        } catch (RemoteException e) {
            Log.w(TAG, "CarService crashed, finish now");
            stopSelf();
        }
    }

    private AlertDialog createDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        AlertDialog dialog = builder.setMessage(R.string.usernotice)
                .setPositiveButton(R.string.dismiss_now, (DialogInterface d, int w) -> {
                    stopDialog(true);
                })
                .setNegativeButton(R.string.dismiss_forever, (DialogInterface d, int w) -> {
                    Settings.Secure.putInt(getContentResolver(),
                            CarSettings.Secure.KEY_ENABLE_INITIAL_NOTICE_SCREEN_TO_USER,
                            /* enable= */ 0);
                    stopDialog(true);
                })
                .setOnDismissListener((DialogInterface d) -> {
                    stopDialog(false);
                })
                .create();
        return dialog;
    }
}
