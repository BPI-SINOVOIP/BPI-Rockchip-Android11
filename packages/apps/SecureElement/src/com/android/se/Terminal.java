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
 * Copyright (c) 2015-2017, The Linux Foundation.
 */

/*
 * Contributed by: Giesecke & Devrient GmbH.
 */

package com.android.se;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.hardware.secure_element.V1_0.ISecureElement;
import android.hardware.secure_element.V1_0.ISecureElementHalCallback;
import android.hardware.secure_element.V1_0.LogicalChannelResponse;
import android.hardware.secure_element.V1_0.SecureElementStatus;
import android.os.Build;
import android.os.Handler;
import android.os.HwBinder;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.se.omapi.ISecureElementListener;
import android.se.omapi.ISecureElementReader;
import android.se.omapi.ISecureElementSession;
import android.se.omapi.SEService;
import android.util.Log;

import com.android.se.SecureElementService.SecureElementSession;
import com.android.se.internal.ByteArrayConverter;
import com.android.se.security.AccessControlEnforcer;
import com.android.se.security.ChannelAccess;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.MissingResourceException;
import java.util.NoSuchElementException;

/**
 * Each Terminal represents a Secure Element.
 * Communicates to the SE via SecureElement HAL.
 */
public class Terminal {

    private final String mTag;
    private final Map<Integer, Channel> mChannels = new HashMap<Integer, Channel>();
    private final Object mLock = new Object();
    private final String mName;
    public boolean mIsConnected = false;
    private Context mContext;
    private boolean mDefaultApplicationSelectedOnBasicChannel = true;

    private static final boolean DEBUG = Build.IS_DEBUGGABLE;
    private static final int GET_SERVICE_DELAY_MILLIS = 4 * 1000;
    private static final int EVENT_GET_HAL = 1;

    private final int mMaxGetHalRetryCount = 5;
    private int mGetHalRetryCount = 0;

    private ISecureElement mSEHal;
    private android.hardware.secure_element.V1_2.ISecureElement mSEHal12;

    /** For each Terminal there will be one AccessController object. */
    private AccessControlEnforcer mAccessControlEnforcer;

    private static final String SECURE_ELEMENT_PRIVILEGED_OPERATION_PERMISSION =
            "android.permission.SECURE_ELEMENT_PRIVILEGED_OPERATION";

    public static final byte[] ISD_R_AID =
            new byte[]{
                    (byte) 0xA0,
                    (byte) 0x00,
                    (byte) 0x00,
                    (byte) 0x05,
                    (byte) 0x59,
                    (byte) 0x10,
                    (byte) 0x10,
                    (byte) 0xFF,
                    (byte) 0xFF,
                    (byte) 0xFF,
                    (byte) 0xFF,
                    (byte) 0x89,
                    (byte) 0x00,
                    (byte) 0x00,
                    (byte) 0x01,
                    (byte) 0x00,
            };

    private ISecureElementHalCallback.Stub mHalCallback = new ISecureElementHalCallback.Stub() {
        @Override
        public void onStateChange(boolean state) {
            stateChange(state, "");
        }
    };

    private android.hardware.secure_element.V1_1.ISecureElementHalCallback.Stub mHalCallback11 =
            new android.hardware.secure_element.V1_1.ISecureElementHalCallback.Stub() {
        @Override
        public void onStateChange_1_1(boolean state, String reason) {
            stateChange(state, reason);
        }

        public void onStateChange(boolean state) {
            return;
        }
    };

