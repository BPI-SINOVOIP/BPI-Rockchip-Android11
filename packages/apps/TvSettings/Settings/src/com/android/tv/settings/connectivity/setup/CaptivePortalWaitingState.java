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

package com.android.tv.settings.connectivity.setup;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkInfo;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.ViewModelProviders;

import com.android.tv.settings.connectivity.util.State;
import com.android.tv.settings.connectivity.util.StateMachine;

/**
 * State for hosting captive portal log in.
 */
public class CaptivePortalWaitingState implements State {
    private final FragmentActivity mActivity;
    private Fragment mFragment;


    public CaptivePortalWaitingState(FragmentActivity activity) {
        mActivity = activity;
    }

    @Override
    public void processForward() {
        mFragment = null;
        openCaptivePortalPage();
        StateMachine stateMachine = ViewModelProviders.of(mActivity).get(StateMachine.class);
        stateMachine.getListener().onComplete(StateMachine.CONTINUE);
    }

    @Override
    public void processBackward() {
        StateMachine stateMachine = ViewModelProviders.of(mActivity).get(StateMachine.class);
        stateMachine.back();
    }

    private void openCaptivePortalPage() {
        ConnectivityManager connectivityManager = (ConnectivityManager) mActivity.getSystemService(
                Context.CONNECTIVITY_SERVICE);

        Network[] networks = connectivityManager.getAllNetworks();

        for (Network network : networks) {
            NetworkInfo networkInfo = connectivityManager.getNetworkInfo(network);
            if (networkInfo.isConnected()
                    && networkInfo.getType() == ConnectivityManager.TYPE_WIFI) {
                connectivityManager.startCaptivePortalApp(network);
                return;
            }
        }
    }

    @Override
    public Fragment getFragment() {
        return mFragment;
    }
}
