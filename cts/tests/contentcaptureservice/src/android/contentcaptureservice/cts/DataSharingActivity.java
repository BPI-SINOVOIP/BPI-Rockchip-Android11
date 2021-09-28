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

import static com.google.common.truth.Truth.assertThat;

import android.content.LocusId;
import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.os.ParcelFileDescriptor;
import android.view.contentcapture.ContentCaptureManager;
import android.view.contentcapture.DataShareRequest;
import android.view.contentcapture.DataShareWriteAdapter;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Random;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class DataSharingActivity extends AbstractContentCaptureActivity {

    private static final Executor sExecutor = Executors.newCachedThreadPool();
    private static final Random sRandom = new Random();
    private static final String sLocusId = "DataShare_CTSTest";
    private static final String sMimeType = "application/octet-stream";

    boolean mSessionFinished = false;
    boolean mSessionSucceeded = false;
    int mSessionErrorCode = 0;
    boolean mSessionRejected = false;
    byte[] mDataWritten = new byte[10_000];

    @Override
    public void onStart() {
        super.onStart();

        ContentCaptureManager manager =
                getApplicationContext().getSystemService(ContentCaptureManager.class);

        assertThat(manager).isNotNull();
        assertThat(manager.isContentCaptureEnabled()).isTrue();

        manager.shareData(
                new DataShareRequest(new LocusId(sLocusId), sMimeType),
                sExecutor, new DataShareWriteAdapter() {
                    @Override
                    public void onWrite(ParcelFileDescriptor destination) {
                        sRandom.nextBytes(mDataWritten);

                        try (OutputStream outputStream =
                                     new ParcelFileDescriptor.AutoCloseOutputStream(destination)) {
                            outputStream.write(mDataWritten);
                        } catch (IOException e) {
                            // fall through, sessionSucceeded will stay false.
                        }

                        mSessionSucceeded = true;
                        mSessionFinished = true;
                    }

                    @Override
                    public void onRejected() {
                        mSessionRejected = true;
                        mSessionSucceeded = false;
                        mSessionFinished = true;
                    }

                    @Override
                    public void onError(int errorCode) {
                        mSessionErrorCode = errorCode;
                        mSessionSucceeded = false;
                        mSessionFinished = true;
                    }
                });
    }

    @Override
    public void assertDefaultEvents(@NonNull Session session) {
        // Do nothing - this test operates with file sharing.
    }
}
