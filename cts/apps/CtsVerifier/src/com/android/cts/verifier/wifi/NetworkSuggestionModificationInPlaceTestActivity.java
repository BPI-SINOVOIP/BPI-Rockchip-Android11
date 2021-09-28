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

package com.android.cts.verifier.wifi;

import android.content.Context;
import android.os.Bundle;

import com.android.cts.verifier.R;
import com.android.cts.verifier.wifi.testcase.NetworkSuggestionTestCase;

/**
 * Test activity for suggestion which is modified while connected to it.
 */
public class NetworkSuggestionModificationInPlaceTestActivity extends BaseTestActivity {
    @Override
    protected BaseTestCase getTestCase(Context context) {
        return new NetworkSuggestionTestCase(
                context,
                false, /* setBssid */
                false, /* setRequiresAppInteraction */
                false, /* simulateConnectionFailure */
                true /* setMeteredPostConnection */);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setInfoResources(R.string.wifi_test_network_suggestion_modification_in_place,
                R.string.wifi_test_network_suggestion_modification_in_place_info, 0);
    }
}
