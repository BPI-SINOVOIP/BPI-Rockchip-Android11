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

package android.telecom.cts.redirectiontestapp;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.telecom.PhoneAccountHandle;
import android.os.IBinder;
import android.telecom.CallRedirectionService;
import android.text.TextUtils;
import android.util.Log;

public class CtsCallRedirectionServiceController extends Service {
    private static final String TAG = CallRedirectionService.class.getSimpleName();
    public static final String CONTROL_INTERFACE_ACTION =
            "android.telecom.cts.redirectiontestapp.ACTION_CONTROL_CALL_REDIRECTION_SERVICE";
    public static final ComponentName CONTROL_INTERFACE_COMPONENT =
            ComponentName.unflattenFromString(
                    "android.telecom.cts.redirectiontestapp/.CtsCallRedirectionServiceController");

    // Constants for call redirection decisions
    public static final int NO_DECISION_YET = 0;
    public static final int RESPONSE_TIMEOUT = 1;
    public static final int PLACE_CALL_UNMODIFIED = 2;
    public static final int PLACE_REDIRECTED_CALL = 3;
    public static final int CANCEL_CALL = 4;

    private int mDecision = NO_DECISION_YET;

    // Redirection information, only valid if decision is PLACE_REDIRECTED_CALL.
    private Uri mTargetHandle = null;
    private Uri mDestinationUri = null;
    private PhoneAccountHandle mRedirectedPhoneAccount = null;
    private PhoneAccountHandle mOriginalPhoneAccount = null;
    private boolean mConfirmFirst = false;

    private static CtsCallRedirectionServiceController sCallRedirectionServiceController = null;

    private final IBinder mControllerInterface = new ICtsCallRedirectionServiceController.Stub() {
                @Override
                public void reset() {
                    mDecision = NO_DECISION_YET;
                }

                @Override
                public void setRedirectCall(Uri targetHandle,
                                            PhoneAccountHandle redirectedPhoneAccount,
                                            boolean confirmFirst) {
                    Log.i(TAG, "redirectCall");
                    mDecision = PLACE_REDIRECTED_CALL;
                    mTargetHandle = targetHandle;
                    mRedirectedPhoneAccount = redirectedPhoneAccount;
                    mConfirmFirst = confirmFirst;
                }

                @Override
                public void setCancelCall() {
                    Log.i(TAG, "cancelCall");
                    mDecision = CANCEL_CALL;
                }

                @Override
                public void setPlaceCallUnmodified() {
                    Log.i(TAG, "placeCallUnmodified");
                    mDecision = PLACE_CALL_UNMODIFIED;
                }
            };

    public static CtsCallRedirectionServiceController getInstance() {
        return sCallRedirectionServiceController;
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (CONTROL_INTERFACE_ACTION.equals(intent.getAction())) {
            Log.i(TAG, "onBind: returning control interface");
            sCallRedirectionServiceController = this;
            return mControllerInterface;
        }
        Log.i(TAG, "onBind: ACTION is not matched");
        return null;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        sCallRedirectionServiceController = null;
        return false;
    }

    public int getCallRedirectionDecision() {
        return mDecision;
    }

    public Uri getTargetHandle() {
        return mTargetHandle;
    }

    public PhoneAccountHandle getTargetPhoneAccount() {
        return mRedirectedPhoneAccount != null ? mRedirectedPhoneAccount : mOriginalPhoneAccount;
    }

    public void setOriginalPhoneAccount(PhoneAccountHandle originalPhoneAccount) {
        mOriginalPhoneAccount = originalPhoneAccount;
    }

    public void setDestinationUri(Uri destinationUri) {
        mDestinationUri = destinationUri;
    }

    public boolean isConfirmFirst() {
        return mConfirmFirst;
    }
}
