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

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.graphics.drawable.Icon;
import android.os.IBinder;
import android.telecom.Call;
import android.telecom.CallRedirectionService;
import android.telecom.PhoneAccountHandle;

import android.text.TextUtils;
import android.util.Log;

import java.util.List;

/**
 * Provides a CTS-test implementation of {@link CallRedirectionService}.
 * This emulates a third-party implementation of {@link CallRedirectionService}.
 */
public class CtsCallRedirectionService extends CallRedirectionService {
    private static final String TAG = CtsCallRedirectionService.class.getSimpleName();

    @Override
    public void onPlaceCall(Uri handle, PhoneAccountHandle initialPhoneAccount,
            boolean allowInteractiveResponse) {
        Log.i(TAG, "onPlaceCall");
        CtsCallRedirectionServiceController controller =
                CtsCallRedirectionServiceController.getInstance();
        if (controller != null) {
            controller.setDestinationUri(handle);
            controller.setOriginalPhoneAccount(initialPhoneAccount);
            int decision = controller.getCallRedirectionDecision();
            if (decision == CtsCallRedirectionServiceController.PLACE_CALL_UNMODIFIED) {
                placeCallUnmodified();
            } else if (decision == CtsCallRedirectionServiceController.CANCEL_CALL) {
                cancelCall();
            } else if (decision == CtsCallRedirectionServiceController.PLACE_REDIRECTED_CALL) {
                redirectCall(controller.getTargetHandle(), controller.getTargetPhoneAccount(),
                        controller.isConfirmFirst());
            }
        } else {
            Log.w(TAG, "No control interface.");
        }
    }
}
