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
package com.android.car;

import static android.car.media.CarMediaManager.MEDIA_SOURCE_MODE_BROWSE;
import static android.car.media.CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK;

import android.annotation.TestApi;
import android.app.ActivityManager;
import android.car.Car;
import android.car.media.CarMediaManager;
import android.car.media.CarMediaManager.MediaSourceChangedListener;
import android.car.media.CarMediaManager.MediaSourceMode;
import android.car.media.ICarMedia;
import android.car.media.ICarMediaSourceListener;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.media.session.MediaController;
import android.media.session.MediaController.TransportControls;
import android.media.session.MediaSession;
import android.media.session.MediaSession.Token;
import android.media.session.MediaSessionManager;
import android.media.session.MediaSessionManager.OnActiveSessionsChangedListener;
import android.media.session.PlaybackState;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.service.media.MediaBrowserService;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.user.CarUserService;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Deque;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/**
 * CarMediaService manages the currently active media source for car apps. This is different from
 * the MediaSessionManager's active sessions, as there can only be one active source in the car,
 * through both browse and playback.
 *
 * In the car, the active media source does not necessarily have an active MediaSession, e.g. if
 * it were being browsed only. However, that source is still considered the active source, and
 * should be the source displayed in any Media related UIs (Media Center, home screen, etc).
 */
public class CarMediaService extends ICarMedia.Stub implements CarServiceBase {

    private static final String SOURCE_KEY = "media_source_component";
    private static final String SOURCE_KEY_SEPARATOR = "_";
    private static final String PLAYBACK_STATE_KEY = "playback_state";
    private static final String SHARED_PREF = "com.android.car.media.car_media_service";
    private static final String COMPONENT_NAME_SEPARATOR = ",";
    private static final String MEDIA_CONNECTION_ACTION = "com.android.car.media.MEDIA_CONNECTION";
    private static final String EXTRA_AUTOPLAY = "com.android.car.media.autoplay";

    private static final int MEDIA_SOURCE_MODES = 2;

    // XML configuration options for autoplay on media source change.
    private static final int AUTOPLAY_CONFIG_NEVER = 0;
    private static final int AUTOPLAY_CONFIG_ALWAYS = 1;
    // This mode uses the current source's last stored playback state to resume playback
    private static final int AUTOPLAY_CONFIG_RETAIN_PER_SOURCE = 2;
    // This mode uses the previous source's playback state to resume playback
    private static final int AUTOPLAY_CONFIG_RETAIN_PREVIOUS = 3;

    private final Context mContext;
    private final CarUserService mUserService;
    private final UserManager mUserManager;
    private final MediaSessionManager mMediaSessionManager;
    private final MediaSessionUpdater mMediaSessionUpdater = new MediaSessionUpdater();
    @GuardedBy("mLock")
    private ComponentName[] mPrimaryMediaComponents = new ComponentName[MEDIA_SOURCE_MODES];
    private SharedPreferences mSharedPrefs;
    // MediaController for the current active user's active media session. This controller can be
    // null if playback has not been started yet.
    private MediaController mActiveUserMediaController;
    private SessionChangedListener mSessionsListener;
    private int mPlayOnMediaSourceChangedConfig;
    private int mPlayOnBootConfig;
    private boolean mIndependentPlaybackConfig;
    private int mCurrentPlaybackState;

    private boolean mPendingInit;

    @GuardedBy("mLock")
    private final RemoteCallbackList<ICarMediaSourceListener>[] mMediaSourceListeners =
            new RemoteCallbackList[MEDIA_SOURCE_MODES];

    private final Handler mMainHandler = new Handler(Looper.getMainLooper());


    private final HandlerThread mHandlerThread  = CarServiceUtils.getHandlerThread(
            getClass().getSimpleName());
    // Handler to receive PlaybackState callbacks from the active media controller.
    private final Handler mHandler = new Handler(mHandlerThread.getLooper());
    private final Object mLock = new Object();

    /** The component name of the last media source that was removed while being primary. */
    private ComponentName[] mRemovedMediaSourceComponents = new ComponentName[MEDIA_SOURCE_MODES];

    private final IntentFilter mPackageUpdateFilter;
    private boolean mIsPackageUpdateReceiverRegistered;

