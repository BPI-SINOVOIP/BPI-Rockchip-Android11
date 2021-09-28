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

package com.android.bluetooth.avrcpcontroller;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import com.android.bluetooth.BluetoothObexTransport;

import java.io.IOException;
import java.lang.ref.WeakReference;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;
import javax.obex.ResponseCodes;

/**
 * A client to a remote device's BIP Image Pull Server, as defined by a PSM passed in at
 * construction time.
 *
 * Once the client connection is established you can use this client to get image properties and
 * download images. The connection to the server is held open to service multiple requests.
 *
 * Client is good for one connection lifecycle. Please call shutdown() to clean up safely. Once a
 * disconnection has occurred, please create a new client.
 */
public class AvrcpBipClient {
    private static final String TAG = "AvrcpBipClient";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    // AVRCP Controller BIP Image Initiator/Cover Art UUID - AVRCP 1.6 Section 5.14.2.1
    private static final byte[] BLUETOOTH_UUID_AVRCP_COVER_ART = new byte[] {
        (byte) 0x71,
        (byte) 0x63,
        (byte) 0xDD,
        (byte) 0x54,
        (byte) 0x4A,
        (byte) 0x7E,
        (byte) 0x11,
        (byte) 0xE2,
        (byte) 0xB4,
        (byte) 0x7C,
        (byte) 0x00,
        (byte) 0x50,
        (byte) 0xC2,
        (byte) 0x49,
        (byte) 0x00,
        (byte) 0x48
    };

    private static final int CONNECT = 0;
    private static final int DISCONNECT = 1;
    private static final int REQUEST = 2;
    private static final int REFRESH_OBEX_SESSION = 3;

    private final Handler mHandler;
    private final HandlerThread mThread;

    private final BluetoothDevice mDevice;
    private final int mPsm;
    private int mState = BluetoothProfile.STATE_DISCONNECTED;

    private BluetoothSocket mSocket;
    private BluetoothObexTransport mTransport;
    private ClientSession mSession;

    private final Callback mCallback;

    /**
     * Callback object used to be notified of when a request has been completed.
     */
    interface Callback {

        /**
         * Notify of a connection state change in the client
         *
         * @param oldState The old state of the client
         * @param newState The new state of the client
         */
        void onConnectionStateChanged(int oldState, int newState);

        /**
         * Notify of a get image properties completing
         *
         * @param status A status code to indicate a success or error
         * @param properties The BipImageProperties object returned if successful, null otherwise
         */
        void onGetImagePropertiesComplete(int status, String imageHandle,
                BipImageProperties properties);

        /**
         * Notify of a get image operation completing
         *
         * @param status A status code of the request. success or error
         * @param image The BipImage object returned if successful, null otherwise
         */
        void onGetImageComplete(int status, String imageHandle, BipImage image);
    }

    /**
     * Creates a BIP image pull client and connects to a remote device's BIP image push server.
     */
    public AvrcpBipClient(BluetoothDevice remoteDevice, int psm, Callback callback) {
        if (remoteDevice == null) {
            throw new NullPointerException("Remote device is null");
        }
        if (callback == null) {
            throw new NullPointerException("Callback is null");
        }

        mDevice = remoteDevice;
        mPsm = psm;
        mCallback = callback;

        mThread = new HandlerThread("AvrcpBipClient");
        mThread.start();

        Looper looper = mThread.getLooper();

        mHandler = new AvrcpBipClientHandler(looper, this);
        mHandler.obtainMessage(CONNECT).sendToTarget();
    }

    /**
     * Refreshes this client's OBEX session
     */
    public void refreshSession() {
        debug("Refresh client session");
        if (!isConnected()) {
            error("Tried to do a reconnect operation on a client that is not connected");
            return;
        }
        try {
            mHandler.obtainMessage(REFRESH_OBEX_SESSION).sendToTarget();
        } catch (IllegalStateException e) {
            // Means we haven't been started or we're already stopped. Doing this makes this call
            // always safe no matter the state.
            return;
        }
    }

    /**
     * Safely disconnects the client from the server
     */
    public void shutdown() {
        debug("Shutdown client");
        try {
            mHandler.obtainMessage(DISCONNECT).sendToTarget();
        } catch (IllegalStateException e) {
            // Means we haven't been started or we're already stopped. Doing this makes this call
            // always safe no matter the state.
            return;
        }
        mThread.quitSafely();
    }

