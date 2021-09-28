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

import static android.content.Intent.EXTRA_RESULT_RECEIVER;
import static android.content.pm.PackageInstaller.EXTRA_SESSION;
import static android.content.pm.PackageInstaller.EXTRA_SESSION_ID;
import static android.content.pm.PackageInstaller.SessionInfo;
import static android.content.pm.PackageInstaller.SessionParams;
import static android.content.pm.PackageInstaller.SessionParams.MODE_FULL_INSTALL;

import static com.android.cts.sessioninspector.Constants.ACTION_ABANDON_SESSION;
import static com.android.cts.sessioninspector.Constants.ACTION_CREATE_SESSION;
import static com.android.cts.sessioninspector.Constants.ACTION_GET_SESSION;
import static com.android.cts.sessioninspector.Constants.REFERRER_URI;

import android.app.Activity;
import android.os.Bundle;
import android.os.RemoteCallback;

public class SessionInspectorActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        RemoteCallback remoteCallback = getIntent().getParcelableExtra(EXTRA_RESULT_RECEIVER);
        final Bundle result = new Bundle();
        String action = getIntent().getAction();
        try {
            switch (action) {
                case ACTION_CREATE_SESSION:
                    SessionParams params = new SessionParams(MODE_FULL_INSTALL);
                    params.setReferrerUri(REFERRER_URI);
                    final int session =
                            getPackageManager().getPackageInstaller().createSession(params);
                    result.putInt(EXTRA_SESSION_ID, session);
                    break;
                case ACTION_GET_SESSION: {
                    final int sessionId = getIntent().getIntExtra(EXTRA_SESSION_ID, 0);
                    final SessionInfo sessionInfo =
                            getPackageManager().getPackageInstaller().getSessionInfo(sessionId);
                    result.putParcelable(EXTRA_SESSION, sessionInfo);
                    break;
                }
                case ACTION_ABANDON_SESSION: {
                    final int sessionId = getIntent().getIntExtra(EXTRA_SESSION_ID, 0);
                    getPackageManager().getPackageInstaller().abandonSession(sessionId);
                    break;
                }
                default:
                    throw new IllegalArgumentException("Unrecognized action: " + action);
            }
        } catch (Exception e) {
            result.putSerializable("error", e);
        }
        remoteCallback.sendResult(result);
        finish();
    }
}
