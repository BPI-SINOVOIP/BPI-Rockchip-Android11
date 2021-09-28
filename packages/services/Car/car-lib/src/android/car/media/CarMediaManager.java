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
package android.car.media;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.annotation.TestApi;
import android.car.Car;
import android.car.CarManagerBase;
import android.content.ComponentName;
import android.os.IBinder;
import android.os.RemoteException;

import com.android.internal.annotations.GuardedBy;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * API for updating and receiving updates to the primary media source in the car.
 * @hide
 */
@SystemApi
public final class CarMediaManager extends CarManagerBase {

    public static final int MEDIA_SOURCE_MODE_PLAYBACK = 0;
    public static final int MEDIA_SOURCE_MODE_BROWSE = 1;

    /** @hide */
    @IntDef(prefix = { "MEDIA_SOURCE_MODE_" }, value = {
            MEDIA_SOURCE_MODE_PLAYBACK,
            MEDIA_SOURCE_MODE_BROWSE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaSourceMode {}

    private final Object mLock = new Object();

    private final ICarMedia mService;
    @GuardedBy("mLock")
    private Map<MediaSourceChangedListener, ICarMediaSourceListener> mCallbackMap = new HashMap();

    /**
     * Get an instance of the CarMediaManager.
     *
     * Should not be obtained directly by clients, use {@link Car#getCarManager(String)} instead.
     * @hide
     */
    public CarMediaManager(Car car, IBinder service) {
        super(car);
        mService = ICarMedia.Stub.asInterface(service);
    }

    /**
     * Listener for updates to the primary media source
     */
    public interface MediaSourceChangedListener {

        /**
         * Called when the primary media source is changed
         */
        void onMediaSourceChanged(@NonNull ComponentName componentName);
    }

    /**
     * Gets the currently active media source for the provided mode
     *
     * @param mode the mode (playback or browse) for which the media source is active in.
     * @return the active media source in the provided mode, will be non-{@code null}.
     */
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public @NonNull ComponentName getMediaSource(@MediaSourceMode int mode) {
        try {
            return mService.getMediaSource(mode);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Sets the currently active media source for the provided mode
     *
     * @param mode the mode (playback or browse) for which the media source is active in.
     */
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public void setMediaSource(@NonNull ComponentName componentName, @MediaSourceMode int mode) {
        try {
            mService.setMediaSource(componentName, mode);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Register a callback that receives updates to the active media source.
     *
     * @param callback the callback to receive active media source updates.
     * @param mode the mode to receive updates for.
     */
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public void addMediaSourceListener(@NonNull MediaSourceChangedListener callback,
            @MediaSourceMode int mode) {
        try {
            ICarMediaSourceListener binderCallback = new ICarMediaSourceListener.Stub() {
                @Override
                public void onMediaSourceChanged(ComponentName componentName) {
                    callback.onMediaSourceChanged(componentName);
                }
            };
            synchronized (mLock) {
                mCallbackMap.put(callback, binderCallback);
            }
            mService.registerMediaSourceListener(binderCallback, mode);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Unregister a callback that receives updates to the active media source.
     *
     * @param callback the callback to be unregistered.
     * @param mode the mode that the callback was registered to receive updates for.
     */
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public void removeMediaSourceListener(@NonNull MediaSourceChangedListener callback,
            @MediaSourceMode int mode) {
        try {
            synchronized (mLock) {
                ICarMediaSourceListener binderCallback = mCallbackMap.remove(callback);
                mService.unregisterMediaSourceListener(binderCallback, mode);
            }
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Retrieve a list of media sources, ordered by most recently used.
     *
     * @param mode the mode (playback or browse) for which to retrieve media sources from.
     * @return non-{@code null} list of media sources, ordered by most recently used
     */
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public @NonNull List<ComponentName> getLastMediaSources(@MediaSourceMode int mode) {
        try {
            return mService.getLastMediaSources(mode);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /** @hide */
    @Override
    public void onCarDisconnected() {
        synchronized (mLock) {
            mCallbackMap.clear();
        }
    }

    /**
     * Returns whether the browse and playback sources can be changed independently.
     * @return true if the browse and playback sources can be changed independently, false if it
     * isn't or if the value could not be determined.
     * @hide
     */
    @TestApi
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public boolean isIndependentPlaybackConfig() {
        try {
            return mService.isIndependentPlaybackConfig();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Sets whether the browse and playback sources can be changed independently.
     * @param independent whether the browse and playback sources can be changed independently.
     * @hide
     */
    @TestApi
    @RequiresPermission(value = android.Manifest.permission.MEDIA_CONTENT_CONTROL)
    public void setIndependentPlaybackConfig(boolean independent) {
        try {
            mService.setIndependentPlaybackConfig(independent);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }
}
