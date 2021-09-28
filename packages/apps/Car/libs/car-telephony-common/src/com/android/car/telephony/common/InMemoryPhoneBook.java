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

import android.Manifest;
import android.content.Context;
import android.database.Cursor;
import android.provider.ContactsContract;
import android.text.TextUtils;
import android.util.ArrayMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.Observer;
import androidx.lifecycle.Transformations;

import com.android.car.apps.common.log.L;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;

/**
 * A singleton statically accessible helper class which pre-loads contacts list into memory so that
 * they can be accessed more easily and quickly.
 */
public class InMemoryPhoneBook implements Observer<List<Contact>> {
    private static final String TAG = "CD.InMemoryPhoneBook";
    private static InMemoryPhoneBook sInMemoryPhoneBook;

    private final Context mContext;
    private final AsyncQueryLiveData<List<Contact>> mContactListAsyncQueryLiveData;
    /**
     * A map to speed up phone number searching.
     */
    private final Map<I18nPhoneNumberWrapper, Contact> mPhoneNumberContactMap = new HashMap<>();
    /**
     * A map to look up contact by account name and lookup key. Each entry presents a map of lookup
     * key to contacts for one account.
     */
    private final Map<String, Map<String, Contact>> mLookupKeyContactMap = new HashMap<>();

    /**
     * A map which divides contacts by account.
     */
    private final Map<String, List<Contact>> mAccountContactsMap = new ArrayMap<>();
    private boolean mIsLoaded = false;

    /**
     * Initialize the globally accessible {@link InMemoryPhoneBook}. Returns the existing {@link
     * InMemoryPhoneBook} if already initialized. {@link #tearDown()} must be called before init to
     * reinitialize.
     */
    public static InMemoryPhoneBook init(Context context) {
        if (sInMemoryPhoneBook == null) {
            sInMemoryPhoneBook = new InMemoryPhoneBook(context);
            sInMemoryPhoneBook.onInit();
        }
        return get();
    }

    /**
     * Returns if the InMemoryPhoneBook is initialized. get() won't return null or throw if this is
     * true, but it doesn't indicate whether or not contacts are loaded yet.
     * <p>
     * See also: {@link #isLoaded()}
     */
    public static boolean isInitialized() {
        return sInMemoryPhoneBook != null;
    }

    /**
     * Get the global {@link InMemoryPhoneBook} instance.
     */
    public static InMemoryPhoneBook get() {
        if (sInMemoryPhoneBook != null) {
            return sInMemoryPhoneBook;
        } else {
            throw new IllegalStateException("Call init before get InMemoryPhoneBook");
        }
    }

    /**
     * Tears down the globally accessible {@link InMemoryPhoneBook}.
     */
    public static void tearDown() {
        sInMemoryPhoneBook.onTearDown();
        sInMemoryPhoneBook = null;
    }

    private InMemoryPhoneBook(Context context) {
        mContext = context;

        QueryParam contactListQueryParam = new QueryParam(
                ContactsContract.Data.CONTENT_URI,
                null,
                ContactsContract.Data.MIMETYPE + " = ? OR "
                        + ContactsContract.Data.MIMETYPE + " = ? OR "
                        + ContactsContract.Data.MIMETYPE + " = ?",
                new String[]{
                        ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE,
                        ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE,
                        ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_ITEM_TYPE},
                ContactsContract.Contacts.DISPLAY_NAME + " ASC ",
                Manifest.permission.READ_CONTACTS);
        mContactListAsyncQueryLiveData = new AsyncQueryLiveData<List<Contact>>(mContext,
                QueryParam.of(contactListQueryParam), Executors.newSingleThreadExecutor()) {
            @Override
            protected List<Contact> convertToEntity(Cursor cursor) {
                return onCursorLoaded(cursor);
            }
        };
    }

    private void onInit() {
        mContactListAsyncQueryLiveData.observeForever(this);
    }

    private void onTearDown() {
        mContactListAsyncQueryLiveData.removeObserver(this);
    }

    public boolean isLoaded() {
        return mIsLoaded;
    }

    /**
     * Returns a {@link LiveData} which monitors the contact list changes.
     *
     * @deprecated Use {@link #getContactsLiveDataByAccount(String)} instead.
     */
    @Deprecated
    public LiveData<List<Contact>> getContactsLiveData() {
        return mContactListAsyncQueryLiveData;
    }

