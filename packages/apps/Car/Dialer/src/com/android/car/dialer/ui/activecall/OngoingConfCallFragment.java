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

import android.os.Bundle;
import android.os.SystemClock;
import android.telecom.Call;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Chronometer;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Pair;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.dialer.R;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

/**
 * A fragment that displays information about an on-going call with options to hang up.
 */
public class OngoingConfCallFragment extends Fragment {
    private static final String TAG = "CD.OngoingConfCallFrag";

    private Fragment mDialpadFragment;
    private Fragment mOnholdCallFragment;
    private View mConferenceCallProfilesView;
    private MutableLiveData<Boolean> mDialpadState;
    private CarUiRecyclerView mRecyclerView;
    private Chronometer mConferenceTimeTextView;
    private TextView mConferenceTitle;

    private ConferenceProfileAdapter mConfProfileAdapter;

    private String mConferenceTitleString;
    private String mConfStrTitleFormat;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mConferenceTitleString = getString(R.string.ongoing_conf_title);
        mConfStrTitleFormat = getString(R.string.ongoing_conf_title_format);
    }

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View fragmentView = inflater.inflate(R.layout.ongoing_conf_call_fragment,
                container, false);

        mOnholdCallFragment = getChildFragmentManager().findFragmentById(R.id.onhold_user_profile);
        mDialpadFragment = getChildFragmentManager().findFragmentById(R.id.incall_dialpad_fragment);
        mConferenceCallProfilesView = fragmentView.findViewById(R.id.conference_profiles);
        mRecyclerView = mConferenceCallProfilesView.findViewById(R.id.recycler_view);
        mConferenceTimeTextView = mConferenceCallProfilesView.findViewById(R.id.call_duration);
        mConferenceTitle = mConferenceCallProfilesView.findViewById(R.id.conference_title);

        if (mConfProfileAdapter == null) {
            mConfProfileAdapter = new ConferenceProfileAdapter(getContext());
        }
        mRecyclerView.setAdapter(mConfProfileAdapter);

        InCallViewModel inCallViewModel = ViewModelProviders.of(getActivity()).get(
                InCallViewModel.class);

        inCallViewModel.getCallStateAndConnectTime().observe(this,
                this::updateCallDescription);

        inCallViewModel.getConferenceCallDetailList().observe(this, list -> {
            mConfProfileAdapter.setConferenceList(list);
            updateTitle(list.size());
        });

        mDialpadState = inCallViewModel.getDialpadOpenState();
        mDialpadState.observe(this, isDialpadOpen -> {
            if (isDialpadOpen) {
                onOpenDialpad();
            } else {
                onCloseDialpad();
            }
        });

        inCallViewModel.shouldShowOnholdCall().observe(this,
                this::updateOnholdCallFragmentVisibility);

        return fragmentView;
    }

    @VisibleForTesting
    void onOpenDialpad() {
        getChildFragmentManager().beginTransaction()
                .show(mDialpadFragment)
                .commit();
        mConferenceCallProfilesView.setVisibility(View.GONE);
    }

    @VisibleForTesting
    void onCloseDialpad() {
        getChildFragmentManager().beginTransaction()
                .hide(mDialpadFragment)
                .commit();
        mConferenceCallProfilesView.setVisibility(View.VISIBLE);
    }

    private void updateTitle(int numParticipants) {
        String title = String.format(mConfStrTitleFormat, mConferenceTitleString, numParticipants);
        mConferenceTitle.setText(title);
    }

    /** Presents the call state and call duration. */
    private void updateCallDescription(@Nullable Pair<Integer, Long> callStateAndConnectTime) {
        if (callStateAndConnectTime == null || callStateAndConnectTime.first == null) {
            mConferenceTimeTextView.stop();
            mConferenceTimeTextView.setText("");
            return;
        }
        if (callStateAndConnectTime.first == Call.STATE_ACTIVE) {
            mConferenceTimeTextView.setBase(callStateAndConnectTime.second
                    - System.currentTimeMillis() + SystemClock.elapsedRealtime());
            mConferenceTimeTextView.start();
        } else {
            mConferenceTimeTextView.stop();
            mConferenceTimeTextView.setText(TelecomUtils.callStateToUiString(getContext(),
                    callStateAndConnectTime.first));
        }
    }

    private void updateOnholdCallFragmentVisibility(Boolean showOnholdCall) {
        if (showOnholdCall) {
            getChildFragmentManager().beginTransaction().show(mOnholdCallFragment).commit();
        } else {
            getChildFragmentManager().beginTransaction().hide(mOnholdCallFragment).commit();
        }
    }
}
