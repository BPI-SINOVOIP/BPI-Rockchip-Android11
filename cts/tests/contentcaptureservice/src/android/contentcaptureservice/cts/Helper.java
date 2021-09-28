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

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.content.ComponentName;
import android.content.Context;
import android.content.res.Resources;
import android.os.SystemClock;
import android.os.UserHandle;
import android.util.ArraySet;
import android.util.Log;
import android.view.View;
import android.view.contentcapture.ContentCaptureSession;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.compatibility.common.util.Timeout;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper for common funcionalities.
 */
public final class Helper {

    public static final String TAG = "ContentCaptureTest";

    public static final long GENERIC_TIMEOUT_MS = 10_000;

    public static final String MY_PACKAGE = "android.contentcaptureservice.cts";
    public static final String OTHER_PACKAGE = "NOT.android.contentcaptureservice.cts";

    public static final Set<String> NO_PACKAGES = null;
    public static final Set<ComponentName> NO_ACTIVITIES = null;

    public static final long MY_EPOCH = SystemClock.uptimeMillis();

    public static final String RESOURCE_STRING_SERVICE_NAME = "config_defaultContentCaptureService";

    public static final Context sContext = getInstrumentation().getTargetContext();

    private static final Timeout MY_TIMEOUT = new Timeout("MY_TIMEOUT", GENERIC_TIMEOUT_MS, 2F,
            GENERIC_TIMEOUT_MS);

    /**
     * Awaits for a latch to be counted down.
     */
    public static void await(@NonNull CountDownLatch latch, @NonNull String fmt,
            @Nullable Object... args)
            throws InterruptedException {
        final boolean called = latch.await(GENERIC_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        if (!called) {
            throw new IllegalStateException(String.format(fmt, args)
                    + " in " + GENERIC_TIMEOUT_MS + "ms");
        }
    }

    /**
     * Sets the content capture service.
     */
    public static void setService(@NonNull String service) {
        final int userId = getCurrentUserId();
        Log.d(TAG, "Setting service for user " + userId + " to " + service);
        // TODO(b/123540602): use @TestingAPI to get max duration constant
        runShellCommand("cmd content_capture set temporary-service %d %s 12000", userId, service);
    }

    /**
     * Resets the content capture service.
     */
    public static void resetService() {
        final int userId = getCurrentUserId();
        Log.d(TAG, "Resetting back user " + userId + " to default service");
        runShellCommand("cmd content_capture set temporary-service %d", userId);
    }

    /**
     * Enables / disables the default service.
     */
    public static void setDefaultServiceEnabled(boolean enabled) {
        final int userId = getCurrentUserId();
        Log.d(TAG, "setDefaultServiceEnabled(user=" + userId + ", enabled= " + enabled + ")");
        runShellCommand("cmd content_capture set default-service-enabled %d %s", userId,
                Boolean.toString(enabled));
    }

    /**
     * Gets the component name for a given class.
     */
    public static ComponentName componentNameFor(@NonNull Class<?> clazz) {
        return new ComponentName(MY_PACKAGE, clazz.getName());
    }

    /**
     * Creates a view that can be added to a parent and is important for content capture
     */
    public static TextView newImportantView(@NonNull Context context, @NonNull String text) {
        final TextView child = new TextView(context);
        child.setText(text);
        child.setImportantForContentCapture(View.IMPORTANT_FOR_CONTENT_CAPTURE_YES);
        Log.v(TAG, "newImportantView(text=" + text + ", id=" + child.getAutofillId() + ")");
        return child;
    }

    /**
     * Creates a view that can be added to a parent and is important for content capture
     */
    public static TextView newImportantView(@NonNull Context context,
            @NonNull ContentCaptureSession session, @NonNull String text) {
        final TextView child = newImportantView(context, text);
        child.setContentCaptureSession(session);
        return child;
    }

    /**
     * Gets a string from the Android resources.
     */
    public static String getInternalString(@NonNull String id) {
        final Resources resources = sContext.getResources();
        final int stringId = resources.getIdentifier(id, "string", "android");
        return resources.getString(stringId);
    }

    /**
     * Runs an {@code assertion}, retrying until {@link #MY_TIMEOUT} is reached.
     */
    public static <T> T eventually(@NonNull String description, @NonNull Callable<T> assertion)
            throws Exception {
        return MY_TIMEOUT.run(description, assertion);
    }

    /**
     * Creates a Set with the given objects.
     */
    public static <T> ArraySet<T> toSet(@SuppressWarnings("unchecked") @Nullable T... objs) {
        final ArraySet<T> set = new ArraySet<>();
        if (objs != null) {
            for (int i = 0; i < objs.length; i++) {
                final T t = objs[i];
                set.add(t);
            }
        }
        return set;
    }

    /**
     * Gets the content of the given file.
     */
    public static String read(@NonNull File file) throws IOException {
        Log.d(TAG, "Reading " + file);
        final byte[] bytes = Files.readAllBytes(Paths.get(file.getAbsolutePath()));
        return bytes == null ? null : new String(bytes);
    }

    private static int getCurrentUserId() {
        return UserHandle.myUserId();
    }

    private Helper() {
        throw new UnsupportedOperationException("contain static methods only");
    }
}
