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

package com.android.car.uxr;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.util.Log;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.XmlRes;

import com.android.car.ui.recyclerview.ContentLimiting;
import com.android.car.ui.utils.CarUxRestrictionsUtil;
import com.android.car.uxr.CarUxRestrictionsAppConfig.ListConfig;

/**
 * A class that can work together with a {@link ContentLimiting} {@link
 * androidx.recyclerview.widget.RecyclerView.Adapter} object to provide content limiting ability
 * based on changes to the state of the car, by listening for the latest {@link CarUxRestrictions}.
 *
 * <p>This class manages 3 things:
 * <ul>
 *     <li> Communications with the User Experience Restriction Engine
 *     <li> Looking up app-side overrides for customizing the content-limiting behavior of a given
 *     list of items in a particular screen
 *     <li> Relaying the relevant parts of that information to the registered
 *     adapter at the right time
 * </ul>
 *
 * <p>The app-side overrides are accessed via the {@link CarUxRestrictionsAppConfig} object.
 *
 * <p>Because all but one of the dependencies for this class can be instantiated as soon as a
 * {@link Context} is available, we provide a separate {@link #setAdapter(ContentLimiting)}
 * API for linking the targeted adapter. That way the registration can happen in a different part of
 * code, and potentially in a different lifecycle method to provide maximum flexibility.
 */
public class UxrContentLimiterImpl implements UxrContentLimiter {

    private ContentLimiting mAdapter;
    private ListConfig mListConfig;

    private final CarUxRestrictionsUtil mCarUxRestrictionsUtil;
    private final CarUxRestrictionsAppConfig mCarUxRestrictionsAppConfig;
    private final CarUxRestrictionsUtil.OnUxRestrictionsChangedListener mListener =
            new Listener();

    private class Listener implements CarUxRestrictionsUtil.OnUxRestrictionsChangedListener {
        private static final String TAG = "ContentLimitListener";

        @Override
        public void onRestrictionsChanged(@NonNull CarUxRestrictions carUxRestrictions) {
            if (mAdapter == null) {
                Log.w(TAG, "No adapter registered.");
                return;
            }

            int maxItems = getMaxItemsToUse(carUxRestrictions, mAdapter.getConfigurationId());
            logD("New limit " + maxItems);
            mAdapter.setMaxItems(maxItems);
        }

        private int getMaxItemsToUse(CarUxRestrictions carUxRestrictions, @IdRes int id) {
            // Unrelated restrictions are active. Quit early.
            if ((carUxRestrictions.getActiveRestrictions()
                    & CarUxRestrictions.UX_RESTRICTIONS_LIMIT_CONTENT)
                    == 0) {
                logD("Lists are unrestricted.");
                return ContentLimiting.UNLIMITED;
            }

            if (mListConfig == null || mListConfig.getContentLimit() == null) {
                logD("No configs found for adapter with the ID: " + id
                        + " Using the default limit");
                return carUxRestrictions.getMaxCumulativeContentItems();
            }

            logD("Using the provided override.");
            return mListConfig.getContentLimit();
        }

        private void logD(String s) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, s);
            }
        }
    }

    /**
     * Constructs a {@link UxrContentLimiterImpl} object given the app context and the XML resource
     * file to parse the User Experience Restriction override configs from.
     *
     * @param context - the app context
     * @param xmlRes  - the UXR override config XML resource
     */
    public UxrContentLimiterImpl(Context context, @XmlRes int xmlRes) {
        mCarUxRestrictionsUtil = CarUxRestrictionsUtil.getInstance(context);
        mCarUxRestrictionsAppConfig = CarUxRestrictionsAppConfig.getInstance(context, xmlRes);
    }

    @Override
    public void setAdapter(ContentLimiting adapter) {
        mAdapter = adapter;
        int key = mAdapter.getConfigurationId();
        if (mCarUxRestrictionsAppConfig.getMapping().containsKey(key)) {
            mListConfig = mCarUxRestrictionsAppConfig.getMapping().get(key);
            Integer overriddenMessageResId = mListConfig.getScrollingLimitedMessageResId();
            if (overriddenMessageResId != null) {
                mAdapter.setScrollingLimitedMessageResId(overriddenMessageResId);
            }
        }
    }

    /**
     * Start listening for changes to {@link CarUxRestrictions}.
     */
    public void start() {
        mCarUxRestrictionsUtil.register(mListener);
    }

    /**
     * Stop listening for changes to {@link CarUxRestrictions}.
     */
    public void stop() {
        mCarUxRestrictionsUtil.unregister(mListener);
    }
}
