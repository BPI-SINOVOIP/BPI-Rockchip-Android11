/*
 * Copyright (c) 2015, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

package com.android.ims;

import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.telephony.Rlog;

import com.android.ims.internal.IRcsService;
import com.android.ims.internal.IRcsPresence;

import java.util.HashMap;

/**
 * Provides APIs for Rcs services, currently it supports presence only.
 * This class is the starting point for any RCS actions.
 * You can acquire an instance of it with {@link #getInstance getInstance()}.
 *
 * @hide
 */
public class RcsManager {
    /**
     * For accessing the RCS related service.
     * Internal use only.
     *
     * @hide
     */
    private static final String RCS_SERVICE = "rcs";

    /**
     * Part of the ACTION_RCS_SERVICE_AVAILABLE and ACTION_RCS_SERVICE_UNAVAILABLE intents.
     * A long value; the subId corresponding to the RCS service. For MSIM implementation.
     *
     * @see #ACTION_RCS_SERVICE_AVAILABLE
     * @see #ACTION_RCS_SERVICE_UNAVAILABLE
     */
    public static final String EXTRA_SUBID = "android:subid";

    /**
     * Action to broadcast when RcsService is available.
     *
     * @see #EXTRA_SUBID
     */
    public static final String ACTION_RCS_SERVICE_AVAILABLE =
            "com.android.ims.ACTION_RCS_SERVICE_AVAILABLE";

    /**
     * Action to broadcast when RcsService is unavailable (such as ims is not registered).
     *
     * @see #EXTRA_SUBID
     */
    public static final String ACTION_RCS_SERVICE_UNAVAILABLE =
            "com.android.ims.ACTION_RCS_SERVICE_UNAVAILABLE";

    /**
     * Action to broadcast when RcsService is died.
     * The caller can listen to the intent to clean the pending request.
     *
     * It takes the extra parameter subid as well since it depends on OEM implementation for
     * RcsService. It will not send broadcast for ACTION_RCS_SERVICE_UNAVAILABLE under the case.
     *
     * @see #EXTRA_SUBID
     */
    public static final String ACTION_RCS_SERVICE_DIED =
            "com.android.ims.ACTION_RCS_SERVICE_DIED";

    ;

    private static final String TAG = "RcsManager";
    private static final boolean DBG = true;

    private static HashMap<Integer, RcsManager> sRcsManagerInstances =
            new HashMap<Integer, RcsManager>();

    private Context mContext;
    private int mSubId;
    private IRcsService mRcsService = null;
    private RcsServiceDeathRecipient mDeathRecipient = new RcsServiceDeathRecipient();

    // Interface for presence
    // TODO: Could add other RCS service such RcsChat, RcsFt later.
    private RcsPresence  mRcsPresence = null;

    /**
     * Gets a manager instance.
     *
     * @param context application context for creating the manager object
     * @param subId the subscription ID for the RCS Service
     * @return the manager instance corresponding to the subId
     */
    public static RcsManager getInstance(Context context, int subId) {
        synchronized (sRcsManagerInstances) {
            if (sRcsManagerInstances.containsKey(subId)){
                return sRcsManagerInstances.get(subId);
            }

            RcsManager mgr = new RcsManager(context, subId);
            sRcsManagerInstances.put(subId, mgr);

            return mgr;
        }
    }

    private RcsManager(Context context, int subId) {
        mContext = context;
        mSubId = subId;
        createRcsService(true);
    }

    /**
     * return true if the rcs service is ready for use.
     */
    public boolean isRcsServiceAvailable() {
        if (DBG) Rlog.d(TAG, "call isRcsServiceAvailable ...");

        boolean ret = false;

        try {
            checkAndThrowExceptionIfServiceUnavailable();

            ret = mRcsService.isRcsServiceAvailable();
        }  catch (RemoteException e) {
            // return false under the case.
            Rlog.e(TAG, "isRcsServiceAvailable RemoteException", e);
        }catch (RcsException e){
            // return false under the case.
            Rlog.e(TAG, "isRcsServiceAvailable RcsException", e);
        }

        if (DBG) Rlog.d(TAG, "isRcsServiceAvailable ret =" + ret);
        return ret;
    }

    /**
     * Gets the presence interface
     *
     * @return the RcsPresence instance.
     * @throws  if getting the RcsPresence interface results in an error.
     */
    public RcsPresence getRcsPresenceInterface() throws RcsException {

        if (mRcsPresence == null) {
            checkAndThrowExceptionIfServiceUnavailable();

            try {
                IRcsPresence rcsPresence = mRcsService.getRcsPresenceInterface();
                if (rcsPresence == null) {
                    throw new RcsException("getRcsPresenceInterface()",
                            ResultCode.ERROR_SERVICE_NOT_AVAILABLE);
                }
                mRcsPresence = new RcsPresence(rcsPresence);
            } catch (RemoteException e) {
                throw new RcsException("getRcsPresenceInterface()", e,
                        ResultCode.ERROR_SERVICE_NOT_AVAILABLE);
            }
        }
        if (DBG) Rlog.d(TAG, "getRcsPresenceInterface(), mRcsPresence= " + mRcsPresence);
        return mRcsPresence;
    }

    /**
     * Binds the RCS service only if the service is not created.
     */
    private void checkAndThrowExceptionIfServiceUnavailable()
            throws  RcsException {
        if (mRcsService == null) {
            createRcsService(true);

            if (mRcsService == null) {
                throw new RcsException("Service is unavailable",
                        ResultCode.ERROR_SERVICE_NOT_AVAILABLE);
            }
        }
    }

    private static String getRcsServiceName(int subId) {
        // use the same mechanism as IMS_SERVICE?
        return RCS_SERVICE;
    }

    /**
     * Binds the RCS service.
     */
    private void createRcsService(boolean checkService) {
        if (checkService) {
            IBinder binder = ServiceManager.checkService(getRcsServiceName(mSubId));

            if (binder == null) {
                return;
            }
        }

        IBinder b = ServiceManager.getService(getRcsServiceName(mSubId));

        if (b != null) {
            try {
                b.linkToDeath(mDeathRecipient, 0);
            } catch (RemoteException e) {
            }
        }

        mRcsService = IRcsService.Stub.asInterface(b);
    }

    /**
     * Death recipient class for monitoring RCS service.
     */
    private class RcsServiceDeathRecipient implements IBinder.DeathRecipient {
        @Override
        public void binderDied() {
            mRcsService = null;
            mRcsPresence = null;

            if (mContext != null) {
                Intent intent = new Intent(ACTION_RCS_SERVICE_DIED);
                intent.putExtra(EXTRA_SUBID, mSubId);
                mContext.sendBroadcast(new Intent(intent));
            }
        }
    }
}
