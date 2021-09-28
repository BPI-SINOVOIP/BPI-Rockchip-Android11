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

package com.android.car.dialer.ui.favorite;

import android.app.Application;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.arch.common.FutureData;
import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.R;
import com.android.car.dialer.storage.FavoriteNumberRepository;
import com.android.car.dialer.ui.common.DialerListViewModel;
import com.android.car.dialer.ui.common.entity.ActionButton;
import com.android.car.dialer.ui.common.entity.Header;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.PhoneNumber;

import java.util.ArrayList;
import java.util.List;

/**
 * View model for {@link FavoriteFragment}.
 */
public class FavoriteViewModel extends DialerListViewModel {
    private final FavoriteNumberRepository mFavoriteNumberRepository;
    private final LiveData<FutureData<List<Object>>> mFavoriteContacts;
    private final LiveData<List<Contact>> mBluetoothFavoriteContacts;

    public FavoriteViewModel(Application application) {
        super(application);
        mFavoriteNumberRepository = FavoriteNumberRepository.getRepository(application);
        mBluetoothFavoriteContacts = new BluetoothFavoriteContactsLiveData(application);

        MediatorLiveData<List<Object>> favoriteContacts = new MediatorLiveData<>();
        favoriteContacts.addSource(mFavoriteNumberRepository.getFavoriteContacts(), contacts -> {
            List<Object> contactList = new ArrayList<>();
            if (mBluetoothFavoriteContacts.getValue() != null
                    && !mBluetoothFavoriteContacts.getValue().isEmpty()) {
                contactList.add(new Header(application.getString(R.string.phone_favorites)));
                contactList.addAll(mBluetoothFavoriteContacts.getValue());
            }
            contactList.add(new Header(application.getString(R.string.local_favorites)));
            if (contacts != null) {
                contactList.addAll(contacts);
            }
            contactList.add(new ActionButton());
            favoriteContacts.setValue(contactList);
        });
        favoriteContacts.addSource(mBluetoothFavoriteContacts, contacts -> {
            List<Object> contactList = new ArrayList<>();
            if (contacts != null && !contacts.isEmpty()) {
                contactList.add(new Header(application.getString(R.string.phone_favorites)));
                contactList.addAll(contacts);
            }
            contactList.add(new Header(application.getString(R.string.local_favorites)));
            if (mFavoriteNumberRepository.getFavoriteContacts().getValue() != null) {
                contactList.addAll(mFavoriteNumberRepository.getFavoriteContacts().getValue());
            }
            contactList.add(new ActionButton());
            favoriteContacts.setValue(contactList);
        });
        mFavoriteContacts = LiveDataFunctions.loadingSwitchMap(
                favoriteContacts,
                input -> LiveDataFunctions.dataOf(input == null || input.isEmpty() ? null : input));

    }

    /**
     * Returns favorite contact list live data.
     */
    public LiveData<FutureData<List<Object>>> getFavoriteContacts() {
        return mFavoriteContacts;
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
}
