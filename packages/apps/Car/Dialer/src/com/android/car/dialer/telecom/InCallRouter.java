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

package com.android.car.dialer.telecom;

import android.content.Context;
import android.content.Intent;
import android.telecom.Call;

import androidx.annotation.MainThread;
import androidx.preference.PreferenceManager;

import com.android.car.dialer.Constants;
import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.notification.InCallNotificationController;
import com.android.car.dialer.ui.activecall.InCallActivity;
import com.android.car.dialer.ui.activecall.InCallViewModel;

import java.util.ArrayList;

/**
 * Routes a call to different path depending on its state. If there is any {@link
 * InCallServiceImpl.ActiveCallListChangedCallback} that already handles the call, i.e. the {@link
 * InCallViewModel} that actively updates the in call page, then we don't show HUN for the ringing
 * call or attempt to start the in call page again.
 */
class InCallRouter {

    private static final String TAG = "CD.InCallRouter";

    private final Context mContext;
    private final InCallNotificationController mInCallNotificationController;
    private final ArrayList<InCallServiceImpl.ActiveCallListChangedCallback>
            mActiveCallListChangedCallbacks = new ArrayList<>();
    private final ProjectionCallHandler mProjectionCallHandler;

    InCallRouter(Context context) {
        mContext = context;
        mInCallNotificationController = InCallNotificationController.get();
        mProjectionCallHandler = new ProjectionCallHandler(context);
    }

    void start() {
        mProjectionCallHandler.start();
        mActiveCallListChangedCallbacks.add(mProjectionCallHandler);
    }

    void stop() {
        mActiveCallListChangedCallbacks.remove(mProjectionCallHandler);
        mProjectionCallHandler.stop();
    }

    /**
     * Routes the added call to the correct path:
     * <ul>
     * <li> First dispatches it to the {@link InCallServiceImpl.ActiveCallListChangedCallback}s.
     * <li> If the ringing call is not handled by callbacks, it will show a HUN.
     * <li> If the call is in other state and not handled by callbacks, it will try to launch the in
     * call page.
     */
    void onCallAdded(Call call) {
        boolean isHandled = routeToActiveCallListChangedCallback(call);
        if (isHandled) {
            return;
        }

        int state = call.getState();
        if (state == Call.STATE_RINGING) {
            routeToNotification(call);
            // Otherwise, no operations. Incoming call will be displayed outside of Dialer app
            // such as cluster.
        } else if (state != Call.STATE_DISCONNECTED) {
            // Don't launch the in call page if state is disconnected.
            // Otherwise, the InCallActivity finishes right after onCreate() and flashes.
            routeToFullScreenIncomingCallPage(false);
        }
    }

    /**
     * Called by {@link InCallServiceImpl#onCallRemoved(Call)}. It notifies the {@link
     * InCallServiceImpl.ActiveCallListChangedCallback}s to update the active call list.
     */
    void onCallRemoved(Call call) {
        for (InCallServiceImpl.ActiveCallListChangedCallback callback :
                mActiveCallListChangedCallbacks) {
            callback.onTelecomCallRemoved(call);
        }
    }

    @MainThread
    void registerActiveCallListChangedCallback(
            InCallServiceImpl.ActiveCallListChangedCallback callback) {
        mActiveCallListChangedCallbacks.add(callback);
    }

    @MainThread
    void unregisterActiveCallHandler(InCallServiceImpl.ActiveCallListChangedCallback callback) {
        mActiveCallListChangedCallbacks.remove(callback);
    }

    /**
     * Dispatches the call to {@link InCallServiceImpl.ActiveCallListChangedCallback}.
     */
    private boolean routeToActiveCallListChangedCallback(Call call) {
        boolean isHandled = false;
        for (InCallServiceImpl.ActiveCallListChangedCallback callback :
                mActiveCallListChangedCallbacks) {
            if (callback.onTelecomCallAdded(call)) {
                isHandled = true;
            }
        }

        return isHandled;
    }

    /**
     * Presents the ringing call in HUN.
     */
    private void routeToNotification(Call call) {
        if (shouldShowIncomingCallHun()) {
            mInCallNotificationController.showInCallNotification(call);
        }
        call.registerCallback(new Call.Callback() {
            @Override
            public void onStateChanged(Call call, int state) {
                L.d(TAG, "Ringing call state changed to %d", state);
                if (call.getState() != Call.STATE_DISCONNECTED) {
                    // Don't launch the in call page if state is disconnected. Otherwise, the
                    // InCallActivity finishes right after onCreate() and flashes.
                    routeToFullScreenIncomingCallPage(false);
                }
                mInCallNotificationController.cancelInCallNotification(call);
                call.unregisterCallback(this);
            }
        });
    }

    /**
     * Launches {@link InCallActivity} and presents the on going call in the in call page.
     */
    void routeToFullScreenIncomingCallPage(boolean showDialpad) {
        // It has been configured not to show the fullscreen incall ui.
        if (!shouldShowFullScreenUi()) {
            return;
        }

        Intent launchIntent = new Intent(mContext, InCallActivity.class);
        launchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        launchIntent.putExtra(Constants.Intents.EXTRA_SHOW_DIALPAD, showDialpad);
        mContext.startActivity(launchIntent);
    }

    private boolean shouldShowIncomingCallHun() {
        boolean shouldSuppressHunByDefault =
                mContext.getResources().getBoolean(R.bool.config_should_suppress_incoming_call_hun);
        return !PreferenceManager.getDefaultSharedPreferences(mContext)
                .getBoolean(mContext.getString(R.string.pref_no_incoming_call_hun_key),
                        shouldSuppressHunByDefault);
    }

    private boolean shouldShowFullScreenUi() {
        boolean shouldShowFullScreenUiByDefault =
                mContext.getResources().getBoolean(R.bool.config_show_fullscreen_incall_ui);
        return PreferenceManager.getDefaultSharedPreferences(mContext)
                .getBoolean(mContext.getString(R.string.pref_show_fullscreen_active_call_ui_key),
                        shouldShowFullScreenUiByDefault);
    }
}
