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

package com.googlecode.android_scripting.facade;

import android.net.NetworkCapabilities;
import android.net.wifi.aware.WifiAwareNetworkInfo;

import com.googlecode.android_scripting.jsonrpc.JsonSerializable;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Utility class for ConnectivityManager/Service events. Encapsulates the data in a JSON format
 * to be used in the test script.
 *
 * Note that not all information is encapsulated. Add to the *Event classes as more information
 * is needed.
 */
public class ConnectivityEvents {
    /**
     * Translates a socket keep-alive event to JSON.
     */
    public static class SocketKeepaliveEvent implements JsonSerializable {
        private String mId;
        private String mSocketKeepaliveEvent;

        public SocketKeepaliveEvent(String id, String event) {
            mId = id;
            mSocketKeepaliveEvent = event;
        }

        /**
         * Create a JSON data-structure.
         */
        public JSONObject toJSON() throws JSONException {
            JSONObject socketKeepalive = new JSONObject();

            socketKeepalive.put(
                    ConnectivityConstants.SocketKeepaliveContainer.ID,
                    mId);
            socketKeepalive.put(
                    ConnectivityConstants.SocketKeepaliveContainer.SOCKET_KEEPALIVE_EVENT,
                    mSocketKeepaliveEvent);

            return socketKeepalive;
        }
    }

    /**
     * Translates a ConnectivityManager.NetworkCallback to JSON.
     */
    public static class NetworkCallbackEventBase implements JsonSerializable {
        private String mId;
        private String mNetworkCallbackEvent;
        private long mCreateTimestamp;
        private long mCurrentTimestamp;

        public NetworkCallbackEventBase(String id, String event, long createTimestamp) {
            mId = id;
            mNetworkCallbackEvent = event;
            mCreateTimestamp = createTimestamp;
            mCurrentTimestamp = System.currentTimeMillis();
        }

        /**
         * Create a JSON data-structure.
         */
        public JSONObject toJSON() throws JSONException {
            JSONObject networkCallback = new JSONObject();

            networkCallback.put(ConnectivityConstants.NetworkCallbackContainer.ID, mId);
            networkCallback.put(
                    ConnectivityConstants.NetworkCallbackContainer.NETWORK_CALLBACK_EVENT,
                    mNetworkCallbackEvent);
            networkCallback.put(ConnectivityConstants.NetworkCallbackContainer.CREATE_TIMESTAMP,
                    mCreateTimestamp);
            networkCallback.put(ConnectivityConstants.NetworkCallbackContainer.CURRENT_TIMESTAMP,
                    mCurrentTimestamp);

            return networkCallback;
        }
    }

    /**
     * Specializes NetworkCallbackEventBase to add information for the onLosing() callback.
     */
    public static class NetworkCallbackEventOnLosing extends NetworkCallbackEventBase {
        private int mMaxMsToLive;

        public NetworkCallbackEventOnLosing(String id, String event, long createTimestamp,
                int maxMsToLive) {
            super(id, event, createTimestamp);
            mMaxMsToLive = maxMsToLive;
        }

        /**
         * Create a JSON data-structure.
         */
        public JSONObject toJSON() throws JSONException {
            JSONObject json = super.toJSON();
            json.put(ConnectivityConstants.NetworkCallbackContainer.MAX_MS_TO_LIVE, mMaxMsToLive);
            return json;
        }
    }

    /**
     * Specializes NetworkCallbackEventBase to add information for the onCapabilitiesChanged()
     * callback.
     */
    public static class NetworkCallbackEventOnCapabilitiesChanged extends NetworkCallbackEventBase {
        private NetworkCapabilities mNetworkCapabilities;

        public NetworkCallbackEventOnCapabilitiesChanged(String id, String event,
                long createTimestamp, NetworkCapabilities networkCapabilities) {
            super(id, event, createTimestamp);
            mNetworkCapabilities = networkCapabilities;
        }

        /**
         * Create a JSON data-structure.
         */
        public JSONObject toJSON() throws JSONException {
            JSONObject json = super.toJSON();
            json.put(ConnectivityConstants.NetworkCallbackContainer.RSSI,
                    mNetworkCapabilities.getSignalStrength());
            if (mNetworkCapabilities.getNetworkSpecifier() != null) {
                json.put("network_specifier",
                        mNetworkCapabilities.getNetworkSpecifier().toString());
            }
            if (mNetworkCapabilities.getTransportInfo() instanceof WifiAwareNetworkInfo) {
                WifiAwareNetworkInfo anc =
                        (WifiAwareNetworkInfo) mNetworkCapabilities.getTransportInfo();

                String ipv6 = anc.getPeerIpv6Addr().toString();
                if (ipv6.charAt(0) == '/') {
                    ipv6 = ipv6.substring(1);
                }
                json.put("aware_ipv6", ipv6);
                if (anc.getPort() != 0) {
                    json.put("aware_port", anc.getPort());
                }
                if (anc.getTransportProtocol() != -1) {
                    json.put("aware_transport_protocol", anc.getTransportProtocol());
                }
            }
            return json;
        }
    }

    /**
     * Specializes NetworkCallbackEventBase to add information for the onCapabilitiesChanged()
     * callback.
     */
    public static class NetworkCallbackEventOnLinkPropertiesChanged extends
            NetworkCallbackEventBase {
        private String mInterfaceName;

        public NetworkCallbackEventOnLinkPropertiesChanged(String id, String event,
                long createTimestamp, String interfaceName) {
            super(id, event, createTimestamp);
            mInterfaceName = interfaceName;
        }

        /**
         * Create a JSON data-structure.
         */
        public JSONObject toJSON() throws JSONException {
            JSONObject json = super.toJSON();
            json.put(ConnectivityConstants.NetworkCallbackContainer.INTERFACE_NAME, mInterfaceName);
            return json;
        }
    }
}
