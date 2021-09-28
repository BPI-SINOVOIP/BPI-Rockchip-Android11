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

package com.android.cts.rollback.lib;

import static com.google.common.truth.Truth.assertThat;

import android.app.ActivityManager;
import android.app.AlarmManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.VersionedPackage;
import android.content.rollback.PackageRollbackInfo;
import android.content.rollback.RollbackInfo;
import android.content.rollback.RollbackManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.cts.install.lib.LocalIntentSender;
import com.android.cts.install.lib.TestApp;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.CountDownLatch;
import java.util.function.Predicate;
import java.util.function.Supplier;

/**
 * Utilities to facilitate testing rollbacks.
 */
public class RollbackUtils {

    private static final String TAG = "RollbackTest";

    /**
     * Time between repeated checks in {@link #retry}.
     */
    private static final long RETRY_CHECK_INTERVAL_MILLIS = 500;

    /**
     * Maximum number of checks in {@link #retry} before a timeout occurs.
     */
    private static final long RETRY_MAX_INTERVALS = 20;


    /**
     * Gets the RollbackManager for the instrumentation context.
     */
    public static RollbackManager getRollbackManager() {
        Context context = InstrumentationRegistry.getContext();
        RollbackManager rm = (RollbackManager) context.getSystemService(Context.ROLLBACK_SERVICE);
        if (rm == null) {
            throw new AssertionError("Failed to get RollbackManager");
        }
        return rm;
    }

    /**
     * Returns a rollback for the given rollback Id, if found. Otherwise, returns null.
     */
    private static RollbackInfo getRollbackById(List<RollbackInfo> rollbacks, int rollbackId) {
        for (RollbackInfo rollback :rollbacks) {
            if (rollback.getRollbackId() == rollbackId) {
                return rollback;
            }
        }
        return null;
    }

    /**
     * Returns an available rollback for the given package name. Returns null
     * if there are no available rollbacks, and throws an assertion if there
     * is more than one.
     */
    public static RollbackInfo getAvailableRollback(String packageName) {
        RollbackManager rm = getRollbackManager();
        return getUniqueRollbackInfoForPackage(rm.getAvailableRollbacks(), packageName);
    }

    /**
     * Returns a recently committed rollback for the given package name. Returns null
     * if there are no available rollbacks, and throws an assertion if there
     * is more than one.
     */
    public static RollbackInfo getCommittedRollback(String packageName) {
        RollbackManager rm = getRollbackManager();
        return getUniqueRollbackInfoForPackage(rm.getRecentlyCommittedRollbacks(), packageName);
    }

    /**
     * Returns a recently committed rollback for the given rollback Id.
     * Returns null if no committed rollback with a matching Id was found.
     */
    public static RollbackInfo getCommittedRollbackById(int rollbackId) {
        RollbackManager rm = getRollbackManager();
        return getRollbackById(rm.getRecentlyCommittedRollbacks(), rollbackId);
    }

    /**
     * Commit the given rollback.
     * @throws AssertionError if the rollback fails.
     */
    public static void rollback(int rollbackId, TestApp... causePackages)
            throws InterruptedException {
        List<VersionedPackage> causes = new ArrayList<>();
        for (TestApp cause : causePackages) {
            causes.add(cause.getVersionedPackage());
        }

        RollbackManager rm = getRollbackManager();
        rm.commitRollback(rollbackId, causes, LocalIntentSender.getIntentSender());
        Intent result = LocalIntentSender.getIntentSenderResult();
        int status = result.getIntExtra(RollbackManager.EXTRA_STATUS,
                RollbackManager.STATUS_FAILURE);
        if (status != RollbackManager.STATUS_SUCCESS) {
            String message = result.getStringExtra(RollbackManager.EXTRA_STATUS_MESSAGE);
            throw new AssertionError(message);
        }
    }

    /**
     * Forwards the device clock time by {@code offsetMillis}.
     */
    public static void forwardTimeBy(long offsetMillis) {
        setTime(System.currentTimeMillis() + offsetMillis);
        Log.i(TAG, "Forwarded time on device by " + offsetMillis + " millis");
    }

    /**
     * Returns the RollbackInfo with a given package in the list of rollbacks.
     * Throws an assertion failure if there is more than one such rollback
     * info. Returns null if there are no such rollback infos.
     */
    public static RollbackInfo getUniqueRollbackInfoForPackage(List<RollbackInfo> rollbacks,
            String packageName) {
        RollbackInfo found = null;
        for (RollbackInfo rollback : rollbacks) {
            for (PackageRollbackInfo info : rollback.getPackages()) {
                if (packageName.equals(info.getPackageName())) {
                    assertThat(found).isNull();
                    found = rollback;
                    break;
                }
            }
        }
        return found;
    }

