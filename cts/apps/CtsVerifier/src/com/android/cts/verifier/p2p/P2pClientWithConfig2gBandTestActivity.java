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
package com.android.cts.verifier.p2p;

import android.content.Context;

import com.android.cts.verifier.p2p.testcase.P2pClientWithConfig2gBandTestSuite;
import com.android.cts.verifier.p2p.testcase.ReqTestCase;

/**
 * Test activity that tries to join an existing p2p group with config.
 * This activity is invoked from JoinTestListActivity.
 */
public class P2pClientWithConfig2gBandTestActivity extends RequesterTestActivity {

    /**
     * Do not need peer discovery first.
     * For joining a group with config, this device finds the peer by
     * Network Name, says SSID in specified band or frequency.
     */
    @Override
    protected boolean isNoPeerDiscoveryOnResume() {
        return true;
    }

    @Override
    protected ReqTestCase getTestCase(Context context, String testId) {
        return P2pClientWithConfig2gBandTestSuite.getTestCase(context, testId);
    }
}
