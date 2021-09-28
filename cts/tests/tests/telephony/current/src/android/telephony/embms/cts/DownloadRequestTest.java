/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * limitations under the License
 */

package android.telephony.embms.cts;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.Uri;
import android.telephony.cts.embmstestapp.CtsDownloadService;
import android.telephony.mbms.DownloadRequest;

import java.io.File;

import org.junit.Test;

public class DownloadRequestTest {

    @Test
    public void testGetMaxAppIntentSize() {
        // Test that the max intent size is positive
        assertTrue(DownloadRequest.getMaxAppIntentSize() > 0);
    }

    @Test
    public void testGetMaxDestinationUriSize() {
        // Test that the max intent size is positive
        assertTrue(DownloadRequest.getMaxDestinationUriSize() > 0);
    }

    @Test
    public void testBuilderConstruction() {
        File destinationDirectory = new File(getInstrumentation().getContext().getFilesDir(),
                "downloads");
        Uri destinationDirectoryUri = Uri.fromFile(destinationDirectory);
        DownloadRequest.Builder builder = new DownloadRequest.Builder(
                CtsDownloadService.SOURCE_URI_1, destinationDirectoryUri)
                .setSubscriptionId(-1)
                .setServiceInfo(CtsDownloadService.FILE_SERVICE_INFO);
        DownloadRequest request = builder.build();
        assertEquals(request, DownloadRequest.Builder.fromDownloadRequest(request).build());
    }

    @Test
    public void testServiceIdEquivalency() {
        File destinationDirectory = new File(getInstrumentation().getContext().getFilesDir(),
                "downloads");
        Uri destinationDirectoryUri = Uri.fromFile(destinationDirectory);
        DownloadRequest request1 = new DownloadRequest.Builder(
                CtsDownloadService.SOURCE_URI_1, destinationDirectoryUri)
                .setSubscriptionId(-1)
                .setServiceInfo(CtsDownloadService.FILE_SERVICE_INFO)
                .build();

        DownloadRequest request2 = new DownloadRequest.Builder(
                CtsDownloadService.SOURCE_URI_1, destinationDirectoryUri)
                .setSubscriptionId(-1)
                .setServiceId(CtsDownloadService.FILE_SERVICE_INFO.getServiceId())
                .build();

        assertEquals(request1, request2);
    }
}