    /**
     * Listens to {@link Intent#ACTION_PACKAGE_REMOVED}, so we can fall back to a previously used
     * media source when the active source is uninstalled.
     */
    private final BroadcastReceiver mPackageUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getData() == null) {
                return;
            }
            String intentPackage = intent.getData().getSchemeSpecificPart();
            if (Intent.ACTION_PACKAGE_REMOVED.equals(intent.getAction())) {
                synchronized (mLock) {
                    for (int i = 0; i < MEDIA_SOURCE_MODES; i++) {
                        if (mPrimaryMediaComponents[i] != null
                                && mPrimaryMediaComponents[i].getPackageName().equals(
                                intentPackage)) {
                            if (intent.getBooleanExtra(Intent.EXTRA_REPLACING, false)) {
                                // If package is being replaced, it may not be removed from
                                // PackageManager queries  when we check for available
                                // MediaBrowseServices, so we iterate to find the next available
                                // source.
                                for (ComponentName component : getLastMediaSources(i)) {
                                    if (!mPrimaryMediaComponents[i].getPackageName()
                                            .equals(component.getPackageName())) {
                                        mRemovedMediaSourceComponents[i] =
                                                mPrimaryMediaComponents[i];
                                        if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
                                            Log.d(CarLog.TAG_MEDIA,
                                                    "temporarily replacing updated media source "
                                                            + mPrimaryMediaComponents[i]
                                                            + "with backup source: "
                                                            + component);
                                        }
                                        setPrimaryMediaSource(component, i);
                                        return;
                                    }
                                }
                                Log.e(CarLog.TAG_MEDIA, "No available backup media source");
                            } else {
                                if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
                                    Log.d(CarLog.TAG_MEDIA, "replacing removed media source "
                                            + mPrimaryMediaComponents[i] + "with backup source: "
                                            + getLastMediaSource(i));
                                }
                                mRemovedMediaSourceComponents[i] = null;
                                setPrimaryMediaSource(getLastMediaSource(i), i);
                            }
                        }
                    }
                }
            } else if (Intent.ACTION_PACKAGE_REPLACED.equals(intent.getAction())
                    || Intent.ACTION_PACKAGE_ADDED.equals(intent.getAction())) {
                for (int i = 0; i < MEDIA_SOURCE_MODES; i++) {
                    if (mRemovedMediaSourceComponents[i] != null && mRemovedMediaSourceComponents[i]
                            .getPackageName().equals(intentPackage)) {
                        if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
                            Log.d(CarLog.TAG_MEDIA, "restoring removed source: "
                                    + mRemovedMediaSourceComponents[i]);
                        }
                        setPrimaryMediaSource(mRemovedMediaSourceComponents[i], i);
                    }
                }
            }
        }
    };

    private final UserLifecycleListener mUserLifecycleListener = event -> {
        if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
            Log.d(CarLog.TAG_MEDIA, "CarMediaService.onEvent(" + event + ")");
        }

        switch (event.getEventType()) {
            case CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING:
                maybeInitUser(event.getUserId());
                break;
            case CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED:
                onUserUnlock(event.getUserId());
                break;
        }
    };

    public CarMediaService(Context context, CarUserService userService) {
        mContext = context;
        mUserManager = (UserManager) mContext.getSystemService(Context.USER_SERVICE);
        mMediaSessionManager = mContext.getSystemService(MediaSessionManager.class);
        mMediaSourceListeners[MEDIA_SOURCE_MODE_PLAYBACK] = new RemoteCallbackList();
        mMediaSourceListeners[MEDIA_SOURCE_MODE_BROWSE] = new RemoteCallbackList();
        mIndependentPlaybackConfig = mContext.getResources().getBoolean(
                R.bool.config_mediaSourceIndependentPlayback);

        mPackageUpdateFilter = new IntentFilter();
        mPackageUpdateFilter.addAction(Intent.ACTION_PACKAGE_REMOVED);
        mPackageUpdateFilter.addAction(Intent.ACTION_PACKAGE_REPLACED);
        mPackageUpdateFilter.addAction(Intent.ACTION_PACKAGE_ADDED);
        mPackageUpdateFilter.addDataScheme("package");
        mUserService = userService;
        mUserService.addUserLifecycleListener(mUserLifecycleListener);

        mPlayOnMediaSourceChangedConfig =
                mContext.getResources().getInteger(R.integer.config_mediaSourceChangedAutoplay);
        mPlayOnBootConfig = mContext.getResources().getInteger(R.integer.config_mediaBootAutoplay);
    }

    @Override
    // This method is called from ICarImpl after CarMediaService is created.
    public void init() {
        int currentUser = ActivityManager.getCurrentUser();
        maybeInitUser(currentUser);
    }

    private void maybeInitUser(int userId) {
        if (userId == UserHandle.USER_SYSTEM) {
            return;
        }
        if (mUserManager.isUserUnlocked(userId)) {
            initUser(userId);
        } else {
            mPendingInit = true;
        }
    }

    private void initUser(int userId) {
        // SharedPreferences are shared among different users thus only need initialized once. And
        // they should be initialized after user 0 is unlocked because SharedPreferences in
        // credential encrypted storage are not available until after user 0 is unlocked.
        // initUser() is called when the current foreground user is unlocked, and by that time user
        // 0 has been unlocked already, so initializing SharedPreferences in initUser() is fine.
        if (mSharedPrefs == null) {
            mSharedPrefs = mContext.getSharedPreferences(SHARED_PREF, Context.MODE_PRIVATE);
        }

        if (mIsPackageUpdateReceiverRegistered) {
            mContext.unregisterReceiver(mPackageUpdateReceiver);
        }
        UserHandle currentUser = new UserHandle(userId);
        mContext.registerReceiverAsUser(mPackageUpdateReceiver, currentUser,
                mPackageUpdateFilter, null, null);
        mIsPackageUpdateReceiverRegistered = true;

        mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK] = isCurrentUserEphemeral()
                ? getDefaultMediaSource() : getLastMediaSource(MEDIA_SOURCE_MODE_PLAYBACK);
        mPrimaryMediaComponents[MEDIA_SOURCE_MODE_BROWSE] = isCurrentUserEphemeral()
                ? getDefaultMediaSource() : getLastMediaSource(MEDIA_SOURCE_MODE_BROWSE);
        mActiveUserMediaController = null;

        updateMediaSessionCallbackForCurrentUser();
        notifyListeners(MEDIA_SOURCE_MODE_PLAYBACK);
        notifyListeners(MEDIA_SOURCE_MODE_BROWSE);

        startMediaConnectorService(shouldStartPlayback(mPlayOnBootConfig), currentUser);
    }

    /**
     * Starts a service on the current user that binds to the media browser of the current media
     * source. We start a new service because this one runs on user 0, and MediaBrowser doesn't
     * provide an API to connect on a specific user. Additionally, this service will attempt to
     * resume playback using the MediaSession obtained via the media browser connection, which
     * is more reliable than using active MediaSessions from MediaSessionManager.
     */
    private void startMediaConnectorService(boolean startPlayback, UserHandle currentUser) {
        Intent serviceStart = new Intent(MEDIA_CONNECTION_ACTION);
        serviceStart.setPackage(mContext.getResources().getString(R.string.serviceMediaConnection));
        serviceStart.putExtra(EXTRA_AUTOPLAY, startPlayback);
        mContext.startForegroundServiceAsUser(serviceStart, currentUser);
    }

    private boolean sharedPrefsInitialized() {
        if (mSharedPrefs == null) {
            // It shouldn't reach this but let's be cautious.
            Log.e(CarLog.TAG_MEDIA, "SharedPreferences are not initialized!");
            String className = getClass().getName();
            for (StackTraceElement ste : Thread.currentThread().getStackTrace()) {
                // Let's print the useful logs only.
                String log = ste.toString();
                if (log.contains(className)) {
                    Log.e(CarLog.TAG_MEDIA, log);
                }
            }
            return false;
        }
        return true;
    }

    private boolean isCurrentUserEphemeral() {
        return mUserManager.getUserInfo(ActivityManager.getCurrentUser()).isEphemeral();
    }

    @Override
    public void release() {
        mMediaSessionUpdater.unregisterCallbacks();
        mUserService.removeUserLifecycleListener(mUserLifecycleListener);
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*CarMediaService*");
        writer.println("\tCurrent playback media component: "
                + (mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK] == null ? "-"
                : mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK].flattenToString()));
        writer.println("\tCurrent browse media component: "
                + (mPrimaryMediaComponents[MEDIA_SOURCE_MODE_BROWSE] == null ? "-"
                : mPrimaryMediaComponents[MEDIA_SOURCE_MODE_BROWSE].flattenToString()));
        if (mActiveUserMediaController != null) {
            writer.println(
                    "\tCurrent media controller: " + mActiveUserMediaController.getPackageName());
            writer.println(
                    "\tCurrent browse service extra: " + getClassName(mActiveUserMediaController));
        }
        writer.println("\tNumber of active media sessions: " + mMediaSessionManager
                .getActiveSessionsForUser(null, ActivityManager.getCurrentUser()).size());

        writer.println("\tPlayback media source history: ");
        for (ComponentName name : getLastMediaSources(MEDIA_SOURCE_MODE_PLAYBACK)) {
            writer.println("\t" + name.flattenToString());
        }
        writer.println("\tBrowse media source history: ");
        for (ComponentName name : getLastMediaSources(MEDIA_SOURCE_MODE_BROWSE)) {
            writer.println("\t" + name.flattenToString());
        }

    }

    /**
     * @see {@link CarMediaManager#setMediaSource(ComponentName)}
     */
    @Override
    public void setMediaSource(@NonNull ComponentName componentName,
            @MediaSourceMode int mode) {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
            Log.d(CarLog.TAG_MEDIA, "Changing media source to: " + componentName.getPackageName());
        }
        setPrimaryMediaSource(componentName, mode);
    }

    /**
     * @see {@link CarMediaManager#getMediaSource()}
     */
    @Override
    public ComponentName getMediaSource(@CarMediaManager.MediaSourceMode int mode) {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        synchronized (mLock) {
            return mPrimaryMediaComponents[mode];
        }
    }

    /**
     * @see {@link CarMediaManager#registerMediaSourceListener(MediaSourceChangedListener)}
     */
    @Override
    public void registerMediaSourceListener(ICarMediaSourceListener callback,
            @MediaSourceMode int mode) {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        synchronized (mLock) {
            mMediaSourceListeners[mode].register(callback);
        }
    }

    /**
     * @see {@link CarMediaManager#unregisterMediaSourceListener(ICarMediaSourceListener)}
     */
    @Override
    public void unregisterMediaSourceListener(ICarMediaSourceListener callback,
            @MediaSourceMode int mode) {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        synchronized (mLock) {
            mMediaSourceListeners[mode].unregister(callback);
        }
    }

    @Override
    public List<ComponentName> getLastMediaSources(@CarMediaManager.MediaSourceMode int mode) {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        String key = getMediaSourceKey(mode);
        String serialized = mSharedPrefs.getString(key, "");
        return getComponentNameList(serialized).stream()
                .map(name -> ComponentName.unflattenFromString(name)).collect(Collectors.toList());
    }

    /** See {@link CarMediaManager#isIndependentPlaybackConfig}. */
    @Override
    @TestApi
    public boolean isIndependentPlaybackConfig() {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        synchronized (mLock) {
            return mIndependentPlaybackConfig;
        }
    }

    /** See {@link CarMediaManager#setIndependentPlaybackConfig}. */
    @Override
    @TestApi
    public void setIndependentPlaybackConfig(boolean independent) {
        ICarImpl.assertPermission(mContext, android.Manifest.permission.MEDIA_CONTENT_CONTROL);
        synchronized (mLock) {
            mIndependentPlaybackConfig = independent;
        }
    }

    // TODO(b/153115826): this method was used to be called from the ICar binder thread, but it's
    // now called by UserCarService. Currently UserCarServie is calling every listener in one
    // non-main thread, but it's not clear how the final behavior will be. So, for now it's ok
    // to post it to mMainHandler, but once b/145689885 is fixed, we might not need it.
    private void onUserUnlock(int userId) {
        mMainHandler.post(() -> {
            // No need to handle system user, non current foreground user.
            if (userId == UserHandle.USER_SYSTEM
                    || userId != ActivityManager.getCurrentUser()) {
                return;
            }
            if (mPendingInit) {
                initUser(userId);
                mPendingInit = false;
                if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
                    Log.d(CarLog.TAG_MEDIA,
                            "User " + userId + " is now unlocked");
                }
            }
        });
    }

    private void updateMediaSessionCallbackForCurrentUser() {
        if (mSessionsListener != null) {
            mMediaSessionManager.removeOnActiveSessionsChangedListener(mSessionsListener);
        }
        mSessionsListener = new SessionChangedListener(ActivityManager.getCurrentUser());
        mMediaSessionManager.addOnActiveSessionsChangedListener(mSessionsListener, null,
                ActivityManager.getCurrentUser(), null);
        mMediaSessionUpdater.registerCallbacks(mMediaSessionManager.getActiveSessionsForUser(
                null, ActivityManager.getCurrentUser()));
    }

    /**
     * Attempts to stop the current source using MediaController.TransportControls.stop()
     * This method also unregisters callbacks to the active media controller before calling stop(),
     * to preserve the PlaybackState before stopping.
     */
    private void stopAndUnregisterCallback() {
        if (mActiveUserMediaController != null) {
            mActiveUserMediaController.unregisterCallback(mMediaControllerCallback);
            if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
                Log.d(CarLog.TAG_MEDIA, "stopping " + mActiveUserMediaController.getPackageName());
            }
            TransportControls controls = mActiveUserMediaController.getTransportControls();
            if (controls != null) {
                controls.stop();
            } else {
                Log.e(CarLog.TAG_MEDIA, "Can't stop playback, transport controls unavailable "
                        + mActiveUserMediaController.getPackageName());
            }
        }
    }

    private class SessionChangedListener implements OnActiveSessionsChangedListener {
        private final int mCurrentUser;

        SessionChangedListener(int currentUser) {
            mCurrentUser = currentUser;
        }

        @Override
        public void onActiveSessionsChanged(List<MediaController> controllers) {
            if (ActivityManager.getCurrentUser() != mCurrentUser) {
                Log.e(CarLog.TAG_MEDIA, "Active session callback for old user: " + mCurrentUser);
                return;
            }
            mMediaSessionUpdater.registerCallbacks(controllers);
        }
    }

    private class MediaControllerCallback extends MediaController.Callback {

        private final MediaController mMediaController;
        private int mPreviousPlaybackState;

        private MediaControllerCallback(MediaController mediaController) {
            mMediaController = mediaController;
            PlaybackState state = mediaController.getPlaybackState();
            mPreviousPlaybackState = (state == null) ? PlaybackState.STATE_NONE : state.getState();
        }

        private void register() {
            mMediaController.registerCallback(this);
        }

        private void unregister() {
            mMediaController.unregisterCallback(this);
        }

        @Override
        public void onPlaybackStateChanged(@Nullable PlaybackState state) {
            if (state.getState() == PlaybackState.STATE_PLAYING
                    && state.getState() != mPreviousPlaybackState) {
                ComponentName mediaSource = getMediaSource(mMediaController.getPackageName(),
                        getClassName(mMediaController));
                if (mediaSource != null
                        && !mediaSource.equals(mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK])
                        && Log.isLoggable(CarLog.TAG_MEDIA, Log.INFO)) {
                    Log.i(CarLog.TAG_MEDIA, "Changing media source due to playback state change: "
                            + mediaSource.flattenToString());
                }
                setPrimaryMediaSource(mediaSource, MEDIA_SOURCE_MODE_PLAYBACK);
            }
            mPreviousPlaybackState = state.getState();
        }
    }

    private class MediaSessionUpdater {
        private Map<Token, MediaControllerCallback> mCallbacks = new HashMap<>();

        /**
         * Register a {@link MediaControllerCallback} for each given controller. Note that if a
         * controller was already watched, we don't register a callback again. This prevents an
         * undesired revert of the primary media source. Callbacks for previously watched
         * controllers that are not present in the given list are unregistered.
         */
        private void registerCallbacks(List<MediaController> newControllers) {

            List<MediaController> additions = new ArrayList<>(newControllers.size());
            Map<MediaSession.Token, MediaControllerCallback> updatedCallbacks =
                    new HashMap<>(newControllers.size());

            for (MediaController controller : newControllers) {
                MediaSession.Token token = controller.getSessionToken();
                MediaControllerCallback callback = mCallbacks.get(token);
                if (callback == null) {
                    callback = new MediaControllerCallback(controller);
                    callback.register();
                    additions.add(controller);
                }
                updatedCallbacks.put(token, callback);
            }

            for (MediaSession.Token token : mCallbacks.keySet()) {
                if (!updatedCallbacks.containsKey(token)) {
                    mCallbacks.get(token).unregister();
                }
            }

            mCallbacks = updatedCallbacks;
            updatePrimaryMediaSourceWithCurrentlyPlaying(additions);
            // If there are no playing media sources, and we don't currently have the controller
            // for the active source, check the active sessions for a matching controller. If this
            // is called after a user switch, its possible for a matching controller to already be
            // active before the user is unlocked, so we check all of the current controllers
            if (mActiveUserMediaController == null) {
                updateActiveMediaController(newControllers);
            }
        }

        /**
         * Unregister all MediaController callbacks
         */
        private void unregisterCallbacks() {
            for (Map.Entry<Token, MediaControllerCallback> entry : mCallbacks.entrySet()) {
                entry.getValue().unregister();
            }
        }
    }

    /**
     * Updates the primary media source, then notifies content observers of the change
     * Will update both the playback and browse sources if independent playback is not supported
     */
    private void setPrimaryMediaSource(@NonNull ComponentName componentName,
            @CarMediaManager.MediaSourceMode int mode) {
        synchronized (mLock) {
            if (mPrimaryMediaComponents[mode] != null
                    && mPrimaryMediaComponents[mode].equals((componentName))) {
                return;
            }
        }

        if (!mIndependentPlaybackConfig) {
            setPlaybackMediaSource(componentName);
            setBrowseMediaSource(componentName);
        } else if (mode == MEDIA_SOURCE_MODE_PLAYBACK) {
            setPlaybackMediaSource(componentName);
        } else if (mode == MEDIA_SOURCE_MODE_BROWSE) {
            setBrowseMediaSource(componentName);
        }
    }

    private void setPlaybackMediaSource(ComponentName playbackMediaSource) {
        stopAndUnregisterCallback();

        mActiveUserMediaController = null;
        synchronized (mLock) {
            mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK] = playbackMediaSource;
        }

        if (playbackMediaSource != null
                && !TextUtils.isEmpty(playbackMediaSource.flattenToString())) {
            if (!isCurrentUserEphemeral()) {
                saveLastMediaSource(playbackMediaSource, MEDIA_SOURCE_MODE_PLAYBACK);
            }
            if (playbackMediaSource
                    .equals(mRemovedMediaSourceComponents[MEDIA_SOURCE_MODE_PLAYBACK])) {
                mRemovedMediaSourceComponents[MEDIA_SOURCE_MODE_PLAYBACK] = null;
            }
        }

        notifyListeners(MEDIA_SOURCE_MODE_PLAYBACK);

        startMediaConnectorService(shouldStartPlayback(mPlayOnMediaSourceChangedConfig),
                new UserHandle(ActivityManager.getCurrentUser()));
        // Reset current playback state for the new source, in the case that the app is in an error
        // state (e.g. not signed in). This state will be updated from the app callback registered
        // below, to make sure mCurrentPlaybackState reflects the current source only.
        mCurrentPlaybackState = PlaybackState.STATE_NONE;
        updateActiveMediaController(mMediaSessionManager
                .getActiveSessionsForUser(null, ActivityManager.getCurrentUser()));
    }

    private void setBrowseMediaSource(ComponentName browseMediaSource) {
        synchronized (mLock) {
            mPrimaryMediaComponents[MEDIA_SOURCE_MODE_BROWSE] = browseMediaSource;
        }

        if (browseMediaSource != null && !TextUtils.isEmpty(browseMediaSource.flattenToString())) {
            if (!isCurrentUserEphemeral()) {
                saveLastMediaSource(browseMediaSource, MEDIA_SOURCE_MODE_BROWSE);
            }
            if (browseMediaSource
                    .equals(mRemovedMediaSourceComponents[MEDIA_SOURCE_MODE_BROWSE])) {
                mRemovedMediaSourceComponents[MEDIA_SOURCE_MODE_BROWSE] = null;
            }
        }

        notifyListeners(MEDIA_SOURCE_MODE_BROWSE);
    }

    private void notifyListeners(@CarMediaManager.MediaSourceMode int mode) {
        synchronized (mLock) {
            int i = mMediaSourceListeners[mode].beginBroadcast();
            while (i-- > 0) {
                try {
                    ICarMediaSourceListener callback =
                            mMediaSourceListeners[mode].getBroadcastItem(i);
                    callback.onMediaSourceChanged(mPrimaryMediaComponents[mode]);
                } catch (RemoteException e) {
                    Log.e(CarLog.TAG_MEDIA, "calling onMediaSourceChanged failed " + e);
                }
            }
            mMediaSourceListeners[mode].finishBroadcast();
        }
    }

    private MediaController.Callback mMediaControllerCallback = new MediaController.Callback() {
        @Override
        public void onPlaybackStateChanged(PlaybackState state) {
            if (!isCurrentUserEphemeral()) {
                savePlaybackState(state);
            }
        }
    };

    /**
     * Finds the currently playing media source, then updates the active source if the component
     * name is different.
     */
    private void updatePrimaryMediaSourceWithCurrentlyPlaying(
            List<MediaController> controllers) {
        for (MediaController controller : controllers) {
            if (controller.getPlaybackState() != null
                    && controller.getPlaybackState().getState() == PlaybackState.STATE_PLAYING) {
                String newPackageName = controller.getPackageName();
                String newClassName = getClassName(controller);
                if (!matchPrimaryMediaSource(newPackageName, newClassName,
                        MEDIA_SOURCE_MODE_PLAYBACK)) {
                    ComponentName mediaSource = getMediaSource(newPackageName, newClassName);
                    if (Log.isLoggable(CarLog.TAG_MEDIA, Log.INFO)) {
                        if (mediaSource != null) {
                            Log.i(CarLog.TAG_MEDIA,
                                    "MediaController changed, updating media source to: "
                                            + mediaSource.flattenToString());
                        } else {
                            // Some apps, like Chrome, have a MediaSession but no
                            // MediaBrowseService. Media Center doesn't consider such apps as
                            // valid media sources.
                            Log.i(CarLog.TAG_MEDIA,
                                    "MediaController changed, but no media browse service found "
                                            + "in package: " + newPackageName);
                        }
                    }
                    setPrimaryMediaSource(mediaSource, MEDIA_SOURCE_MODE_PLAYBACK);
                }
                return;
            }
        }
    }

    private boolean matchPrimaryMediaSource(@NonNull String newPackageName,
            @NonNull String newClassName, @CarMediaManager.MediaSourceMode int mode) {
        synchronized (mLock) {
            if (mPrimaryMediaComponents[mode] != null
                    && mPrimaryMediaComponents[mode].getPackageName().equals(newPackageName)) {
                // If the class name of currently active source is not specified, only checks
                // package name; otherwise checks both package name and class name.
                if (TextUtils.isEmpty(newClassName)) {
                    return true;
                } else {
                    return newClassName.equals(mPrimaryMediaComponents[mode].getClassName());
                }
            }
        }
        return false;
    }

    /**
     * Returns {@code true} if the provided component has a valid {@link MediaBrowseService}.
     */
    @VisibleForTesting
    public boolean isMediaService(@NonNull ComponentName componentName) {
        return getMediaService(componentName) != null;
    }

    /*
     * Gets the media service that matches the componentName for the current foreground user.
     */
    private ComponentName getMediaService(@NonNull ComponentName componentName) {
        String packageName = componentName.getPackageName();
        String className = componentName.getClassName();

        PackageManager packageManager = mContext.getPackageManager();
        Intent mediaIntent = new Intent();
        mediaIntent.setPackage(packageName);
        mediaIntent.setAction(MediaBrowserService.SERVICE_INTERFACE);
        List<ResolveInfo> mediaServices = packageManager.queryIntentServicesAsUser(mediaIntent,
                PackageManager.GET_RESOLVED_FILTER, ActivityManager.getCurrentUser());

        for (ResolveInfo service : mediaServices) {
            String serviceName = service.serviceInfo.name;
            if (!TextUtils.isEmpty(serviceName)
                    // If className is not specified, returns the first service in the package;
                    // otherwise returns the matched service.
                    // TODO(b/136274456): find a proper way to handle the case where there are
                    //  multiple services and the className is not specified.

                    && (TextUtils.isEmpty(className) || serviceName.equals(className))) {
                return new ComponentName(packageName, serviceName);
            }
        }

        if (Log.isLoggable(CarLog.TAG_MEDIA, Log.DEBUG)) {
            Log.d(CarLog.TAG_MEDIA, "No MediaBrowseService with ComponentName: "
                    + componentName.flattenToString());
        }
        return null;
    }

    /*
     * Gets the component name of the media service.
     */
    @Nullable
    private ComponentName getMediaSource(@NonNull String packageName, @NonNull String className) {
        return getMediaService(new ComponentName(packageName, className));
    }

    private void saveLastMediaSource(@NonNull ComponentName component, int mode) {
        if (!sharedPrefsInitialized()) {
            return;
        }
        String componentName = component.flattenToString();
        String key = getMediaSourceKey(mode);
        String serialized = mSharedPrefs.getString(key, null);
        if (serialized == null) {
            mSharedPrefs.edit().putString(key, componentName).apply();
        } else {
            Deque<String> componentNames = new ArrayDeque<>(getComponentNameList(serialized));
            componentNames.remove(componentName);
            componentNames.addFirst(componentName);
            mSharedPrefs.edit().putString(key, serializeComponentNameList(componentNames)).apply();
        }
    }

    private @NonNull ComponentName getLastMediaSource(int mode) {
        if (sharedPrefsInitialized()) {
            String key = getMediaSourceKey(mode);
            String serialized = mSharedPrefs.getString(key, "");
            if (!TextUtils.isEmpty(serialized)) {
                for (String name : getComponentNameList(serialized)) {
                    ComponentName componentName = ComponentName.unflattenFromString(name);
                    if (isMediaService(componentName)) {
                        return componentName;
                    }
                }
            }
        }
        return getDefaultMediaSource();
    }

    private ComponentName getDefaultMediaSource() {
        String defaultMediaSource = mContext.getString(R.string.config_defaultMediaSource);
        ComponentName defaultComponent = ComponentName.unflattenFromString(defaultMediaSource);
        if (isMediaService(defaultComponent)) {
            return defaultComponent;
        }
        return null;
    }

    private String serializeComponentNameList(Deque<String> componentNames) {
        return componentNames.stream().collect(Collectors.joining(COMPONENT_NAME_SEPARATOR));
    }

    private List<String> getComponentNameList(@NonNull String serialized) {
        String[] componentNames = serialized.split(COMPONENT_NAME_SEPARATOR);
        return (Arrays.asList(componentNames));
    }

    private void savePlaybackState(PlaybackState playbackState) {
        if (!sharedPrefsInitialized()) {
            return;
        }
        int state = playbackState != null ? playbackState.getState() : PlaybackState.STATE_NONE;
        mCurrentPlaybackState = state;
        String key = getPlaybackStateKey();
        mSharedPrefs.edit().putInt(key, state).apply();
    }

    /**
     * Builds a string key for saving the playback state for a specific media source (and user)
     */
    private String getPlaybackStateKey() {
        synchronized (mLock) {
            return PLAYBACK_STATE_KEY + ActivityManager.getCurrentUser()
                    + (mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK] == null ? ""
                    : mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK].flattenToString());
        }
    }

    private String getMediaSourceKey(int mode) {
        return SOURCE_KEY + mode + SOURCE_KEY_SEPARATOR + ActivityManager.getCurrentUser();
    }

    /**
     * Updates active media controller from the list that has the same component name as the primary
     * media component. Clears callback and resets media controller to null if not found.
     */
    private void updateActiveMediaController(List<MediaController> mediaControllers) {
        if (mPrimaryMediaComponents[MEDIA_SOURCE_MODE_PLAYBACK] == null) {
            return;
        }
        if (mActiveUserMediaController != null) {
            mActiveUserMediaController.unregisterCallback(mMediaControllerCallback);
            mActiveUserMediaController = null;
        }
        for (MediaController controller : mediaControllers) {
            if (matchPrimaryMediaSource(controller.getPackageName(), getClassName(controller),
                    MEDIA_SOURCE_MODE_PLAYBACK)) {
                mActiveUserMediaController = controller;
                PlaybackState state = mActiveUserMediaController.getPlaybackState();
                if (!isCurrentUserEphemeral()) {
                    savePlaybackState(state);
                }
                // Specify Handler to receive callbacks on, to avoid defaulting to the calling
                // thread; this method can be called from the MediaSessionManager callback.
                // Using the version of this method without passing a handler causes a
                // RuntimeException for failing to create a Handler.
                mActiveUserMediaController.registerCallback(mMediaControllerCallback, mHandler);
                return;
            }
        }
    }

    /**
     * Returns whether we should autoplay the current media source
     */
    private boolean shouldStartPlayback(int config) {
        switch (config) {
            case AUTOPLAY_CONFIG_NEVER:
                return false;
            case AUTOPLAY_CONFIG_ALWAYS:
                return true;
            case AUTOPLAY_CONFIG_RETAIN_PER_SOURCE:
                if (!sharedPrefsInitialized()) {
                    return false;
                }
                return mSharedPrefs.getInt(getPlaybackStateKey(), PlaybackState.STATE_NONE)
                        == PlaybackState.STATE_PLAYING;
            case AUTOPLAY_CONFIG_RETAIN_PREVIOUS:
                return mCurrentPlaybackState == PlaybackState.STATE_PLAYING;
            default:
                Log.e(CarLog.TAG_MEDIA, "Unsupported playback configuration: " + config);
                return false;
        }
    }

    @NonNull
    private static String getClassName(@NonNull MediaController controller) {
        Bundle sessionExtras = controller.getExtras();
        String value =
                sessionExtras == null ? "" : sessionExtras.getString(
                        Car.CAR_EXTRA_BROWSE_SERVICE_FOR_SESSION);
        return value != null ? value : "";
    }
}
