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
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.apps.common.LetterTileDrawable;
import com.android.car.dialer.R;
import com.android.car.dialer.ui.view.ContactAvatarOutputlineProvider;
import com.android.car.telephony.common.CallDetail;
import com.android.car.telephony.common.TelecomUtils;

import java.util.concurrent.CompletableFuture;

/**
 * A fragment that displays information about onhold call.
 */
public class OnHoldCallUserProfileFragment extends Fragment {

    private TextView mTitle;
    private ImageView mAvatarView;
    private View mSwapCallsView;
    private LiveData<Call> mPrimaryCallLiveData;
    private CompletableFuture<Void> mPhoneNumberInfoFuture;
    private LetterTileDrawable mDefaultAvatar;
    private Chronometer mTimeTextView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mDefaultAvatar = TelecomUtils.createLetterTile(getContext(), null, null);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View fragmentView = inflater.inflate(R.layout.onhold_user_profile, container, false);

        mTitle = fragmentView.findViewById(R.id.title);
        mAvatarView = fragmentView.findViewById(R.id.icon);
        mAvatarView.setOutlineProvider(ContactAvatarOutputlineProvider.get());

        mSwapCallsView = fragmentView.findViewById(R.id.swap_calls_view);
        mSwapCallsView.setOnClickListener(v -> swapCalls());

        InCallViewModel inCallViewModel = ViewModelProviders.of(getActivity()).get(
                InCallViewModel.class);
        inCallViewModel.getSecondaryCallDetail().observe(this, this::updateProfile);
        mPrimaryCallLiveData = inCallViewModel.getPrimaryCall();

        mTimeTextView = fragmentView.findViewById(R.id.time);
        inCallViewModel.getSecondaryCallConnectTime().observe(this, this::updateConnectTime);

        return fragmentView;
    }

    /** Presents the onhold call duration. */
    protected void updateConnectTime(Long connectTime) {
        if (connectTime == null) {
            mTimeTextView.stop();
            mTimeTextView.setText("");
            return;
        }
        mTimeTextView.setBase(connectTime
                - System.currentTimeMillis() + SystemClock.elapsedRealtime());
        mTimeTextView.start();
    }

    private void updateProfile(@Nullable CallDetail callDetail) {
        if (callDetail == null) {
            return;
        }

        mAvatarView.setImageDrawable(mDefaultAvatar);

        if (mPhoneNumberInfoFuture != null) {
            mPhoneNumberInfoFuture.cancel(true);
        }

        if (callDetail.isConference()) {
            mTitle.setText(getString(R.string.ongoing_conf_title));
            return;
        }

        String number = callDetail.getNumber();
        mTitle.setText(TelecomUtils.getFormattedNumber(getContext(), number));

        mPhoneNumberInfoFuture = TelecomUtils.getPhoneNumberInfo(getContext(), number)
                .thenAcceptAsync((info) -> {
                    mTitle.setText(info.getDisplayName());
                    TelecomUtils.setContactBitmapAsync(getContext(), mAvatarView,
                            info.getAvatarUri(), info.getInitials(), info.getDisplayName());
                }, getContext().getMainExecutor());
    }

    private void swapCalls() {
        // Hold primary call and the secondary call will automatically come to the foreground.
        if (mPrimaryCallLiveData.getValue().getState() != Call.STATE_HOLDING) {
            mPrimaryCallLiveData.getValue().hold();
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mPhoneNumberInfoFuture != null) {
            mPhoneNumberInfoFuture.cancel(true);
        }
    }
}
