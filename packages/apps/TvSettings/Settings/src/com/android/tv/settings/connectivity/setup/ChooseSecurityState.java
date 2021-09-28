/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.leanback.widget.GuidanceStylist;
import androidx.leanback.widget.GuidedAction;
import androidx.lifecycle.ViewModelProviders;

import com.android.settingslib.wifi.AccessPoint;
import com.android.tv.settings.R;
import com.android.tv.settings.connectivity.security.WifiSecurityHelper;
import com.android.tv.settings.connectivity.util.State;
import com.android.tv.settings.connectivity.util.StateMachine;

import java.util.List;

/**
 * State responsible for choosing security type.
 */
public class ChooseSecurityState implements State {
    private final FragmentActivity mActivity;
    private Fragment mFragment;

    public ChooseSecurityState(FragmentActivity activity) {
        mActivity = activity;
    }

    @Override
    public void processForward() {
        mFragment = new ChooseSecurityFragment();
        FragmentChangeListener listener = (FragmentChangeListener) mActivity;
        if (listener != null) {
            listener.onFragmentChange(mFragment, true);
        }
    }

    @Override
    public void processBackward() {
        mFragment = new ChooseSecurityFragment();
        FragmentChangeListener listener = (FragmentChangeListener) mActivity;
        if (listener != null) {
            listener.onFragmentChange(mFragment, false);
        }
    }

    @Override
    public Fragment getFragment() {
        return mFragment;
    }

    /**
     * Fragment that needs user to select security type.
     */
    public static class ChooseSecurityFragment extends WifiConnectivityGuidedStepFragment {
        private StateMachine mStateMachine;
        private UserChoiceInfo mUserChoiceInfo;

        @Override
        public GuidanceStylist.Guidance onCreateGuidance(Bundle savedInstanceState) {
            return new GuidanceStylist.Guidance(
                    getString(R.string.security_type),
                    null,
                    null,
                    null);
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            mUserChoiceInfo = ViewModelProviders
                    .of(getActivity())
                    .get(UserChoiceInfo.class);
            mStateMachine = ViewModelProviders
                    .of(getActivity())
                    .get(StateMachine.class);
            super.onCreate(savedInstanceState);
        }

        @Override
        public void onCreateActions(List<GuidedAction> actions, Bundle savedInstanceState) {
            actions.addAll(WifiSecurityHelper.getSecurityTypes(getActivity()));
        }

        @Override
        public void onGuidedActionClicked(GuidedAction action) {
            mUserChoiceInfo.put(UserChoiceInfo.SECURITY, (int) action.getId());
            if (action.getId() == AccessPoint.SECURITY_NONE) {
                mStateMachine.getListener().onComplete(StateMachine.OPTIONS_OR_CONNECT);
            } else {
                mStateMachine.getListener().onComplete(StateMachine.CONTINUE);
            }
        }
    }
}
