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

package android.telephony.ims.cts;

import android.telephony.ims.feature.RcsFeature;
import android.util.Log;

public class TestRcsFeature extends RcsFeature {
    private static final String TAG = "CtsTestImsService";

    private final TestImsService.ReadyListener mReadyListener;
    private final TestImsService.RemovedListener mRemovedListener;
    private final TestImsService.CapabilitiesSetListener mCapSetListener;

    TestRcsFeature(TestImsService.ReadyListener readyListener,
            TestImsService.RemovedListener listener,
            TestImsService.CapabilitiesSetListener setListener) {
        mReadyListener = readyListener;
        mRemovedListener = listener;
        mCapSetListener = setListener;

        setFeatureState(STATE_READY);
    }

    @Override
    public void onFeatureReady() {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "TestRcsFeature.onFeatureReady called");
        }
        mReadyListener.onReady();
    }

    @Override
    public void onFeatureRemoved() {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "TestRcsFeature.onFeatureRemoved called");
        }
        mRemovedListener.onRemoved();
    }
}
