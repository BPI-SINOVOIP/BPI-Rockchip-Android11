/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.sessioninspector;

import static com.android.cts.sessioninspector.Constants.ACTION_ABANDON_SESSION;
import static com.android.cts.sessioninspector.Constants.ACTION_CREATE_SESSION;
import static com.android.cts.sessioninspector.Constants.ACTION_GET_SESSION;
import static com.android.cts.sessioninspector.Constants.ACTIVITY_NAME;
import static com.android.cts.sessioninspector.Constants.REFERRER_URI;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.net.Uri;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.RemoteCallback;

import androidx.test.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.UUID;
import java.util.concurrent.TimeUnit;

@RunWith(JUnit4.class)
public class SessionInspectorTest {

    @Test
    public void testOnlyOwnerCanSee() throws Exception {
        String myPackage = Constants.PACKAGE_A.equals(getContext().getPackageName())
                ? Constants.PACKAGE_A : Constants.PACKAGE_B;
        String otherPackage = Constants.PACKAGE_A.equals(myPackage) ? Constants.PACKAGE_B
                : Constants.PACKAGE_A;

        int sessionId = createSession(myPackage);

        final PackageInstaller.SessionInfo sessionToMe = getSessionInfo(myPackage, sessionId);
        final PackageInstaller.SessionInfo sessionToOther = getSessionInfo(otherPackage, sessionId);

        abandonSession(myPackage, sessionId);

        assertEquals(REFERRER_URI, sessionToMe.getReferrerUri());
        assertNull(sessionToOther.getReferrerUri());
    }

    private Context getContext() {
        return InstrumentationRegistry.getContext();
    }

    private int createSession(String packageName) throws Exception {
        Bundle result = sendCommand(new Intent(ACTION_CREATE_SESSION).setComponent(
                new ComponentName(packageName, ACTIVITY_NAME)));
        return result.getInt(PackageInstaller.EXTRA_SESSION_ID, 0);
    }

    private PackageInstaller.SessionInfo getSessionInfo(String packageName, int session)
            throws Exception {
        Bundle result = sendCommand(new Intent(ACTION_GET_SESSION).putExtra(
                PackageInstaller.EXTRA_SESSION_ID, session).setComponent(
                new ComponentName(packageName, ACTIVITY_NAME)));
        return result.getParcelable(PackageInstaller.EXTRA_SESSION);
    }

    private void abandonSession(String packageName, int sessionId) throws Exception {
        sendCommand(new Intent(ACTION_ABANDON_SESSION).putExtra(PackageInstaller.EXTRA_SESSION_ID,
                sessionId).setComponent(new ComponentName(packageName, ACTIVITY_NAME)));
    }

    private Bundle sendCommand(Intent intent) throws Exception {
        ConditionVariable condition = new ConditionVariable();
        final Bundle[] resultHolder = new Bundle[1];
        RemoteCallback callback = new RemoteCallback(result -> {
            resultHolder[0] = result;
            condition.open();
        });
        intent.putExtra(Intent.EXTRA_RESULT_RECEIVER, callback);
        intent.setData(Uri.parse("https://" + UUID.randomUUID()));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        getContext().startActivity(intent);
        condition.block(TimeUnit.SECONDS.toMillis(10));
        Bundle result = resultHolder[0];
        if (result.containsKey("error")) {
            throw (Exception) result.getSerializable("error");
        }
        return result;
    }
}
