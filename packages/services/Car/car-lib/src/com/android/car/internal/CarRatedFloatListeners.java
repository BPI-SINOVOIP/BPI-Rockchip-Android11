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

package com.android.car.internal;

import android.util.SparseArray;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Represent listeners for a property grouped by their rate.
 * T is a type of EventListener such as CarPropertyEventCallback
 * in {@link android.car.hardware.property.CarPropertyManager}
 * @param <T>
 * @hide
 */
public class CarRatedFloatListeners<T> {
    private static final float NANOSECOND_PER_SECOND = 1000 * 1000 * 1000;
    private final Map<T, Float> mListenersToRate = new HashMap<>(4);

    private final Map<T, Long> mListenersUpdateTime = new HashMap<>(4);

    private float mUpdateRate;

    // key: areaId, value: lastUpdateTime in nanosecond
    protected SparseArray<Long> mAreaIdToLastUpdateTime = new SparseArray<>();

    protected CarRatedFloatListeners(float rate) {
        mUpdateRate = rate;
    }

    /** Check listener */
    public boolean contains(T listener) {
        return mListenersToRate.containsKey(listener);
    }
    /** Return current rate after updating */
    public float getRate() {
        return mUpdateRate;
    }

    /**
     * Remove given listener from the list and update rate if necessary.
     *
     * @param listener
     * @return true if rate was updated. Otherwise, returns false.
     */
    public boolean remove(T listener) {
        mListenersToRate.remove(listener);
        mListenersUpdateTime.remove(listener);
        if (mListenersToRate.isEmpty()) {
            return false;
        }
        Float updateRate = Collections.max(mListenersToRate.values());
        if (updateRate != mUpdateRate) {
            mUpdateRate = updateRate;
            return true;
        }
        return false;
    }

    public boolean isEmpty() {
        return mListenersToRate.isEmpty();
    }

    /**
     * Add given listener to the list and update rate if necessary.
     *
     * @param listener if null, add part is skipped.
     * @param updateRate
     * @return true if rate was updated. Otherwise, returns false.
     */
    public boolean addAndUpdateRate(T listener, float updateRate) {
        Float oldUpdateRate = mListenersToRate.put(listener, updateRate);
        mListenersUpdateTime.put(listener, 0L);
        if (mUpdateRate < updateRate) {
            mUpdateRate = updateRate;
            return true;
        } else if (oldUpdateRate != null && oldUpdateRate == mUpdateRate) {
            Float newUpdateRate = Collections.max(mListenersToRate.values());
            if (newUpdateRate != mUpdateRate) {
                mUpdateRate = newUpdateRate;
                return true;
            }
        }
        return false;
    }

    /**
     * Check whether listener should be notified by events.
     *
     * @param listener
     * @param eventTimeStamp
     * @return true if listener need to be notified.
     */
    public boolean needUpdateForSelectedListener(T listener, long eventTimeStamp) {
        Long nextUpdateTime = mListenersUpdateTime.get(listener);
        Float updateRate = mListenersToRate.get(listener);
        /** Update ON_CHANGE property. */
        if (updateRate == 0) {
            return true;
        }
        if (nextUpdateTime <= eventTimeStamp) {
            Float cycle = NANOSECOND_PER_SECOND / updateRate;
            nextUpdateTime = eventTimeStamp + cycle.longValue();
            mListenersUpdateTime.put(listener, nextUpdateTime);
            return true;
        }
        return false;
    }

    /**
     * @param areaId AreaId in CarPropertyValue
     * @param eventTime TimeStamp in CarPropertyValue
     * @return true if eventTime is greater than the last event time for the same areaId.
     */
    public boolean needUpdateForAreaId(int areaId, long eventTime) {
        long lastUpdateTime = mAreaIdToLastUpdateTime.get(areaId, 0L);
        if (eventTime >= lastUpdateTime) {
            mAreaIdToLastUpdateTime.put(areaId, eventTime);
            return true;
        }
        return false;
    }


    public Collection<T> getListeners() {
        return mListenersToRate.keySet();
    }
}

