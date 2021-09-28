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

package com.googlecode.android_scripting.facade.net;

import android.app.Service;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.IpSecManager.UdpEncapsulationSocket;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.SocketKeepalive;
import android.net.util.KeepaliveUtils;

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.facade.ConnectivityConstants;
import com.googlecode.android_scripting.facade.ConnectivityEvents.SocketKeepaliveEvent;
import com.googlecode.android_scripting.facade.EventFacade;
import com.googlecode.android_scripting.facade.FacadeManager;
import com.googlecode.android_scripting.jsonrpc.RpcReceiver;
import com.googlecode.android_scripting.rpc.Rpc;

import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Socket keepalive SL4A APIs
 */
public class SocketKeepaliveFacade extends RpcReceiver {

    private enum Event {
        Invalid(0),
        Started(1 << 0),
        Stopped(1 << 1),
        Error(1 << 2),
        OnDataReceived(1 << 3),
        EventAll(1 << 0 | 1 << 1 | 1 << 2 | 1 << 3);

        private int mType;
        Event(int type) {
            mType = type;
        }
        private int getType() {
            return mType;
        }
    }

    class SocketKeepaliveReceiver extends SocketKeepalive.Callback {

        private int mEvents;
        public String mId;
        public SocketKeepalive mSocketKeepalive;

        SocketKeepaliveReceiver(int events) {
            super();
            mEvents = events;
            mId = this.toString();
        }

        public void startListeningForEvents(int events) {
            mEvents |= events & Event.EventAll.getType();
        }

        public void stopListeningForEvents(int events) {
            mEvents &= ~(events & Event.EventAll.getType());
        }

        private void handleEvent(Event e) {
            Log.d("SocketKeepaliveFacade: SocketKeepaliveCallback - " + e.toString());
            if ((mEvents & e.getType()) == e.getType()) {
                mEventFacade.postEvent(
                        ConnectivityConstants.EventSocketKeepaliveCallback,
                        new SocketKeepaliveEvent(mId, e.toString()));
            }
        }

        @Override
        public void onStarted() {
            handleEvent(Event.Started);
        }

        @Override
        public void onStopped() {
            handleEvent(Event.Stopped);
        }

        @Override
        public void onError(int error) {
            handleEvent(Event.Error);
        }

        @Override
        public void onDataReceived() {
            handleEvent(Event.OnDataReceived);
        }
    }

    private final ConnectivityManager mManager;
    private final Service mService;
    private final Context mContext;
    private final EventFacade mEventFacade;
    private static HashMap<String, SocketKeepaliveReceiver> sSocketKeepaliveReceiverMap =
            new HashMap<String, SocketKeepaliveReceiver>();
    private final Executor mExecutor = Executors.newSingleThreadExecutor();
    public SocketKeepaliveFacade(FacadeManager manager) {
        super(manager);
        mService = manager.getService();
        mContext = mService.getBaseContext();
        mManager = (ConnectivityManager) mService.getSystemService(Context.CONNECTIVITY_SERVICE);
        mEventFacade = manager.getReceiver(EventFacade.class);
    }

    /**
     * Start NATT socket keepalive
     * @param udpEncapsulationSocketId : hash key of {@link UdpEncapsulationSocket}
     * @param srcAddrString : source addr in string
     * @param dstAddrString : destination addr in string
     * @param intervalSeconds : keepalive interval in seconds
     * @return Keepalive key if successful, null if not
     */
    @Rpc(description = "start natt socket keepalive")
    public String startNattSocketKeepalive(
            String udpEncapsulationSocketId,
            String srcAddrString,
            String dstAddrString,
            Integer intervalSeconds) {
        try {
            UdpEncapsulationSocket udpEncapSocket =
                    IpSecManagerFacade.getUdpEncapsulationSocket(udpEncapsulationSocketId);
            Network network = mManager.getActiveNetwork();
            InetAddress srcAddr = InetAddress.getByName(srcAddrString);
            InetAddress dstAddr = InetAddress.getByName(dstAddrString);
            Log.d("SocketKeepaliveFacade: startNattKeepalive intervalSeconds:" + intervalSeconds);
            SocketKeepaliveReceiver socketKeepaliveReceiver = new SocketKeepaliveReceiver(
                    Event.EventAll.getType());
            SocketKeepalive socketKeepalive = mManager.createSocketKeepalive(
                    network,
                    udpEncapSocket,
                    srcAddr,
                    dstAddr,
                    mExecutor,
                    socketKeepaliveReceiver);
            socketKeepalive.start(intervalSeconds);
            if (socketKeepalive != null) {
                socketKeepaliveReceiver.mSocketKeepalive = socketKeepalive;
                String key = socketKeepaliveReceiver.mId;
                sSocketKeepaliveReceiverMap.put(key, socketKeepaliveReceiver);
                return key;
            } else {
                Log.e("SocketKeepaliveFacade: Failed to start Natt socketkeepalive");
                return null;
            }
        } catch (UnknownHostException e) {
            Log.e("SocketKeepaliveFacade: SocketNattKeepalive exception", e);
            return null;
        }
    }