    /**
     * Determines if this client is connected to the server
     *
     * @return True if connected, False otherwise
     */
    public synchronized int getState() {
        return mState;
    }

    /**
     * Determines if this client is connected to the server
     *
     * @return True if connected, False otherwise
     */
    public boolean isConnected() {
        return getState() == BluetoothProfile.STATE_CONNECTED;
    }

    /**
     * Return the L2CAP PSM used to connect to the server.
     *
     * @return The L2CAP PSM
     */
    public int getL2capPsm() {
        return mPsm;
    }

    /**
     * Retrieve the image properties associated with the given imageHandle
     */
    public boolean getImageProperties(String imageHandle) {
        RequestGetImageProperties request =  new RequestGetImageProperties(imageHandle);
        boolean status = mHandler.sendMessage(mHandler.obtainMessage(REQUEST, request));
        if (!status) {
            error("Adding messages failed, connection state: " + isConnected());
            return false;
        }
        return true;
    }

    /**
     * Download the image object associated with the given imageHandle
     */
    public boolean getImage(String imageHandle, BipImageDescriptor descriptor) {
        RequestGetImage request =  new RequestGetImage(imageHandle, descriptor);
        boolean status = mHandler.sendMessage(mHandler.obtainMessage(REQUEST, request));
        if (!status) {
            error("Adding messages failed, connection state: " + isConnected());
            return false;
        }
        return true;
    }

    /**
     * Update our client's connection state and notify of the new status
     */
    private void setConnectionState(int state) {
        int oldState = -1;
        synchronized (this) {
            oldState = mState;
            mState = state;
        }
        if (oldState != state)  {
            mCallback.onConnectionStateChanged(oldState, mState);
        }
    }

    /**
     * Connects to the remote device's BIP Image Pull server
     */
    private synchronized void connect() {
        debug("Connect using psm: " + mPsm);
        if (isConnected()) {
            warn("Already connected");
            return;
        }

        try {
            setConnectionState(BluetoothProfile.STATE_CONNECTING);

            mSocket = mDevice.createL2capSocket(mPsm);
            mSocket.connect();

            mTransport = new BluetoothObexTransport(mSocket);
            mSession = new ClientSession(mTransport);

            HeaderSet headerSet = new HeaderSet();
            headerSet.setHeader(HeaderSet.TARGET, BLUETOOTH_UUID_AVRCP_COVER_ART);

            headerSet = mSession.connect(headerSet);
            int responseCode = headerSet.getResponseCode();
            if (responseCode == ResponseCodes.OBEX_HTTP_OK) {
                setConnectionState(BluetoothProfile.STATE_CONNECTED);
                debug("Connection established");
            } else {
                error("Error connecting, code: " + responseCode);
                disconnect();
            }
        } catch (IOException e) {
            error("Exception while connecting to AVRCP BIP server", e);
            disconnect();
        }
    }

    /**
     * Disconnect and reconnect the OBEX session.
     */
    private synchronized void refreshObexSession() {
        if (mSession == null) return;

        try {
            setConnectionState(BluetoothProfile.STATE_DISCONNECTING);
            mSession.disconnect(null);
            debug("Disconnected from OBEX session");
        } catch (IOException e) {
            error("Exception while disconnecting from AVRCP BIP server", e);
            disconnect();
            return;
        }

        try {
            setConnectionState(BluetoothProfile.STATE_CONNECTING);

            HeaderSet headerSet = new HeaderSet();
            headerSet.setHeader(HeaderSet.TARGET, BLUETOOTH_UUID_AVRCP_COVER_ART);

            headerSet = mSession.connect(headerSet);
            int responseCode = headerSet.getResponseCode();
            if (responseCode == ResponseCodes.OBEX_HTTP_OK) {
                setConnectionState(BluetoothProfile.STATE_CONNECTED);
                debug("Reconnection established");
            } else {
                error("Error reconnecting, code: " + responseCode);
                disconnect();
            }
        } catch (IOException e) {
            error("Exception while reconnecting to AVRCP BIP server", e);
            disconnect();
        }
    }

