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

package com.googlecode.android_scripting.facade.wifi;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.NetworkInfo;
import android.net.wifi.WpsInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pGroupList;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pDnsSdServiceRequest;
import android.net.wifi.p2p.nsd.WifiP2pServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pServiceRequest;
import android.net.wifi.p2p.nsd.WifiP2pUpnpServiceInfo;
import android.net.wifi.p2p.nsd.WifiP2pUpnpServiceRequest;
import android.os.Bundle;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import com.android.internal.util.Protocol;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.EventFacade;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;
import com.googlecode.android_scripting.rpc.RpcParameter;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * WifiP2pManager functions.
 */
public class WifiP2pManagerFacade extends RpcReceiver {

    class WifiP2pActionListener implements WifiP2pManager.ActionListener {
        private final EventFacade mEventFacade;
        private final String mEventType;
        private final String TAG;

        public WifiP2pActionListener(EventFacade eventFacade, String tag) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
            TAG = tag;
        }

        @Override
        public void onSuccess() {
            mEventFacade.postEvent(mEventType + TAG + "OnSuccess", null);
        }

        @Override
        public void onFailure(int reason) {
            Log.d("WifiActionListener  " + mEventType);
            Bundle msg = new Bundle();
            if (reason == WifiP2pManager.P2P_UNSUPPORTED) {
                msg.putString("reason", "P2P_UNSUPPORTED");
            } else if (reason == WifiP2pManager.ERROR) {
                msg.putString("reason", "ERROR");
            } else if (reason == WifiP2pManager.BUSY) {
                msg.putString("reason", "BUSY");
            } else if (reason == WifiP2pManager.NO_SERVICE_REQUESTS) {
                msg.putString("reason", "NO_SERVICE_REQUESTS");
            } else {
                msg.putInt("reason", reason);
            }
            mEventFacade.postEvent(mEventType + TAG + "OnFailure", msg);
        }
    }

    class WifiP2pConnectionInfoListener implements WifiP2pManager.ConnectionInfoListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        public WifiP2pConnectionInfoListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onConnectionInfoAvailable(WifiP2pInfo info) {
            Bundle msg = new Bundle();
            msg.putBoolean("groupFormed", info.groupFormed);
            msg.putBoolean("isGroupOwner", info.isGroupOwner);
            InetAddress addr = info.groupOwnerAddress;
            String hostName = null;
            String hostAddress = null;
            if (addr != null) {
                hostName = addr.getHostName();
                hostAddress = addr.getHostAddress();
            }
            msg.putString("groupOwnerHostName", hostName);
            msg.putString("groupOwnerHostAddress", hostAddress);
            mEventFacade.postEvent(mEventType + "OnConnectionInfoAvailable", msg);
        }
    }

    class WifiP2pDnsSdServiceResponseListener implements
            WifiP2pManager.DnsSdServiceResponseListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        public WifiP2pDnsSdServiceResponseListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onDnsSdServiceAvailable(String instanceName, String registrationType,
                WifiP2pDevice srcDevice) {
            Bundle msg = new Bundle();
            msg.putString("InstanceName", instanceName);
            msg.putString("RegistrationType", registrationType);
            msg.putString("SourceDeviceName", srcDevice.deviceName);
            msg.putString("SourceDeviceAddress", srcDevice.deviceAddress);
            mEventFacade.postEvent(mEventType + "OnDnsSdServiceAvailable", msg);
        }
    }

    class WifiP2pDnsSdTxtRecordListener implements WifiP2pManager.DnsSdTxtRecordListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        public WifiP2pDnsSdTxtRecordListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onDnsSdTxtRecordAvailable(String fullDomainName,
                Map<String, String> txtRecordMap, WifiP2pDevice srcDevice) {
            Bundle msg = new Bundle();
            msg.putString("FullDomainName", fullDomainName);
            Bundle txtMap = new Bundle();
            for (String key : txtRecordMap.keySet()) {
                txtMap.putString(key, txtRecordMap.get(key));
            }
            msg.putBundle("TxtRecordMap", txtMap);
            msg.putString("SourceDeviceName", srcDevice.deviceName);
            msg.putString("SourceDeviceAddress", srcDevice.deviceAddress);
            mEventFacade.postEvent(mEventType + "OnDnsSdTxtRecordAvailable", msg);
        }
    }

    class WifiP2pGroupInfoListener implements WifiP2pManager.GroupInfoListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        public WifiP2pGroupInfoListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onGroupInfoAvailable(WifiP2pGroup group) {
            mEventFacade.postEvent(mEventType + "OnGroupInfoAvailable", parseGroupInfo(group));
        }
    }

    class WifiP2pPeerListListener implements WifiP2pManager.PeerListListener {
        private final EventFacade mEventFacade;

        public WifiP2pPeerListListener(EventFacade eventFacade) {
            mEventFacade = eventFacade;
        }

        @Override
        public void onPeersAvailable(WifiP2pDeviceList newPeers) {
            Collection<WifiP2pDevice> devices = newPeers.getDeviceList();
            Log.d(devices.toString());
            if (devices.size() > 0) {
                mP2pPeers.clear();
                mP2pPeers.addAll(devices);
                Bundle msg = new Bundle();
                msg.putParcelableList("Peers", mP2pPeers);
                mEventFacade.postEvent(mEventType + "OnPeersAvailable", msg);
            }
        }
    }

    class WifiP2pPersistentGroupInfoListener implements WifiP2pManager.PersistentGroupInfoListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        public WifiP2pPersistentGroupInfoListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onPersistentGroupInfoAvailable(WifiP2pGroupList groups) {
            ArrayList<Bundle> gs = new ArrayList<Bundle>();
            for (WifiP2pGroup g : groups.getGroupList()) {
                gs.add(parseGroupInfo(g));
            }
            mEventFacade.postEvent(mEventType + "OnPersistentGroupInfoAvailable", gs);
        }
    }

    class WifiP2pOngoingPeerConfigListener implements WifiP2pManager.OngoingPeerInfoListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        WifiP2pOngoingPeerConfigListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onOngoingPeerAvailable(WifiP2pConfig config) {
            Bundle msg = new Bundle();
            mEventFacade.postEvent(mEventType + "OnOngoingPeerAvailable", config);
        }
    }

    class WifiP2pUpnpServiceResponseListener implements WifiP2pManager.UpnpServiceResponseListener {
        private final EventFacade mEventFacade;
        private final String mEventType;

        WifiP2pUpnpServiceResponseListener(EventFacade eventFacade) {
            mEventType = "WifiP2p";
            mEventFacade = eventFacade;
        }

        @Override
        public void onUpnpServiceAvailable(List<String> uniqueServiceNames,
                WifiP2pDevice srcDevice) {
            Bundle msg = new Bundle();
            msg.putParcelable("Device", srcDevice);
            msg.putStringArrayList("ServiceList", new ArrayList(uniqueServiceNames));
            mEventFacade.postEvent(mEventType + "OnUpnpServiceAvailable", msg);
        }
    }

    class WifiP2pStateChangedReceiver extends BroadcastReceiver {
        private final EventFacade mEventFacade;

        WifiP2pStateChangedReceiver(EventFacade eventFacade) {
            mEventFacade = eventFacade;
        }

        @Override
        public void onReceive(Context c, Intent intent) {
            Bundle mResults = new Bundle();
            String action = intent.getAction();
            if (action.equals(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION)) {
                Log.d("Wifi P2p State Changed.");
                int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, 0);
                if (state == WifiP2pManager.WIFI_P2P_STATE_DISABLED) {
                    Log.d("Disabled");
                    isP2pEnabled = false;
                } else if (state == WifiP2pManager.WIFI_P2P_STATE_ENABLED) {
                    Log.d("Enabled");
                    isP2pEnabled = true;
                }
            } else if (action.equals(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)) {
                Log.d("Wifi P2p Peers Changed. Requesting peers.");
                WifiP2pDeviceList peers = intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_P2P_DEVICE_LIST);
                Log.d(peers.toString());
                wifiP2pRequestPeers();
            } else if (action.equals(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)) {
                Log.d("Wifi P2p Connection Changed.");
                WifiP2pInfo p2pInfo = intent.getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_INFO);
                NetworkInfo networkInfo = intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);
                WifiP2pGroup group = intent.getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_GROUP);
                if (networkInfo.isConnected()) {
                    Log.d("Wifi P2p Connected.");
                    mResults.putParcelable("P2pInfo", p2pInfo);
                    mResults.putParcelable("Group", group);
                    mEventFacade.postEvent(mEventType + "Connected", mResults);
                } else {
                    mEventFacade.postEvent(mEventType + "Disconnected", null);
                }
            } else if (action.equals(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION)) {
                Log.d("Wifi P2p This Device Changed.");
                WifiP2pDevice device = intent
                        .getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_DEVICE);
                mResults.putParcelable("Device", device);
                mEventFacade.postEvent(mEventType + "ThisDeviceChanged", mResults);
            } else if (action.equals(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION)) {
                Log.d("Wifi P2p Discovery Changed.");
                int state = intent.getIntExtra(WifiP2pManager.EXTRA_DISCOVERY_STATE, 0);
                if (state == WifiP2pManager.WIFI_P2P_DISCOVERY_STARTED) {
                    Log.d("discovery started.");
                } else if (state == WifiP2pManager.WIFI_P2P_DISCOVERY_STOPPED) {
                    Log.d("discovery stoped.");
                }
            }
        }
    }

    private final static String mEventType = "WifiP2p";

    private WifiP2pManager.Channel mChannel;
    private final EventFacade mEventFacade;
    private final WifiP2pManager mP2p;
    private final WifiP2pStateChangedReceiver mP2pStateChangedReceiver;
    private final Service mService;
    private final IntentFilter mStateChangeFilter;
    private final Map<Integer, WifiP2pServiceRequest> mServiceRequests;

    private boolean isP2pEnabled;
    private int mServiceRequestCnt = 0;
    private WifiP2pServiceInfo mServiceInfo = null;
    private List<WifiP2pDevice> mP2pPeers = new ArrayList<WifiP2pDevice>();

    public WifiP2pManagerFacade(FacadeManager manager) {
        super(manager);
        mService = manager.getService();
        mP2p = (WifiP2pManager) mService.getSystemService(Context.WIFI_P2P_SERVICE);
        mEventFacade = manager.getReceiver(EventFacade.class);

        mStateChangeFilter = new IntentFilter(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
        mStateChangeFilter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION);
        mStateChangeFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
        mStateChangeFilter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION);
        mStateChangeFilter.setPriority(999);

        mP2pStateChangedReceiver = new WifiP2pStateChangedReceiver(mEventFacade);
        mServiceRequests = new HashMap<Integer, WifiP2pServiceRequest>();
    }

    public Bundle parseGroupInfo(WifiP2pGroup group) {
        Bundle msg = new Bundle();
        msg.putString("Interface", group.getInterface());
        msg.putString("NetworkName", group.getNetworkName());
        msg.putString("Passphrase", group.getPassphrase());
        msg.putInt("NetworkId", group.getNetworkId());
        msg.putString("OwnerName", group.getOwner().deviceName);
        msg.putString("OwnerAddress", group.getOwner().deviceAddress);
        return msg;
    }

    @Override
    public void shutdown() {
        mService.unregisterReceiver(mP2pStateChangedReceiver);
    }

    @Rpc(description = "Accept p2p connection invitation.")
    public void wifiP2pAcceptConnection() throws RemoteException {
        Log.d("Accepting p2p connection.");
        Messenger m = mP2p.getP2pStateMachineMessenger();
        int user_accept = Protocol.BASE_WIFI_P2P_SERVICE + 2;
        Message msg = Message.obtain();
        msg.what = user_accept;
        m.send(msg);
    }

    @Rpc(description = "Reject p2p connection invitation.")
    public void wifiP2pRejectConnection() throws RemoteException {
        Log.d("Rejecting p2p connection.");
        Messenger m = mP2p.getP2pStateMachineMessenger();
        int user_accept = Protocol.BASE_WIFI_P2P_SERVICE + 3;
        Message msg = Message.obtain();
        msg.what = user_accept;
        m.send(msg);
    }

    /**
     * Confirm p2p keypad connection invitation.
     */
    @Rpc(description = "Confirm p2p keypad connection invitation.")
    public void wifiP2pConfirmConnection() throws RemoteException {
        Log.d("Confirm p2p connection.");
        Messenger m = mP2p.getP2pStateMachineMessenger();
        int user_confirm = Protocol.BASE_WIFI_P2P_SERVICE + 7;
        Message msg = Message.obtain();
        msg.what = user_confirm;
        m.send(msg);
    }

    @Rpc(description = "Register a local service for service discovery. One of the \"CreateXxxServiceInfo functions needs to be called first.\"")
    public void wifiP2pAddLocalService() {
        mP2p.addLocalService(mChannel, mServiceInfo,
                new WifiP2pActionListener(mEventFacade, "AddLocalService"));
    }

    @Rpc(description = "Add a service discovery request.")
    public Integer wifiP2pAddServiceRequest(
            @RpcParameter(name = "protocolType") Integer protocolType) {
        WifiP2pServiceRequest request = WifiP2pServiceRequest.newInstance(protocolType);
        mServiceRequestCnt += 1;
        mServiceRequests.put(mServiceRequestCnt, request);
        mP2p.addServiceRequest(mChannel, request, new WifiP2pActionListener(mEventFacade,
                "AddServiceRequest"));
        return mServiceRequestCnt;
    }

    /**
     * Add a service upnp discovery request.
     * @param query The part of service specific query
     */
    @Rpc(description = "Add a service upnp discovery request.")
    public Integer wifiP2pAddUpnpServiceRequest(
            @RpcParameter(name = "query") String query) {
        WifiP2pUpnpServiceRequest request = WifiP2pUpnpServiceRequest.newInstance(query);
        mServiceRequestCnt += 1;
        mServiceRequests.put(mServiceRequestCnt, request);
        mP2p.addServiceRequest(mChannel, request, new WifiP2pActionListener(mEventFacade,
                "AddUpnpServiceRequest"));
        return mServiceRequestCnt;
    }

    /**
     * Create a service discovery request to get the TXT data from the specified
     * Bonjour service.
     *
     * @param instanceName instance name. Can be null.
     * e.g)
     *  "MyPrinter"
     * @param serviceType service type. Cannot be null.
     * e.g)
     *  "_afpovertcp._tcp."(Apple File Sharing over TCP)
     *  "_ipp._tcp" (IP Printing over TCP)
     *  "_http._tcp" (http service)
     */
    @Rpc(description = "Add a service dns discovery request.")
    public Integer wifiP2pAddDnssdServiceRequest(
            @RpcParameter(name = "serviceType") String serviceType,
            @RpcParameter(name = "instanceName") String instanceName) {
        WifiP2pDnsSdServiceRequest request;
        if (instanceName != null) {
            request = WifiP2pDnsSdServiceRequest.newInstance(instanceName, serviceType);
        } else {
            request = WifiP2pDnsSdServiceRequest.newInstance(serviceType);
        }
        mServiceRequestCnt += 1;
        mServiceRequests.put(mServiceRequestCnt, request);
        mP2p.addServiceRequest(mChannel, request, new WifiP2pActionListener(mEventFacade,
                "AddDnssdServiceRequest"));
        return mServiceRequestCnt;
    }

    @Rpc(description = "Cancel any ongoing connect negotiation.")
    public void wifiP2pCancelConnect() {
        mP2p.cancelConnect(mChannel, new WifiP2pActionListener(mEventFacade, "CancelConnect"));
    }

    @Rpc(description = "Clear all registered local services of service discovery.")
    public void wifiP2pClearLocalServices() {
        mP2p.clearLocalServices(mChannel,
                new WifiP2pActionListener(mEventFacade, "ClearLocalServices"));
    }

    @Rpc(description = "Clear all registered service discovery requests.")
    public void wifiP2pClearServiceRequests() {
        mP2p.clearServiceRequests(mChannel,
                new WifiP2pActionListener(mEventFacade, "ClearServiceRequests"));
    }

    /**
     * Connects to a discovered wifi p2p device
     * @param config JSONObject Dictionary of p2p connection parameters
     * @throws JSONException
     */
    @Rpc(description = "Connects to a discovered wifi p2p device.")
    public void wifiP2pConnect(@RpcParameter(name = "config") JSONObject config)
            throws JSONException {
        WifiP2pConfig wifiP2pConfig = genWifiP2pConfig(config);
        mP2p.connect(mChannel, wifiP2pConfig,
                new WifiP2pActionListener(mEventFacade, "Connect"));
    }

    @Rpc(description = "Create a Bonjour service info object to be used for wifiP2pAddLocalService.")
    public void wifiP2pCreateBonjourServiceInfo(
            @RpcParameter(name = "instanceName") String instanceName,
            @RpcParameter(name = "serviceType") String serviceType,
            @RpcParameter(name = "txtMap") JSONObject txtMap) throws JSONException {
        Map<String, String> map = new HashMap<String, String>();
        Iterator<String> keyIterator = txtMap.keys();
        while (keyIterator.hasNext()) {
            String key = keyIterator.next();
            map.put(key, txtMap.getString(key));
        }
        mServiceInfo = WifiP2pDnsSdServiceInfo.newInstance(instanceName, serviceType, map);
    }

    @Rpc(description = "Create a wifi p2p group.")
    public void wifiP2pCreateGroup() {
        mP2p.createGroup(mChannel, new WifiP2pActionListener(mEventFacade, "CreateGroup"));
    }

    /**
     * Create a group with config.
     *
     * @param config JSONObject Dictionary of p2p connection parameters
     * @throws JSONException
     */
    @Rpc(description = "Create a wifi p2p group with config.")
    public void wifiP2pCreateGroupWithConfig(@RpcParameter(name = "config") JSONObject config)
            throws JSONException {
        WifiP2pConfig wifiP2pConfig = genWifiP2pConfig(config);
        mP2p.createGroup(mChannel, wifiP2pConfig,
                new WifiP2pActionListener(mEventFacade, "CreateGroup"));
    }

    @Rpc(description = "Create a Upnp service info object to be used for wifiP2pAddLocalService.")
    public void wifiP2pCreateUpnpServiceInfo(
            @RpcParameter(name = "uuid") String uuid,
            @RpcParameter(name = "device") String device,
            @RpcParameter(name = "services") JSONArray services) throws JSONException {
        List<String> serviceList = new ArrayList<String>();
        for (int i = 0; i < services.length(); i++) {
            serviceList.add(services.getString(i));
            Log.d("wifiP2pCreateUpnpServiceInfo, services: " + services.getString(i));
        }
        mServiceInfo = WifiP2pUpnpServiceInfo.newInstance(uuid, device, serviceList);
    }

    @Rpc(description = "Delete a stored persistent group from the system settings.")
    public void wifiP2pDeletePersistentGroup(@RpcParameter(name = "netId") Integer netId) {
        mP2p.deletePersistentGroup(mChannel, netId,
                new WifiP2pActionListener(mEventFacade, "DeletePersistentGroup"));
    }

    private boolean wifiP2pDeviceMatches(WifiP2pDevice d, String deviceId) {
        return d.deviceName.equals(deviceId) || d.deviceAddress.equals(deviceId);
    }

    @Rpc(description = "Start peers discovery for wifi p2p.")
    public void wifiP2pDiscoverPeers() {
        mP2p.discoverPeers(mChannel, new WifiP2pActionListener(mEventFacade, "DiscoverPeers"));
    }

    @Rpc(description = "Initiate service discovery.")
    public void wifiP2pDiscoverServices() {
        mP2p.discoverServices(mChannel,
                new WifiP2pActionListener(mEventFacade, "DiscoverServices"));
    }

    @Rpc(description = "Initialize wifi p2p. Must be called before any other p2p functions.")
    public void wifiP2pInitialize() {
        mService.registerReceiver(mP2pStateChangedReceiver, mStateChangeFilter);
        mChannel = mP2p.initialize(mService, mService.getMainLooper(), null);
    }

    @Rpc(description = "Sets the listening channel and operating channel of the current group created with initialize")
    public void wifiP2pSetChannelsForCurrentGroup(
            @RpcParameter(name = "listeningChannel") Integer listeningChannel,
            @RpcParameter(name = "operatingChannel") Integer operatingChannel) {
        mP2p.setWifiP2pChannels(mChannel, listeningChannel, operatingChannel,
                new WifiP2pActionListener(mEventFacade, "SetChannels"));
    }

    @Rpc(description = "Close the current wifi p2p connection created with initialize.")
    public void wifiP2pClose() {
        if (mChannel != null) {
            mChannel.close();
        }
    }

    @Rpc(description = "Returns true if wifi p2p is enabled, false otherwise.")
    public Boolean wifiP2pIsEnabled() {
        return isP2pEnabled;
    }

    @Rpc(description = "Remove the current p2p group.")
    public void wifiP2pRemoveGroup() {
        mP2p.removeGroup(mChannel, new WifiP2pActionListener(mEventFacade, "RemoveGroup"));
    }

    @Rpc(description = "Remove a registered local service added with wifiP2pAddLocalService.")
    public void wifiP2pRemoveLocalService() {
        mP2p.removeLocalService(mChannel, mServiceInfo,
                new WifiP2pActionListener(mEventFacade, "RemoveLocalService"));
    }

    @Rpc(description = "Remove a service discovery request.")
    public void wifiP2pRemoveServiceRequest(@RpcParameter(name = "index") Integer index) {
        mP2p.removeServiceRequest(mChannel, mServiceRequests.remove(index),
                new WifiP2pActionListener(mEventFacade, "RemoveServiceRequest"));
    }

    @Rpc(description = "Request device connection info.")
    public void wifiP2pRequestConnectionInfo() {
        mP2p.requestConnectionInfo(mChannel, new WifiP2pConnectionInfoListener(mEventFacade));
    }

    @Rpc(description = "Create a wifi p2p group.")
    public void wifiP2pRequestGroupInfo() {
        mP2p.requestGroupInfo(mChannel, new WifiP2pGroupInfoListener(mEventFacade));
    }

    @Rpc(description = "Request peers that are discovered for wifi p2p.")
    public void wifiP2pRequestPeers() {
        mP2p.requestPeers(mChannel, new WifiP2pPeerListListener(mEventFacade));
    }

    @Rpc(description = "Request a list of all the persistent p2p groups stored in system.")
    public void wifiP2pRequestPersistentGroupInfo() {
        mP2p.requestPersistentGroupInfo(mChannel,
                new WifiP2pPersistentGroupInfoListener(mEventFacade));
    }

    @Rpc(description = "Set p2p device name.")
    public void wifiP2pSetDeviceName(@RpcParameter(name = "devName") String devName) {
        mP2p.setDeviceName(mChannel, devName,
                new WifiP2pActionListener(mEventFacade, "SetDeviceName"));
    }

    @Rpc(description = "Register a callback to be invoked on receiving Bonjour service discovery response.")
    public void wifiP2pSetDnsSdResponseListeners() {
        mP2p.setDnsSdResponseListeners(mChannel,
                new WifiP2pDnsSdServiceResponseListener(mEventFacade),
                new WifiP2pDnsSdTxtRecordListener(mEventFacade));
    }

    /**
     * Register a callback to be invoked on receiving
     * Upnp service discovery response.
     */
    @Rpc(description = "Register a callback to be invoked on receiving "
            + "Upnp service discovery response.")
    public void wifiP2pSetUpnpResponseListeners() {
        mP2p.setUpnpServiceResponseListener(mChannel,
                new WifiP2pUpnpServiceResponseListener(mEventFacade));
    }

    @Rpc(description = "Stop an ongoing peer discovery.")
    public void wifiP2pStopPeerDiscovery() {
        mP2p.stopPeerDiscovery(mChannel,
                new WifiP2pActionListener(mEventFacade, "StopPeerDiscovery"));
    }

    private WpsInfo genWpsInfo(JSONObject j) throws JSONException {
        if (j == null) {
            return null;
        }
        WpsInfo wpsInfo = new WpsInfo();
        if (j.has("setup")) {
            wpsInfo.setup = j.getInt("setup");
        }
        if (j.has("BSSID")) {
            wpsInfo.BSSID = j.getString("BSSID");
        }
        if (j.has("pin")) {
            wpsInfo.pin = j.getString("pin");
        }
        return wpsInfo;
    }

    private WifiP2pConfig genWifiP2pConfig(JSONObject j) throws JSONException,
            NumberFormatException {
        if (j == null) {
            return null;
        }
        WifiP2pConfig config = new WifiP2pConfig();
        if (j.has("networkName") && j.has("passphrase")) {
            WifiP2pConfig.Builder b = new WifiP2pConfig.Builder();
            b.setNetworkName(j.getString("networkName"));
            b.setPassphrase(j.getString("passphrase"));
            if (j.has("groupOwnerBand")) {
                b.setGroupOperatingBand(Integer.parseInt(j.getString("groupOwnerBand")));
            }
            config = b.build();
        }
        if (j.has("deviceAddress")) {
            config.deviceAddress = j.getString("deviceAddress");
        }
        if (j.has("wpsInfo")) {
            config.wps = genWpsInfo(j.getJSONObject("wpsInfo"));
        }
        if (j.has("groupOwnerIntent")) {
            config.groupOwnerIntent = j.getInt("groupOwnerIntent");
        }
        if (j.has("netId")) {
            config.netId = j.getInt("netId");
        }
        return config;
    }

    /**
     * Set saved WifiP2pConfig for an ongoing peer connection
     * @param wifiP2pConfig JSONObject Dictionary of p2p connection parameters
     * @throws JSONException
     */
    @Rpc(description = "Set saved WifiP2pConfig for an ongoing peer connection")
    public void setP2pPeerConfigure(@RpcParameter(name = "config") JSONObject wifiP2pConfig)
            throws JSONException {
        mP2p.setOngoingPeerConfig(mChannel, genWifiP2pConfig(wifiP2pConfig),
                new WifiP2pActionListener(mEventFacade, "setP2pPeerConfigure"));
    }

    /**
     * Request saved WifiP2pConfig which used for an ongoing peer connection
     */
    @Rpc(description = "Request saved WifiP2pConfig which used for an ongoing peer connection")
    public void requestP2pPeerConfigure() {
        mP2p.requestOngoingPeerConfig(mChannel, new WifiP2pOngoingPeerConfigListener(mEventFacade));
    }
}
