/*
 * Copyright 2018 The Android Open Source Project
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
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.widget.ListView;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.util.ArrayList;
import java.util.List;
import android.util.Log;

public class BleCocClientTestBaseActivity extends PassFailButtons.Activity {
    public static final String TAG = "BleCocClientTestBase";

    private static final boolean STEP_EXECUTION = false;

    private final int TEST_BLE_LE_CONNECTED = 0;
    private final int TEST_BLE_GOT_PSM = 1;
    private final int TEST_BLE_COC_CONNECTED = 2;
    private final int TEST_BLE_CONNECTION_TYPE_CHECKED = 3;
    private final int TEST_BLE_DATA_8BYTES_SENT = 4;
    private final int TEST_BLE_DATA_8BYTES_READ = 5;
    private final int TEST_BLE_DATA_EXCHANGED = 6;
    private final int TEST_BLE_CLIENT_DISCONNECTED = 7;
    private static final int PASS_FLAG_ALL = 0x00FF;

    private TestAdapter mTestAdapter;
    private long mPassed;
    private Dialog mDialog;
    private Handler mHandler;

    private static final long BT_ON_DELAY = 10000;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.ble_server_start);
        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);

        mTestAdapter = new TestAdapter(this, setupTestList());
        ListView listView = (ListView) findViewById(R.id.ble_server_tests);
        listView.setAdapter(mTestAdapter);

        mPassed = 0;
        mHandler = new Handler();
    }

    @Override
    public void onResume() {
        super.onResume();

        IntentFilter filter = new IntentFilter();

        filter.addAction(BleCocClientService.BLE_LE_CONNECTED);
        filter.addAction(BleCocClientService.BLE_GOT_PSM);
        filter.addAction(BleCocClientService.BLE_COC_CONNECTED);
        filter.addAction(BleCocClientService.BLE_CONNECTION_TYPE_CHECKED);
        filter.addAction(BleCocClientService.BLE_DATA_8BYTES_SENT);
        filter.addAction(BleCocClientService.BLE_DATA_8BYTES_READ);
        filter.addAction(BleCocClientService.BLE_DATA_LARGEBUF_READ);
        filter.addAction(BleCocClientService.BLE_LE_DISCONNECTED);

        filter.addAction(BleCocClientService.BLE_BLUETOOTH_DISCONNECTED);
        filter.addAction(BleCocClientService.BLE_BLUETOOTH_DISABLED);
        filter.addAction(BleCocClientService.BLE_BLUETOOTH_MISMATCH_SECURE);
        filter.addAction(BleCocClientService.BLE_BLUETOOTH_MISMATCH_INSECURE);
        filter.addAction(BleCocClientService.BLE_CLIENT_ERROR);

        registerReceiver(mBroadcast, filter);
    }

    @Override
    public void onPause() {
        super.onPause();
        unregisterReceiver(mBroadcast);
        closeDialog();
    }

    private synchronized void closeDialog() {
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }

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

    private List<Integer> setupTestList() {
        ArrayList<Integer> testList = new ArrayList<Integer>();
        testList.add(R.string.ble_coc_client_le_connect);
        testList.add(R.string.ble_coc_client_get_psm);
        testList.add(R.string.ble_coc_client_coc_connect);
        testList.add(R.string.ble_coc_client_check_connection_type);
        testList.add(R.string.ble_coc_client_send_data_8bytes);
        testList.add(R.string.ble_coc_client_receive_data_8bytes);
        testList.add(R.string.ble_coc_client_data_exchange);
        testList.add(R.string.ble_client_disconnect_name);
        return testList;
    }

    private void showErrorDialog(int titleId, int messageId, boolean finish) {
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
        builder.create().show();
    }

    private BroadcastReceiver mBroadcast = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            boolean showProgressDialog = false;
            closeDialog();

            String action = intent.getAction();
            String newAction = null;
            String actionName = null;
            long previousPassed = mPassed;
            final Intent startIntent = new Intent(BleCocClientTestBaseActivity.this, BleCocClientService.class);
            if (action != null) {
                Log.d(TAG, "Processing " + action);
            }
            switch (action) {
            case BleCocClientService.BLE_LE_CONNECTED:
                actionName = getString(R.string.ble_coc_client_le_connect);
                mTestAdapter.setTestPass(TEST_BLE_LE_CONNECTED);
                mPassed |= (1 << TEST_BLE_LE_CONNECTED);
                // Start LE Service Discovery and then read the PSM
                newAction = BleCocClientService.BLE_COC_CLIENT_ACTION_GET_PSM;
                break;

            case BleCocClientService.BLE_GOT_PSM:
                actionName = getString(R.string.ble_coc_client_get_psm);
                mTestAdapter.setTestPass(TEST_BLE_GOT_PSM);
                mPassed |= (1 << TEST_BLE_GOT_PSM);
                // Connect the LE CoC
                newAction = BleCocClientService.BLE_COC_CLIENT_ACTION_COC_CLIENT_CONNECT;
                break;

            case BleCocClientService.BLE_COC_CONNECTED:
                actionName = getString(R.string.ble_coc_client_coc_connect);
                mTestAdapter.setTestPass(TEST_BLE_COC_CONNECTED);
                mPassed |= (1 << TEST_BLE_COC_CONNECTED);
                // Check the connection type
                newAction = BleCocClientService.BLE_COC_CLIENT_ACTION_CHECK_CONNECTION_TYPE;
                break;

            case BleCocClientService.BLE_CONNECTION_TYPE_CHECKED:
                actionName = getString(R.string.ble_coc_client_check_connection_type);
                mTestAdapter.setTestPass(TEST_BLE_CONNECTION_TYPE_CHECKED);
                mPassed |= (1 << TEST_BLE_CONNECTION_TYPE_CHECKED);
                // Send 8 bytes
                newAction = BleCocClientService.BLE_COC_CLIENT_ACTION_SEND_DATA_8BYTES;
                break;

            case BleCocClientService.BLE_DATA_8BYTES_SENT:
                actionName = getString(R.string.ble_coc_client_send_data_8bytes);
                mTestAdapter.setTestPass(TEST_BLE_DATA_8BYTES_SENT);
                mPassed |= (1 << TEST_BLE_DATA_8BYTES_SENT);
                // Read 8 bytes
                newAction = BleCocClientService.BLE_COC_CLIENT_ACTION_READ_DATA_8BYTES;
                break;

            case BleCocClientService.BLE_DATA_8BYTES_READ:
                actionName = getString(R.string.ble_coc_client_receive_data_8bytes);
                mTestAdapter.setTestPass(TEST_BLE_DATA_8BYTES_READ);
                mPassed |= (1 << TEST_BLE_DATA_8BYTES_READ);
                // Do data exchanges
                newAction = BleCocClientService.BLE_COC_CLIENT_ACTION_EXCHANGE_DATA;
                break;

            case BleCocClientService.BLE_DATA_LARGEBUF_READ:
                actionName = getString(R.string.ble_coc_client_data_exchange);
                mTestAdapter.setTestPass(TEST_BLE_DATA_EXCHANGED);
                mPassed |= (1 << TEST_BLE_DATA_EXCHANGED);
                // Disconnect
                newAction = BleCocClientService.BLE_CLIENT_ACTION_CLIENT_DISCONNECT;
                break;

            case BleCocClientService.BLE_BLUETOOTH_DISCONNECTED:
                mTestAdapter.setTestPass(TEST_BLE_CLIENT_DISCONNECTED);
                mPassed |= (1 << TEST_BLE_CLIENT_DISCONNECTED);
                // all tests done
                newAction = null;
                break;

            case BleCocClientService.BLE_BLUETOOTH_DISABLED:
                showErrorDialog(R.string.ble_bluetooth_disable_title, R.string.ble_bluetooth_disable_message, true);
                break;

            case BleCocClientService.BLE_BLUETOOTH_MISMATCH_SECURE:
                showErrorDialog(R.string.ble_bluetooth_mismatch_title, R.string.ble_bluetooth_mismatch_secure_message, true);
                break;

            case BleCocClientService.BLE_BLUETOOTH_MISMATCH_INSECURE:
                showErrorDialog(R.string.ble_bluetooth_mismatch_title, R.string.ble_bluetooth_mismatch_insecure_message, true);
                break;

            default:
                Log.e(TAG, "onReceive: Error: unhandled action=" + action);
            }

            if (previousPassed != mPassed) {
                String logMessage = String.format("Passed Flags has changed from 0x%08X to 0x%08X. Delta=0x%08X",
                                                  previousPassed, mPassed, mPassed ^ previousPassed);
                Log.d(TAG, logMessage);
            }

            mTestAdapter.notifyDataSetChanged();

            if (newAction != null) {
                Log.d(TAG, "Starting " + newAction);
                startIntent.setAction(newAction);
                if (STEP_EXECUTION) {
                    closeDialog();
                    final boolean showProgressDialogValue = showProgressDialog;
                    mDialog = new AlertDialog.Builder(BleCocClientTestBaseActivity.this)
                            .setTitle(actionName)
                            .setMessage(R.string.ble_test_finished)
                            .setCancelable(false)
                            .setPositiveButton(R.string.ble_test_next,
                                    new DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick(DialogInterface dialog, int which) {
                                            closeDialog();
                                            if (showProgressDialogValue) {
                                                showProgressDialog();
                                            }
                                            startService(startIntent);
                                        }
                                    })
                            .show();
                } else {
                    if (showProgressDialog) {
                        showProgressDialog();
                    }
                    startService(startIntent);
                }
            } else {
                closeDialog();
            }

            if (mPassed == PASS_FLAG_ALL) {
                Log.d(TAG, "All Tests Passed.");
                getPassButton().setEnabled(true);
            }
        }
    };
}
