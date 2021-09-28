/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.cts.verifier.bluetooth;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.widget.ListView;
import android.widget.Toast;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.util.ArrayList;
import java.util.List;

public class BleConnectionPriorityServerBaseActivity extends PassFailButtons.Activity {

    public static final int CONNECTION_PRIORITY_HIGH = 0;

    private TestAdapter mTestAdapter;

    private Dialog mDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.ble_connection_priority_server_test);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.ble_connection_priority_server_name,
                R.string.ble_connection_priority_server_info, -1);

        getPassButton().setEnabled(false);

        mTestAdapter = new TestAdapter(this, setupTestList());
        ListView listView = (ListView) findViewById(R.id.ble_server_connection_tests);
        listView.setAdapter(mTestAdapter);

        startService(new Intent(this, BleConnectionPriorityServerService.class));
    }

    @Override
    public void onResume() {
        super.onResume();

        IntentFilter filter = new IntentFilter();
        filter.addAction(BleConnectionPriorityServerService.ACTION_BLUETOOTH_DISABLED);
        filter.addAction(BleConnectionPriorityServerService.ACTION_CONNECTION_PRIORITY_FINISH);
        filter.addAction(BleServerService.BLE_ADVERTISE_UNSUPPORTED);
        filter.addAction(BleServerService.BLE_OPEN_FAIL);
        filter.addAction(BleConnectionPriorityServerService.ACTION_START_CONNECTION_PRIORITY_TEST);
        registerReceiver(mBroadcast, filter);
    }

    @Override
    public void onPause() {
        super.onPause();
        unregisterReceiver(mBroadcast);
        closeDialog();
    }

    private List<Integer> setupTestList() {
        ArrayList<Integer> testList = new ArrayList<Integer>();
        testList.add(R.string.ble_connection_priority_client_description);
        return testList;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopService(new Intent(this, BleConnectionPriorityServerService.class));
    }

    private void closeDialog() {
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }

    private void showErrorDialog(int titleId, int messageId, boolean finish) {
        closeDialog();

        AlertDialog.Builder builder = new AlertDialog.Builder(this)
                .setTitle(titleId)
                .setMessage(messageId);
        if (finish) {
            builder.setOnCancelListener(new Dialog.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    finish();
                }
            });
        }
        mDialog = builder.create();
        mDialog.show();
    }

    private BroadcastReceiver mBroadcast = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            boolean passedAll = false;
            String action = intent.getAction();
            switch (action) {
            case BleConnectionPriorityServerService.ACTION_BLUETOOTH_DISABLED:
                new AlertDialog.Builder(context)
                        .setTitle(R.string.ble_bluetooth_disable_title)
                        .setMessage(R.string.ble_bluetooth_disable_message)
                        .setOnCancelListener(new Dialog.OnCancelListener() {
                            @Override
                            public void onCancel(DialogInterface dialog) {
                                finish();
                            }
                        })
                        .create().show();
                break;
            case BleConnectionPriorityServerService.ACTION_START_CONNECTION_PRIORITY_TEST:
                showProgressDialog();
                break;
            case BleConnectionPriorityServerService.ACTION_CONNECTION_PRIORITY_FINISH:
                String resultMsg = getString(R.string.ble_server_connection_priority_result_passed);

                closeDialog();

                mTestAdapter.setTestPass(CONNECTION_PRIORITY_HIGH);
                mDialog = new AlertDialog.Builder(BleConnectionPriorityServerBaseActivity.this)
                        .setMessage(resultMsg)
                        .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                closeDialog();
                            }
                        })
                        .setOnCancelListener(new DialogInterface.OnCancelListener() {
                            @Override
                            public void onCancel(DialogInterface dialog) {
                                closeDialog();
                            }
                        })
                        .create();
                mDialog.show();

                getPassButton().setEnabled(true);
                mTestAdapter.notifyDataSetChanged();
                break;

            case BleServerService.BLE_OPEN_FAIL:
                setTestResultAndFinish(false);
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(BleConnectionPriorityServerBaseActivity.this, R.string.bt_open_failed_message, Toast.LENGTH_SHORT).show();
                    }
                });
                break;
            case BleServerService.BLE_ADVERTISE_UNSUPPORTED:
                showErrorDialog(R.string.bt_advertise_unsupported_title, R.string.bt_advertise_unsupported_message, true);
                break;
            }

        }
    };

    private synchronized void showProgressDialog() {
        closeDialog();

        ProgressDialog dialog = new ProgressDialog(this);
        dialog.setTitle(R.string.ble_test_running);
        dialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        dialog.setMessage(getString(R.string.ble_test_running_message));
        dialog.setCanceledOnTouchOutside(false);
        mDialog = dialog;
        mDialog.show();
    }
}
