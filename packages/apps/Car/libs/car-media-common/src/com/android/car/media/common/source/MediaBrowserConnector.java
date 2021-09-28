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

package com.android.car.media.common.source;

import static com.android.car.apps.common.util.CarAppsDebugUtils.idHash;

import android.content.ComponentName;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.media.MediaBrowserCompat;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Preconditions;

import com.android.car.media.common.MediaConstants;

import java.util.Objects;

/**
 * A helper class to connect to a single {@link MediaSource} to its {@link MediaBrowserCompat}.
 * Connecting to a new one automatically disconnects the previous browser. Changes in the
 * connection status are sent via {@link MediaBrowserConnector.Callback}.
 */

public class MediaBrowserConnector {

    private static final String TAG = "MediaBrowserConnector";

    /**
     * Represents the state of the connection to the media browser service given to
     * {@link #connectTo}.
     */
    public enum ConnectionStatus {
        /**
         * The connection request to the browser is being initiated.
         * Sent from {@link #connectTo} just before calling {@link MediaBrowserCompat#connect}.
         */
        CONNECTING,
        /**
         * The connection to the browser has been established and it can be used.
         * Sent from {@link MediaBrowserCompat.ConnectionCallback#onConnected} if
         * {@link MediaBrowserCompat#isConnected} also returns true.
         */
        CONNECTED,
        /**
         * The connection to the browser was refused.
         * Sent from {@link MediaBrowserCompat.ConnectionCallback#onConnectionFailed} or from
         * {@link MediaBrowserCompat.ConnectionCallback#onConnected} if
         * {@link MediaBrowserCompat#isConnected} returns false.
         */
        REJECTED,
        /**
         * The browser crashed and that calls should NOT be made to it anymore.
         * Called from {@link MediaBrowserCompat.ConnectionCallback#onConnectionSuspended} and from
         * {@link #connectTo} when {@link MediaBrowserCompat#connect} throws
         * {@link IllegalStateException}.
         */
        SUSPENDED,
        /**
         * The connection to the browser is being closed.
         * When connecting to a new browser and the old browser is connected, this is sent
         * from {@link #connectTo} just before calling {@link MediaBrowserCompat#disconnect} on the
         * old browser.
         */
        DISCONNECTING
    }

    /**
     * Encapsulates a {@link ComponentName} with its {@link MediaBrowserCompat} and the
     * {@link ConnectionStatus}.
     */
    public static class BrowsingState {
        @NonNull public final MediaSource mMediaSource;
        @NonNull public final MediaBrowserCompat mBrowser;
        @NonNull public final ConnectionStatus mConnectionStatus;