    /**
     * Returns an available rollback matching the specified package name. If no such rollback is
     * available, getAvailableRollbacks is called repeatedly until one becomes available. An
     * assertion is raised if this does not occur after a certain number of checks.
     */
    public static RollbackInfo waitForAvailableRollback(String packageName)
            throws InterruptedException {
        return retry(() -> getAvailableRollback(packageName),
                Objects::nonNull, "Rollback did not become available.");
    }

    /**
     * If there is no available rollback matching the specified package name, this returns
     * immediately. If such a rollback is available, getAvailableRollbacks is called repeatedly
     * until it is no longer available. An assertion is raised if this does not occur after a
     * certain number of checks.
     */
    public static void waitForUnavailableRollback(String packageName) throws InterruptedException {
        retry(() -> getAvailableRollback(packageName), Objects::isNull,
                "Rollback did not become unavailable");
    }

    private static boolean hasRollbackInclude(List<RollbackInfo> rollbacks, String packageName) {
        return rollbacks.stream().anyMatch(
                ri -> ri.getPackages().stream().anyMatch(
                        pri -> packageName.equals(pri.getPackageName())));
    }

    /**
     * Retries until all rollbacks including {@code packageName} are gone. An assertion is raised if
     * this does not occur after a certain number of checks.
     */
    public static void waitForRollbackGone(
            Supplier<List<RollbackInfo>> supplier, String packageName) throws InterruptedException {
        retry(supplier, rollbacks -> !hasRollbackInclude(rollbacks, packageName),
                "Rollback containing " + packageName + " did not go away");
    }

    private static <T> T retry(Supplier<T> supplier, Predicate<T> predicate, String message)
            throws InterruptedException {
        for (int i = 0; i < RETRY_MAX_INTERVALS; i++) {
            T result = supplier.get();
            if (predicate.test(result)) {
                return result;
            }
            Thread.sleep(RETRY_CHECK_INTERVAL_MILLIS);
        }
        throw new AssertionError(message);
    }

    /**
     * Send broadcast to crash {@code packageName} {@code count} times. If {@code count} is at least
     * {@link PackageWatchdog#TRIGGER_FAILURE_COUNT}, watchdog crash detection will be triggered.
     */
    public static void sendCrashBroadcast(String packageName,
            int count) throws InterruptedException, IOException {
        for (int i = 0; i < count; ++i) {
            launchPackageForCrash(packageName);
        }
    }

    private static void setTime(long millis) {
        Context context = InstrumentationRegistry.getContext();
        AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        am.setTime(millis);
    }

    /**
     * Launches {@code packageName} with {@link Intent#ACTION_MAIN} and
     * waits for a CRASH broadcast from the launched app.
     */
    private static void launchPackageForCrash(String packageName)
            throws InterruptedException, IOException {
        // Force stop the package before launching it to make sure it isn't
        // stuck in a non-launchable state. And wait a second afterwards to
        // avoid interfering with when we launch the app.
        Log.i(TAG, "Force stopping " + packageName);
        Context context = InstrumentationRegistry.getContext();
        ActivityManager am = context.getSystemService(ActivityManager.class);
        am.forceStopPackage(packageName);
        Thread.sleep(1000);

        // Register a receiver to listen for the CRASH broadcast.
        CountDownLatch latch = new CountDownLatch(1);
        IntentFilter crashFilter = new IntentFilter();
        crashFilter.addAction("com.android.tests.rollback.CRASH");
        crashFilter.addCategory(Intent.CATEGORY_DEFAULT);
        BroadcastReceiver crashReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Log.i(TAG, "Received CRASH broadcast from " + packageName);
                latch.countDown();
            }
        };
        context.registerReceiver(crashReceiver, crashFilter);

        // Launch the app.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(packageName);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        Log.i(TAG, "Launching " + packageName + " with " + intent);
        context.startActivity(intent);

        Log.i(TAG, "Waiting for CRASH broadcast from " + packageName);
        latch.await();

        context.unregisterReceiver(crashReceiver);

        // Sleep long enough for packagewatchdog to be notified of crash
        Thread.sleep(1000);
    }
}

