/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.cts.verifier.wifiaware.testcase;

import static com.android.cts.verifier.wifiaware.CallbackUtils.CALLBACK_TIMEOUT_SEC;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.aware.WifiAwareNetworkInfo;
import android.net.wifi.aware.WifiAwareNetworkSpecifier;
import android.util.Log;
import android.util.Pair;

import com.android.cts.verifier.R;
import com.android.cts.verifier.wifiaware.CallbackUtils;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Inet6Address;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.Arrays;

/**
 * Test case for data-path, in-band test cases:
 * open/passphrase * solicited/unsolicited * publish/subscribe.
 *
 * Subscribe test sequence:
 * 1. Attach
 *    wait for results (session)
 * 2. Subscribe
 *    wait for results (subscribe session)
 * 3. Wait for discovery (possibly with ranging)
 * 4. Send message
 *    Wait for success
 * 5. Wait for rx message
 * 6. Request network
 *    Wait for network
 * 7. Create socket and bind to server
 * 8. Send/receive data to validate connection
 * 9. Destroy session
 *
 * Publish test sequence:
 * 1. Attach
 *    wait for results (session)
 * 2. Publish
 *    wait for results (publish session)
 * 3. Wait for rx message
 * 4. Start a ServerSocket
 * 5. Request network
 * 6. Send message
 *    Wait for success
 * 7. Wait for network
 * 8. Receive/Send data to validate connection
 * 9. Destroy session
 */
public class DataPathInBandTestCase extends DiscoveryBaseTestCase {
    private static final String TAG = "DataPathInBandTestCase";
    private static final boolean DBG = true;

    private static final byte[] MSG_PUB_TO_SUB = "Ready".getBytes();
    private static final String PASSPHRASE = "Some super secret password";
    private static final byte[] PMK = "01234567890123456789012345678901".getBytes();

    private static final byte[] MSG_CLIENT_TO_SERVER = "GET SOME BYTES".getBytes();
    private static final byte[] MSG_SERVER_TO_CLIENT = "PUT SOME OTHER BYTES".getBytes();

    private boolean mIsSecurityOpen;
    private boolean mUsePmk;
    private boolean mIsPublish;
    private Thread mClientServerThread;
    private ConnectivityManager mCm;
    private CallbackUtils.NetworkCb mNetworkCb;

    private static int sSDKLevel = android.os.Build.VERSION.SDK_INT;

    public DataPathInBandTestCase(Context context, boolean isSecurityOpen, boolean isPublish,
            boolean isUnsolicited, boolean usePmk) {
        super(context, isUnsolicited, false);

        mIsSecurityOpen = isSecurityOpen;
        mUsePmk = usePmk;
        mIsPublish = isPublish;
    }

