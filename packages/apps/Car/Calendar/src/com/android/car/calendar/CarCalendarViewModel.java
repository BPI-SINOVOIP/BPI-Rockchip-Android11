/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.calendar;

import android.content.ContentResolver;
import android.os.Handler;
import android.os.HandlerThread;
import android.telephony.TelephonyManager;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModel;

import com.android.car.calendar.common.EventDescriptions;
import com.android.car.calendar.common.EventLocations;
import com.android.car.calendar.common.EventsLiveData;

import java.time.Clock;
import java.util.Locale;

class CarCalendarViewModel extends ViewModel {
    private static final String TAG = "CarCalendarViewModel";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private final HandlerThread mHandlerThread = new HandlerThread("CarCalendarBackground");
    private final Clock mClock;
    private final ContentResolver mResolver;
    private final Locale mLocale;
    private final TelephonyManager mTelephonyManager;

    @Nullable private EventsLiveData mEventsLiveData;

    CarCalendarViewModel(ContentResolver resolver, Locale locale,
            TelephonyManager telephonyManager, Clock clock) {
        mLocale = locale;
        if (DEBUG) Log.d(TAG, "Creating view model");
        mResolver = resolver;
        mTelephonyManager = telephonyManager;
        mHandlerThread.start();
        mClock = clock;
    }

    /** Creates an {@link EventsLiveData} lazily and always returns the same instance. */
    EventsLiveData getEventsLiveData() {
        if (mEventsLiveData == null) {
            mEventsLiveData =
                    new EventsLiveData(
                            mClock,
                            new Handler(mHandlerThread.getLooper()),
                            mResolver,
                            new EventDescriptions(mLocale, mTelephonyManager),
                            new EventLocations());
        }
        return mEventsLiveData;
    }

    @Override
    protected void onCleared() {
        super.onCleared();
        mHandlerThread.quitSafely();
    }
}
