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

package com.android.cts.install.lib.testapp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Process;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Scanner;

/**
 * A broadcast receiver to check for and update user app data version
 * and user handle compatibility.
 */
public class ProcessUserData extends BroadcastReceiver {

    /**
     * Exception thrown in case of issue with user data.
     */
    public static class UserDataException extends Exception {
        public UserDataException(String message) {
            super(message);
        }

        public UserDataException(String message, Throwable cause) {
           super(message, cause);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals("PROCESS_USER_DATA")) {
            try {
                processUserData(context);
                setResultCode(1);
            } catch (UserDataException e) {
                setResultCode(0);
                setResultData(e.getMessage());
            }
        } else if (intent.getAction().equals("GET_USER_DATA_VERSION")) {
            setResultCode(getUserDataVersion(context));
        }
    }

    /**
     * Update the app's user data version to match the app version, and confirm
     * the user data is for the correct user.
     *
     * @param context The application context.
     * @throws UserDataException in case of problems with app user data.
     */
    public void processUserData(Context context) throws UserDataException {
        Resources res = context.getResources();
        String packageName = context.getPackageName();

        String userHandle = Process.myUserHandle().toString();

        int appVersionId = res.getIdentifier("app_version", "integer", packageName);
        int appVersion = res.getInteger(appVersionId);

        int splitVersionId = res.getIdentifier("split_version", "integer", packageName);
        int splitVersion = res.getInteger(splitVersionId);

        // Make sure the app version and split versions are compatible.
        if (appVersion != splitVersion) {
            throw new UserDataException("Split version " + splitVersion
                    + " does not match app version " + appVersion);
        }

        // Read the version of the app's user data and ensure it is compatible
        // with our version of the application. Also ensure that the user data is
        // for the correct user.
        File versionFile = new File(context.getFilesDir(), "userdata.txt");
        try {
            Scanner s = new Scanner(versionFile);
            int userDataVersion = s.nextInt();
            s.nextLine();

            if (userDataVersion > appVersion) {
                throw new UserDataException("User data is from version " + userDataVersion
                        + ", which is not compatible with this version " + appVersion
                        + " of the RollbackTestApp");
            }

            String readUserHandle = s.nextLine();
            s.close();

            if (!readUserHandle.equals(userHandle)) {
                throw new UserDataException("User handle expected to be: " + userHandle
                        + ", but was actually " + readUserHandle);
            }
        } catch (FileNotFoundException e) {
            // No problem. This is a fresh install of the app or the user data
            // has been wiped.
        }

        // Record the current version of the app in the user data.
        try {
            PrintWriter pw = new PrintWriter(versionFile);
            pw.println(appVersion);
            pw.println(userHandle);
            pw.close();
        } catch (IOException e) {
            throw new UserDataException("Unable to write user data.", e);
        }
    }

    /**
     * Return the app's user data version or -1 if userdata.txt doesn't exist.
     */
    private int getUserDataVersion(Context context) {
        File versionFile = new File(context.getFilesDir(), "userdata.txt");
        try (Scanner s = new Scanner(versionFile);) {
            int dataVersion = s.nextInt();
            return dataVersion;
        } catch (FileNotFoundException e) {
            // No problem. This is a fresh install of the app or the user data
            // has been wiped.
            return -1;
        }
    }
}