    private void stateChange(boolean state, String reason) {
        synchronized (mLock) {
            Log.i(mTag, "OnStateChange:" + state + " reason:" + reason);
            mIsConnected = state;
            if (!state) {
                if (mAccessControlEnforcer != null) {
                    mAccessControlEnforcer.reset();
                }
                SecureElementStatsLog.write(
                        SecureElementStatsLog.SE_STATE_CHANGED,
                        SecureElementStatsLog.SE_STATE_CHANGED__STATE__DISCONNECTED,
                        reason,
                        mName);
            } else {
                // If any logical channel in use is in the channel list, it should be closed
                // because the access control enfocer allowed to open it by checking the access
                // rules retrieved before. Now we are going to retrieve the rules again and
                // the new rules can be different from the previous ones.
                closeChannels();
                try {
                    initializeAccessControl();
                } catch (Exception e) {
                    // ignore
                }
                mDefaultApplicationSelectedOnBasicChannel = true;
                SecureElementStatsLog.write(
                        SecureElementStatsLog.SE_STATE_CHANGED,
                        SecureElementStatsLog.SE_STATE_CHANGED__STATE__CONNECTED,
                        reason,
                        mName);
            }
        }
    }

    class SecureElementDeathRecipient implements HwBinder.DeathRecipient {
        @Override
        public void serviceDied(long cookie) {
            Log.e(mTag, mName + " died");
            SecureElementStatsLog.write(
                    SecureElementStatsLog.SE_STATE_CHANGED,
                    SecureElementStatsLog.SE_STATE_CHANGED__STATE__HALCRASH,
                    "HALCRASH",
                    mName);
            synchronized (mLock) {
                mIsConnected = false;
                if (mAccessControlEnforcer != null) {
                    mAccessControlEnforcer.reset();
                }
            }
            mHandler.sendMessageDelayed(mHandler.obtainMessage(EVENT_GET_HAL, 0),
                    GET_SERVICE_DELAY_MILLIS);
        }
    }

