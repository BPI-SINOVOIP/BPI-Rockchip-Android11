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

import android.telephony.ims.feature.CapabilityChangeRequest;
import android.telephony.ims.feature.MmTelFeature;
import android.telephony.ims.stub.ImsRegistrationImplBase;
import android.util.Log;

import java.util.List;

public class TestMmTelFeature extends MmTelFeature {

    private final TestImsService.RemovedListener mRemovedListener;
    private final TestImsService.ReadyListener mReadyListener;
    private final TestImsService.CapabilitiesSetListener mCapSetListener;

    private static final String TAG = "CtsTestImsService";

    private MmTelCapabilities mCapabilities =
            new MmTelCapabilities(MmTelCapabilities.CAPABILITY_TYPE_SMS);
    private TestImsSmsImpl mSmsImpl;

    TestMmTelFeature(TestImsService.ReadyListener readyListener,
            TestImsService.RemovedListener removedListener,
            TestImsService.CapabilitiesSetListener setListener) {
        mReadyListener = readyListener;
        mRemovedListener = removedListener;
        mCapSetListener = setListener;
        mSmsImpl = new TestImsSmsImpl();
        // Must set the state to READY in the constructor - onFeatureReady depends on the state
        // being ready.
        setFeatureState(STATE_READY);
    }

    public TestImsSmsImpl getSmsImplementation() {
        return mSmsImpl;
    }

    @Override
    public boolean queryCapabilityConfiguration(int capability, int radioTech) {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "queryCapabilityConfiguration called with capability: " + capability);
        }
        return mCapabilities.isCapable(capability);
    }

    @Override
    public void changeEnabledCapabilities(CapabilityChangeRequest request,
            CapabilityCallbackProxy c) {
        List<CapabilityChangeRequest.CapabilityPair> pairs = request.getCapabilitiesToEnable();
        for (CapabilityChangeRequest.CapabilityPair pair : pairs) {
            if (pair.getRadioTech() == ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
                mCapabilities.addCapabilities(pair.getCapability());
            }
        }
        pairs = request.getCapabilitiesToDisable();
        for (CapabilityChangeRequest.CapabilityPair pair : pairs) {
            if (pair.getRadioTech() == ImsRegistrationImplBase.REGISTRATION_TECH_LTE) {
                mCapabilities.removeCapabilities(pair.getCapability());
            }
        }
        mCapSetListener.onSet();
    }

    @Override
    public void onFeatureReady() {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "TestMmTelFeature.onFeatureReady called");
        }
        mReadyListener.onReady();
    }

    @Override
    public void onFeatureRemoved() {
        if (ImsUtils.VDBG) {
            Log.d(TAG, "TestMmTelFeature.onFeatureRemoved called");
        }
        mRemovedListener.onRemoved();
    }

    public void setCapabilities(MmTelCapabilities capabilities) {
        mCapabilities = capabilities;
    }

    public MmTelCapabilities getCapabilities() {
        return mCapabilities;
    }
}
