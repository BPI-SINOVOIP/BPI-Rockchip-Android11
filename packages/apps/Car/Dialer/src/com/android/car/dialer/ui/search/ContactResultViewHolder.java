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
 * limitations under the License.
 */

package com.android.car.dialer.ui.search;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.apps.common.util.ViewUtils;
import com.android.car.dialer.R;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.ui.common.ContactResultsLiveData;
import com.android.car.dialer.ui.common.DialerUtils;
import com.android.car.dialer.ui.common.OnItemClickedListener;
import com.android.car.dialer.ui.common.QueryStyle;
import com.android.car.dialer.ui.view.ContactAvatarOutputlineProvider;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.TelecomUtils;

import com.bumptech.glide.Glide;

/**
 * A {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} that will parse relevant views out
 * of a {@code contact_result} layout.
 */
public class ContactResultViewHolder extends RecyclerView.ViewHolder {
    private static final String TAG = "CD.ContactResultVH";

    private final Context mContext;
    private final View mContactCard;
    private final TextView mContactName;
    private final TextView mContactNumber;
    private final ImageView mContactPicture;
    @Nullable
    private final ContactResultsAdapter.OnShowContactDetailListener mOnShowContactDetailListener;
    @Nullable
    private final OnItemClickedListener mOnItemClickedListener;

    public ContactResultViewHolder(View view,
            @Nullable ContactResultsAdapter.OnShowContactDetailListener onShowContactDetailListener,
            @Nullable OnItemClickedListener onItemClickedListener) {
        super(view);
        mContext = view.getContext();
        mContactCard = view.findViewById(R.id.contact_result);
        mContactName = view.findViewById(R.id.contact_name);
        mContactNumber = view.findViewById(R.id.phone_number);
        mContactPicture = view.findViewById(R.id.contact_picture);
        if (mContactPicture != null) {
            mContactPicture.setOutlineProvider(ContactAvatarOutputlineProvider.get());
        }
        mOnShowContactDetailListener = onShowContactDetailListener;
        mOnItemClickedListener = onItemClickedListener;
    }

    /**
     * Populates the view that is represented by this ViewHolder with the information in the
     * provided {@link Contact}.
     */
    public void bindSearchResult(ContactResultsLiveData.ContactResultListItem contactResult,
            Integer sortMethod) {
        Contact contact = contactResult.getContact();

        ViewUtils.setText(mContactName,
                TelecomUtils.isSortByFirstName(sortMethod) ? contact.getDisplayName()
                        : contact.getDisplayNameAlt());
        TelecomUtils.setContactBitmapAsync(mContext, mContactPicture, contact, sortMethod);

        if (DialerUtils.hasContactDetail(itemView.getResources(), contact)) {
            mContactCard.setOnClickListener(
                    v -> {
                        if (mOnShowContactDetailListener != null) {
                            mOnShowContactDetailListener.onShowContactDetail(contact);
                        }
                    });
        } else {
            itemView.setEnabled(false);
        }
    }

    /**
     * Populates the view that is represented by this ViewHolder with the information in the
     * provided {@link Contact}.
     */
    public void bindTypeDownResult(ContactResultsLiveData.ContactResultListItem contactResult,
            Integer sortMethod) {
        Contact contact = contactResult.getContact();

        QueryStyle queryStyle = new QueryStyle(mContext, R.style.TextAppearance_TypeDownListSpan);
        ViewUtils.setText(mContactNumber,
                queryStyle.getStringWithQueryInSpecialStyle(contactResult.getNumber(),
                        contactResult.getSearchQuery()));
        ViewUtils.setText(mContactName,
                TelecomUtils.isSortByFirstName(sortMethod) ? contact.getDisplayName()
                        : contact.getDisplayNameAlt());
        mContactCard.setOnClickListener(
                v -> {
                    if (mOnItemClickedListener != null) {
                        mOnItemClickedListener.onItemClicked(contactResult);
                    }
                    UiCallManager.get().placeCall(mContactNumber.getText().toString());
                });
        TelecomUtils.setContactBitmapAsync(mContext, mContactPicture, contact, sortMethod);
    }

    void recycle() {
        itemView.setEnabled(true);
        mContactCard.setOnClickListener(null);
        if (mContactPicture != null) {
            Glide.with(mContext).clear(mContactPicture);
        }
    }
}
