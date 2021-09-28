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

import android.app.Service;
import android.content.Intent;
import android.content.LocusId;
import android.os.Binder;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.view.contentcapture.ContentCaptureManager;
import android.view.contentcapture.DataShareRequest;
import android.view.contentcapture.DataShareWriteAdapter;

import androidx.annotation.Nullable;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Random;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

public class OutOfProcessDataSharingService extends Service {
    private static final Executor sExecutor = Executors.newCachedThreadPool();
    private static final Random sRandom = new Random();

    boolean mSessionFinished = false;
    boolean mSessionSucceeded = false;
    int mSessionErrorCode = 0;
    boolean mSessionRejected = false;
    byte[] mDataWritten = new byte[10_000];
    String mLocusId = "DataShare_CTSTest";
    String mMimeType = "application/octet-stream";

    private final IBinder mBinder = new LocalBinder();

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        shareData();
        return START_NOT_STICKY;
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private void shareData() {
        ContentCaptureManager manager =
                getApplicationContext().getSystemService(ContentCaptureManager.class);

        assertThat(manager).isNotNull();
        assertThat(manager.isContentCaptureEnabled()).isTrue();

        manager.shareData(
                new DataShareRequest(new LocusId(mLocusId), mMimeType),
                sExecutor, new DataShareWriteAdapter() {
                    @Override
                    public void onWrite(ParcelFileDescriptor destination) {
                        try (OutputStream outputStream =
                                     new ParcelFileDescriptor.AutoCloseOutputStream(destination)) {
                            if (DataSharingServiceTest.sKillingStage
                                    == DataSharingServiceTest.KillingStage.BEFORE_WRITE) {
                                Process.killProcess(Process.myPid());
                                return;
                            }
                            sRandom.nextBytes(mDataWritten);
                            outputStream.write(mDataWritten);
                            if (DataSharingServiceTest.sKillingStage
                                    == DataSharingServiceTest.KillingStage.DURING_WRITE) {
                                Process.killProcess(Process.myPid());
                                return;
                            }
                            mSessionSucceeded = true;
                        } catch (IOException e) {
                        }

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

    public class LocalBinder extends Binder {
        OutOfProcessDataSharingService getService() {
            return OutOfProcessDataSharingService.this;
        }
    }

}
