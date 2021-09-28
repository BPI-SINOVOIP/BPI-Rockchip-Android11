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
package android.view.textclassifier.cts;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.SafeCleanerRule;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Custom {@link TestWatcher} that does TextClassifierService setup and reset tasks for the tests.
 */
final class TextClassifierTestWatcher extends TestWatcher {

    private static final String TAG = "TextClassifierTestWatcher";
    private static final long GENERIC_TIMEOUT_MS = 10_000;
    // TODO: Use default value defined in TextClassificationConstants when TestApi is ready
    private static final String DEFAULT_TEXT_CLASSIFIER_SERVICE_PACKAGE_OVERRIDE = null;
    private static final boolean SYSTEM_TEXT_CLASSIFIER_ENABLED_DEFAULT = true;

    private String mOriginalOverrideService;
    private boolean mOriginalSystemTextClassifierEnabled;

    private static final ArrayList<Throwable> sExceptions = new ArrayList<>();

    private static ServiceWatcher sServiceWatcher;

    @Override
    protected void starting(Description description) {
        super.starting(description);
        prepareDevice();
        // get original settings
        mOriginalOverrideService = getOriginalOverrideService();
        mOriginalSystemTextClassifierEnabled = isSystemTextClassifierEnabled();

        // set system TextClassifier enabled
        runShellCommand("device_config put textclassifier system_textclassifier_enabled true");

        setService();
    }

    @Override
    protected void finished(Description description) {
        super.finished(description);
        // restore original settings
        runShellCommand("device_config put textclassifier system_textclassifier_enabled "
                + mOriginalSystemTextClassifierEnabled);
        // restore service and make sure service disconnected.
        // clear the static values.
        try {
            resetService();
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            resetStaticState();
        }
    }

    /**
     * Wait for the TextClassifierService to connect. Note that the system requires a query to the
     * TextClassifierService before it is first connected.
     *
     * @return the CtsTextClassifierService when connected.
     *
     * @throws InterruptedException if the current thread is interrupted while waiting.
     * @throws AssertionError if no CtsTextClassifierService is returned.
     */
    CtsTextClassifierService getService() throws InterruptedException, AssertionError {
        CtsTextClassifierService service = waitServiceLazyConnect();
        if (service == null) {
            throw new AssertionError("Can not get service.");
        }
        return service;
    }

    /**
     * Waits for the current application to idle. Default wait timeout is 10 seconds
     */
    static void waitForIdle() {
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .waitForIdle();
    }

    private static void setServiceWatcher() {
        if (sServiceWatcher == null) {
            sServiceWatcher = new ServiceWatcher();
        }
    }

    private static void clearServiceWatcher() {
        if (sServiceWatcher != null) {
            sServiceWatcher.mService = null;
            sServiceWatcher = null;
        }
    }

    private static void resetStaticState() {
        sExceptions.clear();
        clearServiceWatcher();
    }

    private void prepareDevice() {
        Log.v(TAG, "prepareDevice()");
        // Unlock screen.
        runShellCommand("input keyevent KEYCODE_WAKEUP");

        // Dismiss keyguard, in case it's set as "Swipe to unlock".
        runShellCommand("wm dismiss-keyguard");
    }

    @Nullable
    private String getOriginalOverrideService() {
        final String deviceConfigSetting = runShellCommand(
                "device_config get textclassifier textclassifier_service_package_override");
        if (!TextUtils.isEmpty(deviceConfigSetting) && !deviceConfigSetting.equals("null")) {
            return deviceConfigSetting;
        }
        return DEFAULT_TEXT_CLASSIFIER_SERVICE_PACKAGE_OVERRIDE;
    }

    private boolean isSystemTextClassifierEnabled() {
        final String deviceConfigSetting = runShellCommand(
                "device_config get textclassifier system_textclassifier_enabled");
        if (!TextUtils.isEmpty(deviceConfigSetting) && !deviceConfigSetting.equals("null")) {
            return deviceConfigSetting.toLowerCase().equals("true");
        }
        return SYSTEM_TEXT_CLASSIFIER_ENABLED_DEFAULT;
    }

    private void setService() {
        setServiceWatcher();
        // set the test service
        runShellCommand("device_config put textclassifier textclassifier_service_package_override "
                + CtsTextClassifierService.MY_PACKAGE);
        // Wait for the current bound TCS to be unbounded.
        try {
            Thread.sleep(1_000);
        } catch (InterruptedException e) {
            Log.e(TAG, "Error while sleeping");
        }
    }

    private void resetOriginalService() {
        Log.d(TAG, "reset to " + mOriginalOverrideService);
        runShellCommand(
                "device_config put textclassifier textclassifier_service_package_override "
                        + mOriginalOverrideService);
    }

    private void resetService() throws InterruptedException {
        resetOriginalService();
        if (sServiceWatcher != null && sServiceWatcher.mService != null) {
            sServiceWatcher.waitOnDisconnected();
        } else {
            waitForIdle();
        }
    }

    /**
     * Returns the TestRule that runs clean up after a test is finished. See {@link SafeCleanerRule}
     * for more details.
     */
    public SafeCleanerRule newSafeCleaner() {
        return new SafeCleanerRule()
                .add(() -> {
                    return getExceptions();
                });
    }

    /**
     * Gets the exceptions that were thrown while the service handled requests.
     */
    @NonNull
    private static List<Throwable> getExceptions() throws Exception {
        return Collections.unmodifiableList(sExceptions);
    }

    private static void addException(@NonNull String fmt, @Nullable Object...args) {
        final String msg = String.format(fmt, args);
        Log.e(TAG, msg);
        sExceptions.add(new IllegalStateException(msg));
    }

    private CtsTextClassifierService waitServiceLazyConnect() throws InterruptedException {
        if (sServiceWatcher != null) {
            return sServiceWatcher.waitOnConnected();
        }
        return null;
    }

    public static final class ServiceWatcher {
        private final CountDownLatch mCreated = new CountDownLatch(1);
        private final CountDownLatch mDestroyed = new CountDownLatch(1);

        CtsTextClassifierService mService;

        public static void onConnected(CtsTextClassifierService service) {
            Log.i(TAG, "onConnected:  sServiceWatcher=" + sServiceWatcher);

            if (sServiceWatcher == null) {
                addException("onConnected() without a watcher");
                return;
            }

            if (sServiceWatcher.mService != null) {
                addException("onConnected(): already created: " + sServiceWatcher);
                return;
            }

            sServiceWatcher.mService = service;
            sServiceWatcher.mCreated.countDown();
        }

        public static void onDisconnected() {
            Log.i(TAG, "onDisconnected:  sServiceWatcher=" + sServiceWatcher);

            if (sServiceWatcher == null) {
                addException("onDisconnected() without a watcher");
                return;
            }

            if (sServiceWatcher.mService == null) {
                addException("onDisconnected(): no service on %s", sServiceWatcher);
                return;
            }
            sServiceWatcher.mDestroyed.countDown();
        }

        @NonNull
        public CtsTextClassifierService waitOnConnected() throws InterruptedException {
            await(mCreated, "not created");

            if (mService == null) {
                throw new IllegalStateException("not created");
            }
            return mService;
        }

        public void waitOnDisconnected() throws InterruptedException {
            await(mDestroyed, "not destroyed");
        }

        private void await(@NonNull CountDownLatch latch, @NonNull String fmt,
                @Nullable Object... args)
                throws InterruptedException {
            final boolean called = latch.await(GENERIC_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            if (!called) {
                throw new IllegalStateException(String.format(fmt, args)
                        + " in " + GENERIC_TIMEOUT_MS + "ms");
            }
        }
    }
}
