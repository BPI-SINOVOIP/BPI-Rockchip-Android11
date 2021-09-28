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
package com.android.wallpaper.util;

import android.app.Activity;
import android.app.WallpaperColors;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Rect;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.service.wallpaper.IWallpaperConnection;
import android.service.wallpaper.IWallpaperEngine;
import android.service.wallpaper.IWallpaperService;
import android.util.Log;
import android.view.View;
import android.view.WindowManager.LayoutParams;

import androidx.annotation.Nullable;

import com.android.systemui.shared.system.WallpaperEngineCompat;

/**
 * Implementation of {@link IWallpaperConnection} that handles communication with a
 * {@link android.service.wallpaper.WallpaperService}
 */
public class WallpaperConnection extends IWallpaperConnection.Stub implements ServiceConnection {

    /**
     * Returns whether live preview is available in framework.
     */
    public static boolean isPreviewAvailable() {
        return WallpaperEngineCompat.supportsScalePreview();
    }

    private static final String TAG = "WallpaperConnection";
    private final Activity mActivity;
    private final Intent mIntent;
    private final WallpaperConnectionListener mListener;
    private Rect mWallpaperPreviewRect;
    private IWallpaperService mService;
    private IWallpaperEngine mEngine;
    private boolean mConnected;
    private boolean mIsVisible;
    private boolean mIsEngineVisible;
    private boolean mEngineReady;
    private WallpaperEngineCompat mEngineCompat;

    /**
     * @param intent used to bind the wallpaper service
     * @param activity Activity that owns the window where the wallpaper is rendered
     * @param listener if provided, it'll be notified of connection/desconnection events
     * @param wallpaperPositionInScreen an optional Rect with the position to render the wallpaper
     *                                  in global coordinates. If null, the wallpaper will be
     *                                  rendered in full screen.
     */
    public WallpaperConnection(Intent intent, Activity activity,
            @Nullable WallpaperConnectionListener listener,
            @Nullable Rect wallpaperPositionInScreen) {
        mActivity = activity;
        mIntent = intent;
        mListener = listener;
        mWallpaperPreviewRect = wallpaperPositionInScreen;
    }

    /**
     * Bind the Service for this connection.
     */
    public boolean connect() {
        synchronized (this) {
            if (mConnected) {
                return true;
            }
            if (!mActivity.bindService(mIntent, this,
                    Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT)) {
                return false;
            }

            mConnected = true;
        }

        if (mListener != null) {
            mListener.onConnected();
        }

        return true;
    }

    /**
     * Disconnect and destroy the WallpaperEngine for this connection.
     */
    public void disconnect() {
        synchronized (this) {
            mConnected = false;
            if (mEngine != null) {
                try {
                    mEngine.destroy();
                } catch (RemoteException e) {
                    // Ignore
                }
                mEngine = null;
                mEngineCompat = null;
            }
            try {
                mActivity.unbindService(this);
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "Can't unbind wallpaper service. "
                        + "It might have crashed, just ignoring.", e);
            }
            mService = null;
        }
        if (mListener != null) {
            mListener.onDisconnected();
        }
    }

    /**
     * @see ServiceConnection#onServiceConnected(ComponentName, IBinder)
     */
    public void onServiceConnected(ComponentName name, IBinder service) {
        mService = IWallpaperService.Stub.asInterface(service);
        try {
            View root = mActivity.getWindow().getDecorView();
            int displayId = root.getDisplay().getDisplayId();

            mService.attach(this, root.getWindowToken(),
                    LayoutParams.TYPE_APPLICATION_MEDIA,
                    true, root.getWidth(), root.getHeight(),
                    new Rect(0, 0, 0, 0), displayId);
        } catch (RemoteException e) {
            Log.w(TAG, "Failed attaching wallpaper; clearing", e);
        }
    }

    /**
     * @see ServiceConnection#onServiceDisconnected(ComponentName)
     */
    public void onServiceDisconnected(ComponentName name) {
        mService = null;
        mEngine = null;
        mEngineCompat = null;
        Log.w(TAG, "Wallpaper service gone: " + name);
    }

    /**
     * @see IWallpaperConnection#attachEngine(IWallpaperEngine, int)
     */
    public void attachEngine(IWallpaperEngine engine, int displayId) {
        synchronized (this) {
            if (mConnected) {
                mEngine = engine;
                mEngineCompat = new WallpaperEngineCompat(mEngine);
                updateEnginePosition();
                if (mIsVisible) {
                    setEngineVisibility(true);
                }

                // Some wallpapers don't trigger #onWallpaperColorsChanged from remote. Requesting
                // wallpaper color here to ensure the #onWallpaperColorsChanged would get called.
                try {
                    mEngine.requestWallpaperColors();
                } catch (RemoteException e) {
                    Log.w(TAG, "Failed requesting wallpaper colors", e);
                }
            } else {
                try {
                    engine.destroy();
                } catch (RemoteException e) {
                    // Ignore
                }
            }
        }
    }

    private void updateEnginePosition() {
        if (mWallpaperPreviewRect != null && mEngineCompat != null) {
            mEngineCompat.scalePreview(mWallpaperPreviewRect);
        }
    }

    /**
     * Returns the engine handled by this WallpaperConnection
     */
    public IWallpaperEngine getEngine() {
        return mEngine;
    }

    /**
     * @see IWallpaperConnection#setWallpaper(String)
     */
    public ParcelFileDescriptor setWallpaper(String name) {
        return null;
    }

    @Override
    public void onWallpaperColorsChanged(WallpaperColors colors, int displayId) {
        mActivity.runOnUiThread(() -> {
            if (mListener != null) {
                mListener.onWallpaperColorsChanged(colors, displayId);
            }
        });
    }

    @Override
    public void engineShown(IWallpaperEngine engine)  {
        mEngineReady = true;
        mActivity.runOnUiThread(() -> {
            if (mListener != null) {
                mListener.onEngineShown();
            }
        });
    }

    /**
     * Update the wallpaper's position to match the given Rect (in gobal coordinates)
     */
    public void updatePreviewPosition(Rect positionGlobalRect) {
        mWallpaperPreviewRect = positionGlobalRect;
        updateEnginePosition();
    }

    /**
     * Returns true if the wallpaper engine has been initialized.
     */
    public boolean isEngineReady() {
        return mEngineReady;
    }

    /**
     * Sets the engine's visibility.
     */
    public void setVisibility(boolean visible) {
        mIsVisible = visible;
        setEngineVisibility(visible);
    }

    private void setEngineVisibility(boolean visible) {
        if (mEngine != null && visible != mIsEngineVisible) {
            try {
                mEngine.setVisibility(visible);
                mIsEngineVisible = visible;
            } catch (RemoteException e) {
                Log.w(TAG, "Failure setting wallpaper visibility ", e);
            }
        }
    }

    /**
     * Interface to be notified of connect/disconnect events from {@link WallpaperConnection}
     */
    public interface WallpaperConnectionListener {
        /**
         * Called after the Wallpaper service has been bound.
         */
        default void onConnected() {}

        /**
         * Called after the Wallpaper engine has been terminated and the service has been unbound.
         */
        default void onDisconnected() {}

        /**
         * Called after the wallpaper has been rendered for the first time.
         */
        default void onEngineShown() {}

        /**
         * Called after the wallpaper color is available or updated.
         */
        default void onWallpaperColorsChanged(WallpaperColors colors, int displayId) {}
    }
}
