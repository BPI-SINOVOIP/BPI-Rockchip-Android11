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

package android.externalservice.common;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * A class that encapsulates information about a running service process
 * and information about how it was created.
 */
public class RunningServiceInfo implements Parcelable {
    /** The UNIX user ID of the running service process. */
    public int uid;

    /** The UNIX process ID of the running service process. */
    public int pid;

    /** The package name that the process is running under. */
    public String packageName;

    /** The value reported from the test's ZygotePreload.getZygotePid(). */
    public int zygotePid;

    /** The value reported from the test's ZygotePreload.getZygotePackage(). */
    public String zygotePackage;

    public void writeToParcel(Parcel dest, int parcelableFlags) {
        dest.writeInt(uid);
        dest.writeInt(pid);
        dest.writeString(packageName);
        dest.writeInt(zygotePid);
        dest.writeString(zygotePackage);
    }

    public static final Parcelable.Creator<RunningServiceInfo> CREATOR
            = new Parcelable.Creator<RunningServiceInfo>() {
        public RunningServiceInfo createFromParcel(Parcel source) {
            RunningServiceInfo info = new RunningServiceInfo();
            info.uid = source.readInt();
            info.pid = source.readInt();
            info.packageName = source.readString();
            info.zygotePid = source.readInt();
            info.zygotePackage = source.readString();
            return info;
        }
        public RunningServiceInfo[] newArray(int size) {
            return new RunningServiceInfo[size];
        }
    };

    public int describeContents() {
        return 0;
    }
}
