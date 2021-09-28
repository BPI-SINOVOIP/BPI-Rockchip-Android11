/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tradefed.util;

/** Used to store process related(USER, PID, NAME, START TIME IN SECOND SINCE EPOCH) information. */
public class ProcessInfo {

    private String mUser;
    private int mPid;
    private String mName;
    private long mStartTime;

    /**
     * Constructs the process info object based on the user, process id and name of the process.
     *
     * @param user username of process owner
     * @param pid process id number
     * @param name process name
     */
    public ProcessInfo(String user, int pid, String name) {
        mUser = user;
        mPid = pid;
        mName = name;
    }

    /**
     * Constructs the process info object based on the user, process id, name of the process,
     * process start time.
     *
     * @param user username of process owner
     * @param pid process id number
     * @param name process name
     * @param startTime process start time in second since epoch
     */
    public ProcessInfo(String user, int pid, String name, long startTime) {
        mUser = user;
        mPid = pid;
        mName = name;
        mStartTime = startTime;
    }

    /** Returns the username of the process's owner. */
    public String getUser() {
        return mUser;
    }

    /** Returns the process ID number. */
    public int getPid() {
        return mPid;
    }

    /** Returns the process name. */
    public String getName() {
        return mName;
    }

    /**
     * Returns the process start time in second since epoch. For device process, the start time
     * would use device time
     */
    public long getStartTime() {
        return mStartTime;
    }

    @Override
    public String toString() {
        return "ProcessInfo [mUser="
                + mUser
                + ", mPid="
                + mPid
                + ", mName="
                + mName
                + ", mStartTime="
                + mStartTime
                + "]";
    }
}

