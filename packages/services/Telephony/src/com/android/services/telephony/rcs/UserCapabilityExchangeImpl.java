/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.services.telephony.rcs;

import android.content.Context;
import android.net.Uri;
import android.os.RemoteException;
import android.telephony.ims.RcsContactUceCapability;
import android.telephony.ims.RcsUceAdapter;
import android.telephony.ims.aidl.IRcsUceControllerCallback;
import android.util.Log;

import com.android.ims.RcsFeatureManager;
import com.android.ims.ResultCode;
import com.android.phone.R;
import com.android.service.ims.presence.ContactCapabilityResponse;
import com.android.service.ims.presence.PresenceBase;
import com.android.service.ims.presence.PresencePublication;
import com.android.service.ims.presence.PresencePublisher;
import com.android.service.ims.presence.PresenceSubscriber;
import com.android.service.ims.presence.SubscribePublisher;

import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

/**
 * Implements User Capability Exchange using Presence.
 */
public class UserCapabilityExchangeImpl implements RcsFeatureController.Feature, SubscribePublisher,
        PresencePublisher {

    private static final String LOG_TAG = "RcsUceImpl";

    private int mSlotId;
    private int mSubId;

    private final PresencePublication mPresencePublication;
    private final PresenceSubscriber mPresenceSubscriber;

    private final ConcurrentHashMap<Integer, IRcsUceControllerCallback> mPendingCapabilityRequests =
            new ConcurrentHashMap<>();

    UserCapabilityExchangeImpl(Context context, int slotId, int subId) {
        mSlotId = slotId;
        mSubId = subId;
        logi("created");

        String[] volteError = context.getResources().getStringArray(
                R.array.config_volte_provision_error_on_publish_response);
        String[] rcsError = context.getResources().getStringArray(
                R.array.config_rcs_provision_error_on_publish_response);

        // Initialize PresencePublication
        mPresencePublication = new PresencePublication(null /*PresencePublisher*/, context,
                volteError, rcsError);
        // Initialize PresenceSubscriber
        mPresenceSubscriber = new PresenceSubscriber(null /*SubscribePublisher*/, context,
                volteError, rcsError);

        onAssociatedSubscriptionUpdated(mSubId);
    }


    // Runs on main thread.
    @Override
    public void onRcsConnected(RcsFeatureManager rcsFeatureManager) {
        logi("onRcsConnected");
        mPresencePublication.updatePresencePublisher(this);
        mPresenceSubscriber.updatePresenceSubscriber(this);
    }

    // Runs on main thread.
    @Override
    public void onRcsDisconnected() {
        logi("onRcsDisconnected");
        mPresencePublication.removePresencePublisher();
        mPresenceSubscriber.removePresenceSubscriber();
    }

    // Runs on main thread.
    @Override
    public void onAssociatedSubscriptionUpdated(int subId) {
        mPresencePublication.handleAssociatedSubscriptionChanged(subId);
        mPresenceSubscriber.handleAssociatedSubscriptionChanged(subId);
    }

    /**
     * Should be called before destroying this instance.
     * This instance is not usable after this method is called.
     */
    // Called on main thread.
    public void onDestroy() {
        onRcsDisconnected();
    }

    /**
     * @return the UCE Publish state.
     */
    // May happen on a Binder thread, PresencePublication locks to get result.
    public int getUcePublishState() {
        int publishState = mPresencePublication.getPublishState();
        return toUcePublishState(publishState);
    }

    /**
     * Perform a capabilities request and call {@link IRcsUceControllerCallback} with the result.
     */
    // May happen on a Binder thread, PresenceSubscriber locks when requesting Capabilities.
    public void requestCapabilities(List<Uri> contactNumbers, IRcsUceControllerCallback c) {
        List<String> numbers = contactNumbers.stream()
                .map(UserCapabilityExchangeImpl::getNumberFromUri).collect(Collectors.toList());
        int taskId = mPresenceSubscriber.requestCapability(numbers,
                new ContactCapabilityResponse() {
                    @Override
                    public void onSuccess(int reqId) {
                        logi("onSuccess called for reqId:" + reqId);
                    }

                    @Override
                    public void onError(int reqId, int resultCode) {
                        IRcsUceControllerCallback c = mPendingCapabilityRequests.remove(reqId);
                        try {
                            if (c != null) {
                                c.onError(toUceError(resultCode));
                            } else {
                                logw("onError called for unknown reqId:" + reqId);
                            }
                        } catch (RemoteException e) {
                            logi("Calling back to dead service");
                        }
                    }

                    @Override
                    public void onFinish(int reqId) {
                        logi("onFinish called for reqId:" + reqId);
                    }

                    @Override
                    public void onTimeout(int reqId) {
                        IRcsUceControllerCallback c = mPendingCapabilityRequests.remove(reqId);
                        try {
                            if (c != null) {
                                c.onError(RcsUceAdapter.ERROR_REQUEST_TIMEOUT);
                            } else {
                                logw("onTimeout called for unknown reqId:" + reqId);
                            }
                        } catch (RemoteException e) {
                            logi("Calling back to dead service");
                        }
                    }

                    @Override
                    public void onCapabilitiesUpdated(int reqId,
                            List<RcsContactUceCapability> contactCapabilities,
                            boolean updateLastTimestamp) {
                        IRcsUceControllerCallback c = mPendingCapabilityRequests.remove(reqId);
                        try {
                            if (c != null) {
                                c.onCapabilitiesReceived(contactCapabilities);
                            } else {
                                logw("onCapabilitiesUpdated, unknown reqId:" + reqId);
                            }
                        } catch (RemoteException e) {
                            logw("onCapabilitiesUpdated on dead service");
                        }
                    }
                });
        if (taskId < 0) {
            try {
                c.onError(toUceError(taskId));
                return;
            } catch (RemoteException e) {
                logi("Calling back to dead service");
            }
        }
        mPendingCapabilityRequests.put(taskId, c);
    }

    @Override
    public int getPublisherState() {
        return 0;
    }

    @Override
    public int requestPublication(RcsContactUceCapability capabilities, String contactUri,
            int taskId) {
        return 0;
    }

    @Override
    public int requestCapability(String[] formatedContacts, int taskId) {
        return 0;
    }

    @Override
    public int requestAvailability(String formattedContact, int taskId) {
        return 0;
    }

    @Override
    public int getStackStatusForCapabilityRequest() {
        return 0;
    }

    @Override
    public void updatePublisherState(int publishState) {

    }

    private static String getNumberFromUri(Uri uri) {
        String number = uri.getSchemeSpecificPart();
        String[] numberParts = number.split("[@;:]");

        if (numberParts.length == 0) {
            return null;
        }
        return numberParts[0];
    }

    private static int toUcePublishState(int publishState) {
        switch (publishState) {
            case PresenceBase.PUBLISH_STATE_200_OK:
                return RcsUceAdapter.PUBLISH_STATE_OK;
            case PresenceBase.PUBLISH_STATE_NOT_PUBLISHED:
                return RcsUceAdapter.PUBLISH_STATE_NOT_PUBLISHED;
            case PresenceBase.PUBLISH_STATE_VOLTE_PROVISION_ERROR:
                return RcsUceAdapter.PUBLISH_STATE_VOLTE_PROVISION_ERROR;
            case PresenceBase.PUBLISH_STATE_RCS_PROVISION_ERROR:
                return RcsUceAdapter.PUBLISH_STATE_RCS_PROVISION_ERROR;
            case PresenceBase.PUBLISH_STATE_REQUEST_TIMEOUT:
                return RcsUceAdapter.PUBLISH_STATE_REQUEST_TIMEOUT;
            case PresenceBase.PUBLISH_STATE_OTHER_ERROR:
                return RcsUceAdapter.PUBLISH_STATE_OTHER_ERROR;
            default:
                return RcsUceAdapter.PUBLISH_STATE_OTHER_ERROR;
        }
    }

    private static int toUceError(int resultCode) {
        switch (resultCode) {
            case ResultCode.SUBSCRIBE_NOT_REGISTERED:
                return RcsUceAdapter.ERROR_NOT_REGISTERED;
            case ResultCode.SUBSCRIBE_REQUEST_TIMEOUT:
                return RcsUceAdapter.ERROR_REQUEST_TIMEOUT;
            case ResultCode.SUBSCRIBE_FORBIDDEN:
                return RcsUceAdapter.ERROR_FORBIDDEN;
            case ResultCode.SUBSCRIBE_NOT_FOUND:
                return RcsUceAdapter.ERROR_NOT_FOUND;
            case ResultCode.SUBSCRIBE_TOO_LARGE:
                return RcsUceAdapter.ERROR_REQUEST_TOO_LARGE;
            case ResultCode.SUBSCRIBE_INSUFFICIENT_MEMORY:
                return RcsUceAdapter.ERROR_INSUFFICIENT_MEMORY;
            case ResultCode.SUBSCRIBE_LOST_NETWORK:
                return RcsUceAdapter.ERROR_LOST_NETWORK;
            case ResultCode.SUBSCRIBE_ALREADY_IN_QUEUE:
                return RcsUceAdapter.ERROR_ALREADY_IN_QUEUE;
            default:
                return RcsUceAdapter.ERROR_GENERIC_FAILURE;
        }
    }

    private void logi(String log) {
        Log.i(LOG_TAG, getLogPrefix().append(log).toString());
    }

    private void logw(String log) {
        Log.w(LOG_TAG, getLogPrefix().append(log).toString());
    }

    private StringBuilder getLogPrefix() {
        StringBuilder builder = new StringBuilder("[");
        builder.append(mSlotId);
        builder.append("->");
        builder.append(mSubId);
        builder.append("] ");
        return builder;
    }
}
