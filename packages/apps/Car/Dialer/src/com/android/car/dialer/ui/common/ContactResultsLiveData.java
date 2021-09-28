/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.dialer.ui.common;

import android.Manifest;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.livedata.SharedPreferencesLiveData;
import com.android.car.dialer.ui.common.entity.ContactSortingInfo;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;
import com.android.car.telephony.common.ObservableAsyncQuery;
import com.android.car.telephony.common.QueryParam;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Represents a list of {@link Contact} based on the search query
 */
public class ContactResultsLiveData extends
        MediatorLiveData<List<ContactResultsLiveData.ContactResultListItem>> {
    private static final String[] CONTACT_DETAILS_PROJECTION = {
            ContactsContract.CommonDataKinds.Phone._ID,
            ContactsContract.CommonDataKinds.Phone.LOOKUP_KEY,
            ContactsContract.CommonDataKinds.Phone.NUMBER,
    };
    private final Context mContext;
    private final SearchQueryParamProvider mSearchQueryParamProvider;
    private final ObservableAsyncQuery mObservableAsyncQuery;
    private final LiveData<String> mSearchQueryLiveData;
    private final LiveData<List<Contact>> mContactListLiveData;
    private final SharedPreferencesLiveData mSortOrderPreferenceLiveData;
    private String mSearchQuery;
    private boolean mShowOnlyOneEntry;

    /**
     * @param searchQueryLiveData represents a list of strings that are used to query the data
     * @param sortOrderPreferenceLiveData has the information on how to order the acquired contacts.
     * @param showOnlyOneEntry determines whether to show only entry per contact.
     */
    public ContactResultsLiveData(Context context,
            LiveData<String> searchQueryLiveData,
            SharedPreferencesLiveData sortOrderPreferenceLiveData,
            boolean showOnlyOneEntry) {
        mContext = context;
        mShowOnlyOneEntry = showOnlyOneEntry;
        mSearchQueryParamProvider = new SearchQueryParamProvider(searchQueryLiveData);
        mObservableAsyncQuery = new ObservableAsyncQuery(context, mSearchQueryParamProvider,
                this::onQueryFinished);

        mContactListLiveData = LiveDataFunctions.switchMapNonNull(
                UiBluetoothMonitor.get().getFirstHfpConnectedDevice(),
                device -> InMemoryPhoneBook.get()
                        .getContactsLiveDataByAccount(device.getAddress()));
        addSource(mContactListLiveData, this::onContactsChange);
        mSearchQueryLiveData = searchQueryLiveData;
        addSource(mSearchQueryLiveData, this::onSearchQueryChanged);

        mSortOrderPreferenceLiveData = sortOrderPreferenceLiveData;
        addSource(mSortOrderPreferenceLiveData, this::onSortOrderChanged);
    }

    /**
     * This constructor only allows one entry per contact.
     *
     * @param searchQueryLiveData represents a list of strings that are used to query the data
     * @param sortOrderPreferenceLiveData has the information on how to order the acquired contacts.
     */
    public ContactResultsLiveData(Context context,
            LiveData<String> searchQueryLiveData,
            SharedPreferencesLiveData sortOrderPreferenceLiveData) {
        this(context, searchQueryLiveData, sortOrderPreferenceLiveData, true);
    }

    private void onContactsChange(List<Contact> contactList) {
        if (contactList == null || contactList.isEmpty()) {
            mObservableAsyncQuery.stopQuery();
            setValue(Collections.emptyList());
        } else {
            onSearchQueryChanged(mSearchQueryLiveData.getValue());
        }
    }

    private void onSearchQueryChanged(String searchQuery) {
        mSearchQuery = searchQuery;
        if (TextUtils.isEmpty(searchQuery)) {
            mObservableAsyncQuery.stopQuery();
            setValue(Collections.emptyList());
        } else {
            mObservableAsyncQuery.startQuery();
        }
    }

    private void onSortOrderChanged(SharedPreferences unusedSharedPreferences) {
        setValue(getValue());
    }

    private void onQueryFinished(@Nullable Cursor cursor) {
        if (cursor == null) {
            setValue(Collections.emptyList());
            return;
        }

        List<ContactResultListItem> contactResults = new ArrayList<>();
        while (cursor.moveToNext()) {
            int lookupKeyColIdx = cursor.getColumnIndex(
                    ContactsContract.CommonDataKinds.Phone.LOOKUP_KEY);
            int numberIdx = cursor.getColumnIndex(ContactsContract.CommonDataKinds.Phone.NUMBER);
            String lookupKey = cursor.getString(lookupKeyColIdx);
            String number = cursor.getString(numberIdx);
            List<Contact> lookupResults = InMemoryPhoneBook.get().lookupContactByKey(lookupKey);
            for (Contact contact : lookupResults) {
                contactResults.add(new ContactResultListItem(contact, number, mSearchQuery));
            }
        }

        if (mShowOnlyOneEntry) {
            Set<Contact> set = new HashSet<>();
            contactResults = contactResults.stream()
                    .filter(o -> set.add(o.getContact()))
                    .collect(Collectors.toList());
        }

        setValue(contactResults);
        cursor.close();
    }

    /**
     * Sort and replace null list with empty list.
     */
    @Override
    public void setValue(List<ContactResultListItem> contactResults) {
        if (contactResults != null && !contactResults.isEmpty()) {
            Collections.sort(contactResults, (o1, o2) -> ContactSortingInfo.getSortingInfo(
                    mContext, mSortOrderPreferenceLiveData).first.compare(o1.mContact,
                    o2.mContact));
        }
        super.setValue(contactResults == null ? Collections.EMPTY_LIST : contactResults);
    }

    private static class SearchQueryParamProvider implements QueryParam.Provider {
        private final LiveData<String> mSearchQueryLiveData;

        private SearchQueryParamProvider(LiveData<String> searchQueryLiveData) {
            mSearchQueryLiveData = searchQueryLiveData;
        }

        @Nullable
        @Override
        public QueryParam getQueryParam() {
            Uri lookupUri = Uri.withAppendedPath(
                    ContactsContract.CommonDataKinds.Phone.CONTENT_FILTER_URI,
                    Uri.encode(mSearchQueryLiveData.getValue()));
            return new QueryParam(lookupUri, CONTACT_DETAILS_PROJECTION, null,
                    /* selectionArgs= */null, /* orderBy= */null,
                    Manifest.permission.READ_CONTACTS);
        }
    }

    /**
     * Represent a contact search result.
     */
    public static class ContactResultListItem {
        private final Contact mContact;
        private final String mNumber;
        private final String mSearchQuery;

        public ContactResultListItem(Contact contact, String number, String searchQuery) {
            mContact = contact;
            mNumber = number;
            mSearchQuery = searchQuery;
        }

        /**
         * Returns the contact.
         */
        public Contact getContact() {
            return mContact;
        }

        /**
         * Returns the number. It is a string read from column
         * {@link ContactsContract.CommonDataKinds.Phone#NUMBER}.
         */
        public String getNumber() {
            return mNumber;
        }

        /**
         * Returns the search query that initiates the search.
         */
        public String getSearchQuery() {
            return mSearchQuery;
        }
    }
}
