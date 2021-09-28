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

package com.android.car.garagemode;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.app.job.JobSnapshot;
import android.content.Intent;
import android.os.Handler;
import android.os.UserHandle;
import android.util.ArraySet;

import com.android.car.CarLocalServices;
import com.android.car.CarPowerManagementService;
import com.android.car.CarStatsLogHelper;
import com.android.car.user.CarUserService;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;

/**
 * Class that interacts with JobScheduler, controls system idleness and monitor jobs which are
 * in GarageMode interest
 */

class GarageMode {
    private static final Logger LOG = new Logger("GarageMode");

    /**
     * When changing this field value, please update
     * {@link com.android.server.job.controllers.idle.CarIdlenessTracker} as well.
     */
    public static final String ACTION_GARAGE_MODE_ON =
            "com.android.server.jobscheduler.GARAGE_MODE_ON";

    /**
     * When changing this field value, please update
     * {@link com.android.server.job.controllers.idle.CarIdlenessTracker} as well.
     */
    public static final String ACTION_GARAGE_MODE_OFF =
            "com.android.server.jobscheduler.GARAGE_MODE_OFF";

    @VisibleForTesting
    static final long JOB_SNAPSHOT_INITIAL_UPDATE_MS = 10_000; // 10 seconds

    private static final long JOB_SNAPSHOT_UPDATE_FREQUENCY_MS = 1_000; // 1 second
    private static final long USER_STOP_CHECK_INTERVAL = 10_000; // 10 secs
    private static final int ADDITIONAL_CHECKS_TO_DO = 1;

    private final Controller mController;
    private final JobScheduler mJobScheduler;
    private final Object mLock = new Object();
    private final Handler mHandler;

    private CarPowerManagementService mCarPowerManagementService;
    @GuardedBy("mLock")
    private boolean mGarageModeActive;
    @GuardedBy("mLock")
    private int mAdditionalChecksToDo = ADDITIONAL_CHECKS_TO_DO;
    @GuardedBy("mLock")
    private boolean mIdleCheckerIsRunning;
    @GuardedBy("mLock")
    private List<String> mPendingJobs = new ArrayList<>();

    private final Runnable mRunnable = new Runnable() {
        @Override
        public void run() {
            if (!mGarageModeActive) {
                LOG.d("Garage Mode is inactive. Stopping the idle-job checker.");
                finish();
                return;
            }
            int numberRunning = numberOfIdleJobsRunning();
            if (numberRunning > 0) {
                LOG.d("" + numberRunning + " jobs are still running. Need to wait more ...");
                synchronized (mLock) {
                    mAdditionalChecksToDo = ADDITIONAL_CHECKS_TO_DO;
                }
            } else {
                // No idle-mode jobs are running.
                // Are there any scheduled idle jobs that could run now?
                int numberReadyToRun = numberOfPendingJobs();
                if (numberReadyToRun == 0) {
                    LOG.d("No jobs are running. No jobs are pending. Exiting Garage Mode.");
                    finish();
                    return;
                }
                int numAdditionalChecks;
                synchronized (mLock) {
                    numAdditionalChecks = mAdditionalChecksToDo;
                    if (mAdditionalChecksToDo > 0) {
                        mAdditionalChecksToDo--;
                    }
                }
                if (numAdditionalChecks == 0) {
                    LOG.d("No jobs are running. Waited too long for "
                            + numberReadyToRun + " pending jobs. Exiting Garage Mode.");
                    finish();
                    return;
                }
                LOG.d("No jobs are running. Waiting " + numAdditionalChecks
                        + " more cycles for " + numberReadyToRun + " pending jobs.");
            }
            mHandler.postDelayed(mRunnable, JOB_SNAPSHOT_UPDATE_FREQUENCY_MS);
        }
    };

