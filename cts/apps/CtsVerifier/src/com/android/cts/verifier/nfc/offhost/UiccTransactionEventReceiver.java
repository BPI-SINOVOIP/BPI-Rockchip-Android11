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

package com.android.cts.verifier.nfc.offhost;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.nfc.NfcAdapter;
import android.util.Log;

import com.android.cts.verifier.nfc.hce.HceUtils;

import java.util.Arrays;

public class UiccTransactionEventReceiver extends BroadcastReceiver {
    final byte[] transactionData1 = new byte[]{0x54};
    final byte[] transactionData2 = new byte[]{0x52};
    final byte[] transactionData3 = new byte[]{0x02};
    @Override
    public void onReceive(Context context, Intent intent) {
        Log.d("UiccTransactionEventReceiver", "onReceive ");
        if(intent != null){
            Log.d("UiccTransactionEventReceiver", "uri : " + intent.getDataString());
            byte[] transactionData = intent.getByteArrayExtra(NfcAdapter.EXTRA_DATA);
            Log.d("UiccTransactionEventReceiver", "data " + HceUtils.getHexBytes(null, transactionData));
            if (Arrays.equals(transactionData, transactionData1)) {
                intent.setClassName(context.getPackageName(), UiccTransactionEvent1EmulatorActivity.class.getName());
            } else if (Arrays.equals(transactionData, transactionData2)) {
                intent.setClassName(context.getPackageName(), UiccTransactionEvent2EmulatorActivity.class.getName());
            } else if (Arrays.equals(transactionData, transactionData3)) {
                intent.setClassName(context.getPackageName(), UiccTransactionEvent3EmulatorActivity.class.getName());
            }
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            context.startActivity(intent);
        }
    }
}