    /**
     * Returns a LiveData that represents all contacts within an account.
     *
     * @param accountName the name of an account that contains all the contacts. For the contacts
     *                    from a Bluetooth connected phone, the account name is equal to the
     *                    Bluetooth address.
     */
    public LiveData<List<Contact>> getContactsLiveDataByAccount(String accountName) {
        return Transformations.map(mContactListAsyncQueryLiveData,
                contacts -> contacts == null ? null : mAccountContactsMap.get(accountName));
    }

    /**
     * Looks up a {@link Contact} by the given phone number. Returns null if can't find a Contact or
     * the {@link InMemoryPhoneBook} is still loading.
     */
    @Nullable
    public Contact lookupContactEntry(String phoneNumber) {
        L.v(TAG, String.format("lookupContactEntry: %s", TelecomUtils.piiLog(phoneNumber)));
        if (!isLoaded()) {
            L.w(TAG, "looking up a contact while loading.");
        }

        if (TextUtils.isEmpty(phoneNumber)) {
            L.w(TAG, "looking up an empty phone number.");
            return null;
        }

        I18nPhoneNumberWrapper i18nPhoneNumber = I18nPhoneNumberWrapper.Factory.INSTANCE.get(
                mContext, phoneNumber);
        return mPhoneNumberContactMap.get(i18nPhoneNumber);
    }

    /**
     * Looks up a {@link Contact} by the given lookup key and account name. Account name could be
     * null for locally added contacts. Returns null if can't find the contact entry.
     */
    @Nullable
    public Contact lookupContactByKey(String lookupKey, @Nullable String accountName) {
        if (!isLoaded()) {
            L.w(TAG, "looking up a contact while loading.");
        }
        if (TextUtils.isEmpty(lookupKey)) {
            L.w(TAG, "looking up an empty lookup key.");
            return null;
        }
        if (mLookupKeyContactMap.containsKey(accountName)) {
            return mLookupKeyContactMap.get(accountName).get(lookupKey);
        }

        return null;
    }

    /**
     * Iterates all the accounts and returns a list of contacts that match the lookup key. This API
     * is discouraged to use whenever the account name is available where {@link
     * #lookupContactByKey(String, String)} should be used instead.
     */
    @NonNull
    public List<Contact> lookupContactByKey(String lookupKey) {
        if (!isLoaded()) {
            L.w(TAG, "looking up a contact while loading.");
        }

        if (TextUtils.isEmpty(lookupKey)) {
            L.w(TAG, "looking up an empty lookup key.");
            return Collections.emptyList();
        }
        List<Contact> results = new ArrayList<>();
        // Iterate all the accounts to get all the match contacts with given lookup key.
        for (Map<String, Contact> subMap : mLookupKeyContactMap.values()) {
            if (subMap.containsKey(lookupKey)) {
                results.add(subMap.get(lookupKey));
            }
        }

        return results;
    }

    private List<Contact> onCursorLoaded(Cursor cursor) {
        Map<String, Map<String, Contact>> contactMap = new LinkedHashMap<>();
        List<Contact> contactList = new ArrayList<>();

        while (cursor.moveToNext()) {
            int accountNameColumn = cursor.getColumnIndex(
                    ContactsContract.RawContacts.ACCOUNT_NAME);
            int lookupKeyColumn = cursor.getColumnIndex(ContactsContract.Data.LOOKUP_KEY);
            String accountName = cursor.getString(accountNameColumn);
            String lookupKey = cursor.getString(lookupKeyColumn);

            if (!contactMap.containsKey(accountName)) {
                contactMap.put(accountName, new HashMap<>());
            }

            Map<String, Contact> subMap = contactMap.get(accountName);
            subMap.put(lookupKey, Contact.fromCursor(mContext, cursor, subMap.get(lookupKey)));
        }

        mAccountContactsMap.clear();
        for (String accountName : contactMap.keySet()) {
            Map<String, Contact> subMap = contactMap.get(accountName);
            contactList.addAll(subMap.values());
            List<Contact> accountContacts = new ArrayList<>();
            accountContacts.addAll(subMap.values());
            mAccountContactsMap.put(accountName, accountContacts);
        }

        mLookupKeyContactMap.clear();
        mLookupKeyContactMap.putAll(contactMap);

        mPhoneNumberContactMap.clear();
        for (Contact contact : contactList) {
            for (PhoneNumber phoneNumber : contact.getNumbers()) {
                mPhoneNumberContactMap.put(phoneNumber.getI18nPhoneNumberWrapper(), contact);
            }
        }
        return contactList;
    }

    @Override
    public void onChanged(List<Contact> contacts) {
        L.d(TAG, "Contacts loaded:" + (contacts == null ? 0 : contacts.size()));
        mIsLoaded = true;
    }
}
