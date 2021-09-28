/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.cts.verifier.telecom;

import android.content.Context;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.telecom.Connection;
import android.telecom.PhoneAccount;
import android.telecom.TelecomManager;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

/**
 * This test verifies functionality associated with the Self-Managed
 * {@link android.telecom.ConnectionService} APIs.  It ensures that Telecom will show an incoming
 * call UI when a new incoming self-managed call is added when there is already an ongoing managed
 * call or when there is an ongoing self-managed call in another app.
 */
public class SelfManagedIncomingCallTestActivity extends PassFailButtons.Activity {
    private static final String TAG = "SelfManagedIncomingCall";
    private Uri TEST_DIAL_NUMBER_1 = Uri.fromParts("tel", "6505551212", null);
    private Uri TEST_DIAL_NUMBER_2 = Uri.fromParts("tel", "4085551212", null);

    private ImageView mStep1Status;
    private Button mRegisterPhoneAccount;
    private ImageView mStep2Status;
    private Button mVerifyCall;
    private Button mPlaceCall;
    private ImageView mStep3Status;

    private CtsConnection.Listener mConnectionListener = new CtsConnection.Listener() {
        @Override
        void onShowIncomingCallUi(CtsConnection connection) {
            // The system should have displayed the incoming call UI; this is a fail.
            Log.w(TAG, "Step 3 fail - got unexpected onShowIncomingCallUi");
            mStep3Status.setImageResource(R.drawable.fs_error);
            getPassButton().setEnabled(false);
        };

        @Override
        void onAnswer(CtsConnection connection, int videoState) {
            // Call was answered, so disconnect it now.
            Log.i(TAG, "Step 3 - Incoming call answered.");
            connection.onDisconnect();
        };

        @Override
        void onDisconnect(CtsConnection connection) {
            super.onDisconnect(connection);

            cleanupConnectionServices();

            TelecomManager telecomManager =
                    (TelecomManager) getSystemService(Context.TELECOM_SERVICE);
            if (telecomManager == null || !telecomManager.isInManagedCall()) {
                // Should still be in a managed call; only one would need to be disconnected.
                Log.w(TAG, "Step 3 fail - not in managed call as expected.");
                mStep3Status.setImageResource(R.drawable.fs_error);
                return;
            }
            Log.i(TAG, "Step 3 pass - call disconnected");
            mStep3Status.setImageResource(R.drawable.fs_good);
            getPassButton().setEnabled(true);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        View view = getLayoutInflater().inflate(R.layout.telecom_self_managed_answer, null);
        setContentView(view);
        setInfoResources(R.string.telecom_incoming_self_mgd_test,
                R.string.telecom_incoming_self_mgd_info, -1);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        mStep1Status = view.findViewById(R.id.step_1_status);
        mRegisterPhoneAccount = view.findViewById(R.id.telecom_incoming_self_mgd_register_button);
        mRegisterPhoneAccount.setOnClickListener(v -> {
            PhoneAccountUtils.registerTestSelfManagedPhoneAccount(this);
            PhoneAccount account = PhoneAccountUtils.getSelfManagedPhoneAccount(this);
            if (account != null &&
                    account.isEnabled() &&
                    account.hasCapabilities(PhoneAccount.CAPABILITY_SELF_MANAGED)) {
                mRegisterPhoneAccount.setEnabled(false);
                mVerifyCall.setEnabled(true);
                Log.i(TAG, "Step 1 pass - account registered");
                mStep1Status.setImageResource(R.drawable.fs_good);
            } else {
                Log.w(TAG, "Step 1 fail - account not registered");
                mStep1Status.setImageResource(R.drawable.fs_error);
            }
        });

        mStep2Status = view.findViewById(R.id.step_2_status);
        mVerifyCall = view.findViewById(R.id.telecom_incoming_self_mgd_verify_call_button);
        mVerifyCall.setOnClickListener(v -> {
            TelecomManager telecomManager =
                    (TelecomManager) getSystemService(Context.TELECOM_SERVICE);
            if (telecomManager == null || !telecomManager.isInManagedCall()) {
                Log.w(TAG, "Step 2 fail - expected to be in a managed call");
                mStep2Status.setImageResource(R.drawable.fs_error);
                mPlaceCall.setEnabled(false);
            } else {
                mStep2Status.setImageResource(R.drawable.fs_good);
                Log.i(TAG, "Step 2 pass - device in a managed call");
                mVerifyCall.setEnabled(false);
                mPlaceCall.setEnabled(true);
            }
        });


        // telecom_incoming_self_mgd_place_call_button
        mPlaceCall = view.findViewById(R.id.telecom_incoming_self_mgd_place_call_button);
        mPlaceCall.setOnClickListener(v -> {
            (new AsyncTask<Void, Void, Throwable>() {
                @Override
                protected Throwable doInBackground(Void... params) {
                    try {
                        Bundle extras = new Bundle();
                        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS,
                                TEST_DIAL_NUMBER_1);
                        TelecomManager telecomManager =
                                (TelecomManager) getSystemService(Context.TELECOM_SERVICE);
                        if (telecomManager == null) {
                            Log.w(TAG, "Step 2 fail - telecom manager null");
                            mStep2Status.setImageResource(R.drawable.fs_error);
                            return new Throwable("Could not get telecom service.");
                        }
                        telecomManager.addNewIncomingCall(
                                PhoneAccountUtils.TEST_SELF_MANAGED_PHONE_ACCOUNT_HANDLE, extras);

                        CtsConnectionService ctsConnectionService =
                                CtsConnectionService.waitForAndGetConnectionService();
                        if (ctsConnectionService == null) {
                            Log.w(TAG, "Step 2 fail - ctsConnectionService null");
                            mStep2Status.setImageResource(R.drawable.fs_error);
                            return new Throwable("Could not get connection service.");
                        }

                        CtsConnection connection = ctsConnectionService.waitForAndGetConnection();
                        if (connection == null) {
                            Log.w(TAG, "Step 2 fail - could not get connection");
                            mStep2Status.setImageResource(R.drawable.fs_error);
                            return new Throwable("Could not get connection.");
                        }
                        connection.addListener(mConnectionListener);

                        // Wait until the connection knows its audio state changed; at this point
                        // Telecom knows about the connection and can answer.
                        connection.waitForAudioStateChanged();
                        // Make it active to simulate an answer.
                        connection.setActive();

                        // Removes the hold capability of the self-managed call, so that the follow
                        // incoming call can trigger the incoming call UX that allow user to answer
                        // the incoming call to disconnect the ongoing call.
                        int capabilities = connection.getConnectionCapabilities();
                        capabilities &= ~Connection.CAPABILITY_HOLD;
                        connection.setConnectionCapabilities(capabilities);
                        Log.w(TAG, "Step 2 - connection added");
                        return null;
                    } catch (Throwable t) {
                        return t;
                    }
                }

                @Override
                protected void onPostExecute(Throwable t) {
                    if (t == null) {
                        mStep2Status.setImageResource(R.drawable.fs_good);
                        mPlaceCall.setEnabled(false);
                    } else {
                        Log.i(TAG, "Step 2 pass - connection added");
                        mStep2Status.setImageResource(R.drawable.fs_error);
                    }
                }
            }).execute();


        });

        mStep3Status = view.findViewById(R.id.step_3_status);
        mVerifyCall.setEnabled(false);
        mPlaceCall.setEnabled(false);
    }

    private void cleanupConnectionServices() {
        CtsSelfManagedConnectionService ctsSelfConnSvr =
                CtsSelfManagedConnectionService.getConnectionService();
        if (ctsSelfConnSvr != null) {
            ctsSelfConnSvr.getConnections()
                    .stream()
                    .forEach((c) -> {
                        c.onDisconnect();
                    });
        }

        CtsConnectionService ctsConnectionService =
                CtsConnectionService.getConnectionService();
        if (ctsConnectionService != null) {
            ctsConnectionService.getConnections()
                    .stream()
                    .forEach((c) -> {
                        c.onDisconnect();
                    });
        }
        PhoneAccountUtils.unRegisterTestSelfManagedPhoneAccount(this);
    }
}
