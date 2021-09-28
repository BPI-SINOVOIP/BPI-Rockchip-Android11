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
package com.android.wallpaper.testing;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;

import com.android.wallpaper.model.WallpaperRotationInitializer;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.WallpaperPreferences;

/**
 * Test implementation of {@link WallpaperRotationInitializer}.
 */
public class TestWallpaperRotationInitializer implements WallpaperRotationInitializer {

    private boolean mIsRotationInitialized;
    @NetworkPreference
    private int mNetworkPreference;
    private Listener mListener;
    @RotationInitializationState
    private int mRotationInitializationState;

    TestWallpaperRotationInitializer() {
        mIsRotationInitialized = false;
        mNetworkPreference = NETWORK_PREFERENCE_WIFI_ONLY;
        mRotationInitializationState = WallpaperRotationInitializer.ROTATION_NOT_INITIALIZED;
    }

    TestWallpaperRotationInitializer(@RotationInitializationState int rotationState) {
        mIsRotationInitialized = false;
        mNetworkPreference = NETWORK_PREFERENCE_WIFI_ONLY;
        mRotationInitializationState = rotationState;
    }

    private TestWallpaperRotationInitializer(Parcel unused) {
        mIsRotationInitialized = false;
        mNetworkPreference = NETWORK_PREFERENCE_WIFI_ONLY;
    }

    @Override
    public void setFirstWallpaperInRotation(Context context,
            @NetworkPreference int networkPreference,
            Listener listener) {
        mListener = listener;
        mNetworkPreference = networkPreference;
    }

    @Override
    public boolean startRotation(Context appContext) {
        WallpaperPreferences wallpaperPreferences =
                InjectorProvider.getInjector().getPreferences(appContext);
        wallpaperPreferences.setWallpaperPresentationMode(
                WallpaperPreferences.PRESENTATION_MODE_ROTATING);
        return true;
    }

    @Override
    public void fetchRotationInitializationState(Context context, RotationStateListener listener) {
        listener.onRotationStateReceived(mRotationInitializationState);
    }

    @Override
    public int getRotationInitializationStateDirty(Context context) {
        return mRotationInitializationState;
    }

    /** Sets the mocked rotation initialization state. */
    public void setRotationInitializationState(@RotationInitializationState int rotationState) {
        mRotationInitializationState = rotationState;
    }

    /**
     * Simulates completion of the asynchronous task to download the first wallpaper in a rotation..
     *
     * @param isSuccessful Whether the first wallpaper downloaded successfully.
     */
    public void finishDownloadingFirstWallpaper(boolean isSuccessful) {
        if (isSuccessful) {
            mIsRotationInitialized = true;
            mListener.onFirstWallpaperInRotationSet();
        } else {
            mIsRotationInitialized = false;
            mListener.onError();
        }
    }

    /** Returns whether a wallpaper rotation is initialized. */
    public boolean isRotationInitialized() {
        return mIsRotationInitialized;
    }

    /** Returns whether a wallpaper rotation is enabled for WiFi-only. */
    public boolean isWifiOnly() {
        return mNetworkPreference == NETWORK_PREFERENCE_WIFI_ONLY;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public static final Parcelable.Creator<TestWallpaperRotationInitializer> CREATOR =
            new Parcelable.Creator<TestWallpaperRotationInitializer>() {
                @Override
                public TestWallpaperRotationInitializer createFromParcel(Parcel in) {
                    return new TestWallpaperRotationInitializer(in);
                }

                @Override
                public TestWallpaperRotationInitializer[] newArray(int size) {
                    return new TestWallpaperRotationInitializer[size];
                }
            };
}
