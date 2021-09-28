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
package android.contentcaptureservice.cts;

import static android.contentcaptureservice.cts.Helper.eventually;
import static android.contentcaptureservice.cts.Helper.sContext;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.view.contentcapture.ContentCaptureManager;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;

/**
 * Activity that is running out of the CTS test process.
 *
 * <p>As such, it uses files to keep track of lifecycle events.
 */
public class OutOfProcessActivity extends Activity {

    private static final String TAG = OutOfProcessActivity.class.getSimpleName();
    private static final String ACTION_EXTRA = "action";

    public static final ComponentName COMPONENT_NAME = new ComponentName(Helper.MY_PACKAGE,
            OutOfProcessActivity.class.getName());

    public static final int ACTION_NONE = 0;
    public static final int ACTION_CHECK_MANAGER_AND_FINISH = 1;

    public static final String NO_MANAGER = "NoMgr";
    public static final String HAS_MANAGER = "Mgr:";

    @Override
    protected void onStart() {
        Log.i(TAG, "onStart()");
        super.onStart();

        final int action = getIntent().getIntExtra(ACTION_EXTRA, ACTION_NONE);
        final File marker = getStartedMarker(this);

        try {
            if (!marker.createNewFile()) {
                Log.e(TAG, "cannot write started file");
            }
        } catch (IOException e) {
            Log.e(TAG, "cannot write started file: " + e);
        }
        if (action == ACTION_CHECK_MANAGER_AND_FINISH) {
            final ContentCaptureManager ccm = getSystemService(ContentCaptureManager.class);
            write(marker, ccm == null ? NO_MANAGER : HAS_MANAGER + ccm);
            Log.v(TAG, "So Long, and Thanks for All the Fish!");
            finish();
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

    /**
     * Gets the file that signals that the activity has entered {@link Activity#onStop()}.
     *
     * @param context Context of the app
     * @return The marker file that is written onStop()
     */
    @NonNull public static File getStoppedMarker(@NonNull Context context) {
        return new File(context.getFilesDir(), "stopped");
    }

    /**
     * Gets the file that signals that the activity has entered {@link Activity#onStart()}.
     *
     * @param context Context of the app
     * @return The marker file that is written onStart()
     */
    @NonNull public static File getStartedMarker(@NonNull Context context) {
        return new File(context.getFilesDir(), "started");
    }

    public static void startAndWaitOutOfProcessActivity() throws Exception {
        final Intent startIntent = new Intent(sContext,
                OutOfProcessActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        getStartedMarker(sContext).delete();
        sContext.startActivity(startIntent);
        eventually("getStartedMarker()", () -> {
            return getStartedMarker(sContext).exists();
        });
        getStartedMarker(sContext).delete();
    }

    public static void startActivity(@NonNull Context context, int action) throws Exception {
        final Intent startIntent = new Intent(context,
                OutOfProcessActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (action != ACTION_NONE) {
            startIntent.putExtra(ACTION_EXTRA, action);
        }
        getStartedMarker(context).delete();
        context.startActivity(startIntent);
    }

    public static File waitActivityStopped(@NonNull Context context) throws Exception {
        return eventually("getStoppedMarker()", () -> {
            final File file = getStoppedMarker(context);
            return file.exists() ? file : null;
        });
    }

    public static File waitActivityStarted(@NonNull Context context) throws Exception {
        return eventually("getStartedMarker()", () -> {
            final File file = getStartedMarker(context);
            return file.exists() ? file : null;
        });
    }

    public static void killOutOfProcessActivity() throws Exception {
        // Waiting for activity to stop (stop marker appears)
        eventually("getStoppedMarker()", () -> {
            return getStoppedMarker(sContext).exists();
        });

        // Kill it!
        runShellCommand("am broadcast --receiver-foreground "
                + "-n android.contentcaptureservice.cts/.SelfDestructReceiver");
    }

    private void write(@NonNull File file, @NonNull String string) {
        Log.d(TAG, "writing '" + string + "' to " + file);
        try {
            Files.write(Paths.get(file.getAbsolutePath()), string.getBytes(),
                    StandardOpenOption.TRUNCATE_EXISTING);
        } catch (IOException e) {
            Log.e(TAG, "error writing to " + file, e);
        }
    }
}
