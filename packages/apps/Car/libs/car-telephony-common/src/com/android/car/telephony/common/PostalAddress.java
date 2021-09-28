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

package com.android.car.telephony.common;

import android.content.Intent;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Parcel;
import android.os.Parcelable;
import android.provider.ContactsContract;

import androidx.annotation.Nullable;

import com.android.car.apps.common.NavigationUtils;
import com.android.car.apps.common.log.L;

/**
 * Encapsulates data about an address entry. Typically loaded from the local Address store.
 */
public class PostalAddress implements Parcelable {
    private static final String TAG = "CD.PostalAddress";

    /**
     * The formatted address.
     */
    private String mFormattedAddress;

    /**
     * The address type. See more at {@link ContactsContract.CommonDataKinds.StructuredPostal#TYPE}
     */
    private int mType;

    /**
     * The user defined label. See more at
     * {@link ContactsContract.CommonDataKinds.StructuredPostal#LABEL}
     */
    @Nullable
    private String mLabel;

    /**
     * Parses a PostalAddress entry for a Cursor loaded from the Address Database.
     */
    public static PostalAddress fromCursor(Cursor cursor) {
        int formattedAddressColumn = cursor.getColumnIndex(
                ContactsContract.CommonDataKinds.StructuredPostal.FORMATTED_ADDRESS);
        int addressTypeColumn = cursor.getColumnIndex(
                ContactsContract.CommonDataKinds.StructuredPostal.TYPE);
        int labelColumn = cursor.getColumnIndex(
                ContactsContract.CommonDataKinds.StructuredPostal.LABEL);

        PostalAddress postalAddress = new PostalAddress();
        postalAddress.mFormattedAddress = cursor.getString(formattedAddressColumn);
        postalAddress.mType = cursor.getInt(addressTypeColumn);
        postalAddress.mLabel = cursor.getString(labelColumn);

        return postalAddress;
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof PostalAddress
                && mFormattedAddress.equals(((PostalAddress) obj).mFormattedAddress);
    }

    /**
     * Returns {@link #mFormattedAddress}
     */
    public String getFormattedAddress() {
        return mFormattedAddress;
    }

    /**
     * Returns {@link #mType}
     */
    public int getType() {
        return mType;
    }

    /**
     * Returns {@link #mLabel}
     */
    @Nullable
    public String getLabel() {
        return mLabel;
    }

    /**
     * Returns a human readable string label. For example, Home, Work, etc.
     */
    public CharSequence getReadableLabel(Resources res) {
        return ContactsContract.CommonDataKinds.StructuredPostal.getTypeLabel(res, mType, mLabel);
    }

    /**
     * Returns the address Uri for {@link #mFormattedAddress}.
     */
    public Intent getAddressIntent(Resources res) {
        L.d(TAG, "The address is: " + TelecomUtils.piiLog(mFormattedAddress));
        return NavigationUtils.getViewAddressIntent(res, mFormattedAddress);
    }

    /**
     * Returns the navigation Uri for {@link #mFormattedAddress}.
     */
    public Intent getNavigationIntent(Resources res) {
        L.d(TAG, "The address is: " + TelecomUtils.piiLog(mFormattedAddress));
        return NavigationUtils.getNavigationIntent(res, mFormattedAddress);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(mType);
        dest.writeString(mLabel);
        dest.writeString(mFormattedAddress);
    }

    /**
     * Create {@link PostalAddress} object from saved parcelable.
     */
    public static Creator<PostalAddress> CREATOR = new Creator<PostalAddress>() {
        @Override
        public PostalAddress createFromParcel(Parcel source) {
            PostalAddress postalAddress = new PostalAddress();
            postalAddress.mType = source.readInt();
            postalAddress.mLabel = source.readString();
            postalAddress.mFormattedAddress = source.readString();
            return postalAddress;
        }

        @Override
        public PostalAddress[] newArray(int size) {
            return new PostalAddress[size];
        }
    };
}
