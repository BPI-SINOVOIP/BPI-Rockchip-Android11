/*
 * Copyright (C) 2016 Google Inc.
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

package android.location.cts.gnss;

import android.location.GnssStatus;
import android.location.cts.common.TestMeasurementUtil;
import android.location.cts.common.TestUtils;
import android.util.Log;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CountDownLatch;

/**
 * Used for receiving notifications when GNSS status has changed.
 */
public class TestGnssStatusCallback extends GnssStatus.Callback {

    private final String mTag;
    private GnssStatus mGnssStatus = null;
    // Timeout in sec for count down latch wait
    private static final int TIMEOUT_IN_SEC = 90;
    private final CountDownLatch mLatchStart;
    private final CountDownLatch mLatchStatus;
    private final CountDownLatch mLatchTtff;
    private final CountDownLatch mLatchStop;

    // Store list of Satellites including Gnss Band, constellation & SvId
    private Set<String> mGnssUsedSvStringIds;

    public TestGnssStatusCallback(String tag, int gpsStatusCountToCollect) {
        this.mTag = tag;
        mLatchStart = new CountDownLatch(1);
        mLatchStatus = new CountDownLatch(gpsStatusCountToCollect);
        mLatchTtff = new CountDownLatch(1);
        mLatchStop = new CountDownLatch(1);
        mGnssUsedSvStringIds = new HashSet<>();
    }

    @Override
    public void onStarted() {
        Log.i(mTag, "Gnss Status Listener Started");
        mLatchStart.countDown();
    }

    @Override
    public void onStopped() {
        Log.i(mTag, "Gnss Status Listener Stopped");
        mLatchStop.countDown();
    }

    @Override
    public void onFirstFix(int ttffMillis) {
        Log.i(mTag, "Gnss Status Listener Received TTFF");
        mLatchTtff.countDown();
    }

    @Override
    public void onSatelliteStatusChanged(GnssStatus status) {
        Log.i(mTag, "Gnss Status Listener Received Status Update");
        mGnssStatus = status;
        for (int i = 0; i < status.getSatelliteCount(); i++) {
            if (!status.usedInFix(i)) {
                continue;
            }
            if (status.hasCarrierFrequencyHz(i)) {
                mGnssUsedSvStringIds.add(
                    TestMeasurementUtil.getUniqueSvStringId(status.getConstellationType(i),
                        status.getSvid(i), status.getCarrierFrequencyHz(i)));
            } else {
                mGnssUsedSvStringIds.add(
                    TestMeasurementUtil.getUniqueSvStringId(status.getConstellationType(i),
                        status.getSvid(i)));
            }
        }
        mLatchStatus.countDown();
    }

    /**
     * Returns the list of SV String Ids which were used in fix during the collect
     *
     * @return mGnssUsedSvStringIds - Set of SV string Ids
     */
    public Set<String> getGnssUsedSvStringIds() {
        return mGnssUsedSvStringIds;
    }

    /**
     * Get GNSS Status.
     *
     * @return mGnssStatus GNSS Status
     */
    public GnssStatus getGnssStatus() {
        return mGnssStatus;
    }

    public boolean awaitStart() throws InterruptedException {
        return TestUtils.waitFor(mLatchStart, TIMEOUT_IN_SEC);
    }

    public boolean awaitStatus() throws InterruptedException {
        return TestUtils.waitFor(mLatchStatus, TIMEOUT_IN_SEC);
    }

    public boolean awaitTtff() throws InterruptedException {
        return TestUtils.waitFor(mLatchTtff, TIMEOUT_IN_SEC);
    }

    public boolean awaitStop() throws InterruptedException {
        return TestUtils.waitFor(mLatchStop, TIMEOUT_IN_SEC);
    }
}