    private HwBinder.DeathRecipient mDeathRecipient = new SecureElementDeathRecipient();

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message message) {
            switch (message.what) {
                case EVENT_GET_HAL:
                    try {
                        if (mName.startsWith(SecureElementService.ESE_TERMINAL)) {
                            initialize(true);
                        } else {
                            initialize(false);
                        }
                    } catch (Exception e) {
                        Log.e(mTag, mName + " could not be initialized again");
                        if (mGetHalRetryCount < mMaxGetHalRetryCount) {
                            mGetHalRetryCount++;
                            sendMessageDelayed(obtainMessage(EVENT_GET_HAL, 0),
                                    GET_SERVICE_DELAY_MILLIS);
                        } else {
                            Log.e(mTag, mName + " reach maximum retry count");
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    };

    public Terminal(String name, Context context) {
        mContext = context;
        mName = name;
        mTag = "SecureElement-Terminal-" + getName();
    }

    /**
     * Initializes the terminal
     *
     * @throws NoSuchElementException if there is no HAL implementation for the specified SE name
     * @throws RemoteException if there is a failure communicating with the remote
     */
    public void initialize(boolean retryOnFail) throws NoSuchElementException, RemoteException {
        android.hardware.secure_element.V1_1.ISecureElement mSEHal11 = null;
        synchronized (mLock) {
            try {
                mSEHal = mSEHal11 = mSEHal12 =
                        android.hardware.secure_element.V1_2.ISecureElement.getService(mName,
                                                                                       retryOnFail);
            } catch (Exception e) {
                Log.d(mTag, "SE Hal V1.2 is not supported");
            }
            if (mSEHal12 == null) {
                try {
                    mSEHal = mSEHal11 =
                            android.hardware.secure_element.V1_1.ISecureElement.getService(mName,
                                    retryOnFail);
                } catch (Exception e) {
                    Log.d(mTag, "SE Hal V1.1 is not supported");
                }

                if (mSEHal11 == null) {
                    mSEHal = ISecureElement.getService(mName, retryOnFail);
                    if (mSEHal == null) {
                        throw new NoSuchElementException("No HAL is provided for " + mName);
                    }
                }
            }
            if (mSEHal11 != null || mSEHal12 != null) {
                mSEHal11.init_1_1(mHalCallback11);
            } else {
                mSEHal.init(mHalCallback);
            }
            mSEHal.linkToDeath(mDeathRecipient, 0);
        }
        Log.i(mTag, mName + " was initialized");
        SecureElementStatsLog.write(
                SecureElementStatsLog.SE_STATE_CHANGED,
                SecureElementStatsLog.SE_STATE_CHANGED__STATE__INITIALIZED,
                "INIT",
                mName);
    }

    private ArrayList<Byte> byteArrayToArrayList(byte[] array) {
        ArrayList<Byte> list = new ArrayList<Byte>();
        if (array == null) {
            return list;
        }

        for (Byte b : array) {
            list.add(b);
        }
        return list;
    }

    private byte[] arrayListToByteArray(ArrayList<Byte> list) {
        Byte[] byteArray = list.toArray(new Byte[list.size()]);
        int i = 0;
        byte[] result = new byte[list.size()];
        for (Byte b : byteArray) {
            result[i++] = b.byteValue();
        }
        return result;
    }

    /**
     * Closes the given channel
     */
    public void closeChannel(Channel channel) {
        if (channel == null) {
            return;
        }
        synchronized (mLock) {
            if (mIsConnected) {
                try {
                    byte status = mSEHal.closeChannel((byte) channel.getChannelNumber());
                    /* For Basic Channels, errors are expected.
                     * Underlying implementations use this call as an indication when there
                     * aren't any users actively using the channel, and the chip can go
                     * into low power state.
                     */
                    if (!channel.isBasicChannel() && status != SecureElementStatus.SUCCESS) {
                        Log.e(mTag, "Error closing channel " + channel.getChannelNumber());
                    }
                } catch (RemoteException e) {
                    Log.e(mTag, "Exception in closeChannel() " + e);
                }
            }
            mChannels.remove(channel.getChannelNumber(), channel);
            if (mChannels.get(channel.getChannelNumber()) != null) {
                Log.e(mTag, "Removing channel failed");
            }
        }
    }

    /**
     * Cleans up all the channels in use.
     */
    public synchronized void closeChannels() {
        Collection<Channel> col = mChannels.values();
        Channel[] channelList = col.toArray(new Channel[col.size()]);
        for (Channel channel : channelList) {
            channel.close();
        }
    }

    /**
     * Closes the terminal.
     */
    public void close() {
        synchronized (mLock) {
            if (mSEHal != null) {
                try {
                    mSEHal.unlinkToDeath(mDeathRecipient);
                } catch (RemoteException e) {
                    // ignore
                }
            }
        }
    }

    public String getName() {
        return mName;
    }

    /**
     * Returns the ATR of the Secure Element, or null if not available.
     */
    public byte[] getAtr() {
        if (!mIsConnected) {
            return null;
        }

        try {
            ArrayList<Byte> responseList = mSEHal.getAtr();
            if (responseList.isEmpty()) {
                return null;
            }
            byte[] atr = arrayListToByteArray(responseList);
            if (DEBUG) {
                Log.i(mTag, "ATR : " + ByteArrayConverter.byteArrayToHexString(atr));
            }
            return atr;
        } catch (RemoteException e) {
            Log.e(mTag, "Exception in getAtr()" + e);
            return null;
        }
    }

    /**
     * Selects the default application on the basic channel.
     *
     * If there is an exception selecting the default application, select
     * is performed with the default access control aid.
     */
    public void selectDefaultApplication() {
        try {
            select(null);
        } catch (NoSuchElementException e) {
            if (getAccessControlEnforcer() != null) {
                try {
                    select(mAccessControlEnforcer.getDefaultAccessControlAid());
                } catch (Exception ignore) {
                }
            }
        } catch (Exception ignore) {
        }
    }

    private void select(byte[] aid) throws IOException {
        int commandSize = (aid == null ? 0 : aid.length) + 5;
        byte[] selectCommand = new byte[commandSize];
        selectCommand[0] = 0x00;
        selectCommand[1] = (byte) 0xA4;
        selectCommand[2] = 0x04;
        selectCommand[3] = 0x00;
        if (aid != null && aid.length != 0) {
            selectCommand[4] = (byte) aid.length;
            System.arraycopy(aid, 0, selectCommand, 5, aid.length);
        } else {
            selectCommand[4] = 0x00;
        }
        byte[] selectResponse = transmit(selectCommand);
        if (selectResponse.length < 2) {
            selectResponse = null;
            throw new NoSuchElementException("Response length is too small");
        }
        int sw1 = selectResponse[selectResponse.length - 2] & 0xFF;
        int sw2 = selectResponse[selectResponse.length - 1] & 0xFF;
        if (sw1 != 0x90 || sw2 != 0x00) {
            selectResponse = null;
            throw new NoSuchElementException("Status word is incorrect");
        }
    }

    /**
     * Opens a Basic Channel with the given AID and P2 paramters
     */
    public Channel openBasicChannel(SecureElementSession session, byte[] aid, byte p2,
            ISecureElementListener listener, String packageName, int pid) throws IOException,
            NoSuchElementException {
        if (aid != null && aid.length == 0) {
            aid = null;
        } else if (aid != null && (aid.length < 5 || aid.length > 16)) {
            throw new IllegalArgumentException("AID out of range");
        } else if (!mIsConnected) {
            throw new IOException("Secure Element is not connected");
        }

        ChannelAccess channelAccess = null;
        if (packageName != null) {
            Log.w(mTag, "Enable access control on basic channel for " + packageName);
            SecureElementStatsLog.write(
                    SecureElementStatsLog.SE_OMAPI_REPORTED,
                    SecureElementStatsLog.SE_OMAPI_REPORTED__OPERATION__OPEN_CHANNEL,
                    mName,
                    packageName);
            try {
                // For application without privilege permission or carrier privilege,
                // openBasicChannel with UICC terminals should be rejected.
                channelAccess = setUpChannelAccess(aid, packageName, pid, true);
            } catch (MissingResourceException e) {
                return null;
            }
        }

        synchronized (mLock) {
            if (mChannels.get(0) != null) {
                Log.e(mTag, "basic channel in use");
                return null;
            }
            if (aid == null && !mDefaultApplicationSelectedOnBasicChannel) {
                Log.e(mTag, "default application is not selected");
                return null;
            }

            ArrayList<byte[]> responseList = new ArrayList<byte[]>();
            byte[] status = new byte[1];

            try {
                mSEHal.openBasicChannel(byteArrayToArrayList(aid), p2,
                        new ISecureElement.openBasicChannelCallback() {
                            @Override
                            public void onValues(ArrayList<Byte> responseObject, byte halStatus) {
                                status[0] = halStatus;
                                responseList.add(arrayListToByteArray(responseObject));
                                return;
                            }
                        });
            } catch (RemoteException e) {
                throw new IOException(e.getMessage());
            }

            byte[] selectResponse = responseList.get(0);
            if (status[0] == SecureElementStatus.CHANNEL_NOT_AVAILABLE) {
                return null;
            } else if (status[0] == SecureElementStatus.UNSUPPORTED_OPERATION) {
                throw new UnsupportedOperationException("OpenBasicChannel() failed");
            } else if (status[0] == SecureElementStatus.IOERROR) {
                throw new IOException("OpenBasicChannel() failed");
            } else if (status[0] == SecureElementStatus.NO_SUCH_ELEMENT_ERROR) {
                throw new NoSuchElementException("OpenBasicChannel() failed");
            }

            Channel basicChannel = new Channel(session, this, 0, selectResponse, aid,
                    listener);
            basicChannel.setChannelAccess(channelAccess);

            if (aid != null) {
                mDefaultApplicationSelectedOnBasicChannel = false;
            }
            mChannels.put(0, basicChannel);
            return basicChannel;
        }
    }

    /**
     * Opens a logical Channel without Channel Access initialization.
     */
    public Channel openLogicalChannelWithoutChannelAccess(byte[] aid) throws IOException,
            NoSuchElementException {
        return openLogicalChannel(null, aid, (byte) 0x00, null, null, 0);
    }

    /**
     * Opens a logical Channel with AID.
     */
    public Channel openLogicalChannel(SecureElementSession session, byte[] aid, byte p2,
            ISecureElementListener listener, String packageName, int pid) throws IOException,
            NoSuchElementException {
        if (aid != null && aid.length == 0) {
            aid = null;
        } else if (aid != null && (aid.length < 5 || aid.length > 16)) {
            throw new IllegalArgumentException("AID out of range");
        } else if (!mIsConnected) {
            throw new IOException("Secure Element is not connected");
        }

        ChannelAccess channelAccess = null;
        if (packageName != null) {
            Log.w(mTag, "Enable access control on logical channel for " + packageName);
            SecureElementStatsLog.write(
                    SecureElementStatsLog.SE_OMAPI_REPORTED,
                    SecureElementStatsLog.SE_OMAPI_REPORTED__OPERATION__OPEN_CHANNEL,
                    mName,
                    packageName);
            try {
                channelAccess = setUpChannelAccess(aid, packageName, pid, false);
            } catch (MissingResourceException | UnsupportedOperationException e) {
                return null;
            }
        }

        synchronized (mLock) {
            LogicalChannelResponse[] responseArray = new LogicalChannelResponse[1];
            byte[] status = new byte[1];

            try {
                mSEHal.openLogicalChannel(byteArrayToArrayList(aid), p2,
                        new ISecureElement.openLogicalChannelCallback() {
                            @Override
                            public void onValues(LogicalChannelResponse response, byte halStatus) {
                                status[0] = halStatus;
                                responseArray[0] = response;
                                return;
                            }
                        });
            } catch (RemoteException e) {
                throw new IOException(e.getMessage());
            }

            if (status[0] == SecureElementStatus.CHANNEL_NOT_AVAILABLE) {
                return null;
            } else if (status[0] == SecureElementStatus.UNSUPPORTED_OPERATION) {
                throw new UnsupportedOperationException("OpenLogicalChannel() failed");
            } else if (status[0] == SecureElementStatus.IOERROR) {
                throw new IOException("OpenLogicalChannel() failed");
            } else if (status[0] == SecureElementStatus.NO_SUCH_ELEMENT_ERROR) {
                throw new NoSuchElementException("OpenLogicalChannel() failed");
            }
            if (responseArray[0].channelNumber <= 0 || status[0] != SecureElementStatus.SUCCESS) {
                return null;
            }
            int channelNumber = responseArray[0].channelNumber;
            byte[] selectResponse = arrayListToByteArray(responseArray[0].selectResponse);
            Channel logicalChannel = new Channel(session, this, channelNumber,
                    selectResponse, aid, listener);
            logicalChannel.setChannelAccess(channelAccess);

            mChannels.put(channelNumber, logicalChannel);
            return logicalChannel;
        }
    }

    /**
     * Returns true if the given AID can be selected on the Terminal
     */
    public boolean isAidSelectable(byte[] aid) {
        if (aid == null) {
            throw new NullPointerException("aid must not be null");
        } else if (!mIsConnected) {
            Log.e(mTag, "Secure Element is not connected");
            return false;
        }

        synchronized (mLock) {
            LogicalChannelResponse[] responseArray = new LogicalChannelResponse[1];
            byte[] status = new byte[1];
            try {
                mSEHal.openLogicalChannel(byteArrayToArrayList(aid), (byte) 0x00,
                        new ISecureElement.openLogicalChannelCallback() {
                            @Override
                            public void onValues(LogicalChannelResponse response, byte halStatus) {
                                status[0] = halStatus;
                                responseArray[0] = response;
                                return;
                            }
                        });
                if (status[0] == SecureElementStatus.SUCCESS) {
                    mSEHal.closeChannel(responseArray[0].channelNumber);
                    return true;
                }
                return false;
            } catch (RemoteException e) {
                Log.e(mTag, "Error in isAidSelectable() returning false" + e);
                return false;
            }
        }
    }

    /**
     * Transmits the specified command and returns the response.
     *
     * @param cmd the command APDU to be transmitted.
     * @return the response received.
     */
    public byte[] transmit(byte[] cmd) throws IOException {
        if (!mIsConnected) {
            Log.e(mTag, "Secure Element is not connected");
            throw new IOException("Secure Element is not connected");
        }

        byte[] rsp = transmitInternal(cmd);
        int sw1 = rsp[rsp.length - 2] & 0xFF;
        int sw2 = rsp[rsp.length - 1] & 0xFF;

        if (sw1 == 0x6C) {
            cmd[cmd.length - 1] = rsp[rsp.length - 1];
            rsp = transmit(cmd);
        } else if (sw1 == 0x61) {
            do {
                byte[] getResponseCmd = new byte[]{
                        cmd[0], (byte) 0xC0, 0x00, 0x00, (byte) sw2
                };
                byte[] tmp = transmitInternal(getResponseCmd);
                byte[] aux = rsp;
                rsp = new byte[aux.length + tmp.length - 2];
                System.arraycopy(aux, 0, rsp, 0, aux.length - 2);
                System.arraycopy(tmp, 0, rsp, aux.length - 2, tmp.length);
                sw1 = rsp[rsp.length - 2] & 0xFF;
                sw2 = rsp[rsp.length - 1] & 0xFF;
            } while (sw1 == 0x61);
        }
        return rsp;
    }

    private byte[] transmitInternal(byte[] cmd) throws IOException {
        ArrayList<Byte> response;
        try {
            response = mSEHal.transmit(byteArrayToArrayList(cmd));
        } catch (RemoteException e) {
            throw new IOException(e.getMessage());
        }
        if (response.isEmpty()) {
            throw new IOException("Error in transmit()");
        }
        byte[] rsp = arrayListToByteArray(response);
        if (DEBUG) {
            Log.i(mTag, "Sent : " + ByteArrayConverter.byteArrayToHexString(cmd));
            Log.i(mTag, "Received : " + ByteArrayConverter.byteArrayToHexString(rsp));
        }
        return rsp;
    }

    /**
     * Checks if the application is authorized to receive the transaction event.
     */
    public boolean[] isNfcEventAllowed(PackageManager packageManager, byte[] aid,
            String[] packageNames) {
        // Attempt to initialize the access control enforcer if it failed in the previous attempt
        // due to a kind of temporary failure or no rule was found.
        if (mAccessControlEnforcer == null || mAccessControlEnforcer.isNoRuleFound()) {
            try {
                initializeAccessControl();
                // Just finished to initialize the access control enforcer.
                // It is too much to check the refresh tag in this case.
            } catch (Exception e) {
                Log.i(mTag, "isNfcEventAllowed Exception: " + e.getMessage());
                return null;
            }
        }
        mAccessControlEnforcer.setPackageManager(packageManager);

        synchronized (mLock) {
            try {
                return mAccessControlEnforcer.isNfcEventAllowed(aid, packageNames);
            } catch (Exception e) {
                Log.i(mTag, "isNfcEventAllowed Exception: " + e.getMessage());
                return null;
            }
        }
    }

    /**
     * Returns true if the Secure Element is present
     */
    public boolean isSecureElementPresent() {
        try {
            return mSEHal.isCardPresent();
        } catch (RemoteException e) {
            Log.e(mTag, "Error in isSecureElementPresent() " + e);
            return false;
        }
    }

    /**
     * Reset the Secure Element. Return true if success, false otherwise.
     */
    public boolean reset() {
        if (mSEHal12 == null) {
            return false;
        }
        mContext.enforceCallingOrSelfPermission(
                android.Manifest.permission.SECURE_ELEMENT_PRIVILEGED_OPERATION,
                "Need SECURE_ELEMENT_PRIVILEGED_OPERATION permission");

        try {
            byte status = mSEHal12.reset();
            // Successfully trigger reset. HAL service should send onStateChange
            // after secure element reset and initialization process complete
            if (status == SecureElementStatus.SUCCESS) {
                return true;
            }
            Log.e(mTag, "Error reseting terminal " + mName);
        } catch (RemoteException e) {
            Log.e(mTag, "Exception in reset()" + e);
        }
        return false;
    }

    /**
     * Initialize the Access Control and set up the channel access.
     */
    private ChannelAccess setUpChannelAccess(byte[] aid, String packageName, int pid,
            boolean isBasicChannel) throws IOException, MissingResourceException {
        boolean checkRefreshTag = true;
        if (isPrivilegedApplication(packageName)) {
            return ChannelAccess.getPrivilegeAccess(packageName, pid);
        }
        // Attempt to initialize the access control enforcer if it failed
        // due to a kind of temporary failure or no rule was found in the previous attempt.
        // For privilege access, do not attempt to initialize the access control enforcer
        // if no rule was found in the previous attempt.
        if (mAccessControlEnforcer == null || mAccessControlEnforcer.isNoRuleFound()) {
            initializeAccessControl();
            // Just finished to initialize the access control enforcer.
            // It is too much to check the refresh tag in this case.
            checkRefreshTag = false;
        }
        mAccessControlEnforcer.setPackageManager(mContext.getPackageManager());

        // Check carrier privilege when AID is not ISD-R
        if (getName().startsWith(SecureElementService.UICC_TERMINAL)
                && !Arrays.equals(aid, ISD_R_AID)) {
            try {
                PackageManager pm = mContext.getPackageManager();
                if (pm != null) {
                    PackageInfo pkgInfo =
                            pm.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
                    // Do not check the refresh tag for carrier privilege
                    if (mAccessControlEnforcer.checkCarrierPrivilege(pkgInfo, false)) {
                        Log.i(mTag, "setUp PrivilegeAccess for CarrierPrivilegeApplication. ");
                        return ChannelAccess.getCarrierPrivilegeAccess(packageName, pid);
                    }
                }
            } catch (NameNotFoundException ne) {
                Log.e(mTag, "checkCarrierPrivilege(): packageInfo is not found. ");
            } catch (Exception e) {
                Log.e(mTag, "checkCarrierPrivilege() Exception: " + e.getMessage());
            }
            if (isBasicChannel) {
                throw new MissingResourceException("openBasicChannel is not allowed.", "", "");
            } else if (aid == null) {
                // openLogicalChannel with null aid is only allowed for privilege applications
                throw new UnsupportedOperationException(
                        "null aid is not accepted in UICC terminal.");
            }
        }

        synchronized (mLock) {
            try {
                ChannelAccess channelAccess =
                        mAccessControlEnforcer.setUpChannelAccess(aid, packageName,
                                checkRefreshTag);
                channelAccess.setCallingPid(pid);
                return channelAccess;
            } catch (IOException | MissingResourceException e) {
                throw e;
            } catch (Exception e) {
                throw new SecurityException("Exception in setUpChannelAccess()" + e);
            }
        }
    }

    /**
     * Initializes the Access Control for this Terminal
     */
    private synchronized void initializeAccessControl() throws IOException,
            MissingResourceException {
        synchronized (mLock) {
            if (mAccessControlEnforcer == null) {
                mAccessControlEnforcer = new AccessControlEnforcer(this);
            }
            try {
                mAccessControlEnforcer.initialize();
            } catch (IOException | MissingResourceException e) {
                // Retrieving access rules failed because of an IO error happened between
                // the terminal and the secure element or the lack of a logical channel available.
                // It might be a temporary failure, so the terminal shall attempt to cache
                // the access rules again later.
                mAccessControlEnforcer = null;
                throw e;
            }
        }
    }

    /**
     * Checks if Secure Element Privilege permission exists for the given package
     */
    private boolean isPrivilegedApplication(String packageName) {
        PackageManager pm = mContext.getPackageManager();
        if (pm != null) {
            return (pm.checkPermission(SECURE_ELEMENT_PRIVILEGED_OPERATION_PERMISSION,
                    packageName) == PackageManager.PERMISSION_GRANTED);
        }
        return false;
    }

    public AccessControlEnforcer getAccessControlEnforcer() {
        return mAccessControlEnforcer;
    }

    public Context getContext() {
        return mContext;
    }

    /**
     * Checks if Carrier Privilege exists for the given package
     */
    public boolean checkCarrierPrivilegeRules(PackageInfo pInfo) {
        boolean checkRefreshTag = true;
        if (mAccessControlEnforcer == null || mAccessControlEnforcer.isNoRuleFound()) {
            try {
                initializeAccessControl();
            } catch (IOException e) {
                return false;
            }
            checkRefreshTag = false;
        }
        mAccessControlEnforcer.setPackageManager(mContext.getPackageManager());

        synchronized (mLock) {
            try {
                return mAccessControlEnforcer.checkCarrierPrivilege(pInfo, checkRefreshTag);
            } catch (Exception e) {
                Log.i(mTag, "checkCarrierPrivilege() Exception: " + e.getMessage());
                return false;
            }
        }
    }

    /** Dump data for debug purpose . */
    public void dump(PrintWriter writer) {
        writer.println("SECURE ELEMENT SERVICE TERMINAL: " + mName);
        writer.println();

        writer.println("mIsConnected:" + mIsConnected);
        writer.println();

        /* Dump the list of currunlty openned channels */
        writer.println("List of open channels:");

        for (Channel channel : mChannels.values()) {
            writer.println("channel " + channel.getChannelNumber() + ": ");
            writer.println("package: " + channel.getChannelAccess().getPackageName());
            writer.println("pid: " + channel.getChannelAccess().getCallingPid());
            writer.println("aid selected: " + channel.hasSelectedAid());
            writer.println("basic channel: " + channel.isBasicChannel());
            writer.println();
        }
        writer.println();

        /* Dump ACE data */
        if (mAccessControlEnforcer != null) {
            mAccessControlEnforcer.dump(writer);
        }
    }

    // Implementation of the SecureElement Reader interface according to OMAPI.
    final class SecureElementReader extends ISecureElementReader.Stub {

        private final SecureElementService mService;
        private final ArrayList<SecureElementSession> mSessions =
                new ArrayList<SecureElementSession>();

        SecureElementReader(SecureElementService service) {
            mService = service;
        }

        public byte[] getAtr() {
            return Terminal.this.getAtr();
        }

        @Override
        public boolean isSecureElementPresent() throws RemoteException {
            return Terminal.this.isSecureElementPresent();
        }

        @Override
        public void closeSessions() {
            synchronized (mLock) {
                while (mSessions.size() > 0) {
                    try {
                        mSessions.get(0).close();
                    } catch (Exception ignore) {
                    }
                }
                mSessions.clear();
            }
        }

        public void removeSession(SecureElementSession session) {
            if (session == null) {
                throw new NullPointerException("session is null");
            }
            mSessions.remove(session);
            synchronized (mLock) {
                if (mSessions.size() == 0) {
                    mDefaultApplicationSelectedOnBasicChannel = true;
                }
            }
        }

        @Override
        public ISecureElementSession openSession() throws RemoteException {
            if (!isSecureElementPresent()) {
                throw new ServiceSpecificException(SEService.IO_ERROR,
                        "Secure Element is not present.");
            }

            synchronized (mLock) {
                SecureElementSession session = mService.new SecureElementSession(this);
                mSessions.add(session);
                return session;
            }
        }

        Terminal getTerminal() {
            return Terminal.this;
        }

        @Override
        public boolean reset() {
            return Terminal.this.reset();
        }
    }
}
