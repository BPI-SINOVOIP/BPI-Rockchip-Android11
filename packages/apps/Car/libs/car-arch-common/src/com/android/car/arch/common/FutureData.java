/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.car.arch.common;

/**
 * Class that holds data with a loading state, and optionally the previous version of the data.
 *
 * @param <T> the output data type
 */
public class FutureData<T> {

    private final boolean mIsLoading;
    private final T mPastData;
    private final T mData;

    /** Returns an instance with a null data value and loading set to true. */
    public static <T> FutureData<T> newLoadingData() {
        return new FutureData<>(true, null);
    }

    /** Returns a loaded instance with the given data value. */
    public static <T> FutureData<T> newLoadedData(T data) {
        return new FutureData<>(false, data);
    }

    /** Returns a loaded instance with the given previous and current data values. */
    public static <T> FutureData<T> newLoadedData(T oldData, T newData) {
        return new FutureData<>(false, oldData, newData);
    }

    /**
     * This should become private.
     * @deprecated Use {@link #newLoadingData}, and {@link #newLoadedData} instead.
     */
    @Deprecated
    public FutureData(boolean isLoading, T data) {
        this(isLoading, null, data);
    }

    private FutureData(boolean isLoading, T oldData, T newData) {
        mIsLoading = isLoading;
        mPastData = oldData;
        mData = newData;
    }

    /**
     * Gets if the data is in the process of being loaded
     */
    public boolean isLoading() {
        return mIsLoading;
    }

    /**
     * Gets the data, if done loading. If currently loading, returns null
     */
    public T getData() {
        return mData;
    }

    /** If done loading, returns the previous version of the data, otherwise null. */
    public T getPastData() {
        return mPastData;
    }
}
