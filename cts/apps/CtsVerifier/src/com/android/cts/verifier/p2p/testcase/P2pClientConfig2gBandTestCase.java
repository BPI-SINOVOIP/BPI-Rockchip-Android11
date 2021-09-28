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

package com.android.cts.verifier.p2p.testcase;

import android.content.Context;
import android.net.wifi.p2p.WifiP2pConfig;

import com.android.cts.verifier.R;

/**
 * Test case to join a p2p group with 2G band config.
 */
public class P2pClientConfig2gBandTestCase extends P2pClientConfigTestCase {

    public P2pClientConfig2gBandTestCase(Context context) {
        super(context, WifiP2pConfig.GROUP_OWNER_BAND_2GHZ);
    }
}
