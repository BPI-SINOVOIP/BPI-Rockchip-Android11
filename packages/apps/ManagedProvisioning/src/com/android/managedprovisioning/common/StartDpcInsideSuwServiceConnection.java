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

package com.android.managedprovisioning.common;

import static android.content.Context.BIND_AUTO_CREATE;

import android.annotation.IntDef;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;

import com.android.internal.annotations.VisibleForTesting;

import com.google.android.setupwizard.util.INetworkInterceptService;

/**
 * This wrapper performs system configuration that is necessary for some DPCs to run properly, and
 * reverts the system to its previous state after the DPC has finished.  One call should be made to
 * {@link #triggerDpcStart(Context, Runnable)} to start the DPC, and one call should be made to
 * {@link #dpcFinished()} after the DPC activity returns a result.  Whenever the activity that
 * hosts this connection is destroyed, {@link #unbind(Context)} should also be called.
 */
public class StartDpcInsideSuwServiceConnection implements ServiceConnection {
    @VisibleForTesting
    static final String NETWORK_INTERCEPT_SERVICE_ACTION =
            "com.google.android.setupwizard.NetworkInterceptService.BIND";

    @VisibleForTesting
    static final String SETUP_WIZARD_PACKAGE_NAME = "com.google.android.setupwizard";

    private static final String DPC_STATE_KEY = "dpc_state";
    private static final String NETWORK_INTERCEPT_SERVICE_BINDING_INITIATED_KEY =
            "network_intercept_service_binding_initiated";
    private static final String NETWORK_INTERCEPT_WAS_INITIALLY_ENABLED_KEY =
            "network_intercept_was_initially_enabled";

    private static final int DPC_STATE_NOT_STARTED = 1;
    private static final int DPC_STATE_STARTED = 2;
    private static final int DPC_STATE_FINISHED = 3;

    @IntDef({DPC_STATE_NOT_STARTED,
            DPC_STATE_STARTED,
            DPC_STATE_FINISHED})
    private @interface DpcState {}

    private Runnable mDpcIntentSender;
    private INetworkInterceptService mNetworkInterceptService;
    private @DpcState int mDpcState;
    private boolean mNetworkInterceptServiceBindingInitiated;
    private boolean mNetworkInterceptWasInitiallyEnabled;

    public StartDpcInsideSuwServiceConnection() {
        mDpcState = DPC_STATE_NOT_STARTED;
        mNetworkInterceptServiceBindingInitiated = false;
        mNetworkInterceptWasInitiallyEnabled = true;
    }

    public StartDpcInsideSuwServiceConnection(Context context, Bundle savedInstanceState,
            Runnable dpcIntentSender) {
        // Three statements are required to assign an int value from a bundle into an IntDef
        final int savedDpcState = savedInstanceState.getInt(DPC_STATE_KEY, DPC_STATE_NOT_STARTED);
        final @DpcState int dpcStateAsIntDef = savedDpcState;
        mDpcState = dpcStateAsIntDef;

        mNetworkInterceptServiceBindingInitiated = savedInstanceState.getBoolean(
                NETWORK_INTERCEPT_SERVICE_BINDING_INITIATED_KEY, false);
        mNetworkInterceptWasInitiallyEnabled = savedInstanceState.getBoolean(
                NETWORK_INTERCEPT_WAS_INITIALLY_ENABLED_KEY, true);

        if (mNetworkInterceptServiceBindingInitiated) {
            // bindService() succeeded previously, which implies that triggerDpc() was previously
            // called.  Attempt to bind again, so that we have a service connection ready for when
            // dpcFinished() is called.  Additionally, if we never received the onServiceConnected()
            // callback previously, it means that the DPC was never actually started, so the DPC
            // will be started when this new service connection is established.
            mNetworkInterceptServiceBindingInitiated = false;
            if (mDpcState != DPC_STATE_FINISHED) {
                if (mDpcState == DPC_STATE_NOT_STARTED) {
                    mDpcIntentSender = dpcIntentSender;
                }
                tryBindService(context);
            }
        }
    }

    /**
     * This should be called when onSaveInstanceState() is invoked on the host activity (the one
     * that holds this connection).
     */
    public void saveInstanceState(Bundle outState) {
        outState.putInt(DPC_STATE_KEY, mDpcState);
        outState.putBoolean(NETWORK_INTERCEPT_SERVICE_BINDING_INITIATED_KEY,
                mNetworkInterceptServiceBindingInitiated);
        outState.putBoolean(NETWORK_INTERCEPT_WAS_INITIALLY_ENABLED_KEY,
                mNetworkInterceptWasInitiallyEnabled);
    }

