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

package android.telephony.cts.sms23;

import android.app.Activity;
import android.database.Cursor;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.provider.Telephony.Sms;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Cursor cursor = null;
        Throwable t = null;
        try {
            cursor = getContentResolver()
                    .query(Sms.CONTENT_URI, null, null, null, null);
        } catch (Throwable ex) {
            t = ex;
        } finally {
            Bundle result = new Bundle();
            result.putString("class", getClass().getName());
            result.putInt("queryCount", cursor == null ? -1 : cursor.getCount());
            result.putString("exceptionMessage",
                    t == null ? null : t.getMessage());
            getIntent().<RemoteCallback>getParcelableExtra("callback").sendResult(result);

            if (cursor != null) {
                cursor.close();
            }
            finish();
        }
    }
}
