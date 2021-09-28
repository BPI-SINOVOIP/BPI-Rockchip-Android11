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

package com.android.car.dialer.ui.favorite;

import android.Manifest;
import android.content.Context;
import android.database.Cursor;
import android.provider.ContactsContract;

import androidx.annotation.NonNull;

import com.android.car.telephony.common.AsyncQueryLiveData;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.QueryParam;

import java.util.ArrayList;
import java.util.List;

/** Presents the favorite contacts downloaded from phone. It reads the contacts provider. */
class BluetoothFavoriteContactsLiveData extends AsyncQueryLiveData<List<Contact>> {
    private final Context mContext;

    BluetoothFavoriteContactsLiveData(Context context) {
        super(context, QueryParam.of(new FavoriteQueryParam()));
        mContext = context;
    }

    @Override
    protected List<Contact> convertToEntity(@NonNull Cursor cursor) {
        List<Contact> resultList = new ArrayList<>();

        while (cursor.moveToNext()) {
            Contact favoriteEntry = Contact.fromCursor(mContext, cursor);
            // If there is already a contact with the same phone number, don't add duplicate
            // entries.
            boolean alreadyExists = false;
            for (Contact contact : resultList) {
                if (favoriteEntry.equals(contact) && favoriteEntry.getNumbers().containsAll(
                        contact.getNumbers())) {
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists) {
                resultList.add(favoriteEntry);
            }
        }
        return resultList;
    }

    private static class FavoriteQueryParam extends QueryParam {
        FavoriteQueryParam() {
            super(ContactsContract.Data.CONTENT_URI,
                    null,
                    ContactsContract.Data.MIMETYPE + " = ? AND "
                            + ContactsContract.CommonDataKinds.Phone.STARRED + " = ? ",
                    new String[]{
                            ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE,
                            String.valueOf(1)},
                    ContactsContract.Contacts.DISPLAY_NAME + " ASC ",
                    Manifest.permission.READ_CONTACTS);
        }
    }
}