    /**
     * Configure Setup Wizard to allow the DPC setup activity to send network intents, then start
     * the DPC setup activity.  This should only be called once - subsequent calls have no effect.
     */
    public void triggerDpcStart(Context context, Runnable dpcIntentSender) {
        if (mNetworkInterceptServiceBindingInitiated || mDpcState != DPC_STATE_NOT_STARTED) {
            ProvisionLogger.loge("Duplicate calls to triggerDpcStart() - ignoring");
            return;
        }

        mDpcIntentSender = dpcIntentSender;

        tryBindService(context);

        // If binding to NetworkInterceptService succeeds, DPC will be started from one of the
        // ServiceConnection's callbacks.  If binding failed, we need to start the DPC ourselves
        // here.  Without the service binding, the DPC setup activity may fail, in which case it is
        // the DPC's responsibility to report what happened and decide how to handle the failure.
        if (!mNetworkInterceptServiceBindingInitiated) {
            sendDpcIntentIfNotAlreadySent();
        }
    }

    private void tryBindService(Context context) {
        final Intent networkInterceptServiceIntent = new Intent(NETWORK_INTERCEPT_SERVICE_ACTION);
        networkInterceptServiceIntent.setPackage(SETUP_WIZARD_PACKAGE_NAME);

        try {
            if (context.bindService(networkInterceptServiceIntent, this, BIND_AUTO_CREATE)) {
                mNetworkInterceptServiceBindingInitiated = true;
            } else {
                ProvisionLogger.loge("Failed to bind to SUW NetworkInterceptService");
            }
        } catch(SecurityException e) {
            ProvisionLogger.loge("Access denied to SUW NetworkInterceptService", e);
        }
    }

    /**
     * Reset network interception to its original state.
     */
    public void dpcFinished() {
        if (mNetworkInterceptServiceBindingInitiated && mNetworkInterceptWasInitiallyEnabled) {
            enableNetworkIntentIntercept();
        }
        mDpcState = DPC_STATE_FINISHED;
    }

    /**
     * Unbind from the SUW NetworkInterceptService.  This should be called whenever the activity
     * that hosts this service connection is destroyed, in order to avoid leaking the service
     * connection.
     */
    public void unbind(Context context) {
        if (mNetworkInterceptServiceBindingInitiated) {
            context.unbindService(this);
            mNetworkInterceptServiceBindingInitiated = false;
        }

        mNetworkInterceptService = null;
    }

    @Override
    public void onServiceConnected(ComponentName className, IBinder service) {
        ProvisionLogger.logi("Connection to SUW NetworkInterceptService established");

        mNetworkInterceptService = INetworkInterceptService.Stub.asInterface(service);

        // If the service disconnects and reconnects, don't cache the initial network intercept
        // setting again, since we changed this when we made the first connection.
        if (mDpcState != DPC_STATE_NOT_STARTED) {
            return;
        }

        cacheInitialNetworkInterceptSetting();

        if (mNetworkInterceptWasInitiallyEnabled) {
            disableNetworkIntentIntercept();
        }

        sendDpcIntentIfNotAlreadySent();
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        ProvisionLogger.logw("Connection to SUW NetworkInterceptService lost");
        mNetworkInterceptService = null;
    }

    @Override
    public void onBindingDied(ComponentName name) {
        ProvisionLogger.logw("Connection to SUW NetworkInterceptService died");
        mNetworkInterceptService = null;
    }

    @Override
    public void onNullBinding(ComponentName name) {
        ProvisionLogger.loge("Binding to SUW NetworkInterceptService returned null");
        sendDpcIntentIfNotAlreadySent();
    }

    private void sendDpcIntentIfNotAlreadySent() {
        if (mDpcState == DPC_STATE_NOT_STARTED && mDpcIntentSender != null) {
            mDpcIntentSender.run();
            mDpcState = DPC_STATE_STARTED;
            // Release any reference that mDpcIntentSender has to the host activity
            mDpcIntentSender = null;
        }
    }

    private void cacheInitialNetworkInterceptSetting() {
        try {
            mNetworkInterceptWasInitiallyEnabled =
                    mNetworkInterceptService.isNetworkIntentIntercepted();
        } catch (Exception e) {
            ProvisionLogger.loge("Exception from SUW NetworkInterceptService", e);
        }
    }

    private void disableNetworkIntentIntercept() {
        try {
            if (!mNetworkInterceptService.disableNetworkIntentIntercept()) {
                ProvisionLogger.loge(
                        "Service call to disable SUW network intent interception failed");
            }
        } catch (Exception e) {
            ProvisionLogger.loge("Exception from SUW NetworkInterceptService", e);
        }
    }

    private void enableNetworkIntentIntercept() {
        if (mNetworkInterceptService == null) {
            ProvisionLogger.logw(
                    "Attempt to re-enable SUW network intent interception when service is null");
            return;
        }

        try {
            if (!mNetworkInterceptService.enableNetworkIntentIntercept()) {
                ProvisionLogger.logw(
                        "Service call to re-enable SUW network intent interception failed");
            }
        } catch (Exception e) {
            ProvisionLogger.logw("Exception from SUW NetworkInterceptService", e);
        }
    }
}
