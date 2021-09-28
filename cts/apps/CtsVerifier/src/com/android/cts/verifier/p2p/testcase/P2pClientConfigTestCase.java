/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.verifier.p2p.testcase;

import android.content.Context;
import android.net.wifi.p2p.WifiP2pConfig;

import com.android.cts.verifier.R;

/**
 * Test case to join a p2p group with wps push button.
 */
public class P2pClientConfigTestCase extends ConnectReqTestCase {

    private int mGroupOperatingBand = WifiP2pConfig.GROUP_OWNER_BAND_AUTO;
    private int mGroupOperatingFrequency = 0;
    private String mNetworkName = "DIRECT-XY-HELLO";

    public P2pClientConfigTestCase(Context context) {
        super(context);
    }

    public P2pClientConfigTestCase(Context context, int bandOrFrequency) {
        super(context);
        StringBuilder builder = new StringBuilder(mNetworkName);
        switch(bandOrFrequency) {
            case WifiP2pConfig.GROUP_OWNER_BAND_AUTO:
                break;
            case WifiP2pConfig.GROUP_OWNER_BAND_2GHZ:
                builder.append("-2.4G");
                mGroupOperatingBand = bandOrFrequency;
                break;
            case WifiP2pConfig.GROUP_OWNER_BAND_5GHZ:
                builder.append("-5G");
                mGroupOperatingBand = bandOrFrequency;
                break;
            default:
                builder.append("-" + bandOrFrequency + "MHz");
                mGroupOperatingFrequency = bandOrFrequency;
        }
        mNetworkName = builder.toString();
    }

    @Override
    protected boolean executeTest() throws InterruptedException {

        WifiP2pConfig.Builder b = new WifiP2pConfig.Builder()
                .setNetworkName(mNetworkName)
                .setPassphrase("DEADBEEF");

        if (mGroupOperatingBand > 0) {
            b.setGroupOperatingBand(mGroupOperatingBand);
        } else if (mGroupOperatingFrequency > 0) {
            b.setGroupOperatingFrequency(mGroupOperatingFrequency);
        }

        WifiP2pConfig config = b.build();

        return connectTest(config);
    }

    private String getListenerError(ListenerTest listener) {
        StringBuilder sb = new StringBuilder();
        sb.append(mContext.getText(R.string.p2p_receive_invalid_response_error));
        sb.append(listener.getReason());
        return sb.toString();
    }

    @Override
    public String getTestName() {
        if (mGroupOperatingBand > 0) {
            String bandName = "";
            switch (mGroupOperatingBand) {
                case WifiP2pConfig.GROUP_OWNER_BAND_2GHZ:
                    bandName = "2.4G";
                    break;
                case WifiP2pConfig.GROUP_OWNER_BAND_5GHZ:
                    bandName = "5G";
                    break;
            }
            return "Join p2p group test (" + bandName + " config)";
        } else if (mGroupOperatingFrequency > 0) {
            String freqName = mGroupOperatingFrequency + " MHz";
            return "Join p2p group test (" + freqName + " config)";
        } else {
            return "Join p2p group test (config)";
        }
    }
}
