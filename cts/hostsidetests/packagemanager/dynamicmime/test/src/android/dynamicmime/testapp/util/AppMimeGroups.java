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

package android.dynamicmime.testapp.util;

import static android.dynamicmime.common.Constants.ACTION_REQUEST;
import static android.dynamicmime.common.Constants.ACTION_RESPONSE;
import static android.dynamicmime.common.Constants.EXTRA_GROUP;
import static android.dynamicmime.common.Constants.EXTRA_MIMES;
import static android.dynamicmime.common.Constants.EXTRA_REQUEST;
import static android.dynamicmime.common.Constants.EXTRA_RESPONSE;
import static android.dynamicmime.common.Constants.REQUEST_GET;
import static android.dynamicmime.common.Constants.REQUEST_SET;

import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.content.Intent;

import com.android.compatibility.common.util.BlockingBroadcastReceiver;

import java.util.concurrent.TimeUnit;

/**
 * Class that dispatches MIME groups related commands to another app
 * Target app should register {@link android.dynamicmime.app.AppMimeGroupsReceiver}
 * to handler requests
 */
public class AppMimeGroups {
    private final Context mContext;
    private final String mTargetPackage;

    private AppMimeGroups(Context context, String targetPackage) {
        mContext = context;
        mTargetPackage = targetPackage;
    }

    public static AppMimeGroups with(Context context, String targetPackage) {
        return new AppMimeGroups(context, targetPackage);
    }

    public void set(String mimeGroup, String[] mimeTypes) {
        sendRequestAndAwait(mimeGroup, REQUEST_SET, mimeTypes);
    }

    public String[] get(String mimeGroup) {
        return sendRequestAndAwait(mimeGroup, REQUEST_GET)
                .getStringArrayExtra(EXTRA_RESPONSE);
    }

    private Intent sendRequestAndAwait(String mimeGroup, int requestSet) {
        return sendRequestAndAwait(mimeGroup, requestSet, null);
    }

    private Intent sendRequestAndAwait(String mimeGroup, int request, String[] mimeTypes) {
        BlockingBroadcastReceiver receiver =
                new BlockingBroadcastReceiver(mContext, ACTION_RESPONSE);
        receiver.register();

        mContext.sendBroadcast(getRequestIntent(mimeGroup, mimeTypes, request));

        Intent response = receiver.awaitForBroadcast(TimeUnit.SECONDS.toMillis(60L));

        mContext.unregisterReceiver(receiver);

        assertNotNull(response);

        return response;
    }

    private Intent getRequestIntent(String mimeGroup, String[] mimeTypes, int request) {
        Intent intent = new Intent(ACTION_REQUEST);
        intent.setPackage(mTargetPackage);
        intent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);

        intent.putExtra(EXTRA_GROUP, mimeGroup);
        intent.putExtra(EXTRA_MIMES, mimeTypes);
        intent.putExtra(EXTRA_REQUEST, request);

        return intent;
    }
}
