/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.dialer.ui.contact;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.PostalAddress;

import java.util.ArrayList;

class ContactDetailsAdapter extends RecyclerView.Adapter<ContactDetailsViewHolder> {

    private static final String TAG = "CD.ContactDetailsAdapter";

    private static final int ID_HEADER = 1;
    private static final int ID_NUMBER = 2;
    private static final int ID_ADDRESS = 3;

    interface PhoneNumberPresenter {
        void onClick(Contact contact, PhoneNumber phoneNumber);
    }

    private final Context mContext;
    private final PhoneNumberPresenter mPhoneNumberPresenter;
    private final ArrayList<Object> mItems = new ArrayList<>();
    private Contact mContact;

    ContactDetailsAdapter(
            @NonNull Context context,
            @Nullable Contact contact,
            @NonNull PhoneNumberPresenter phoneNumberPresenter) {
        super();
        mContext = context;
        mPhoneNumberPresenter = phoneNumberPresenter;
        setContact(contact);
    }

    void setContact(Contact contact) {
        L.d(TAG, "setContact %s", contact);
        mContact = contact;
        mItems.clear();

        boolean showHeader = mContext.getResources()
                .getBoolean(R.bool.config_show_contact_details_header);

        if (showHeader || contact == null) {
            // We add null contacts to display an error message
            mItems.add(contact);
        }
        if (contact != null) {
            mItems.addAll(contact.getNumbers());
            if (mContext.getResources().getBoolean(R.bool.config_show_postal_address)) {
                mItems.addAll(contact.getPostalAddresses());
            }
        }
        notifyDataSetChanged();
    }

    @Override
    public int getItemViewType(int position) {
        Object obj = mItems.get(position);
        if (obj == null || obj instanceof Contact) {
            return ID_HEADER;
        } else if (obj instanceof PhoneNumber) {
            return ID_NUMBER;
        } else if (obj instanceof PostalAddress) {
            return ID_ADDRESS;
        } else {
            L.w(TAG, "Unknown view type");
            return -1;
        }
    }

    @Override
    public int getItemCount() {
        return mItems.size();
    }

    @Override
    public ContactDetailsViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        int layoutResId;
        switch (viewType) {
            case ID_HEADER:
                layoutResId = R.layout.contact_details_name_image;
                break;
            case ID_NUMBER:
                layoutResId = R.layout.contact_details_number;
                break;
            case ID_ADDRESS:
                layoutResId = R.layout.contact_details_address;
                break;
            default:
                L.w(TAG, "Unknown view type: %d", viewType);
                return null;
        }

        View view = LayoutInflater.from(parent.getContext()).inflate(layoutResId, parent,
                false);
        return new ContactDetailsViewHolder(view, mPhoneNumberPresenter);
    }

    @Override
    public void onBindViewHolder(ContactDetailsViewHolder viewHolder, int position) {
        switch (viewHolder.getItemViewType()) {
            case ID_HEADER:
                viewHolder.bind(mContext, (Contact) mItems.get(position));
                break;
            case ID_NUMBER:
                viewHolder.bind(mContext, mContact, (PhoneNumber) mItems.get(position));
                break;
            case ID_ADDRESS:
                viewHolder.bind(mContext, (PostalAddress) mItems.get(position));
                break;
            default:
                L.w(TAG, "Unknown view type %d ", viewHolder.getItemViewType());
                return;
        }
    }
}
