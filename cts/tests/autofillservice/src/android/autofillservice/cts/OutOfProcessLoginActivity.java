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

package android.autofillservice.cts;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.File;
import java.io.IOException;

/**
 * Simple activity showing R.layout.login_activity. Started outside of the test process.
 */
public class OutOfProcessLoginActivity extends Activity {
    private static final String TAG = "OutOfProcessLoginActivity";

    private static OutOfProcessLoginActivity sInstance;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        Log.i(TAG, "onCreate(" + savedInstanceState + ")");
        super.onCreate(savedInstanceState);

        setContentView(R.layout.login_activity);

        findViewById(R.id.login).setOnClickListener((v) -> finish());

        sInstance = this;
    }

    @Override
    protected void onStart() {
        Log.i(TAG, "onStart()");
        super.onStart();
        try {
            if (!getStartedMarker(this).createNewFile()) {
                Log.e(TAG, "cannot write started file");
            }
        } catch (IOException e) {
            Log.e(TAG, "cannot write started file: " + e);
        }
    }

    @Override
    protected void onStop() {
        Log.i(TAG, "onStop()");
        super.onStop();

        try {
            if (!getStoppedMarker(this).createNewFile()) {
                Log.e(TAG, "could not write stopped marker");
            } else {
                Log.v(TAG, "wrote stopped marker");
            }
        } catch (IOException e) {
            Log.e(TAG, "could write stopped marker: " + e);
        }
    }

    @Override
    protected void onDestroy() {
        Log.i(TAG, "onDestroy()");
        try {
            if (!getDestroyedMarker(this).createNewFile()) {
                Log.e(TAG, "could not write destroyed marker");
            } else {
                Log.v(TAG, "wrote destroyed marker");
            }
        } catch (IOException e) {
            Log.e(TAG, "could write destroyed marker: " + e);
        }
        super.onDestroy();
        sInstance = null;
    }

    /**
     * Get the file that signals that the activity has entered {@link Activity#onStop()}.
     *
     * @param context Context of the app
     * @return The marker file that is written onStop()
     */
    @NonNull public static File getStoppedMarker(@NonNull Context context) {
        return new File(context.getFilesDir(), "stopped");
    }

    /**
     * Get the file that signals that the activity has entered {@link Activity#onStart()}.
     *
     * @param context Context of the app
     * @return The marker file that is written onStart()
     */
    @NonNull public static File getStartedMarker(@NonNull Context context) {
        return new File(context.getFilesDir(), "started");
    }

   /**
     * Get the file that signals that the activity has entered {@link Activity#onDestroy()}.
     *
     * @param context Context of the app
     * @return The marker file that is written onDestroy()
     */
    @NonNull public static File getDestroyedMarker(@NonNull Context context) {
        return new File(context.getFilesDir(), "destroyed");
    }

    public static void finishIt() {
        Log.v(TAG, "Finishing " + sInstance);
        if (sInstance != null) {
            sInstance.finish();
        }
    }

    public static boolean hasInstance() {
        return sInstance != null;
    }
}