    @Override
    protected boolean executeTest() throws InterruptedException {
        if (DBG) {
            Log.d(TAG,
                    "executeTest: mIsSecurityOpen=" + mIsSecurityOpen + ", mIsPublish=" + mIsPublish
                            + ", mIsUnsolicited=" + mIsUnsolicited);
        }

        mCm = (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        mClientServerThread = null;
        mNetworkCb = null;

        boolean success;
        if (mIsPublish) {
            success = executeTestPublisher();
        } else {
            success = executeTestSubscriber();
        }
        if (!success) {
            return false;
        }

        // destroy session
        mWifiAwareDiscoverySession.close();
        mWifiAwareDiscoverySession = null;

        mListener.onTestMsgReceived(mContext.getString(R.string.aware_status_lifecycle_ok));
        return true;
    }

    @Override
    protected void tearDown() {
        if (mClientServerThread != null) {
            mClientServerThread.interrupt();
        }
        if (mNetworkCb != null) {
            mCm.unregisterNetworkCallback(mNetworkCb);
        }
        super.tearDown();
    }


    private boolean executeTestSubscriber() throws InterruptedException {
        if (DBG) Log.d(TAG, "executeTestSubscriber");
        if (!executeSubscribe()) {
            return false;
        }

        // 5. wait to receive message
        CallbackUtils.DiscoveryCb.CallbackData callbackData = mDiscoveryCb.waitForCallbacks(
                CallbackUtils.DiscoveryCb.ON_MESSAGE_RECEIVED);
        switch (callbackData.callback) {
            case CallbackUtils.DiscoveryCb.TIMEOUT:
                setFailureReason(mContext.getString(R.string.aware_status_receive_timeout));
                Log.e(TAG, "executeTestSubscriber: receive message TIMEOUT");
                return false;
        }
        mListener.onTestMsgReceived(mContext.getString(R.string.aware_status_received));
        if (DBG) Log.d(TAG, "executeTestSubscriber: received message");

        //    validate that received the expected message
        if (!Arrays.equals(MSG_PUB_TO_SUB, callbackData.serviceSpecificInfo)) {
            setFailureReason(mContext.getString(R.string.aware_status_receive_failure));
            Log.e(TAG, "executeTestSubscriber: receive message message content mismatch: rx='"
                    + new String(callbackData.serviceSpecificInfo) + "'");
            return false;
        }

        // 6. request network
        WifiAwareNetworkSpecifier.Builder nsBuilder =
                new WifiAwareNetworkSpecifier.Builder(mWifiAwareDiscoverySession, mPeerHandle);
        if (!mIsSecurityOpen) {
            if (mUsePmk) {
                nsBuilder.setPmk(PMK);
            } else {
                nsBuilder.setPskPassphrase(PASSPHRASE);
            }
        }
        NetworkRequest nr = new NetworkRequest.Builder().addTransportType(
                NetworkCapabilities.TRANSPORT_WIFI_AWARE).setNetworkSpecifier(
                nsBuilder.build()).build();
        mNetworkCb = new CallbackUtils.NetworkCb();
        mCm.requestNetwork(nr, mNetworkCb, CALLBACK_TIMEOUT_SEC * 1000);
        mListener.onTestMsgReceived(
                mContext.getString(R.string.aware_status_network_requested));
        if (DBG) Log.d(TAG, "executeTestSubscriber: requested network");

        // 7. wait for network
        Pair<Network, NetworkCapabilities> info = mNetworkCb.waitForNetworkCapabilities();
        if (info == null) {
            setFailureReason(mContext.getString(R.string.aware_status_network_failed));
            Log.e(TAG, "executeTestSubscriber: network request rejected or timed-out");
            return false;
        }
        if (info.first == null || info.second == null) {
            setFailureReason(mContext.getString(R.string.aware_status_network_failed));
            Log.e(TAG, "executeTestSubscriber: received a null Network or NetworkCapabilities!?");
            return false;
        }
        if (sSDKLevel <= android.os.Build.VERSION_CODES.P) {
            if (info.second.getNetworkSpecifier() != null) {
                setFailureReason(mContext.getString(R.string.aware_status_network_failed_leak));
                Log.e(TAG, "executeTestSubscriber: network request accepted - but leaks NS!");
                return false;
            }
        }

        mListener.onTestMsgReceived(mContext.getString(R.string.aware_status_network_success));
        if (DBG) Log.d(TAG, "executeTestSubscriber: network request granted - AVAILABLE");

        if (!mIsSecurityOpen) {
            if (!(info.second.getTransportInfo() instanceof WifiAwareNetworkInfo)) {
                setFailureReason(mContext.getString(R.string.aware_status_network_failed));
                Log.e(TAG, "executeTestSubscriber: did not get WifiAwareNetworkInfo from peer!?");
                return false;
            }
            WifiAwareNetworkInfo peerAwareInfo =
                    (WifiAwareNetworkInfo) info.second.getTransportInfo();
            Inet6Address peerIpv6 = peerAwareInfo.getPeerIpv6Addr();
            int peerPort = peerAwareInfo.getPort();
            int peerTransportProtocol = peerAwareInfo.getTransportProtocol();
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.aware_status_socket_server_info_rx,
                            peerIpv6.toString(),
                            peerPort));
            if (DBG) {
                Log.d(TAG,
                        "executeTestPublisher: rx peer info IPv6=" + peerIpv6 + ", port=" + peerPort
                                + ", transportProtocol=" + peerTransportProtocol);
            }
            if (peerTransportProtocol != 6) { // 6 == TCP: hard coded at peer
                setFailureReason(mContext.getString(R.string.aware_status_network_failed));
                Log.e(TAG, "executeTestSubscriber: Got incorrect transport protocol from peer");
                return false;
            }
            if (peerPort <= 0) {
                setFailureReason(mContext.getString(R.string.aware_status_network_failed));
                Log.e(TAG, "executeTestSubscriber: Got invalid port from peer (<=0)");
                return false;
            }

            // 8. send/receive - can happen inline here - no need for another thread
            String currentMethod = "";
            try {
                currentMethod = "createSocket";
                Socket socket = info.first.getSocketFactory().createSocket(peerIpv6, peerPort);

                // simple interaction: write X bytes, read Y bytes
                currentMethod = "getOutputStream()";
                OutputStream os = socket.getOutputStream();
                currentMethod = "write()";
                os.write(MSG_CLIENT_TO_SERVER, 0, MSG_CLIENT_TO_SERVER.length);

                byte[] buffer = new byte[1024];
                currentMethod = "getInputStream()";
                InputStream is = socket.getInputStream();
                currentMethod = "read()";
                int numBytes = is.read(buffer, 0, MSG_SERVER_TO_CLIENT.length);

                mListener.onTestMsgReceived(
                        mContext.getString(R.string.aware_status_socket_server_message_from_peer,
                                new String(buffer, 0, numBytes)));

                if (numBytes != MSG_SERVER_TO_CLIENT.length) {
                    setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                    Log.e(TAG,
                            "executeTestSubscriber: didn't read expected number of bytes - only "
                                    + "got -- " + numBytes);
                    return false;
                }
                if (!Arrays.equals(MSG_SERVER_TO_CLIENT,
                        Arrays.copyOf(buffer, MSG_SERVER_TO_CLIENT.length))) {
                    setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                    Log.e(TAG, "executeTestSubscriber: did not get expected message from server.");
                    return false;
                }
                // Sleep 3 second for transmit and receive.
                Thread.sleep(3000);
                currentMethod = "close()";
                os.close();
            } catch (IOException e) {
                setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                Log.e(TAG, "executeTestSubscriber: failure while executing " + currentMethod);
                return false;
            }
        }

