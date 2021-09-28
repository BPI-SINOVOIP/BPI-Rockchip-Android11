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
import android.net.wifi.p2p.WifiP2pInfo;

import com.android.cts.verifier.R;

/**
 * A test case which accepts a connection from p2p client.
 *
 * The requester device tries to join this device.
 */
public class GoWithConfigTestCase extends TestCase {

    protected P2pBroadcastReceiverTest mReceiverTest;

    private int mGroupOperatingBand = WifiP2pConfig.GROUP_OWNER_BAND_AUTO;
    private int mGroupOperatingFrequency = 0;
    private String mNetworkName = "DIRECT-XY-HELLO";

    public GoWithConfigTestCase(Context context) {
        super(context);
    }

    public GoWithConfigTestCase(Context context, int bandOrFrequency) {
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
    protected void setUp() {
        super.setUp();
        mReceiverTest = new P2pBroadcastReceiverTest(mContext);
        mReceiverTest.init(mChannel);
    }

    @Override
    protected boolean executeTest() throws InterruptedException {

        ActionListenerTest listener = new ActionListenerTest();

        /*
         * Add renderer service
         */
        mP2pMgr.addLocalService(mChannel, LocalServices.createRendererService(),
                listener);
        if (!listener.check(ActionListenerTest.SUCCESS, TIMEOUT)) {
            mReason = mContext.getString(R.string.p2p_add_local_service_error);
            return false;
        }

        /*
         * Add IPP service
         */
        mP2pMgr.addLocalService(mChannel, LocalServices.createIppService(),
                listener);
        if (!listener.check(ActionListenerTest.SUCCESS, TIMEOUT)) {
            mReason = mContext.getString(R.string.p2p_add_local_service_error);
            return false;
        }

        /*
         * Add AFP service
         */
        mP2pMgr.addLocalService(mChannel, LocalServices.createAfpService(),
                listener);
        if (!listener.check(ActionListenerTest.SUCCESS, TIMEOUT)) {
            mReason = mContext.getString(R.string.p2p_add_local_service_error);
            return false;
        }

        /*
         * Start up an autonomous group owner.
         */
        WifiP2pConfig.Builder b = new WifiP2pConfig.Builder()
                .setNetworkName(mNetworkName)
                .setPassphrase("DEADBEEF");
        if (mGroupOperatingBand > 0) {
            b.setGroupOperatingBand(mGroupOperatingBand);
        } else if (mGroupOperatingFrequency > 0) {
            b.setGroupOperatingFrequency(mGroupOperatingFrequency);
        }
        WifiP2pConfig config = b.build();
        mP2pMgr.createGroup(mChannel, config, listener);
        if (!listener.check(ActionListenerTest.SUCCESS, TIMEOUT)) {
            mReason = mContext.getString(R.string.p2p_ceate_group_error);
            return false;
        }

        /*
         * Check whether createGroup() is succeeded.
         */
        WifiP2pInfo info = mReceiverTest.waitConnectionNotice(TIMEOUT);
        if (info == null || !info.isGroupOwner) {
            mReason = mContext.getString(R.string.p2p_ceate_group_error);
            return false;
        }

        // wait until p2p client is joining.
        return true;
    }

    @Override
    protected void tearDown() {

        // wait until p2p client is joining.
        synchronized (this) {
            try {
                wait();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        if (mP2pMgr != null) {
            mP2pMgr.cancelConnect(mChannel, null);
            mP2pMgr.removeGroup(mChannel, null);
        }
        if (mReceiverTest != null) {
            mReceiverTest.close();
        }
        super.tearDown();
    }

    @Override
    public String getTestName() {
        String testName = "Accept client connection test";
        if (mGroupOperatingBand > 0) {
            testName += " with " + mGroupOperatingBand + "G band";
        } else if (mGroupOperatingFrequency > 0) {
            testName += " with " + mGroupOperatingFrequency + " MHz";
        }
        return testName;
    }
}
