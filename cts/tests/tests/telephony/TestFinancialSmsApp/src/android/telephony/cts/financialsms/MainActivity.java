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

package android.telephony.cts.financialsms;

import android.app.Activity;
import android.database.CursorWindow;
import android.os.Bundle;
import android.os.RemoteCallback;
import android.telephony.SmsManager;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class MainActivity extends Activity {
    private static int rowNum = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            SmsManager.getDefault().getSmsMessagesForFinancialApp(new Bundle(),
                    getMainExecutor(),
                    new SmsManager.FinancialSmsCallback() {
                        public void onFinancialSmsMessages(CursorWindow msgs) {
                            rowNum = -1;
                            if (msgs != null) {
                                rowNum = msgs.getNumRows();
                            }
                    }});
        } catch (Exception e) {
            Log.w("MainActivity", "received Exception e:" + e);
        }finally {
            Bundle result = new Bundle();
            result.putString("class", getClass().getName());
            result.putInt("rowNum", rowNum);
            getIntent().<RemoteCallback>getParcelableExtra("callback").sendResult(result);
        }
        finish();
    }
}
