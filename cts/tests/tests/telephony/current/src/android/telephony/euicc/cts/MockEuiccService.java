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

package android.telephony.euicc.cts;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.os.Bundle;
import android.service.euicc.DownloadSubscriptionResult;
import android.service.euicc.EuiccService;
import android.service.euicc.GetDefaultDownloadableSubscriptionListResult;
import android.service.euicc.GetDownloadableSubscriptionMetadataResult;
import android.service.euicc.GetEuiccProfileInfoListResult;
import android.telephony.euicc.DownloadableSubscription;
import android.telephony.euicc.EuiccInfo;
import android.telephony.euicc.EuiccManager;
import android.telephony.euicc.EuiccManager.OtaStatus;

import java.io.PrintWriter;

/** Dummy implementation of {@link EuiccService} for testing. */
public class MockEuiccService extends EuiccService {
    static String MOCK_EID = "89000000000000000000000000000000";
    static String MOCK_OS_VERSION = "1.0";
    static final String MOCK_DUMPED_LOG = "log messages";

    interface IMockEuiccServiceCallback {
        void setMethodCalled();

        boolean isMethodCalled();

        void reset();
    }

    private static IMockEuiccServiceCallback sMockEuiccServiceCallback;

    static void setCallback(IMockEuiccServiceCallback callback) {
        sMockEuiccServiceCallback = callback;
    }

    @Override
    public String onGetEid(int slotId) {
        sMockEuiccServiceCallback.setMethodCalled();
        return MOCK_EID;
    }

    @Override
    public @OtaStatus int onGetOtaStatus(int slotId) {
        sMockEuiccServiceCallback.setMethodCalled();
        return EuiccManager.EUICC_OTA_SUCCEEDED;
    }

    @Override
    public void dump(PrintWriter printWriter) {
        sMockEuiccServiceCallback.setMethodCalled();
        printWriter.append(MOCK_DUMPED_LOG);
    }

    @Override
    public void onStartOtaIfNecessary(int slotId, OtaStatusChangedCallback statusChangedCallback) {
        sMockEuiccServiceCallback.setMethodCalled();
        statusChangedCallback.onOtaStatusChanged(EuiccManager.EUICC_OTA_SUCCEEDED);
    }

    @Override
    public GetDownloadableSubscriptionMetadataResult onGetDownloadableSubscriptionMetadata(
            int slotId, DownloadableSubscription subscription, boolean forceDeactivateSim) {
        sMockEuiccServiceCallback.setMethodCalled();

        if (subscription.getEncodedActivationCode() != null) {
            return new GetDownloadableSubscriptionMetadataResult(
                    EuiccService.RESULT_OK, subscription);
        } else {
            return new GetDownloadableSubscriptionMetadataResult(
                    EuiccService.RESULT_RESOLVABLE_ERRORS, null /*subscription*/);
        }
    }

    @Override
    public GetDefaultDownloadableSubscriptionListResult onGetDefaultDownloadableSubscriptionList(
            int slotId, boolean forceDeactivateSim) {
        sMockEuiccServiceCallback.setMethodCalled();

        return new GetDefaultDownloadableSubscriptionListResult(
                EuiccService.RESULT_RESOLVABLE_ERRORS, null /*subscriptions*/);
    }

    @Override
    public DownloadSubscriptionResult onDownloadSubscription(
            int slotId,
            @NonNull DownloadableSubscription subscription,
            boolean switchAfterDownload,
            boolean forceDeactivateSim,
            @Nullable Bundle resolvedBundle) {
        sMockEuiccServiceCallback.setMethodCalled();

        int cardId = 1;

        if (subscription.getEncodedActivationCode() != null) {
            return new DownloadSubscriptionResult(
                    EuiccService.RESULT_OK, 0 /*resolvableErrors*/, cardId);
        } else {
            return new DownloadSubscriptionResult(
                    EuiccService.RESULT_RESOLVABLE_ERRORS,
                    EuiccService.RESULT_RESOLVABLE_ERRORS,
                    cardId);
        }
    }

    @Override
    public @NonNull GetEuiccProfileInfoListResult onGetEuiccProfileInfoList(int slotId) {
        sMockEuiccServiceCallback.setMethodCalled();
        return new GetEuiccProfileInfoListResult(
                EuiccService.RESULT_RESOLVABLE_ERRORS, null /*profiles*/, false /*isRemovable*/);
    }

    @Override
    public @NonNull EuiccInfo onGetEuiccInfo(int slotId) {
        sMockEuiccServiceCallback.setMethodCalled();
        return new EuiccInfo(MOCK_OS_VERSION);
    }

    @Override
    public @Result int onDeleteSubscription(int slotId, String iccid) {
        sMockEuiccServiceCallback.setMethodCalled();
        return EuiccService.RESULT_OK;
    }

    @Override
    public @Result int onSwitchToSubscription(
            int slotId, @Nullable String iccid, boolean forceDeactivateSim) {
        sMockEuiccServiceCallback.setMethodCalled();
        return EuiccService.RESULT_OK;
    }

    @Override
    public int onUpdateSubscriptionNickname(int slotId, String iccid, String nickname) {
        sMockEuiccServiceCallback.setMethodCalled();
        return EuiccService.RESULT_OK;
    }

    @Override
    public int onEraseSubscriptions(int slotId) {
        sMockEuiccServiceCallback.setMethodCalled();
        return EuiccService.RESULT_OK;
    }

    @Override
    public int onRetainSubscriptionsForFactoryReset(int slotId) {
        sMockEuiccServiceCallback.setMethodCalled();
        return EuiccService.RESULT_OK;
    }
}
