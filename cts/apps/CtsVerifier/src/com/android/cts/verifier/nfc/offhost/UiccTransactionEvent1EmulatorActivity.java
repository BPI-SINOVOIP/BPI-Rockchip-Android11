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

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.nfc.NfcAdapter;
import android.nfc.cardemulation.CardEmulation;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import com.android.cts.verifier.nfc.hce.HceUtils;

public class UiccTransactionEvent1EmulatorActivity extends PassFailButtons.Activity {
    static final String TAG = "UiccTransactionEvent1EmulatorActivity";

    TextView mTextView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.pass_fail_text);
        setPassFailButtonClickListeners();
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.R) {
            getPassButton().setEnabled(false);
        } else {
            getPassButton().setEnabled(true);
        }

        mTextView = (TextView) findViewById(R.id.text);
        mTextView.setTextSize(12.0f);
        mTextView.setText(R.string.nfc_offhost_uicc_transaction_event_emulator_help);

        initProcess();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        setContentView(R.layout.pass_fail_text);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        mTextView = (TextView) findViewById(R.id.text);
        mTextView.setTextSize(12.0f);

        setIntent(intent);
        initProcess();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    public static Intent buildReaderIntent(Context context) {
        Intent readerIntent = new Intent(context, SimpleOffhostReaderActivity.class);
        readerIntent.putExtra(SimpleOffhostReaderActivity.EXTRA_APDUS,
                UiccTransactionEvent1Service.APDU_COMMAND_SEQUENCE);
        readerIntent.putExtra(SimpleOffhostReaderActivity.EXTRA_RESPONSES,
                UiccTransactionEvent1Service.APDU_RESPOND_SEQUENCE);
        readerIntent.putExtra(SimpleOffhostReaderActivity.EXTRA_LABEL,
                context.getString(R.string.nfc_offhost_uicc_transaction_event1_reader));
        return readerIntent;
    }

    private void initProcess() {

        Bundle bundle = getIntent().getExtras();
        if(bundle != null){
            byte[] transactionData = bundle.getByteArray(NfcAdapter.EXTRA_DATA);
            if(transactionData != null){
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mTextView.setText("Pass - NFC Action:" + getIntent().getAction() + " uri:" + getIntent().getDataString()
                            + " data:" + HceUtils.getHexBytes(null, transactionData));
                        getPassButton().setEnabled(true);
                    }
                });
            } else {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mTextView.setText("Fail - Action:" + getIntent().getAction() + " uri:" + getIntent().getDataString()
                            + " data: null");
                        getPassButton().setEnabled(false);
                    }
                });
            }
        }
    }
}
