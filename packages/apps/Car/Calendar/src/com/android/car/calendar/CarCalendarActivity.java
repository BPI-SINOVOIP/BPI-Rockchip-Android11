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
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.StrictMode;
import android.telephony.TelephonyManager;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.ViewModel;
import androidx.lifecycle.ViewModelProvider;

import com.android.car.calendar.common.CalendarFormatter;
import com.android.car.calendar.common.Dialer;
import com.android.car.calendar.common.Navigator;

import com.google.common.collect.HashMultimap;
import com.google.common.collect.Multimap;

import java.time.Clock;
import java.util.Collection;
import java.util.Locale;

/** The main Activity for the Car Calendar App. */
public class CarCalendarActivity extends FragmentActivity {
    private static final String TAG = "CarCalendarActivity";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private final Multimap<String, Runnable> mPermissionToCallbacks = HashMultimap.create();

    // Allows tests to replace certain dependencies.
    @VisibleForTesting Dependencies mDependencies;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        maybeEnableStrictMode();

        // Tests can set fake dependencies before onCreate.
        if (mDependencies == null) {
            mDependencies = new Dependencies(
                    Locale.getDefault(),
                    Clock.systemDefaultZone(),
                    getContentResolver(),
                    getSystemService(TelephonyManager.class));
        }

        CarCalendarViewModel carCalendarViewModel =
                new ViewModelProvider(
                                this,
                                new CarCalendarViewModelFactory(
                                        mDependencies.mResolver,
                                        mDependencies.mLocale,
                                        mDependencies.mTelephonyManager,
                                        mDependencies.mClock))
                        .get(CarCalendarViewModel.class);

        CarCalendarView carCalendarView =
                new CarCalendarView(
                        this,
                        carCalendarViewModel,
                        new Navigator(this),
                        new Dialer(this),
                        new CalendarFormatter(
                                this.getApplicationContext(),
                                mDependencies.mLocale,
                                mDependencies.mClock));

        carCalendarView.show();
    }

    private void maybeEnableStrictMode() {
        if (DEBUG) {
            Log.i(TAG, "Enabling strict mode");
            StrictMode.setThreadPolicy(
                    new StrictMode.ThreadPolicy.Builder()
                            .detectAll()
                            .penaltyLog()
                            .penaltyDeath()
                            .build());
            StrictMode.setVmPolicy(
                    new StrictMode.VmPolicy.Builder()
                            .detectAll()
                            .penaltyLog()
                            .penaltyDeath()
                            .build());
        }
    }

    /**
     * Calls the given runnable only if the required permission is granted.
     *
     * <p>If the permission is already granted then the runnable is called immediately. Otherwise
     * the runnable is retained and the permission is requested. If the permission is granted the
     * runnable will be called otherwise it will be discarded.
     */
    void runWithPermission(String permission, Runnable runnable) {
        if (checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) {
            // Run immediately if we already have permission.
            if (DEBUG) Log.d(TAG, "Running with " + permission);
            runnable.run();
        } else {
            // Keep the runnable until the permission is granted.
            if (DEBUG) Log.d(TAG, "Waiting for " + permission);
            mPermissionToCallbacks.put(permission, runnable);
            requestPermissions(new String[] {permission}, /* requestCode= */ 0);
        }
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int i = 0; i < permissions.length; i++) {
            String permission = permissions[i];
            int grantResult = grantResults[i];
            Collection<Runnable> callbacks = mPermissionToCallbacks.removeAll(permission);
            if (grantResult == PackageManager.PERMISSION_GRANTED) {
                Log.e(TAG, "Permission " + permission + " granted");
                callbacks.forEach(Runnable::run);
            } else {
                // TODO(jdp) Also allow a denied runnable.
                Log.e(TAG, "Permission " + permission + " not granted");
            }
        }
    }

    private static class CarCalendarViewModelFactory implements ViewModelProvider.Factory {
        private final ContentResolver mResolver;
        private final TelephonyManager mTelephonyManager;
        private final Locale mLocale;
        private final Clock mClock;

        CarCalendarViewModelFactory(
                ContentResolver resolver,
                Locale locale,
                TelephonyManager telephonyManager,
                Clock clock) {
            mResolver = resolver;
            mLocale = locale;
            mTelephonyManager = telephonyManager;
            mClock = clock;
        }

        @SuppressWarnings("unchecked")
        @NonNull
        @Override
        public <T extends ViewModel> T create(@NonNull Class<T> aClass) {
            return (T) new CarCalendarViewModel(mResolver, mLocale, mTelephonyManager, mClock);
        }
    }

    static class Dependencies {
        private final Locale mLocale;
        private final Clock mClock;
        private final ContentResolver mResolver;
        private final TelephonyManager mTelephonyManager;

        Dependencies(
                Locale locale,
                Clock clock,
                ContentResolver resolver,
                TelephonyManager telephonyManager) {
            mLocale = locale;
            mClock = clock;
            mResolver = resolver;
            mTelephonyManager = telephonyManager;
        }
    }
}
