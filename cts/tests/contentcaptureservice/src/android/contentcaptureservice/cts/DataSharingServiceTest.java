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

package android.contentcaptureservice.cts;

import static androidx.test.core.app.ApplicationProvider.getApplicationContext;

import static com.google.common.truth.Truth.assertThat;

import android.content.Intent;
import android.view.contentcapture.ContentCaptureManager;

import androidx.annotation.NonNull;
import androidx.test.rule.ServiceTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.rules.TestRule;

import java.util.Arrays;

public class DataSharingServiceTest extends AbstractContentCaptureIntegrationTest {

    public final ServiceTestRule mServiceRule = new ServiceTestRule();

    private DataSharingService.LocalBinder mBinder;

    public static KillingStage sKillingStage = KillingStage.NONE;

    enum KillingStage {
        NONE,
        BEFORE_WRITE,
        DURING_WRITE
    }

    @NonNull
    @Override
    protected TestRule getMainTestRule() {
        return (base, description) -> base;
    }

    @Before
    public void setup() throws Exception {
        sKillingStage = KillingStage.NONE;
        mBinder = (DataSharingService.LocalBinder)
                mServiceRule.bindService(
                        new Intent(getApplicationContext(), DataSharingService.class));
    }

    @After
    public void tearDown() throws Exception {
        mServiceRule.unbindService();
    }

    @Test
    public void testDataSharingSessionAccepted() throws Exception {
        DataSharingService dataSharingService = mBinder.getService();
        CtsContentCaptureService ccService = enableService();

        ccService.setDataSharingEnabled(true);
        dataSharingService.shareData();

        PollingCheck.waitFor(() -> dataSharingService.mSessionFinished);
        PollingCheck.waitFor(() -> ccService.mDataShareSessionFinished);

        assertThat(dataSharingService.mSessionSucceeded).isTrue();
        assertThat(ccService.mDataShareSessionSucceeded).isTrue();

        assertThat(Arrays.copyOfRange(ccService.mDataShared, 0,
                dataSharingService.mDataWritten.length))
                .isEqualTo(dataSharingService.mDataWritten);
    }

    @Test
    public void testDataSharingSessionRejected() throws Exception {
        DataSharingService dataSharingService = mBinder.getService();
        CtsContentCaptureService ccService = enableService();

        ccService.setDataSharingEnabled(false);
        dataSharingService.shareData();

        PollingCheck.waitFor(() -> dataSharingService.mSessionFinished);
        PollingCheck.waitFor(() -> ccService.mDataShareSessionFinished);

        assertThat(dataSharingService.mSessionSucceeded).isFalse();
        assertThat(ccService.mDataShareSessionSucceeded).isFalse();
    }

    @Test
    public void testDataShareRequest_valuesPropagatedToReceiver() throws Exception {
        DataSharingService dataSharingService = mBinder.getService();
        CtsContentCaptureService ccService = enableService();

        ccService.setDataSharingEnabled(true);
        dataSharingService.shareData();

        PollingCheck.waitFor(() -> dataSharingService.mSessionFinished);
        PollingCheck.waitFor(() -> ccService.mDataShareSessionFinished);

        assertThat(ccService.mDataShareRequest.getLocusId().getId()).isEqualTo("DataShare_CTSTest");
        assertThat(ccService.mDataShareRequest.getMimeType()).isEqualTo("application/octet-stream");
        assertThat(ccService.mDataShareRequest.getPackageName()).isEqualTo(
                "android.contentcaptureservice.cts");
    }

    @Test
    public void testDataSharingSessionError_concurrentRequests() throws Exception {
        DataSharingService dataSharingService = mBinder.getService();
        CtsContentCaptureService ccService = enableService();

        ccService.setDataSharingEnabled(true);
        dataSharingService.setShouldAttemptConcurrentRequest(true);
        dataSharingService.shareData();

        PollingCheck.waitFor(() -> dataSharingService.mSessionFinished);
        PollingCheck.waitFor(() -> ccService.mDataShareSessionFinished);

        assertThat(dataSharingService.mConcurrentRequestFailed).isTrue();
        assertThat(dataSharingService.mConcurrentRequestErrorCode).isEqualTo(
                ContentCaptureManager.DATA_SHARE_ERROR_CONCURRENT_REQUEST);
    }

    @Test
    public void testDataSharingSessionError_senderWasKilledBeforeWrite() throws Exception {
        CtsContentCaptureService ccService = enableService();

        ccService.setDataSharingEnabled(true);
        sKillingStage = KillingStage.BEFORE_WRITE;
        getApplicationContext().startService(
                new Intent(getApplicationContext(), OutOfProcessDataSharingService.class));

        PollingCheck.waitFor(() -> ccService.mDataShareSessionErrorCode > 0);

        assertThat(ccService.mDataShareSessionErrorCode).isEqualTo(
                ContentCaptureManager.DATA_SHARE_ERROR_UNKNOWN);
    }

    @Test
    public void testDataSharingSessionError_senderWasKilledDuringWrite() throws Exception {
        CtsContentCaptureService ccService = enableService();

        ccService.setDataSharingEnabled(true);
        sKillingStage = KillingStage.DURING_WRITE;
        getApplicationContext().startService(
                new Intent(getApplicationContext(), OutOfProcessDataSharingService.class));

        PollingCheck.waitFor(() -> ccService.mDataShareSessionErrorCode > 0);

        assertThat(ccService.mDataShareSessionErrorCode).isEqualTo(
                ContentCaptureManager.DATA_SHARE_ERROR_UNKNOWN);
    }
}
