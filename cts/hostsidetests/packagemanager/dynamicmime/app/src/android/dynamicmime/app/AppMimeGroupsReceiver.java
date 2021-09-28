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

package android.dynamicmime.app;

import static android.dynamicmime.common.Constants.ACTION_REQUEST;
import static android.dynamicmime.common.Constants.ACTION_RESPONSE;
import static android.dynamicmime.common.Constants.EXTRA_GROUP;
import static android.dynamicmime.common.Constants.EXTRA_MIMES;
import static android.dynamicmime.common.Constants.EXTRA_REQUEST;
import static android.dynamicmime.common.Constants.EXTRA_RESPONSE;
import static android.dynamicmime.common.Constants.REQUEST_GET;
import static android.dynamicmime.common.Constants.REQUEST_SET;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.ArraySet;

import java.util.Set;

/**
 * Receiver of {@link android.dynamicmime.testapp.util.AppMimeGroups} commands
 */
public class AppMimeGroupsReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        if (!ACTION_REQUEST.equals(intent.getAction())) {
            return;
        }

        String mimeGroup = intent.getStringExtra(EXTRA_GROUP);
        String[] mimeTypes = intent.getStringArrayExtra(EXTRA_MIMES);
        int requestCode = intent.getIntExtra(EXTRA_REQUEST, -1);

        Intent response = new Intent(ACTION_RESPONSE);
        try {
            handleRequest(context, mimeGroup, mimeTypes, requestCode, response);
            context.sendBroadcast(response);
        } catch (IllegalArgumentException ignored) {
        }
    }

    private void handleRequest(Context context, String mimeGroup, String[] mimeTypes,
            int requestCode, Intent response) {
        switch (requestCode) {
            case REQUEST_SET:
                context.getPackageManager().setMimeGroup(mimeGroup, new ArraySet<>(mimeTypes));
                break;
            case REQUEST_GET:
                response.putExtra(EXTRA_RESPONSE, getMimeGroup(context, mimeGroup));
                break;
            default:
                throw new IllegalArgumentException("Unexpected request");
        }
    }

    private String[] getMimeGroup(Context context, String mimeGroup) {
        Set<String> mimeTypes = context.getPackageManager().getMimeGroup(mimeGroup);
        return mimeTypes != null ? mimeTypes.toArray(new String[0]) : null;
    }
}
