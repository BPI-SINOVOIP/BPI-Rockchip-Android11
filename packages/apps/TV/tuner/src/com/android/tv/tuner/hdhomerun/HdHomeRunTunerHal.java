/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.tuner.hdhomerun;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import com.android.tv.common.SoftPreconditions;
import com.android.tv.common.compat.TvInputConstantCompat;
import com.android.tv.tuner.api.Tuner;
import com.android.tv.tuner.data.TunerChannel;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;

/** Tuner implementation for HdHomeRun */
public class HdHomeRunTunerHal implements Tuner {
    private static final String TAG = "HdHomeRunTunerHal";
    private static final boolean DEBUG = false;

    private static final String CABLECARD_MODEL = "cablecard";
    private static final String ATSC_MODEL = "atsc";
    private static final String DVBC_MODEL = "dvbc";
    private static final String DVBT_MODEL = "dvbt";

    private final HdHomeRunTunerManager mTunerManager;
    private HdHomeRunDevice mDevice;
    private BufferedInputStream mInputStream;
    private HttpURLConnection mConnection;
    private String mHttpConnectionAddress;
    private final Context mContext;

    @DeliverySystemType private int mDeliverySystemType = DELIVERY_SYSTEM_UNDEFINED;

    public static final char VCHANNEL_SEPARATOR = '.';
    public static final int CONNECTION_TIMEOUT_MS_FOR_URLCONNECTION = 3000; // 3 sec
    public static final int READ_TIMEOUT_MS_FOR_URLCONNECTION = 10000; // 10 sec

    public HdHomeRunTunerHal(Context context) {
        mTunerManager = HdHomeRunTunerManager.getInstance();
        mContext = context;
    }

    @Override
    public boolean openFirstAvailable() {
        SoftPreconditions.checkState(mDevice == null);
        try {
            mDevice = mTunerManager.acquireDevice(mContext);
            if (mDevice != null) {
                if (mDeliverySystemType == DELIVERY_SYSTEM_UNDEFINED) {
                    mDeliverySystemType = nativeGetDeliverySystemType(getDeviceId());
                }
            }
            return mDevice != null;
        } catch (Exception e) {
            Log.w(TAG, "Failed to open first available device", e);
            return false;
        }
    }

    @Override
    public boolean isDeviceOpen() {
        return mDevice != null;
    }

    @Override
    public boolean isReusable() {
        return false;
    }

    @Override
    public long getDeviceId() {
        return mDevice == null ? 0 : mDevice.getDeviceId();
    }

    @Override
    public void close() throws Exception {
        closeInputStreamAndDisconnect();
        if (mDevice != null) {
            mTunerManager.releaseDevice(mDevice);
            mDevice = null;
        }
    }

    @Override
    public synchronized boolean tune(
            int frequency, @ModulationType String modulation, String channelNumber) {
        if (DEBUG) {
            Log.d(
                    TAG,
                    "tune(frequency="
                            + frequency
                            + ", modulation="
                            + modulation
                            + ", channelNumber="
                            + channelNumber
                            + ")");
        }
        closeInputStreamAndDisconnect();
        if (TextUtils.isEmpty(channelNumber)) {
            return false;
        }
        channelNumber =
                channelNumber.replace(TunerChannel.CHANNEL_NUMBER_SEPARATOR, VCHANNEL_SEPARATOR);
        mHttpConnectionAddress = "http://" + getIpAddress() + ":5004/auto/v" + channelNumber;
        return connectAndOpenInputStream();
    }

    private boolean connectAndOpenInputStream() {
        URL url;
        try {
            url = new URL(mHttpConnectionAddress);
        } catch (MalformedURLException e) {
            Log.e(TAG, "Invalid address: " + mHttpConnectionAddress, e);
            return false;
        }
        URLConnection connection;
        try {
            connection = url.openConnection();
            connection.setConnectTimeout(CONNECTION_TIMEOUT_MS_FOR_URLCONNECTION);
            connection.setReadTimeout(READ_TIMEOUT_MS_FOR_URLCONNECTION);
            if (connection instanceof HttpURLConnection) {
                mConnection = (HttpURLConnection) connection;
            }
        } catch (IOException e) {
            Log.e(TAG, "Connection failed: " + mHttpConnectionAddress, e);
            return false;
        }
        try {
            mInputStream = new BufferedInputStream(connection.getInputStream());
        } catch (IOException e) {
            closeInputStreamAndDisconnect();
            Log.e(TAG, "Failed to get input stream from " + mHttpConnectionAddress, e);
            return false;
        }
        if (DEBUG) Log.d(TAG, "tuning to " + mHttpConnectionAddress);
        return true;
    }

    @Override
    public synchronized boolean addPidFilter(int pid, @FilterType int filterType) {
        // no-op
        return true;
    }

    @Override
    public synchronized void stopTune() {
        closeInputStreamAndDisconnect();
    }

    @Override
    public synchronized int readTsStream(byte[] javaBuffer, int javaBufferSize) {
        if (mInputStream != null) {
            try {
                // Note: this call sometimes take more than 500ms, because the data is
                // streamed through network unlike connected tuner devices.
                return mInputStream.read(javaBuffer, 0, javaBufferSize);
            } catch (IOException e) {
                Log.e(TAG, "Failed to read stream", e);
                closeInputStreamAndDisconnect();
            }
        }
        if (connectAndOpenInputStream()) {
            Log.w(TAG, "Tuned by http connection again");
        } else {
            Log.e(TAG, "Tuned by http connection again failed");
        }
        return 0;
    }

    @Override
    public void setHasPendingTune(boolean hasPendingTune) {
        // no-op
    }

    protected int nativeGetDeliverySystemType(long deviceId) {
        String deviceModel = mDevice.getDeviceModel();
        if (SoftPreconditions.checkState(!TextUtils.isEmpty(deviceModel))) {
            if (deviceModel.contains(CABLECARD_MODEL) || deviceModel.contains(ATSC_MODEL)) {
                return DELIVERY_SYSTEM_ATSC;
            } else if (deviceModel.contains(DVBC_MODEL)) {
                return DELIVERY_SYSTEM_DVBC;
            } else if (deviceModel.contains(DVBT_MODEL)) {
                return DELIVERY_SYSTEM_DVBT;
            }
        }
        return DELIVERY_SYSTEM_UNDEFINED;
    }

    private void closeInputStreamAndDisconnect() {
        if (mInputStream != null) {
            try {
                mInputStream.close();
            } catch (IOException e) {
                Log.e(TAG, "Failed to close input stream", e);
            }
            mInputStream = null;
        }
        if (mConnection != null) {
            mConnection.disconnect();
            mConnection = null;
        }
    }

    /** Gets the number of tuners in a given HDHomeRun devices. */
    public static int getNumberOfDevices() {
        return HdHomeRunTunerManager.getInstance().getTunerCount();
    }

    /** Returns the IP address. */
    public String getIpAddress() {
        return HdHomeRunUtils.getIpString(mDevice.getIpAddress());
    }

    /**
     * Marks the device associated to this instance as a scanned device. Scanned device has higher
     * priority among multiple HDHomeRun devices.
     */
    public void markAsScannedDevice(Context context) {
        HdHomeRunTunerManager.markAsScannedDevice(context, mDevice);
    }

    @Override
    @DeliverySystemType
    public int getDeliverySystemType() {
        return Tuner.DELIVERY_SYSTEM_UNDEFINED;
    }

    @Override
    public int getSignalStrength() {
        return TvInputConstantCompat.SIGNAL_STRENGTH_NOT_USED;
    }
}
