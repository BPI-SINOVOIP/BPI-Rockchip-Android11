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

package com.android.server.telecom;

import android.annotation.IntRange;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.UiModeManager;
import android.telecom.Log;
import android.util.LocalLog;

import com.android.internal.util.IndentingPrintWriter;

import java.util.Comparator;
import java.util.List;
import java.util.Objects;
import java.util.PriorityQueue;
import java.util.stream.Collectors;

/**
 * Tracks the package names of apps which enter end exit car mode.
 */
public class CarModeTracker {
    /**
     * Data class holding information about apps which have requested to enter car mode.
     */
    private class CarModeApp {
        private @IntRange(from = 0) int mPriority;
        private @NonNull String mPackageName;

        public CarModeApp(int priority, @NonNull String packageName) {
            mPriority = priority;
            mPackageName = Objects.requireNonNull(packageName);
        }

        /**
         * The priority at which the app requested to enter car mode.
         * Will be the same as the one specified when {@link UiModeManager#enableCarMode(int, int)}
         * was called, or {@link UiModeManager#DEFAULT_PRIORITY} if no priority was specifeid.
         * @return The priority.
         */
        public int getPriority() {
            return mPriority;
        }

        public void setPriority(int priority) {
            mPriority = priority;
        }

        /**
         * @return The package name of the app which requested to enter car mode.
         */
        public String getPackageName() {
            return mPackageName;
        }

        public void setPackageName(String packageName) {
            mPackageName = packageName;
        }
    }

    /**
     * Comparator used to maintain the car mode priority queue ordering.
     */
    private class CarModeAppComparator implements Comparator<CarModeApp> {
        @Override
        public int compare(CarModeApp o1, CarModeApp o2) {
            // highest priority takes precedence.
            return Integer.compare(o2.getPriority(), o1.getPriority());
        }
    }

    /**
     * Priority list of apps which have entered or exited car mode, ordered with the highest
     * priority app at the top of the queue.  Where items have the same priority, they are ordered
     * by insertion time.
     */
    private PriorityQueue<CarModeApp> mCarModeApps = new PriorityQueue<>(2,
            new CarModeAppComparator());

    private final LocalLog mCarModeChangeLog = new LocalLog(20);

    /**
     * Handles a request to enter car mode by a package name.
     * @param priority The priority at which car mode is entered.
     * @param packageName The package name of the app entering car mode.
     */
    public void handleEnterCarMode(@IntRange(from = 0) int priority, @NonNull String packageName) {
        if (mCarModeApps.stream().anyMatch(c -> c.getPriority() == priority)) {
            Log.w(this, "handleEnterCarMode: already in car mode at priority %d (apps: %s)",
                    priority, getCarModePriorityString());
            return;
        }

        if (mCarModeApps.stream().anyMatch(c -> c.getPackageName().equals(packageName))) {
            Log.w(this, "handleEnterCarMode: %s is already in car mode (apps: %s)",
                    packageName, getCarModePriorityString());
            return;
        }

        Log.i(this, "handleEnterCarMode: packageName=%s, priority=%d", packageName, priority);
        mCarModeChangeLog.log("enterCarMode: packageName=" + packageName + ", priority="
                + priority);
        mCarModeApps.add(new CarModeApp(priority, packageName));
    }

    /**
     * Handles a request to exist car mode at a priority level.
     * @param priority The priority level.
     * @param packageName The packagename of the app requesting the change.
     */
    public void handleExitCarMode(@IntRange(from = 0) int priority, @NonNull String packageName) {
        if (!mCarModeApps.stream().anyMatch(c -> c.getPriority() == priority)) {
            Log.w(this, "handleExitCarMode: not in car mode at priority %d (apps=%s)",
                    priority, getCarModePriorityString());
            return;
        }

        if (priority != UiModeManager.DEFAULT_PRIORITY && !mCarModeApps.stream().anyMatch(
                c -> c.getPackageName().equals(packageName) && c.getPriority() == priority)) {
            Log.w(this, "handleExitCarMode: %s didn't enter car mode at priority %d (apps=%s)",
                    packageName, priority, getCarModePriorityString());
            return;
        }

        Log.i(this, "handleExitCarMode: packageName=%s, priority=%d", packageName, priority);
        mCarModeChangeLog.log("exitCarMode: packageName=" + packageName + ", priority="
                + priority);
        mCarModeApps.removeIf(c -> c.getPriority() == priority);
    }

    /**
     * Retrieves a list of the apps which are currently in car mode, ordered by priority such that
     * the highest priority app is first.
     * @return List of apps in car mode.
     */
    public @NonNull List<String> getCarModeApps() {
        return mCarModeApps
                .stream()
                .sorted(mCarModeApps.comparator())
                .map(cma -> cma.getPackageName())
                .collect(Collectors.toList());
    }

    private @NonNull String getCarModePriorityString() {
        return mCarModeApps
                .stream()
                .sorted(mCarModeApps.comparator())
                .map(cma -> "[" + cma.getPriority() + ", " + cma.getPackageName() + "]")
                .collect(Collectors.joining(", "));
    }

    /**
     * Gets the app which is currently in car mode.  This is the highest priority app which has
     * entered car mode.
     * @return The app which is in car mode.
     */
    public @Nullable String getCurrentCarModePackage() {
        CarModeApp app = mCarModeApps.peek();
        return app == null ? null : app.getPackageName();
    }

    /**
     * @return {@code true} if the device is in car mode, {@code false} otherwise.
     */
    public boolean isInCarMode() {
        return !mCarModeApps.isEmpty();
    }

    /**
     * Dumps the state of the car mode tracker to the specified print writer.
     * @param pw
     */
    public void dump(IndentingPrintWriter pw) {
        pw.println("CarModeTracker:");
        pw.increaseIndent();

        pw.println("Current car mode apps:");
        pw.increaseIndent();
        for (CarModeApp app : mCarModeApps) {
            pw.print("[");
            pw.print(app.getPriority());
            pw.print("] ");
            pw.println(app.getPackageName());
        }
        pw.decreaseIndent();

        pw.println("Car mode history:");
        pw.increaseIndent();
        mCarModeChangeLog.dump(pw);
        pw.decreaseIndent();

        pw.decreaseIndent();
    }
}
