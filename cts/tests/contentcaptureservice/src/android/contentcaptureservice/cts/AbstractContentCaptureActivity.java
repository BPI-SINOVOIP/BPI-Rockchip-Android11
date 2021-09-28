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
package android.contentcaptureservice.cts;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.app.Activity;
import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.contentcapture.ContentCaptureManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Base class for all activities.
 */
public abstract class AbstractContentCaptureActivity extends Activity {

    private final String mTag = getClass().getSimpleName();

    private int mRealTaskId;

    @Nullable
    public ContentCaptureManager getContentCaptureManager() {
        return getSystemService(ContentCaptureManager.class);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mRealTaskId = getTaskId();
        Log.d(mTag, "onCreate(): taskId=" + mRealTaskId + ", decorView=" + getDecorView());

        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onStart() {
        Log.d(mTag, "onStart()");
        super.onStart();
    }

    @Override
    protected void onResume() {
        Log.d(mTag, "onResume(): decorViewId=" + getDecorView().getAutofillId());
        super.onResume();
    }

    @Override
    protected void onPause() {
        Log.d(mTag, "onPause()");
        super.onPause();
    }

    @Override
    protected void onStop() {
        Log.d(mTag, "onStop()");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.d(mTag, "onDestroy()");
        super.onDestroy();
    }

    @NonNull
    public final View getDecorView() {
        return getWindow().getDecorView();
    }

    /**
     * Asserts the events generated when this session was launched and finished,
     * without any custom / dynamic operations in between.
     */
    public abstract void assertDefaultEvents(@NonNull Session session);

    /**
     * Gets the real task id associated with the activity, as {@link #getTaskId()} returns
     * {@code -1} after it's gone.
     */
    public final int getRealTaskId() {
        return mRealTaskId;
    }

    /**
     * Runs an action in the UI thread, and blocks caller until the action is finished.
     */
    public final void syncRunOnUiThread(@NonNull Runnable action) {
        syncRunOnUiThread(action, Helper.GENERIC_TIMEOUT_MS);
    }

    /**
     * Calls an action in the UI thread, and blocks caller until the action is finished.
     */
    public final <T> T syncCallOnUiThread(@NonNull Callable<T> action) throws Exception {
        final AtomicReference<T> result = new AtomicReference<>();
        final AtomicReference<Exception> exception  = new AtomicReference<>();
        syncRunOnUiThread(() -> {
            try {
                result.set(action.call());
            } catch (Exception e) {
                exception.set(e);
            }
        });
        final Exception e = exception.get();
        if (e != null) {
            throw e;
        }
        return result.get();
    }

    /**
     * Run an action in the UI thread, and blocks caller until the action is finished or it times
     * out.
     */
    public final void syncRunOnUiThread(@NonNull Runnable action, long timeoutMs) {
        final CountDownLatch latch = new CountDownLatch(1);
        runOnUiThread(() -> {
            action.run();
            latch.countDown();
        });
        try {
            if (!latch.await(timeoutMs, TimeUnit.MILLISECONDS)) {
                // TODO(b/120665995): throw RetryableException (once moved from Autofill to common)
                throw new IllegalStateException(
                        String.format("action on UI thread timed out after %d ms", timeoutMs));
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new RuntimeException("Interrupted", e);
        }
    }

    /**
     * Dumps the {@link ContentCaptureManager} state of the activity on logcat.
     */
    public void dumpIt() {
        final String dump = runShellCommand(
                "dumpsys activity %s --contentcapture",  getComponentName().flattenToString());
        Log.v(mTag, "dump it: " + dump);
    }
}