    private final Runnable mStopUserCheckRunnable = new Runnable() {
        @Override
        public void run() {
            int userToStop = UserHandle.USER_SYSTEM; // BG user never becomes system user.
            int remainingUsersToStop = 0;
            synchronized (mLock) {
                remainingUsersToStop = mStartedBackgroundUsers.size();
                if (remainingUsersToStop < 1) {
                    return;
                }
                userToStop = mStartedBackgroundUsers.valueAt(0);
            }
            if (numberOfIdleJobsRunning() == 0) { // all jobs done or stopped.
                // Keep user until job scheduling is stopped. Otherwise, it can crash jobs.
                if (userToStop != UserHandle.USER_SYSTEM) {
                    CarLocalServices.getService(CarUserService.class).stopBackgroundUser(
                            userToStop);
                    LOG.i("Stopping background user:" + userToStop + " remaining users:"
                            + (remainingUsersToStop - 1));
                }
                synchronized (mLock) {
                    mStartedBackgroundUsers.remove(userToStop);
                    if (mStartedBackgroundUsers.size() == 0) {
                        LOG.i("All background users have stopped");
                        return;
                    }
                }
            } else {
                LOG.i("Waiting for jobs to finish, remaining users:" + remainingUsersToStop);
            }
            // Poll again
            mHandler.postDelayed(mStopUserCheckRunnable, USER_STOP_CHECK_INTERVAL);
        }
    };

    @GuardedBy("mLock")
    private CompletableFuture<Void> mFuture;
    @GuardedBy("mLock")
    private ArraySet<Integer> mStartedBackgroundUsers = new ArraySet<>();

    GarageMode(Controller controller) {
        mGarageModeActive = false;
        mController = controller;
        mJobScheduler = controller.getJobSchedulerService();
        mHandler = controller.getHandler();
    }

    boolean isGarageModeActive() {
        synchronized (mLock) {
            return mGarageModeActive;
        }
    }

    List<String> dump() {
        List<String> outString = new ArrayList<>();
        if (!mGarageModeActive) {
            return outString;
        }
        outString.add("GarageMode idle checker is " + (mIdleCheckerIsRunning ? "" : "not ")
                + "running");
        List<String> jobList = new ArrayList<>();
        int numJobs = getListOfIdleJobsRunning(jobList);
        if (numJobs > 0) {
            outString.add("GarageMode is waiting for " + numJobs + " jobs:");
            // Dump the names of the jobs that we are waiting for
            for (int idx = 0; idx < jobList.size(); idx++) {
                outString.add("   " + (idx + 1) + ": " + jobList.get(idx));
            }
        } else {
            // Dump the names of the pending jobs that we are waiting for
            numJobs = getListOfPendingJobs(jobList);
            outString.add("GarageMode is waiting for " + jobList.size() + " pending idle jobs:");
            for (int idx = 0; idx < jobList.size(); idx++) {
                outString.add("   " + (idx + 1) + ": " + jobList.get(idx));
            }
        }
        return outString;
    }

    void enterGarageMode(CompletableFuture<Void> future) {
        LOG.d("Entering GarageMode");
        if (mCarPowerManagementService == null) {
            mCarPowerManagementService = CarLocalServices.getService(
                    CarPowerManagementService.class);
        }
        if (mCarPowerManagementService != null
                && mCarPowerManagementService.garageModeShouldExitImmediately()) {
            if (future != null && !future.isDone()) {
                future.complete(null);
            }
            synchronized (mLock) {
                mGarageModeActive = false;
            }
            return;
        }
        synchronized (mLock) {
            mGarageModeActive = true;
        }
        updateFuture(future);
        broadcastSignalToJobScheduler(true);
        CarStatsLogHelper.logGarageModeStart();
        startMonitoringThread();
        ArrayList<Integer> startedUsers =
                CarLocalServices.getService(CarUserService.class).startAllBackgroundUsers();
        synchronized (mLock) {
            mStartedBackgroundUsers.addAll(startedUsers);
        }
    }

    void cancel() {
        broadcastSignalToJobScheduler(false);
        synchronized (mLock) {
            if (mFuture == null) {
                cleanupGarageMode();
            } else if (!mFuture.isDone()) {
                mFuture.cancel(true);
            }
            mFuture = null;
            startBackgroundUserStoppingLocked();
        }
    }