    /**
     * Start TCP socket keepalive
     * @param socketId : Hash key of socket object
     * @param intervalSeconds : keepalive interval in seconds
     * @return Keepalive key if successful, null if not
     */
    @Rpc(description = "Start TCP socket keepalive")
    public String startTcpSocketKeepalive(String socketId, Integer intervalSeconds) {
        Socket socket = SocketFacade.getSocket(socketId);
        Network network = mManager.getActiveNetwork();
        Log.d("SocketKeepaliveFacade: Keepalive interval seconds: " + intervalSeconds);
        SocketKeepaliveReceiver socketKeepaliveReceiver = new SocketKeepaliveReceiver(
                Event.EventAll.getType());
        SocketKeepalive socketKeepalive = mManager.createSocketKeepalive(
                network,
                socket,
                mExecutor,
                socketKeepaliveReceiver);
        socketKeepalive.start(intervalSeconds);
        if (socketKeepalive == null) {
            Log.e("SocketKeepaliveFacade: Failed to start TCP SocketKeepalive");
            return null;
        }
        socketKeepaliveReceiver.mSocketKeepalive = socketKeepalive;
        String key = socketKeepaliveReceiver.mId;
        sSocketKeepaliveReceiverMap.put(key, socketKeepaliveReceiver);
        return key;
    }

    /**
     * Stop socket keepalive
     * @param key : {@link SocketKeepaliveReceiver}
     * @return true if stopping keepalive is successful, false if not
     */
    @Rpc(description = "stop socket keepalive")
    public Boolean stopSocketKeepalive(String key) {
        SocketKeepaliveReceiver mSocketKeepaliveReceiver =
                sSocketKeepaliveReceiverMap.get(key);
        if (mSocketKeepaliveReceiver == null) {
            return false;
        }
        mSocketKeepaliveReceiver.mSocketKeepalive.stop();
        return true;
    }

    /**
     * Remove key from the SocketKeepaliveReceiver map
     * @param key : Hash key of the keepalive object
     */
    @Rpc(description = "remove SocketKeepaliveReceiver key")
    public void removeSocketKeepaliveReceiverKey(String key) {
        sSocketKeepaliveReceiverMap.remove(key);
    }

    /**
     * Start listening for a socket keepalive event
     * @param key : hash key of {@link SocketKeepaliveReceiver}
     * @param eventString : type of keepalive event - Started, Stopped, Error
     * @return True if listening for event successful, false if not
     */
    @Rpc(description = "start listening for SocketKeepalive Event")
    public Boolean socketKeepaliveStartListeningForEvent(String key, String eventString) {
        SocketKeepaliveReceiver mSocketKeepaliveReceiver =
                sSocketKeepaliveReceiverMap.get(key);
        int event = Event.valueOf(eventString).getType();
        if (mSocketKeepaliveReceiver == null || event == Event.Invalid.getType()) {
            return false;
        }
        mSocketKeepaliveReceiver.startListeningForEvents(event);
        return true;
    }

    /**
     * Stop listening for a for socket keepalive event
     * @param key : hash key of {@link SocketKeepaliveReceiver}
     * @param eventString : type of keepalive event - Started, Stopped, Error
     * @return True if listening event successful, false if not
     */
    @Rpc(description = "stop listening for SocketKeepalive Event")
    public Boolean socketKeepaliveStopListeningForEvent(String key, String eventString) {
        SocketKeepaliveReceiver mSocketKeepaliveReceiver =
                sSocketKeepaliveReceiverMap.get(key);
        int event = Event.valueOf(eventString).getType();
        if (mSocketKeepaliveReceiver == null || event == Event.Invalid.getType()) {
            return false;
        }
        mSocketKeepaliveReceiver.stopListeningForEvents(event);
        return true;
    }

    /**
     * Get maximum supported keepalives for active network
     * @return Max number of keepalives supported in int
     */
    @Rpc(description = "get max number of keepalives supported for active network")
    public int getSupportedKeepalivesForNetwork() {
        NetworkCapabilities netCap = mManager.getNetworkCapabilities(mManager.getActiveNetwork());
        int[] ka = KeepaliveUtils.getSupportedKeepalives(mContext);
        return KeepaliveUtils.getSupportedKeepalivesForNetworkCapabilities(ka, netCap);
    }

    @Override
    public void shutdown() {}
}
