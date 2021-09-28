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

package com.android.cts.domainsocketapp;

import android.app.Activity;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.Bundle;
import android.util.Log;

import java.io.OutputStream;
import java.io.IOException;

public class DomainSocketActivity extends Activity {

    private static final String DOMAIN_SOCKET_NAME = "RunasConnectAppSocket";

    private static final String MAGIC_MESSAGE = "Connection Succeeded.";

    private static final String TAG = DomainSocketActivity.class.getSimpleName();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        new Thread() {
            public void run() {
                try {
                    new LocalServerSocket(DOMAIN_SOCKET_NAME).accept().getOutputStream()
                            .write(MAGIC_MESSAGE.getBytes());
                } catch (IOException e) {
                    Log.e(TAG, e.getMessage());
                }
            }
        }.start();
    }
}
