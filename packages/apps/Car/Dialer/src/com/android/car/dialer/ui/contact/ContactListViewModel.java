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
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.arch.common.FutureData;
import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.livedata.SharedPreferencesLiveData;
import com.android.car.dialer.ui.common.DialerListViewModel;
import com.android.car.dialer.ui.common.entity.ContactSortingInfo;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;

import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * View model for {@link ContactListFragment}.
 */
public class ContactListViewModel extends DialerListViewModel {
    private final Context mContext;
    private final LiveData<Pair<Integer, List<Contact>>> mSortedContactListLiveData;
    private final LiveData<FutureData<Pair<Integer, List<Contact>>>> mContactList;

    public ContactListViewModel(@NonNull Application application) {
        super(application);
        mContext = application.getApplicationContext();

        SharedPreferencesLiveData preferencesLiveData = getSharedPreferencesLiveData();
        LiveData<List<Contact>> contactListLiveData = LiveDataFunctions.switchMapNonNull(
                UiBluetoothMonitor.get().getFirstHfpConnectedDevice(),
                device -> InMemoryPhoneBook.get().getContactsLiveDataByAccount(
                        device.getAddress()));
        mSortedContactListLiveData = new SortedContactListLiveData(
                mContext, contactListLiveData, preferencesLiveData);
        mContactList = LiveDataFunctions.loadingSwitchMap(mSortedContactListLiveData,
                input -> LiveDataFunctions.dataOf(input));
    }

    /**
     * Returns a live data which represents a list of all contacts.
     */
    public LiveData<FutureData<Pair<Integer, List<Contact>>>> getAllContacts() {
        return mContactList;
    }

    private static class SortedContactListLiveData
            extends MediatorLiveData<Pair<Integer, List<Contact>>> {
        // Class static to make sure only one task is sorting contacts at one time.
        private static ExecutorService sExecutorService = Executors.newSingleThreadExecutor();

        private final LiveData<List<Contact>> mContactListLiveData;
        private final SharedPreferencesLiveData mPreferencesLiveData;
        private final Context mContext;

        private Future<?> mRunnableFuture;

        private SortedContactListLiveData(Context context,
                @NonNull LiveData<List<Contact>> contactListLiveData,
                @NonNull SharedPreferencesLiveData sharedPreferencesLiveData) {
            mContext = context;
            mContactListLiveData = contactListLiveData;
            mPreferencesLiveData = sharedPreferencesLiveData;

            addSource(mPreferencesLiveData, trigger -> onSortOrderChanged());
            addSource(mContactListLiveData, this::sortContacts);
        }

        private void onSortOrderChanged() {
            // When sort order changes, do not set value to trigger an update if there is no data
            // set yet. An update will switch the loading state to loaded.
            if (mContactListLiveData.getValue() == null) {
                return;
            }
            sortContacts(mContactListLiveData.getValue());
        }

        private void sortContacts(@Nullable List<Contact> contactList) {
            if (mRunnableFuture != null) {
                mRunnableFuture.cancel(true);
                mRunnableFuture = null;
            }

            if (contactList == null || contactList.isEmpty()) {
                setValue(null);
                return;
            }

            Pair<Comparator<Contact>, Integer> contactSortingInfo = ContactSortingInfo
                    .getSortingInfo(mContext, mPreferencesLiveData);
            Comparator<Contact> comparator = contactSortingInfo.first;
            Integer sortMethod = contactSortingInfo.second;

            Runnable runnable = () -> {
                Collections.sort(contactList, comparator);
                postValue(new Pair<>(sortMethod, contactList));
            };
            mRunnableFuture = sExecutorService.submit(runnable);
        }

        @Override
        protected void onInactive() {
            super.onInactive();
            if (mRunnableFuture != null) {
                mRunnableFuture.cancel(true);
                mRunnableFuture = null;
            }
        }
    }
}
