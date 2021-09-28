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
 * limitations under the License.
 */

package android.net.wifi.cts;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcel;
import android.os.ResultReceiver;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * An Activity that can start another Activity and wait for its result.
 */
public class WaitForResultActivity extends Activity {
    private final Object mStatusLock = new Object();
    private CountDownLatch mLatch;
    private boolean mStatus = false;

    private static final int REQUEST_CODE_WAIT_FOR_RESULT = 1;
    private static final String WIFI_LOCATION_TEST_APP_LOCATION_STATUS_EXTRA =
            "android.net.wifi.cts.app.extra.STATUS";

    public void startActivityToWaitForResult(@NonNull ComponentName componentName) {
        mLatch = new CountDownLatch(1);
        synchronized (mStatusLock) {
            mStatus = false;
        }
        Intent intent = new Intent()
                .addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
                .setComponent(componentName);
        startActivityForResult(intent, REQUEST_CODE_WAIT_FOR_RESULT);
    }

    @NonNull
    public boolean waitForActivityResult(long timeoutMillis)
            throws InterruptedException {
        assertThat(mLatch.await(timeoutMillis, TimeUnit.MILLISECONDS)).isTrue();
        synchronized (mStatusLock) {
            return mStatus;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == REQUEST_CODE_WAIT_FOR_RESULT) {
            assertThat(resultCode).isEqualTo(RESULT_OK);
            assertThat(data.hasExtra(WIFI_LOCATION_TEST_APP_LOCATION_STATUS_EXTRA)).isTrue();
            synchronized (mStatusLock) {
                mStatus = data.getBooleanExtra(
                        WIFI_LOCATION_TEST_APP_LOCATION_STATUS_EXTRA, false);
            }
            mLatch.countDown();
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    private static final String WIFI_LOCATION_TEST_APP_PACKAGE_NAME =
            "android.net.wifi.cts.app";
    private static final String WIFI_LOCATION_TEST_APP_SCHEDULE_JOB_ACTIVITY =
            WIFI_LOCATION_TEST_APP_PACKAGE_NAME + ".ScheduleJobActivity";
    private static final String RESULT_RECEIVER_EXTRA =
            "android.net.wifi.cts.app.extra.RESULT_RECEIVER";
    private static final String SERVICE_COMPONENT_EXTRA =
            "android.net.wifi.cts.app.extra.SERVICE_COMPONENT";

    private static ResultReceiver convertToGeneric(ResultReceiver receiver) {
        Parcel parcel = Parcel.obtain();
        receiver.writeToParcel(parcel,0);
        parcel.setDataPosition(0);
        ResultReceiver receiverGeneric = ResultReceiver.CREATOR.createFromParcel(parcel);
        parcel.recycle();
        return receiverGeneric;
    }

    public void startServiceToWaitForResult(@NonNull ComponentName serviceComponent) {
        mLatch = new CountDownLatch(1);
        synchronized (mStatusLock) {
            mStatus = false;
        }
        ResultReceiver resultReceiver = new ResultReceiver(null) {
            @Override
            public void onReceiveResult(int resultCode, Bundle data) {
                synchronized (mStatusLock) {
                    mStatus = resultCode == 1;
                }
                mLatch.countDown();
            }
        };
        Intent intent = new Intent()
                .setComponent(new ComponentName(WIFI_LOCATION_TEST_APP_PACKAGE_NAME,
                        WIFI_LOCATION_TEST_APP_SCHEDULE_JOB_ACTIVITY))
                .putExtra(RESULT_RECEIVER_EXTRA, convertToGeneric(resultReceiver))
                .putExtra(SERVICE_COMPONENT_EXTRA, serviceComponent);
        startActivity(intent);
    }

    @NonNull
    public boolean waitForServiceResult(long timeoutMillis)
            throws InterruptedException {
        assertThat(mLatch.await(timeoutMillis, TimeUnit.MILLISECONDS)).isTrue();
        synchronized (mStatusLock) {
            return mStatus;
        }
    }

}