    void finish() {
        synchronized (mLock) {
            if (!mIdleCheckerIsRunning) {
                LOG.i("Finishing Garage Mode. Idle checker is not running.");
                return;
            }
            mIdleCheckerIsRunning = false;
        }
        broadcastSignalToJobScheduler(false);
        CarStatsLogHelper.logGarageModeStop();
        mController.scheduleNextWakeup();
        synchronized (mLock) {
            if (mFuture == null) {
                cleanupGarageMode();
            } else if (!mFuture.isDone()) {
                mFuture.complete(null);
            }
            mFuture = null;
            startBackgroundUserStoppingLocked();
        }
    }

    private void cleanupGarageMode() {
        LOG.d("Cleaning up GarageMode");
        synchronized (mLock) {
            mGarageModeActive = false;
            stopMonitoringThread();
            if (mIdleCheckerIsRunning) {
                // The idle checker has not completed.
                // Schedule it now so it completes promptly.
                mHandler.post(mRunnable);
            }
            startBackgroundUserStoppingLocked();
        }
    }

    private void startBackgroundUserStoppingLocked() {
        if (mStartedBackgroundUsers.size() > 0) {
            mHandler.postDelayed(mStopUserCheckRunnable, USER_STOP_CHECK_INTERVAL);
        }
    }

    private void updateFuture(CompletableFuture<Void> future) {
        synchronized (mLock) {
            mFuture = future;
            if (mFuture != null) {
                mFuture.whenComplete((result, exception) -> {
                    if (exception == null) {
                        LOG.d("GarageMode completed normally");
                    } else if (exception instanceof CancellationException) {
                        LOG.d("GarageMode was canceled");
                    } else {
                        LOG.e("GarageMode ended due to exception: ", exception);
                    }
                    cleanupGarageMode();
                });
            }
        }
    }

    private void broadcastSignalToJobScheduler(boolean enableGarageMode) {
        Intent i = new Intent();
        i.setAction(enableGarageMode ? ACTION_GARAGE_MODE_ON : ACTION_GARAGE_MODE_OFF);
        i.setFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY | Intent.FLAG_RECEIVER_NO_ABORT);
        mController.sendBroadcast(i);
    }

    private void startMonitoringThread() {
        synchronized (mLock) {
            mIdleCheckerIsRunning = true;
            mAdditionalChecksToDo = ADDITIONAL_CHECKS_TO_DO;
        }
        mHandler.postDelayed(mRunnable, JOB_SNAPSHOT_INITIAL_UPDATE_MS);
    }

    private void stopMonitoringThread() {
        mHandler.removeCallbacks(mRunnable);
    }

    private int numberOfIdleJobsRunning() {
        return getListOfIdleJobsRunning(null);
    }

    private int getListOfIdleJobsRunning(List<String> jobList) {
        if (jobList != null) {
            jobList.clear();
        }
        List<JobInfo> startedJobs = mJobScheduler.getStartedJobs();
        if (startedJobs == null) {
            return 0;
        }
        int count = 0;
        for (int idx = 0; idx < startedJobs.size(); idx++) {
            JobInfo jobInfo = startedJobs.get(idx);
            if (jobInfo.isRequireDeviceIdle()) {
                count++;
                if (jobList != null) {
                    jobList.add(jobInfo.toString());
                }
            }
        }
        return count;
    }

    private int numberOfPendingJobs() {
        return getListOfPendingJobs(null);
    }

    private int getListOfPendingJobs(List<String> jobList) {
        if (jobList != null) {
            jobList.clear();
        }
        List<JobSnapshot> allScheduledJobs = mJobScheduler.getAllJobSnapshots();
        if (allScheduledJobs == null) {
            return 0;
        }
        int numberPending = 0;
        for (int idx = 0; idx < allScheduledJobs.size(); idx++) {
            JobSnapshot scheduledJob = allScheduledJobs.get(idx);
            JobInfo jobInfo = scheduledJob.getJobInfo();
            if (scheduledJob.isRunnable() && jobInfo.isRequireDeviceIdle()) {
                numberPending++;
                if (jobList != null) {
                    jobList.add(jobInfo.toString());
                }
            }
        }
        return numberPending;
    }
}