        @VisibleForTesting
        public BrowsingState(@NonNull MediaSource mediaSource, @NonNull MediaBrowserCompat browser,
                @NonNull ConnectionStatus status) {
            mMediaSource = Preconditions.checkNotNull(mediaSource, "source can't be null");
            mBrowser = Preconditions.checkNotNull(browser, "browser can't be null");
            mConnectionStatus = Preconditions.checkNotNull(status, "status can't be null");
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            BrowsingState that = (BrowsingState) o;
            return mMediaSource.equals(that.mMediaSource)
                    && mBrowser.equals(that.mBrowser)
                    && mConnectionStatus == that.mConnectionStatus;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mMediaSource, mBrowser, mConnectionStatus);
        }
    }

    /** The callback to receive the current {@link MediaBrowserCompat} and its connection state. */
    public interface Callback {
        /** Notifies the listener of connection status changes. */
        void onBrowserConnectionChanged(@NonNull BrowsingState state);
    }

    private final Context mContext;
    private final Callback mCallback;
    private final int mMaxBitmapSizePx;

    @Nullable private MediaSource mMediaSource;
    @Nullable private MediaBrowserCompat mBrowser;

    /**
     * Create a new MediaBrowserConnector.
     *
     * @param context The Context with which to build MediaBrowsers.
     */
    public MediaBrowserConnector(@NonNull Context context, @NonNull Callback callback) {
        mContext = context;
        mCallback = callback;
        mMaxBitmapSizePx = mContext.getResources().getInteger(
                com.android.car.media.common.R.integer.media_items_bitmap_max_size_px);
    }

    private String getSourcePackage() {
        if (mMediaSource == null) return null;
        return mMediaSource.getBrowseServiceComponentName().getPackageName();
    }

    /** Counter so callbacks from obsolete connections can be ignored. */
    private int mBrowserConnectionCallbackCounter = 0;

    private class BrowserConnectionCallback extends MediaBrowserCompat.ConnectionCallback {

        private final int mSequenceNumber = ++mBrowserConnectionCallbackCounter;
        private final String mCallbackPackage = getSourcePackage();

        private BrowserConnectionCallback() {
            if (Log.isLoggable(TAG, Log.INFO)) {
                Log.i(TAG, "New Callback: " + idHash(this));
            }
        }

        private boolean isValidCall(String method) {
            if (mSequenceNumber != mBrowserConnectionCallbackCounter) {
                Log.e(TAG, "Callback: " + idHash(this) + " ignoring " + method + " for "
                        + mCallbackPackage + " seq: "
                        + mSequenceNumber + " current: " + mBrowserConnectionCallbackCounter
                        + " package: " + getSourcePackage());
                return false;
            } else if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, method + " " + getSourcePackage() + " mBrowser: " + idHash(mBrowser));
            }
            return true;
        }

        @Override
        public void onConnected() {
            if (isValidCall("onConnected")) {
                if (mBrowser != null && mBrowser.isConnected()) {
                    sendNewState(ConnectionStatus.CONNECTED);
                } else {
                    sendNewState(ConnectionStatus.REJECTED);
                }
            }
        }

        @Override
        public void onConnectionFailed() {
            if (isValidCall("onConnectionFailed")) {
                sendNewState(ConnectionStatus.REJECTED);
            }
        }

        @Override
        public void onConnectionSuspended() {
            if (isValidCall("onConnectionSuspended")) {
                sendNewState(ConnectionStatus.SUSPENDED);
            }
        }
    }

    private void sendNewState(ConnectionStatus cnx) {
        if (mMediaSource == null) {
            Log.e(TAG, "sendNewState mMediaSource is null!");
            return;
        }
        if (mBrowser == null) {
            Log.e(TAG, "sendNewState mBrowser is null!");
            return;
        }
        mCallback.onBrowserConnectionChanged(new BrowsingState(mMediaSource, mBrowser, cnx));
    }

    /**
     * Creates and connects a new {@link MediaBrowserCompat} if the given {@link MediaSource}
     * isn't null. If needed, the previous browser is disconnected.
     * @param mediaSource the media source to connect to.
     * @see MediaBrowserCompat#MediaBrowserCompat(Context, ComponentName,
     * MediaBrowserCompat.ConnectionCallback, android.os.Bundle)
     */
    public void connectTo(@Nullable MediaSource mediaSource) {
        if (mBrowser != null && mBrowser.isConnected()) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Disconnecting: " + getSourcePackage()
                        + " mBrowser: " + idHash(mBrowser));
            }
            sendNewState(ConnectionStatus.DISCONNECTING);
            mBrowser.disconnect();
        }

        mMediaSource = mediaSource;
        if (mMediaSource != null) {
            mBrowser = createMediaBrowser(mMediaSource, new BrowserConnectionCallback());
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Connecting to: " + getSourcePackage()
                        + " mBrowser: " + idHash(mBrowser));
            }
            try {
                sendNewState(ConnectionStatus.CONNECTING);
                mBrowser.connect();
            } catch (IllegalStateException ex) {
                // Is this comment still valid ?
                // Ignore: MediaBrowse could be in an intermediate state (not connected, but not
                // disconnected either.). In this situation, trying to connect again can throw
                // this exception, but there is no way to know without trying.
                Log.e(TAG, "Connection exception: " + ex);
                sendNewState(ConnectionStatus.SUSPENDED);
            }
        } else {
            mBrowser = null;
        }
    }

    // Override for testing.
    @NonNull
    protected MediaBrowserCompat createMediaBrowser(@NonNull MediaSource mediaSource,
            @NonNull MediaBrowserCompat.ConnectionCallback callback) {
        Bundle rootHints = new Bundle();
        rootHints.putInt(MediaConstants.EXTRA_MEDIA_ART_SIZE_HINT_PIXELS, mMaxBitmapSizePx);
        ComponentName browseService = mediaSource.getBrowseServiceComponentName();
        return new MediaBrowserCompat(mContext, browseService, callback, rootHints);
    }
}
