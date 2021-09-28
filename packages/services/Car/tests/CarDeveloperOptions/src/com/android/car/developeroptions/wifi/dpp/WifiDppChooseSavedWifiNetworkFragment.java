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

package com.android.car.developeroptions.wifi.dpp;

import android.app.ActionBar;
import android.app.settings.SettingsEnums;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.ListView;

import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import com.android.car.developeroptions.R;

/**
 * After a camera APP scanned a Wi-Fi DPP QR code, it can trigger
 * {@code WifiDppConfiguratorActivity} to start with this fragment to choose a saved Wi-Fi network.
 */
public class WifiDppChooseSavedWifiNetworkFragment extends WifiDppQrCodeBaseFragment {
    private static final String TAG_FRAGMENT_WIFI_NETWORK_LIST = "wifi_network_list_fragment";

    private ListView mSavedWifiNetworkList;
    private Button mButtonLeft;
    private Button mButtonRight;

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.SETTINGS_WIFI_DPP_CONFIGURATOR;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        final ActionBar actionBar = getActivity().getActionBar();
        if (actionBar != null) {
            actionBar.hide();
        }

        /** Embeded WifiNetworkListFragment as child fragment within
         * WifiDppChooseSavedWifiNetworkFragment. */
        final FragmentManager fragmentManager = getChildFragmentManager();
        final WifiNetworkListFragment fragment = new WifiNetworkListFragment();
        final Bundle args = getArguments();
        if (args != null) {
            fragment.setArguments(args);
        }
        final FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.replace(R.id.wifi_network_list_container, fragment,
                TAG_FRAGMENT_WIFI_NETWORK_LIST);
        fragmentTransaction.commit();
    }

    @Override
    public final View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        return inflater.inflate(R.layout.wifi_dpp_choose_saved_wifi_network_fragment, container,
                /* attachToRoot */ false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        setHeaderIconImageResource(R.drawable.ic_wifi_signal_4);

        mTitle.setText(R.string.wifi_dpp_choose_network);
        mSummary.setText(R.string.wifi_dpp_choose_network_to_connect_device);

        mButtonLeft = view.findViewById(R.id.button_left);
        mButtonLeft.setText(R.string.cancel);
        mButtonLeft.setOnClickListener(v -> {
            String action = null;
            final Intent intent = getActivity().getIntent();
            if (intent != null) {
                action = intent.getAction();
            }
            if (WifiDppConfiguratorActivity.ACTION_CONFIGURATOR_QR_CODE_SCANNER.equals(action) ||
                    WifiDppConfiguratorActivity
                    .ACTION_CONFIGURATOR_QR_CODE_GENERATOR.equals(action)) {
                getFragmentManager().popBackStack();
            } else {
                getActivity().finish();
            }
        });

        mButtonRight = view.findViewById(R.id.button_right);
        mButtonRight.setVisibility(View.GONE);

        if (savedInstanceState == null) {
            // For Talkback to describe this fragment
            mTitleSummaryContainer.sendAccessibilityEvent(
                    AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        }
    }
}
