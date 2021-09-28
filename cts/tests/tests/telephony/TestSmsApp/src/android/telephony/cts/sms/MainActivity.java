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

package android.telephony.cts.sms;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.provider.Telephony.Sms;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

public class MainActivity extends Activity {

    public static final int REQUEST_CODE_READ_SMS = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestPermissions(
                new String[]{android.Manifest.permission.READ_SMS}, REQUEST_CODE_READ_SMS);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
            int[] grantResults) {
        if (requestCode == REQUEST_CODE_READ_SMS) {
            Cursor cursor = null;
            Throwable t = null;
            if (grantResults[0] != PackageManager.PERMISSION_GRANTED) {
                t = new SecurityException("Permission denied");
            }
            try {
                cursor = getContentResolver()
                        .query(Sms.CONTENT_URI, null, null, null, null);
            } catch (Throwable ex) {
                List<Throwable> causes = causes(ex);
                causes.get(causes.size() - 1).initCause(t);
                t = ex;
            } finally {
                Bundle result = new Bundle();
                result.putString("class", getClass().getName());
                result.putInt("queryCount", cursor == null ? -1 : cursor.getCount());
                result.putString("exceptionMessage",
                        causes(t).stream().map(Throwable::getMessage)
                                .collect(Collectors.joining("\n")));
                getIntent().<RemoteCallback>getParcelableExtra("callback").sendResult(result);

                if (cursor != null) {
                    cursor.close();
                }
                finish();
            }
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    private static List<Throwable> causes(Throwable t) {
        ArrayList<Throwable> result = new ArrayList<>();
        while (t != null) {
            result.add(t);
            t = t.getCause();
        }
        return result;
    }
}