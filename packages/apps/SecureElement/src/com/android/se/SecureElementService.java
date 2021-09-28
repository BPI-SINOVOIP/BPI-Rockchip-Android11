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
/*
 * Copyright (c) 2014-2017, The Linux Foundation.
 */
/*
 * Contributed by: Giesecke & Devrient GmbH.
 */

package com.android.se;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.ServiceSpecificException;
import android.se.omapi.ISecureElementChannel;
import android.se.omapi.ISecureElementListener;
import android.se.omapi.ISecureElementReader;
import android.se.omapi.ISecureElementService;
import android.se.omapi.ISecureElementSession;
import android.se.omapi.SEService;
import android.telephony.TelephonyManager;
import android.util.Log;

import com.android.se.Terminal.SecureElementReader;
import com.android.se.internal.ByteArrayConverter;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.PrintWriter;
import java.security.AccessControlException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.NoSuchElementException;

/**
 * Underlying implementation for OMAPI SEService
 */
public final class SecureElementService extends Service {

    public static final String UICC_TERMINAL = "SIM";
    public static final String ESE_TERMINAL = "eSE";
    private final String mTag = "SecureElementService";
    private static final boolean DEBUG = Build.IS_DEBUGGABLE;
    // LinkedHashMap will maintain the order of insertion
    private LinkedHashMap<String, Terminal> mTerminals = new LinkedHashMap<String, Terminal>();
    private int mActiveSimCount = 0;
    private final ISecureElementService.Stub mSecureElementServiceBinder =
            new ISecureElementService.Stub() {

                @Override
                public String[] getReaders() throws RemoteException {
                    return mTerminals.keySet().toArray(new String[mTerminals.size()]);
                }

                @Override
                public ISecureElementReader getReader(String reader)
                        throws RemoteException {
                    Log.d(mTag, "getReader() " + reader);
                    Terminal terminal = getTerminal(reader);
                    return terminal.new SecureElementReader(SecureElementService.this);
                }

                @Override
                public synchronized boolean[] isNFCEventAllowed(String reader, byte[] aid,
                        String[] packageNames)
                        throws RemoteException {
                    if (aid == null || aid.length == 0) {
                        aid = new byte[]{0x00, 0x00, 0x00, 0x00, 0x00};
                    }
                    if (aid.length < 5 || aid.length > 16) {
                        throw new IllegalArgumentException("AID out of range");
                    }
                    if (packageNames == null || packageNames.length == 0) {
                        throw new IllegalArgumentException("package names not specified");
                    }
                    Terminal terminal = getTerminal(reader);
                    return terminal.isNfcEventAllowed(getPackageManager(), aid, packageNames);
                }

                @Override
                protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
                    for (Terminal terminal : mTerminals.values()) {
                        terminal.dump(writer);
                    }
                }
            };

    public SecureElementService() {
        super();
    }

    private void initialize() {
        // listen for events
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(TelephonyManager.ACTION_MULTI_SIM_CONFIG_CHANGED);
        this.registerReceiver(mMultiSimConfigChangedReceiver, intentFilter);
    }

    /** Returns the terminal from the Reader name. */
    private Terminal getTerminal(String reader) {
        if (reader == null) {
            throw new NullPointerException("reader must not be null");
        }
        if (reader.equals("SIM")) {
            reader = "SIM1";
        }
        Terminal terminal = mTerminals.get(reader);
        if (terminal == null) {
            throw new IllegalArgumentException("Reader: " + reader + " doesn't exist");
        }
        return terminal;
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.i(mTag, Thread.currentThread().getName() + " onBind");
        if (ISecureElementService.class.getName().equals(intent.getAction())) {
            return mSecureElementServiceBinder;
        }
        return null;
    }

    @Override
    public void onCreate() {
        Log.i(mTag, Thread.currentThread().getName() + " onCreate");
        initialize();
        createTerminals();
        ServiceManager.addService(Context.SECURE_ELEMENT_SERVICE, mSecureElementServiceBinder);
    }

    /**
     * In case the onDestroy is called, we free the memory and
     * close all the channels.
     */
    public void onDestroy() {
        Log.i(mTag, "onDestroy");
        for (Terminal terminal : mTerminals.values()) {
            terminal.closeChannels();
            terminal.close();
        }
        if (mMultiSimConfigChangedReceiver != null) {
            this.unregisterReceiver(mMultiSimConfigChangedReceiver);
        }
    }

    private void addTerminals(String terminalName) {
        int index = 1;
        String name = null;
        if (terminalName.startsWith(SecureElementService.UICC_TERMINAL)) {
            index = mActiveSimCount + 1;
        }
        try {
            do {
                name = terminalName + Integer.toString(index);
                Terminal terminal = new Terminal(name, this);

                Log.i(mTag, "Check if terminal " + name + " is available.");
                // Only retry on fail for the first terminal of each type.
                terminal.initialize(index == 1);
                mTerminals.put(name, terminal);
                if (terminalName.equals(UICC_TERMINAL)) {
                    mActiveSimCount = index;
                }
            } while (++index > 0);
        } catch (NoSuchElementException e) {
            Log.i(mTag, "No HAL implementation for " + name);
        } catch (RemoteException | RuntimeException e) {
            Log.e(mTag, "Error in getService() for " + name);
        }
    }

    private void createTerminals() {
        // Check for all SE HAL implementations
        addTerminals(ESE_TERMINAL);
        addTerminals(UICC_TERMINAL);
    }

    private void refreshUiccTerminals(int activeSimCount) {
        String name = null;
        synchronized (this) {
            if (activeSimCount < mActiveSimCount) {
                // Remove non-supported UICC terminals
                for (int i = activeSimCount + 1; i <= mActiveSimCount; i++) {
                    name = UICC_TERMINAL + Integer.toString(i);
                    Terminal terminal = mTerminals.get(name);
                    if (terminal != null) {
                        terminal.closeChannels();
                        terminal.close();
                    }
                    mTerminals.remove(name);
                    Log.i(mTag, name + " is removed from available Terminals");
                }
                mActiveSimCount = activeSimCount;
            } else if (activeSimCount > mActiveSimCount) {
                // Try to initialize new UICC terminals
                addTerminals(UICC_TERMINAL);
            }
        }
    }

    private String getPackageNameFromCallingUid(int uid) {
        PackageManager packageManager = getPackageManager();
        if (packageManager != null) {
            String[] packageName = packageManager.getPackagesForUid(uid);
            if (packageName != null && packageName.length > 0) {
                return packageName[0];
            }
        }
        throw new AccessControlException("PackageName can not be determined");
    }

    final class SecureElementSession extends ISecureElementSession.Stub {

        private final SecureElementReader mReader;
        /** List of open channels in use of by this client. */
        private final List<Channel> mChannels = new ArrayList<>();
        private final Object mLock = new Object();
        private boolean mIsClosed;
        private byte[] mAtr;

        SecureElementSession(SecureElementReader reader) {
            if (reader == null) {
                throw new NullPointerException("SecureElementReader cannot be null");
            }
            mReader = reader;
            mAtr = mReader.getAtr();
            mIsClosed = false;
        }

        public ISecureElementReader getReader() throws RemoteException {
            return mReader;
        }

        @Override
        public byte[] getAtr() throws RemoteException {
            return mAtr;
        }

        @Override
        public void close() throws RemoteException {
            closeChannels();
            mReader.removeSession(this);
            synchronized (mLock) {
                mIsClosed = true;
            }
        }

        void removeChannel(Channel channel) {
            synchronized (mLock) {
                if (mChannels != null) {
                    mChannels.remove(channel);
                }
            }
        }

        @Override
        public void closeChannels() throws RemoteException {
            synchronized (mLock) {
                while (mChannels.size() > 0) {
                    try {
                        mChannels.get(0).close();
                    } catch (Exception ignore) {
                        Log.e(mTag, "SecureElementSession Channel - close Exception "
                                + ignore.getMessage());
                    }
                }
            }
        }

        @Override
        public boolean isClosed() throws RemoteException {
            synchronized (mLock) {
                return mIsClosed;
            }
        }

        @Override
        public ISecureElementChannel openBasicChannel(byte[] aid, byte p2,
                ISecureElementListener listener) throws RemoteException {
            if (DEBUG) {
                Log.i(mTag, "openBasicChannel() AID = "
                        + ByteArrayConverter.byteArrayToHexString(aid) + ", P2 = " + p2);
            }
            if (isClosed()) {
                throw new IllegalStateException("Session is closed");
            } else if (listener == null) {
                throw new NullPointerException("listener must not be null");
            } else if ((p2 != 0x00) && (p2 != 0x04) && (p2 != 0x08)
                    && (p2 != (byte) 0x0C)) {
                throw new UnsupportedOperationException("p2 not supported: "
                        + String.format("%02x ", p2 & 0xFF));
            }

            String packageName = getPackageNameFromCallingUid(Binder.getCallingUid());
            Channel channel = null;

            try {
                channel = mReader.getTerminal().openBasicChannel(this, aid, p2, listener,
                        packageName, Binder.getCallingPid());
            } catch (IOException e) {
                throw new ServiceSpecificException(SEService.IO_ERROR, e.getMessage());
            } catch (NoSuchElementException e) {
                throw new ServiceSpecificException(SEService.NO_SUCH_ELEMENT_ERROR, e.getMessage());
            }

            if (channel == null) {
                Log.i(mTag, "OpenBasicChannel() - returning null");
                return null;
            }
            Log.i(mTag, "Open basic channel success. Channel: "
                    + channel.getChannelNumber());

            mChannels.add(channel);
            return channel.new SecureElementChannel();
        }

        @Override
        public ISecureElementChannel openLogicalChannel(byte[] aid, byte p2,
                ISecureElementListener listener) throws RemoteException {
            if (DEBUG) {
                Log.i(mTag, "openLogicalChannel() AID = "
                        + ByteArrayConverter.byteArrayToHexString(aid) + ", P2 = " + p2);
            }
            if (isClosed()) {
                throw new IllegalStateException("Session is closed");
            } else if (listener == null) {
                throw new NullPointerException("listener must not be null");
            } else if ((p2 != 0x00) && (p2 != 0x04) && (p2 != 0x08)
                    && (p2 != (byte) 0x0C)) {
                throw new UnsupportedOperationException("p2 not supported: "
                        + String.format("%02x ", p2 & 0xFF));
            }

            String packageName = getPackageNameFromCallingUid(Binder.getCallingUid());
            Channel channel = null;

            try {
                channel = mReader.getTerminal().openLogicalChannel(this, aid, p2, listener,
                        packageName, Binder.getCallingPid());
            } catch (IOException e) {
                throw new ServiceSpecificException(SEService.IO_ERROR, e.getMessage());
            } catch (NoSuchElementException e) {
                throw new ServiceSpecificException(SEService.NO_SUCH_ELEMENT_ERROR, e.getMessage());
            }

            if (channel == null) {
                Log.i(mTag, "openLogicalChannel() - returning null");
                return null;
            }
            Log.i(mTag, "openLogicalChannel() Success. Channel: "
                    + channel.getChannelNumber());

            mChannels.add(channel);
            return channel.new SecureElementChannel();
        }
    }

    private final BroadcastReceiver mMultiSimConfigChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(TelephonyManager.ACTION_MULTI_SIM_CONFIG_CHANGED)) {
                int activeSimCount =
                        intent.getIntExtra(TelephonyManager.EXTRA_ACTIVE_SIM_SUPPORTED_COUNT, 1);
                Log.i(mTag, "received action MultiSimConfigChanged. Refresh UICC terminals");
                Log.i(mTag, "Current ActiveSimCount:" + activeSimCount
                        + ". Previous ActiveSimCount:" + mActiveSimCount);

                // Check for any change to UICC SE HAL implementations
                refreshUiccTerminals(activeSimCount);
            }
        }
    };
}
