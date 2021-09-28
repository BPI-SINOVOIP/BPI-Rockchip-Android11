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

package com.android.car.dialer.ui.activecall;

import android.content.Intent;
import android.os.Bundle;
import android.telecom.Call;

import androidx.core.util.Pair;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.Constants;
import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.notification.InCallNotificationController;
import com.android.car.telephony.common.CallDetail;

/** Activity for ongoing call and incoming call. */
public class InCallActivity extends FragmentActivity {
    private static final String TAG = "CD.InCallActivity";
    private Fragment mOngoingCallFragment;
    private Fragment mOngoingConfCallFragment;
    private Fragment mIncomingCallFragment;

    private InCallViewModel mInCallViewModel;
    private MutableLiveData<Boolean> mShowIncomingCall;
    private LiveData<Call> mIncomingCallLiveData;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        L.d(TAG, "onCreate");

        setContentView(R.layout.in_call_activity);

        mOngoingCallFragment = getSupportFragmentManager().findFragmentById(
                R.id.ongoing_call_fragment);
        mOngoingConfCallFragment = getSupportFragmentManager().findFragmentById(
                R.id.ongoing_conf_call_fragment);
        mIncomingCallFragment = getSupportFragmentManager().findFragmentById(
                R.id.incoming_call_fragment);

        // Initially hide all fragments to prevent animation flicker
        getSupportFragmentManager().beginTransaction()
                .hide(mIncomingCallFragment)
                .hide(mOngoingCallFragment)
                .hide(mOngoingConfCallFragment)
                .commit();

        mShowIncomingCall = new MutableLiveData<>();
        mInCallViewModel = ViewModelProviders.of(this).get(InCallViewModel.class);
        mIncomingCallLiveData = LiveDataFunctions.iff(mShowIncomingCall,
                mInCallViewModel.getIncomingCall());
        LiveDataFunctions.pair(mInCallViewModel.getPrimaryCallDetail(),
                mIncomingCallLiveData).observe(this, this::updateVisibility);

        handleIntent();
    }

    @Override
    protected void onStop() {
        super.onStop();
        L.d(TAG, "onStop");
        if (mIncomingCallLiveData.getValue() != null) {
            InCallNotificationController.get()
                    .showInCallNotification(mIncomingCallLiveData.getValue());
        }
    }

    @Override
    protected void onNewIntent(Intent i) {
        super.onNewIntent(i);
        L.d(TAG, "onNewIntent");
        setIntent(i);
        handleIntent();
    }

    private void updateVisibility(Pair<CallDetail, Call> callList) {
        CallDetail detail = callList.first;
        Call incomingCall = callList.second;

        if (detail == null && incomingCall == null) {
            L.d(TAG, "No call to show. Finish InCallActivity");
            finish();
            return;
        }

        FragmentTransaction ft = getSupportFragmentManager().beginTransaction();

        if (incomingCall == null) {
            ft.show(detail.isConference() ? mOngoingConfCallFragment : mOngoingCallFragment)
                    .hide(detail.isConference() ? mOngoingCallFragment : mOngoingConfCallFragment)
                    .hide(mIncomingCallFragment);

            mShowIncomingCall.setValue(false);
            setIntent(null);
        } else {
            ft.show(mIncomingCallFragment)
                    .hide(mOngoingCallFragment)
                    .hide(mOngoingConfCallFragment);
        }

        ft.setCustomAnimations(android.R.anim.fade_in, android.R.anim.fade_out).commit();
    }

    private void handleIntent() {
        Intent intent = getIntent();

        if (intent != null) {
            mShowIncomingCall.setValue(
                    getIntent().getBooleanExtra(Constants.Intents.EXTRA_SHOW_INCOMING_CALL, false));
            mInCallViewModel.getDialpadOpenState().setValue(
                    getIntent().getBooleanExtra(Constants.Intents.EXTRA_SHOW_DIALPAD, false));
        } else {
            mShowIncomingCall.setValue(false);
        }
    }
}