    /**
     * Permanently disconnects this client from the remote device's BIP server and notifies of the
     * new connection status.
     *
     */
    private synchronized void disconnect() {
        if (mSession != null) {
            setConnectionState(BluetoothProfile.STATE_DISCONNECTING);

            try {
                mSession.disconnect(null);
                debug("Disconnected from OBEX session");
            } catch (IOException e) {
                error("Exception while disconnecting from AVRCP BIP server: " + e.toString());
            }

            try {
                mSession.close();
                mTransport.close();
                mSocket.close();
                debug("Closed underlying session, transport and socket");
            } catch (IOException e) {
                error("Exception while closing AVRCP BIP session: ", e);
            }

            mSession = null;
            mTransport = null;
            mSocket = null;
        }
        setConnectionState(BluetoothProfile.STATE_DISCONNECTED);
    }

    private void executeRequest(BipRequest request) {
        if (!isConnected()) {
            error("Cannot execute request " + request.toString()
                    + ", we're not connected");
            notifyCaller(request);
            return;
        }

        try {
            request.execute(mSession);
            notifyCaller(request);
            debug("Completed request - " + request.toString());
        } catch (IOException e) {
            error("Request failed: " + request.toString());
            notifyCaller(request);
            disconnect();
        }
    }

    private void notifyCaller(BipRequest request) {
        int type = request.getType();
        int responseCode = request.getResponseCode();
        String imageHandle = null;

        debug("Notifying caller of request complete - " + request.toString());
        switch (type) {
            case BipRequest.TYPE_GET_IMAGE_PROPERTIES:
                imageHandle = ((RequestGetImageProperties) request).getImageHandle();
                BipImageProperties properties =
                        ((RequestGetImageProperties) request).getImageProperties();
                mCallback.onGetImagePropertiesComplete(responseCode, imageHandle, properties);
                break;
            case BipRequest.TYPE_GET_IMAGE:
                imageHandle = ((RequestGetImage) request).getImageHandle();
                BipImage image = ((RequestGetImage) request).getImage();
                mCallback.onGetImageComplete(responseCode, imageHandle, image);
                break;
        }
    }

    /**
     * Handles this AVRCP BIP Image Pull Client's requests
     */
    private static class AvrcpBipClientHandler extends Handler {
        WeakReference<AvrcpBipClient> mInst;

        AvrcpBipClientHandler(Looper looper, AvrcpBipClient inst) {
            super(looper);
            mInst = new WeakReference<>(inst);
        }

        @Override
        public void handleMessage(Message msg) {
            AvrcpBipClient inst = mInst.get();
            switch (msg.what) {
                case CONNECT:
                    if (!inst.isConnected()) {
                        inst.connect();
                    }
                    break;

                case DISCONNECT:
                    if (inst.isConnected()) {
                        inst.disconnect();
                    }
                    break;

                case REFRESH_OBEX_SESSION:
                    if (inst.isConnected()) {
                        inst.refreshObexSession();
                    }
                    break;

                case REQUEST:
                    if (inst.isConnected()) {
                        inst.executeRequest((BipRequest) msg.obj);
                    }
                    break;
            }
        }
    }

    private String getStateName() {
        int state = getState();
        switch (state) {
            case BluetoothProfile.STATE_DISCONNECTED:
                return "Disconnected";
            case BluetoothProfile.STATE_CONNECTING:
                return "Connecting";
            case BluetoothProfile.STATE_CONNECTED:
                return "Connected";
            case BluetoothProfile.STATE_DISCONNECTING:
                return "Disconnecting";
        }
        return "Unknown";
    }

    @Override
    public String toString() {
        return "<AvrcpBipClient" + " device=" + mDevice.getAddress() + " psm=" + mPsm
                + " state=" + getStateName() + ">";
    }

    /**
     * Print to debug if debug is enabled for this class
     */
    private void debug(String msg) {
        if (DBG) {
            Log.d(TAG, "[" + mDevice.getAddress() + "] " + msg);
        }
    }

    /**
     * Print to warn
     */
    private void warn(String msg) {
        Log.w(TAG, "[" + mDevice.getAddress() + "] " + msg);
    }

    /**
     * Print to error
     */
    private void error(String msg) {
        Log.e(TAG, "[" + mDevice.getAddress() + "] " + msg);
    }

    private void error(String msg, Throwable e) {
        Log.e(TAG, "[" + mDevice.getAddress() + "] " + msg, e);
    }
}
