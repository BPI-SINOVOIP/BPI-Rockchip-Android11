/*
 * Copyright (C) 2009 The Android Open Source Project
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
package android.media.cts;


import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.os.IBinder;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.view.WindowManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;


// This is a partial copy of android.view.cts.surfacevalidator.CapturedActivity.
// Common code should be move in a shared library
/** Start this activity to retrieve a MediaProjection through waitForMediaProjection() */
public class MediaProjectionActivity extends Activity {
    private static final String TAG = "MediaProjectionActivity";
    private static final int PERMISSION_CODE = 1;
    private static final int PERMISSION_DIALOG_WAIT_MS = 1000;
    private static final String ACCEPT_RESOURCE_ID = "android:id/button1";

    private MediaProjectionManager mProjectionManager;
    private MediaProjection mMediaProjection;
    private CountDownLatch mCountDownLatch;
    private boolean mProjectionServiceBound;

    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            startActivityForResult(mProjectionManager.createScreenCaptureIntent(), PERMISSION_CODE);
            dismissPermissionDialog();
            mProjectionServiceBound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            mProjectionServiceBound = false;
        }
    };


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // UI automator need the screen ON in dismissPermissionDialog()
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mProjectionManager = getSystemService(MediaProjectionManager.class);
        mCountDownLatch = new CountDownLatch(1);
        bindMediaProjectionService();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mProjectionServiceBound) {
            unbindService(mConnection);
            mProjectionServiceBound = false;
        }
    }

    private void bindMediaProjectionService() {
        Intent intent = new Intent(this, LocalMediaProjectionService.class);
        startService(intent);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != PERMISSION_CODE) {
            throw new IllegalStateException("Unknown request code: " + requestCode);
        }
        if (resultCode != RESULT_OK) {
            throw new IllegalStateException("User denied screen sharing permission");
        }
        Log.d(TAG, "onActivityResult");
        mMediaProjection = mProjectionManager.getMediaProjection(resultCode, data);
        mCountDownLatch.countDown();
    }

    public MediaProjection waitForMediaProjection() throws InterruptedException {
        final long timeOutMs = 125000;
        final int retryCount = 2;
        int count = 0;
        // Sometimes system decides to rotate the permission activity to another orientation
        // right after showing it. This results in: uiautomation thinks that accept button appears,
        // we successfully click it in terms of uiautomation, but nothing happens,
        // because permission activity is already recreated.
        // Thus, we try to click that button multiple times.
        do {
            assertTrue("Can't get the permission", count <= retryCount);
            dismissPermissionDialog();
            count++;
        } while (!mCountDownLatch.await(timeOutMs, TimeUnit.MILLISECONDS));
        return mMediaProjection;
    }

    /** The permission dialog will be auto-opened by the activity - find it and accept */
    public void dismissPermissionDialog() {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        UiObject2 acceptButton = uiDevice.wait(Until.findObject(By.res(ACCEPT_RESOURCE_ID)),
                PERMISSION_DIALOG_WAIT_MS);
        if (acceptButton != null) {
            Log.d(TAG, "found permission dialog after searching all windows, clicked");
            acceptButton.click();
        }
    }

    @Override
    protected void onResume() {
        Log.i(TAG, "onResume");
        super.onResume();
    }

    @Override
    protected void onPause() {
        Log.i(TAG, "onPause");
        super.onPause();
    }
}
