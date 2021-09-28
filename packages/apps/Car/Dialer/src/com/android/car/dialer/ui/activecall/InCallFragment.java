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

import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.telecom.Call;
import android.text.TextUtils;
import android.view.View;
import android.widget.Chronometer;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.apps.common.BackgroundImageView;
import com.android.car.apps.common.LetterTileDrawable;
import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.ui.view.ContactAvatarOutputlineProvider;
import com.android.car.telephony.common.CallDetail;
import com.android.car.telephony.common.TelecomUtils;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.DataSource;
import com.bumptech.glide.load.engine.GlideException;
import com.bumptech.glide.request.RequestListener;
import com.bumptech.glide.request.RequestOptions;
import com.bumptech.glide.request.target.Target;

import java.util.concurrent.CompletableFuture;

/** A fragment that displays information about a call with actions. */
public abstract class InCallFragment extends Fragment {
    private static final String TAG = "CD.InCallFragment";

    private View mUserProfileContainerView;
    private TextView mPhoneNumberView;
    private Chronometer mUserProfileCallStateText;
    private TextView mNameView;
    private ImageView mAvatarView;
    private BackgroundImageView mBackgroundImage;
    private LetterTileDrawable mDefaultAvatar;
    private CompletableFuture<Void> mPhoneNumberInfoFuture;
    private String mCurrentNumber;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mDefaultAvatar = TelecomUtils.createLetterTile(getContext(), null, null);
    }

    /**
     * Shared UI elements between ongoing call and incoming call page: {@link BackgroundImageView}
     * and {@link R.layout#user_profile_large}.
     */
    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        mUserProfileContainerView = view.findViewById(R.id.user_profile_container);
        mNameView = mUserProfileContainerView.findViewById(R.id.user_profile_title);
        mAvatarView = mUserProfileContainerView.findViewById(R.id.user_profile_avatar);
        mAvatarView.setOutlineProvider(ContactAvatarOutputlineProvider.get());
        mPhoneNumberView = mUserProfileContainerView.findViewById(R.id.user_profile_phone_number);
        mUserProfileCallStateText = mUserProfileContainerView.findViewById(
                R.id.user_profile_call_state);
        mBackgroundImage = view.findViewById(R.id.background_image);
    }

    /** Presents the user profile. */
    protected void bindUserProfileView(@Nullable CallDetail callDetail) {
        L.i(TAG, "bindUserProfileView: %s", callDetail);
        if (callDetail == null) {
            return;
        }

        String number = callDetail.getNumber();
        if (mCurrentNumber != null && mCurrentNumber.equals(number)) {
            return;
        }
        mCurrentNumber = number;

        if (mPhoneNumberInfoFuture != null) {
            mPhoneNumberInfoFuture.cancel(true);
        }

        mNameView.setText(TelecomUtils.getFormattedNumber(getContext(), number));
        mPhoneNumberView.setVisibility(View.GONE);
        mAvatarView.setImageDrawable(mDefaultAvatar);

        InCallViewModel inCallViewModel = ViewModelProviders.of(this).get(
                InCallViewModel.class);
        inCallViewModel.getContactListLiveData().observe(this, contacts -> updateProfile(number));
    }

    private void updateProfile(String number) {
        mPhoneNumberInfoFuture = TelecomUtils.getPhoneNumberInfo(getContext(), number)
            .thenAcceptAsync((info) -> {
                if (getContext() == null) {
                    return;
                }

                String nameViewText = info.getDisplayName();
                mNameView.setText(nameViewText);

                String phoneNumberLabel = info.getTypeLabel();
                if (!phoneNumberLabel.isEmpty()) {
                    phoneNumberLabel += " ";
                }

                String bidiWrappedLabel = phoneNumberLabel + TelecomUtils.getBidiWrappedNumber(
                        TelecomUtils.getFormattedNumber(getContext(), number));
                phoneNumberLabel += TelecomUtils.getFormattedNumber(getContext(), number);

                if (!TextUtils.isEmpty(phoneNumberLabel)
                        && !phoneNumberLabel.equals(info.getDisplayName())) {
                    mPhoneNumberView.setText(bidiWrappedLabel);
                    mPhoneNumberView.setVisibility(View.VISIBLE);
                } else {
                    mPhoneNumberView.setVisibility(View.GONE);
                }

                LetterTileDrawable letterTile = TelecomUtils.createLetterTile(
                        getContext(), info.getInitials(), info.getDisplayName());

                Glide.with(this)
                        .load(info.getAvatarUri())
                        .apply(new RequestOptions().centerCrop().error(letterTile))
                        .listener(new RequestListener<Drawable>() {
                            @Override
                            public boolean onLoadFailed(@Nullable GlideException e, Object model,
                                    Target<Drawable> target, boolean isFirstResource) {
                                mBackgroundImage.setAlpha(getResources().getFloat(
                                        R.dimen.config_background_image_error_alpha));
                                mBackgroundImage.setBackgroundColor(letterTile.getColor());
                                return false;
                            }

                            @Override
                            public boolean onResourceReady(Drawable resource, Object model,
                                    Target<Drawable> target, DataSource dataSource,
                                    boolean isFirstResource) {
                                mBackgroundImage.setAlpha(getResources().getFloat(
                                        R.dimen.config_background_image_alpha));
                                mBackgroundImage.setBackgroundDrawable(resource, false);
                                return false;
                            }
                        }).into(mAvatarView);
            }, getContext().getMainExecutor());
    }

    /** Presents the call state and call duration. */
    protected void updateCallDescription(@Nullable Pair<Integer, Long> callStateAndConnectTime) {
        if (callStateAndConnectTime == null || callStateAndConnectTime.first == null) {
            mUserProfileCallStateText.stop();
            mUserProfileCallStateText.setText("");
            return;
        }
        if (callStateAndConnectTime.first == Call.STATE_ACTIVE) {
            mUserProfileCallStateText.setBase(callStateAndConnectTime.second
                    - System.currentTimeMillis() + SystemClock.elapsedRealtime());
            mUserProfileCallStateText.start();
        } else {
            mUserProfileCallStateText.stop();
            mUserProfileCallStateText.setText(TelecomUtils.callStateToUiString(getContext(),
                    callStateAndConnectTime.first));
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
