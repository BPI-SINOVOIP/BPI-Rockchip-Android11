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

import android.app.Application;
import android.content.Context;
import android.net.Uri;
import android.provider.ContactsContract;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.arch.common.FutureData;
import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.storage.FavoriteNumberRepository;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.WorkerExecutor;

import java.util.List;
import java.util.concurrent.Future;

/**
 * View model for the contact details page.
 */
public class ContactDetailsViewModel extends AndroidViewModel {
    private final FavoriteNumberRepository mFavoriteNumberRepository;

    public ContactDetailsViewModel(@NonNull Application application) {
        super(application);
        mFavoriteNumberRepository = FavoriteNumberRepository.getRepository(application);
    }

    /**
     * Builds the {@link LiveData} for the given contact which will update upon contact change and
     * favorite repository change.
     *
     * @param contact The contact entry. It might be out of date and should update when the {@link
     *                InMemoryPhoneBook} changes. It always uses the in memory instance to get the
     *                favorite state for phone numbers.
     */
    public LiveData<FutureData<Contact>> getContactDetails(@Nullable Contact contact) {
        if (contact == null) {
            return LiveDataFunctions.dataOf(new FutureData<>(false, null));
        }

        LiveData<Contact> contactLiveData = new ContactDetailsLiveData(getApplication(), contact);
        return LiveDataFunctions.loadingSwitchMap(contactLiveData,
                input -> LiveDataFunctions.dataOf(input));
    }

    /**
     * Adds the phone number to favorite.
     *
     * @param contact     The contact the phone number belongs to.
     * @param phoneNumber The phone number to add to favorite.
     */
    public void addToFavorite(Contact contact, PhoneNumber phoneNumber) {
        mFavoriteNumberRepository.addToFavorite(contact, phoneNumber);
    }

    /**
     * Removes the phone number from favorite.
     *
     * @param contact     The contact the phone number belongs to.
     * @param phoneNumber The phone number to remove from favorite.
     */
    public void removeFromFavorite(Contact contact, PhoneNumber phoneNumber) {
        mFavoriteNumberRepository.removeFromFavorite(contact, phoneNumber);
    }

    private class ContactDetailsLiveData extends MediatorLiveData<Contact> {
        private final Uri mContactLookupUri;
        private final String mAccountName;
        private final WorkerExecutor mWorkerExecutor;
        private final Context mContext;
        private Contact mContact;
        private Future<?> mRunnableFuture;

        private ContactDetailsLiveData(Context context, @NonNull Contact contact) {
            mContext = context;
            mWorkerExecutor = WorkerExecutor.getInstance();
            mContact = contact;
            mContactLookupUri = mContact.getLookupUri();
            mAccountName = mContact.getAccountName();
            addSource(InMemoryPhoneBook.get().getContactsLiveData(), this::onContactListChanged);
            addSource(mFavoriteNumberRepository.getFavoriteContacts(),
                    this::onFavoriteContactsChanged);
        }

        private void onContactListChanged(List<Contact> contacts) {
            if (mContact != null) {
                Contact inMemoryContact = InMemoryPhoneBook.get().lookupContactByKey(
                        mContact.getLookupKey(), mContact.getAccountName());
                if (inMemoryContact != null) {
                    setValue(inMemoryContact);
                    return;
                }
            }

            if (mRunnableFuture != null) {
                mRunnableFuture.cancel(false);
            }
            mRunnableFuture = mWorkerExecutor.getSingleThreadExecutor().submit(
                    () -> {
                        Uri refreshedContactLookupUri = ContactsContract.Contacts.getLookupUri(
                                mContext.getContentResolver(),
                                mContact == null ? mContactLookupUri : mContact.getLookupUri());
                        if (refreshedContactLookupUri == null) {
                            postValue(null);
                            return;
                        }

                        List<String> pathSegments = refreshedContactLookupUri.getPathSegments();
                        String lookupKey = pathSegments.get(pathSegments.size() - 2);
                        Contact contact = InMemoryPhoneBook.get().lookupContactByKey(lookupKey,
                                mContact == null ? mAccountName : mContact.getAccountName());
                        postValue(contact);
                    }
            );
        }

        private void onFavoriteContactsChanged(List<Contact> favoriteContacts) {
            if (mContact == null) {
                return;
            }
            Contact inMemoryContact = InMemoryPhoneBook.get().lookupContactByKey(
                    mContact.getLookupKey(), mContact.getAccountName());
            setValue(inMemoryContact);
        }

        @Override
        public void setValue(Contact contact) {
            mContact = contact;
            super.setValue(contact);
        }

        @Override
        protected void onInactive() {
            super.onInactive();
            if (mRunnableFuture != null) {
                mRunnableFuture.cancel(true);
            }
        }
    }
}
