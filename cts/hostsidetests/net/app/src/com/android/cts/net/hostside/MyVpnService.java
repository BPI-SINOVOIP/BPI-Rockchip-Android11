/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.cts.net.hostside;

import android.content.Intent;
import android.net.Network;
import android.net.ProxyInfo;
import android.net.VpnService;
import android.os.ParcelFileDescriptor;
import android.content.pm.PackageManager.NameNotFoundException;
import android.text.TextUtils;
import android.util.Log;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;

public class MyVpnService extends VpnService {

    private static String TAG = "MyVpnService";
    private static int MTU = 1799;

    public static final String ACTION_ESTABLISHED = "com.android.cts.net.hostside.ESTABNLISHED";
    public static final String EXTRA_ALWAYS_ON = "is-always-on";
    public static final String EXTRA_LOCKDOWN_ENABLED = "is-lockdown-enabled";

    private ParcelFileDescriptor mFd = null;
    private PacketReflector mPacketReflector = null;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String packageName = getPackageName();
        String cmd = intent.getStringExtra(packageName + ".cmd");
        if ("disconnect".equals(cmd)) {
            stop();
        } else if ("connect".equals(cmd)) {
            start(packageName, intent);
        }

        return START_NOT_STICKY;
    }

    private void start(String packageName, Intent intent) {
        Builder builder = new Builder();

        String addresses = intent.getStringExtra(packageName + ".addresses");
        if (addresses != null) {
            String[] addressArray = addresses.split(",");
            for (int i = 0; i < addressArray.length; i++) {
                String[] prefixAndMask = addressArray[i].split("/");
                try {
                    InetAddress address = InetAddress.getByName(prefixAndMask[0]);
                    int prefixLength = Integer.parseInt(prefixAndMask[1]);
                    builder.addAddress(address, prefixLength);
                } catch (UnknownHostException|NumberFormatException|
                         ArrayIndexOutOfBoundsException e) {
                    continue;
                }
            }
        }

        String routes = intent.getStringExtra(packageName + ".routes");
        if (routes != null) {
            String[] routeArray = routes.split(",");
            for (int i = 0; i < routeArray.length; i++) {
                String[] prefixAndMask = routeArray[i].split("/");
                try {
                    InetAddress address = InetAddress.getByName(prefixAndMask[0]);
                    int prefixLength = Integer.parseInt(prefixAndMask[1]);
                    builder.addRoute(address, prefixLength);
                } catch (UnknownHostException|NumberFormatException|
                         ArrayIndexOutOfBoundsException e) {
                    continue;
                }
            }
        }

        String allowed = intent.getStringExtra(packageName + ".allowedapplications");
        if (allowed != null) {
            String[] packageArray = allowed.split(",");
            for (int i = 0; i < packageArray.length; i++) {
                String allowedPackage = packageArray[i];
                if (!TextUtils.isEmpty(allowedPackage)) {
                    try {
                        builder.addAllowedApplication(allowedPackage);
                    } catch(NameNotFoundException e) {
                        continue;
                    }
                }
            }
        }

        String disallowed = intent.getStringExtra(packageName + ".disallowedapplications");
        if (disallowed != null) {
            String[] packageArray = disallowed.split(",");
            for (int i = 0; i < packageArray.length; i++) {
                String disallowedPackage = packageArray[i];
                if (!TextUtils.isEmpty(disallowedPackage)) {
                    try {
                        builder.addDisallowedApplication(disallowedPackage);
                    } catch(NameNotFoundException e) {
                        continue;
                    }
                }
            }
        }

        ArrayList<Network> underlyingNetworks =
                intent.getParcelableArrayListExtra(packageName + ".underlyingNetworks");
        if (underlyingNetworks == null) {
            // VPN tracks default network
            builder.setUnderlyingNetworks(null);
        } else {
            builder.setUnderlyingNetworks(underlyingNetworks.toArray(new Network[0]));
        }

        boolean isAlwaysMetered = intent.getBooleanExtra(packageName + ".isAlwaysMetered", false);
        builder.setMetered(isAlwaysMetered);

        ProxyInfo vpnProxy = intent.getParcelableExtra(packageName + ".httpProxy");
        builder.setHttpProxy(vpnProxy);
        builder.setMtu(MTU);
        builder.setBlocking(true);
        builder.setSession("MyVpnService");

        Log.i(TAG, "Establishing VPN,"
                + " addresses=" + addresses
                + " routes=" + routes
                + " allowedApplications=" + allowed
                + " disallowedApplications=" + disallowed);

        mFd = builder.establish();
        Log.i(TAG, "Established, fd=" + (mFd == null ? "null" : mFd.getFd()));

        broadcastEstablished();

        mPacketReflector = new PacketReflector(mFd.getFileDescriptor(), MTU);
        mPacketReflector.start();
    }

    private void broadcastEstablished() {
        final Intent bcIntent = new Intent(ACTION_ESTABLISHED);
        bcIntent.putExtra(EXTRA_ALWAYS_ON, isAlwaysOn());
        bcIntent.putExtra(EXTRA_LOCKDOWN_ENABLED, isLockdownEnabled());
        sendBroadcast(bcIntent);
    }

    private void stop() {
        if (mPacketReflector != null) {
            mPacketReflector.interrupt();
            mPacketReflector = null;
        }
        try {
            if (mFd != null) {
                Log.i(TAG, "Closing filedescriptor");
                mFd.close();
            }
        } catch(IOException e) {
        } finally {
            mFd = null;
        }
    }

    @Override
    public void onDestroy() {
        stop();
        super.onDestroy();
    }
}
