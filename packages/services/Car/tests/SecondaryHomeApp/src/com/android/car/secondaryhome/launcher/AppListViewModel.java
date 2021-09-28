/**
 * Copyright (c) 2019 The Android Open Source Project
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

package com.android.car.secondaryhome.launcher;

import android.annotation.NonNull;
import android.app.Application;

import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;

import java.util.List;

/**
 * A view model that provides a list of activities that can be launched.
 */
public final class AppListViewModel extends AndroidViewModel {

    @NonNull
    private final AppListLiveData mAppList;
    @NonNull
    private final PackageIntentReceiver mPackageIntentReceiver;

    public AppListViewModel(Application application) {
        super(application);
        mAppList = new AppListLiveData(application);
        mPackageIntentReceiver = new PackageIntentReceiver(mAppList, application);
    }

    public LiveData<List<AppEntry>> getAppList() {
        return mAppList;
    }

    @Override
    protected void onCleared() {
        getApplication().unregisterReceiver(mPackageIntentReceiver);
    }
}
