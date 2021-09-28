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

package android.accessibility.cts.common;

import static com.android.compatibility.common.util.TestUtils.waitOn;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.CallSuper;
import androidx.test.platform.app.InstrumentationRegistry;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public class InstrumentedAccessibilityService extends AccessibilityService {
    private static final String LOG_TAG = "InstrumentedA11yService";

    private static final boolean DEBUG = false;

    // Match com.android.server.accessibility.AccessibilityManagerService#COMPONENT_NAME_SEPARATOR
    private static final String COMPONENT_NAME_SEPARATOR = ":";
    private static final int TIMEOUT_SERVICE_PERFORM_SYNC = DEBUG ? Integer.MAX_VALUE : 10000;

    private static final HashMap<Class, WeakReference<InstrumentedAccessibilityService>>
            sInstances = new HashMap<>();

    private final Handler mHandler = new Handler();
    final Object mInterruptWaitObject = new Object();

    public boolean mOnInterruptCalled;

    // Timeout disabled in #DEBUG mode to prevent breakpoint-related failures
    public static final int TIMEOUT_SERVICE_ENABLE = DEBUG ? Integer.MAX_VALUE : 10000;

    @Override
    @CallSuper
    protected void onServiceConnected() {
        synchronized (sInstances) {
            sInstances.put(getClass(), new WeakReference<>(this));
            sInstances.notifyAll();
        }
        Log.v(LOG_TAG, "onServiceConnected ["  + this + "]");
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.v(LOG_TAG, "onUnbind [" + this + "]");
        return false;
    }

    @Override
    public void onDestroy() {
        synchronized (sInstances) {
            sInstances.remove(getClass());
        }
        Log.v(LOG_TAG, "onDestroy ["  + this + "]");
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        // Stub method.
    }

    @Override
    public void onInterrupt() {
        synchronized (mInterruptWaitObject) {
            mOnInterruptCalled = true;
            mInterruptWaitObject.notifyAll();
        }
    }

    public void disableSelfAndRemove() {
        disableSelf();

        synchronized (sInstances) {
            sInstances.remove(getClass());
        }
    }

    public void runOnServiceSync(Runnable runner) {
        final SyncRunnable sr = new SyncRunnable(runner, TIMEOUT_SERVICE_PERFORM_SYNC);
        mHandler.post(sr);
        assertTrue("Timed out waiting for runOnServiceSync()", sr.waitForComplete());
    }

    public <T extends Object> T getOnService(Callable<T> callable) {
        AtomicReference<T> returnValue = new AtomicReference<>(null);
        AtomicReference<Throwable> throwable = new AtomicReference<>(null);
        runOnServiceSync(
                () -> {
                    try {
                        returnValue.set(callable.call());
                    } catch (Throwable e) {
                        throwable.set(e);
                    }
                });
        if (throwable.get() != null) {
            throw new RuntimeException(throwable.get());
        }
        return returnValue.get();
    }

    public boolean wasOnInterruptCalled() {
        synchronized (mInterruptWaitObject) {
            return mOnInterruptCalled;
        }
    }

    public Object getInterruptWaitObject() {
        return mInterruptWaitObject;
    }

    private static final class SyncRunnable implements Runnable {
        private final CountDownLatch mLatch = new CountDownLatch(1);
        private final Runnable mTarget;
        private final long mTimeout;

        public SyncRunnable(Runnable target, long timeout) {
            mTarget = target;
            mTimeout = timeout;
        }

        public void run() {
            mTarget.run();
            mLatch.countDown();
        }

        public boolean waitForComplete() {
            try {
                return mLatch.await(mTimeout, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }
    }

    public static <T extends InstrumentedAccessibilityService> T enableService(
            Class<T> clazz) {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final String serviceName = clazz.getSimpleName();
        final Context context = instrumentation.getContext();
        final String enabledServices =
                Settings.Secure.getString(
                        context.getContentResolver(),
                        Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (enabledServices != null) {
            assertFalse("Service is already enabled", enabledServices.contains(serviceName));
        }
        final AccessibilityManager manager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        final List<AccessibilityServiceInfo> serviceInfos =
                manager.getInstalledAccessibilityServiceList();
        for (AccessibilityServiceInfo serviceInfo : serviceInfos) {
            final String serviceId = serviceInfo.getId();
            if (serviceId.endsWith(serviceName)) {
                ShellCommandBuilder.create(instrumentation)
                        .putSecureSetting(
                                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
                                enabledServices + COMPONENT_NAME_SEPARATOR + serviceId)
                        .putSecureSetting(Settings.Secure.ACCESSIBILITY_ENABLED, "1")
                        .run();

                final T instance = getInstanceForClass(clazz, TIMEOUT_SERVICE_ENABLE);
                if (instance == null) {
                    ShellCommandBuilder.create(instrumentation)
                            .putSecureSetting(
                                    Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES, enabledServices)
                            .run();
                    throw new RuntimeException(
                            "Starting accessibility service "
                                    + serviceName
                                    + " took longer than "
                                    + TIMEOUT_SERVICE_ENABLE
                                    + "ms");
                }
                return instance;
            }
        }
        throw new RuntimeException("Accessibility service " + serviceName + " not found");
    }

    public static <T extends InstrumentedAccessibilityService> T getInstanceForClass(
            Class<T> clazz, long timeoutMillis) {
        final long timeoutTimeMillis = SystemClock.uptimeMillis() + timeoutMillis;
        while (SystemClock.uptimeMillis() < timeoutTimeMillis) {
            synchronized (sInstances) {
                final T instance = getInstanceForClass(clazz);
                if (instance != null) {
                    return instance;
                }
                try {
                    sInstances.wait(timeoutTimeMillis - SystemClock.uptimeMillis());
                } catch (InterruptedException e) {
                    return null;
                }
            }
        }
        return null;
    }

    static <T extends InstrumentedAccessibilityService> T getInstanceForClass(
            Class<T> clazz) {
        synchronized (sInstances) {
            final WeakReference<InstrumentedAccessibilityService> ref = sInstances.get(clazz);
            if (ref != null) {
                final T instance = (T) ref.get();
                if (instance == null) {
                    sInstances.remove(clazz);
                } else {
                    return instance;
                }
            }
        }
        return null;
    }

    public static void disableAllServices() {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Object waitLockForA11yOff = new Object();
        final Context context = instrumentation.getContext();
        final AccessibilityManager manager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        // Updates to manager.isEnabled() aren't synchronized
        final AtomicBoolean accessibilityEnabled = new AtomicBoolean(manager.isEnabled());
        manager.addAccessibilityStateChangeListener(
                b -> {
                    synchronized (waitLockForA11yOff) {
                        waitLockForA11yOff.notifyAll();
                        accessibilityEnabled.set(b);
                    }
                });
        final UiAutomation uiAutomation = instrumentation.getUiAutomation(
                UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        ShellCommandBuilder.create(uiAutomation)
                .deleteSecureSetting(Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES)
                .deleteSecureSetting(Settings.Secure.ACCESSIBILITY_ENABLED)
                .run();
        uiAutomation.destroy();

        waitOn(waitLockForA11yOff, () -> !accessibilityEnabled.get(), TIMEOUT_SERVICE_ENABLE,
                "Accessibility turns off");
    }
}