        return true;
    }

    private boolean executeTestPublisher() throws InterruptedException {
        if (DBG) Log.d(TAG, "executeTestPublisher");
        if (!executePublish()) {
            return false;
        }

        // 4. create a ServerSocket
        int port = 0;
        if (!mIsSecurityOpen) {
            ServerSocket server;
            try {
                server = new ServerSocket(0);
            } catch (IOException e) {
                setFailureReason(
                        mContext.getString(R.string.aware_status_socket_failure));
                Log.e(TAG, "executeTestPublisher: failure creating a ServerSocket -- " + e);
                return false;
            }
            port = server.getLocalPort();
            mListener.onTestMsgReceived(
                    mContext.getString(R.string.aware_status_socket_server_socket_started, port));
            if (DBG) Log.d(TAG, "executeTestPublisher: server socket started on port=" + port);

            // accept connections on the server socket - has to be done in a separate thread!
            mClientServerThread = new Thread(() -> {
                String currentMethod = "";

                try {
                    currentMethod = "accept()";
                    Socket socket = server.accept();
                    currentMethod = "getInputStream()";
                    InputStream is = socket.getInputStream();

                    // simple interaction: read X bytes, write Y bytes
                    byte[] buffer = new byte[1024];
                    currentMethod = "read()";
                    int numBytes = is.read(buffer, 0, MSG_CLIENT_TO_SERVER.length);

                    mListener.onTestMsgReceived(mContext.getString(
                            R.string.aware_status_socket_server_message_from_peer,
                            new String(buffer, 0, numBytes)));

                    if (numBytes != MSG_CLIENT_TO_SERVER.length) {
                        setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                        Log.e(TAG,
                                "executeTestPublisher: didn't read expected number of bytes - only "
                                        + "got -- " + numBytes);
                        return;
                    }
                    if (!Arrays.equals(MSG_CLIENT_TO_SERVER,
                            Arrays.copyOf(buffer, MSG_CLIENT_TO_SERVER.length))) {
                        setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                        Log.e(TAG,
                                "executeTestPublisher: did not get expected message from client.");
                        return;
                    }

                    currentMethod = "getOutputStream()";
                    OutputStream os = socket.getOutputStream();
                    currentMethod = "write()";
                    os.write(MSG_SERVER_TO_CLIENT, 0, MSG_SERVER_TO_CLIENT.length);
                    // Sleep 3 second for transmit and receive.
                    Thread.sleep(3000);
                    currentMethod = "close()";
                    os.close();
                } catch (IOException | InterruptedException e) {
                    setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                    Log.e(TAG, "executeTestPublisher: failure while executing " + currentMethod);
                    return;
                }
            });
            mClientServerThread.start();
        }

        // 5. Request network
        WifiAwareNetworkSpecifier.Builder nsBuilder =
                new WifiAwareNetworkSpecifier.Builder(mWifiAwareDiscoverySession, mPeerHandle);
        if (!mIsSecurityOpen) {
            if (mUsePmk) {
                nsBuilder.setPmk(PMK);
            } else {
                nsBuilder.setPskPassphrase(PASSPHRASE);
            }
            nsBuilder.setPort(port).setTransportProtocol(6); // 6 == TCP
        }
        NetworkRequest nr = new NetworkRequest.Builder().addTransportType(
                NetworkCapabilities.TRANSPORT_WIFI_AWARE).setNetworkSpecifier(
                nsBuilder.build()).build();
        mNetworkCb = new CallbackUtils.NetworkCb();
        mCm.requestNetwork(nr, mNetworkCb, CALLBACK_TIMEOUT_SEC * 1000);
        mListener.onTestMsgReceived(
                mContext.getString(R.string.aware_status_network_requested));
        if (DBG) Log.d(TAG, "executeTestPublisher: requested network");

        // 6. send message & wait for send status
        mWifiAwareDiscoverySession.sendMessage(mPeerHandle, MESSAGE_ID, MSG_PUB_TO_SUB);
        CallbackUtils.DiscoveryCb.CallbackData callbackData = mDiscoveryCb.waitForCallbacks(
                CallbackUtils.DiscoveryCb.ON_MESSAGE_SEND_SUCCEEDED
                        | CallbackUtils.DiscoveryCb.ON_MESSAGE_SEND_FAILED);
        switch (callbackData.callback) {
            case CallbackUtils.DiscoveryCb.TIMEOUT:
                setFailureReason(mContext.getString(R.string.aware_status_send_timeout));
                Log.e(TAG, "executeTestPublisher: send message TIMEOUT");
                return false;
            case CallbackUtils.DiscoveryCb.ON_MESSAGE_SEND_FAILED:
                setFailureReason(mContext.getString(R.string.aware_status_send_failed));
                Log.e(TAG, "executeTestPublisher: send message ON_MESSAGE_SEND_FAILED");
                return false;
        }
        mListener.onTestMsgReceived(mContext.getString(R.string.aware_status_send_success));
        if (DBG) Log.d(TAG, "executeTestPublisher: send message succeeded");

        if (callbackData.messageId != MESSAGE_ID) {
            setFailureReason(mContext.getString(R.string.aware_status_send_fail_parameter));
            Log.e(TAG, "executeTestPublisher: send message succeeded but message ID mismatch : "
                    + callbackData.messageId);
            return false;
        }

        // 7. wait for network
        Pair<Network, NetworkCapabilities> info = mNetworkCb.waitForNetworkCapabilities();
        if (info == null) {
            setFailureReason(mContext.getString(R.string.aware_status_network_failed));
            Log.e(TAG, "executeTestPublisher: request network rejected - ON_UNAVAILABLE");
            return false;
        }
        if (info.first == null || info.second == null) {
            setFailureReason(mContext.getString(R.string.aware_status_network_failed));
            Log.e(TAG, "executeTestPublisher: received a null Network or NetworkCapabilities!?");
            return false;
        }
        if (sSDKLevel <= android.os.Build.VERSION_CODES.P) {
            if (info.second.getNetworkSpecifier() != null) {
                setFailureReason(mContext.getString(R.string.aware_status_network_failed_leak));
                Log.e(TAG, "executeTestSubscriber: network request accepted - but leaks NS!");
                return false;
            }
        }
        mListener.onTestMsgReceived(mContext.getString(R.string.aware_status_network_success));
        if (DBG) Log.d(TAG, "executeTestPublisher: network request granted - AVAILABLE");

        // 8. Send/Receive data to validate connection - happens on thread above
        if (!mIsSecurityOpen) {
            mClientServerThread.join(CALLBACK_TIMEOUT_SEC * 1000);
            if (mClientServerThread.isAlive()) {
                setFailureReason(mContext.getString(R.string.aware_status_socket_failure));
                Log.e(TAG,
                        "executeTestPublisher: failure while waiting for client-server thread to "
                                + "finish");
                return false;
            }
        }

        return true;
    }
}
