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

package com.android.car.dialer.ui.search;

import android.app.Application;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.android.car.dialer.ui.common.ContactResultsLiveData;
import com.android.car.dialer.ui.common.DialerListViewModel;

import java.util.List;

/**
 * {link AndroidViewModel} used for search functionality.
 */
public class ContactResultsViewModel extends DialerListViewModel {

    private final ContactResultsLiveData mContactSearchResultsLiveData;
    private final MutableLiveData<String> mSearchQueryLiveData;

    public ContactResultsViewModel(@NonNull Application application) {
        super(application);
        mSearchQueryLiveData = new MutableLiveData<>();
        mContactSearchResultsLiveData = new ContactResultsLiveData(application,
                mSearchQueryLiveData, getSharedPreferencesLiveData());
    }

    /**
     * Sets search query.
     */
    public void setSearchQuery(String searchQuery) {
        if (TextUtils.equals(mSearchQueryLiveData.getValue(), searchQuery)) {
            return;
        }

        mSearchQueryLiveData.setValue(searchQuery);
    }

    /**
     * Returns live data of search results.
     */
    public LiveData<List<ContactResultsLiveData.ContactResultListItem>> getContactSearchResults() {
        return mContactSearchResultsLiveData;
    }

    /**
     * Returns search query.
     */
    public String getSearchQuery() {
        return mSearchQueryLiveData.getValue();
    }

    /**
     * Returns Search Query LiveData.
     */
    public MutableLiveData<String> getSearchQueryLiveData() {
        return mSearchQueryLiveData;
    }
}
