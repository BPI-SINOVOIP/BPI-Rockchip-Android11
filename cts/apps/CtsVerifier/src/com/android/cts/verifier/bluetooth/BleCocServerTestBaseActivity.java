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

import java.util.ArrayList;
import java.util.List;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.util.Log;
import android.widget.ListView;
import android.widget.Toast;

public class BleCocServerTestBaseActivity extends PassFailButtons.Activity {

    public static final boolean DEBUG = true;
    public static final String TAG = "BleCocServerTestBaseActivity";

    private final int TEST_BLE_LE_CONNECTED = 0;
    private final int TEST_BLE_LISTENER_CREATED = 1;
    private final int TEST_BLE_PSM_READ = 2;
    private final int TEST_BLE_COC_CONNECTED = 3;
    private final int TEST_BLE_CONNECTION_TYPE_CHECKED = 4;
    private final int TEST_BLE_DATA_8BYTES_READ = 5;
    private final int TEST_BLE_DATA_8BYTES_SENT = 6;
    private final int TEST_BLE_DATA_EXCHANGED = 7;
    private final int TEST_BLE_SERVER_DISCONNECTED = 8;
    private static final int PASS_FLAG_ALL = 0x01FF;

    private TestAdapter mTestAdapter;
    private long mPassed;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.ble_server_start);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.ble_coc_server_start_name,
                         R.string.ble_server_start_info, -1);
        getPassButton().setEnabled(false);

        mTestAdapter = new TestAdapter(this, setupTestList());
        ListView listView = (ListView) findViewById(R.id.ble_server_tests);
        listView.setAdapter(mTestAdapter);

        mPassed = 0;
    }

    @Override
    public void onResume() {
        super.onResume();

        IntentFilter filter = new IntentFilter();

        filter.addAction(BleCocServerService.BLE_LE_CONNECTED);
        filter.addAction(BleCocServerService.BLE_COC_LISTENER_CREATED);
        filter.addAction(BleCocServerService.BLE_PSM_READ);
        filter.addAction(BleCocServerService.BLE_COC_CONNECTED);
        filter.addAction(BleCocServerService.BLE_CONNECTION_TYPE_CHECKED);
        filter.addAction(BleCocServerService.BLE_DATA_8BYTES_READ);
        filter.addAction(BleCocServerService.BLE_DATA_8BYTES_SENT);
        filter.addAction(BleCocServerService.BLE_DATA_LARGEBUF_READ);

        filter.addAction(BleCocServerService.BLE_BLUETOOTH_MISMATCH_SECURE);
        filter.addAction(BleCocServerService.BLE_BLUETOOTH_MISMATCH_INSECURE);
        filter.addAction(BleCocServerService.BLE_SERVER_DISCONNECTED);

        filter.addAction(BleCocServerService.BLE_BLUETOOTH_DISABLED);
        filter.addAction(BleCocServerService.BLE_OPEN_FAIL);
        filter.addAction(BleCocServerService.BLE_ADVERTISE_UNSUPPORTED);
        filter.addAction(BleCocServerService.BLE_ADD_SERVICE_FAIL);

        registerReceiver(mBroadcast, filter);
    }

    @Override
    public void onPause() {
        super.onPause();
        unregisterReceiver(mBroadcast);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    private List<Integer> setupTestList() {
        ArrayList<Integer> testList = new ArrayList<Integer>();
        testList.add(R.string.ble_coc_server_le_connect);
        testList.add(R.string.ble_coc_server_create_listener);
        testList.add(R.string.ble_coc_server_psm_read);
        testList.add(R.string.ble_coc_server_connection);
        testList.add(R.string.ble_coc_server_check_connection_type);
        testList.add(R.string.ble_coc_server_receive_data_8bytes);
        testList.add(R.string.ble_coc_server_send_data_8bytes);
        testList.add(R.string.ble_coc_server_data_exchange);
        testList.add(R.string.ble_server_receiving_disconnect);
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
            String action = intent.getAction();
            if (DEBUG) {
                Log.d(TAG, "BroadcastReceiver.onReceive: action=" + action);
            }
            String newAction = null;
            final Intent startIntent = new Intent(BleCocServerTestBaseActivity.this, BleCocServerService.class);

            switch (action) {
            case BleCocServerService.BLE_BLUETOOTH_DISABLED:
                showErrorDialog(R.string.ble_bluetooth_disable_title, R.string.ble_bluetooth_disable_message, true);
                break;
            case BleCocServerService.BLE_LE_CONNECTED:
                mTestAdapter.setTestPass(TEST_BLE_LE_CONNECTED);
                mPassed |= (1 << TEST_BLE_LE_CONNECTED);
                break;
            case BleCocServerService.BLE_COC_LISTENER_CREATED:
                mTestAdapter.setTestPass(TEST_BLE_LISTENER_CREATED);
                mPassed |= (1 << TEST_BLE_LISTENER_CREATED);
                break;
            case BleCocServerService.BLE_PSM_READ:
                mTestAdapter.setTestPass(TEST_BLE_PSM_READ);
                mPassed |= (1 << TEST_BLE_PSM_READ);
                break;
            case BleCocServerService.BLE_COC_CONNECTED:
                mTestAdapter.setTestPass(TEST_BLE_COC_CONNECTED);
                mPassed |= (1 << TEST_BLE_COC_CONNECTED);
                break;
            case BleCocServerService.BLE_CONNECTION_TYPE_CHECKED:
                mTestAdapter.setTestPass(TEST_BLE_CONNECTION_TYPE_CHECKED);
                mPassed |= (1 << TEST_BLE_CONNECTION_TYPE_CHECKED);
                break;
            case BleCocServerService.BLE_DATA_8BYTES_READ:
                mTestAdapter.setTestPass(TEST_BLE_DATA_8BYTES_READ);
                mPassed |= (1 << TEST_BLE_DATA_8BYTES_READ);
                // send the next action to send 8 bytes
                newAction = BleCocServerService.BLE_COC_SERVER_ACTION_SEND_DATA_8BYTES;
                break;
            case BleCocServerService.BLE_DATA_8BYTES_SENT:
                mTestAdapter.setTestPass(TEST_BLE_DATA_8BYTES_SENT);
                mPassed |= (1 << TEST_BLE_DATA_8BYTES_SENT);
                // send the next action to send 8 bytes
                newAction = BleCocServerService.BLE_COC_SERVER_ACTION_EXCHANGE_DATA;
                break;
            case BleCocServerService.BLE_DATA_LARGEBUF_READ:
                mTestAdapter.setTestPass(TEST_BLE_DATA_EXCHANGED);
                mPassed |= (1 << TEST_BLE_DATA_EXCHANGED);
                // Disconnect
                newAction = BleCocServerService.BLE_COC_SERVER_ACTION_DISCONNECT;
                break;
            case BleCocServerService.BLE_SERVER_DISCONNECTED:
                mTestAdapter.setTestPass(TEST_BLE_SERVER_DISCONNECTED);
                mPassed |= (1 << TEST_BLE_SERVER_DISCONNECTED);
                // all tests done
                break;
            case BleCocServerService.BLE_BLUETOOTH_MISMATCH_SECURE:
                showErrorDialog(R.string.ble_bluetooth_mismatch_title, R.string.ble_bluetooth_mismatch_secure_message, true);
                break;
            case BleCocServerService.BLE_BLUETOOTH_MISMATCH_INSECURE:
                showErrorDialog(R.string.ble_bluetooth_mismatch_title, R.string.ble_bluetooth_mismatch_insecure_message, true);
                break;
            case BleCocServerService.BLE_ADVERTISE_UNSUPPORTED:
                showErrorDialog(R.string.bt_advertise_unsupported_title, R.string.bt_advertise_unsupported_message, true);
                break;
            case BleCocServerService.BLE_OPEN_FAIL:
                setTestResultAndFinish(false);
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(BleCocServerTestBaseActivity.this, R.string.bt_open_failed_message, Toast.LENGTH_SHORT).show();
                    }
                });
                break;
            case BleCocServerService.BLE_ADD_SERVICE_FAIL:
                showErrorDialog(R.string.bt_add_service_failed_title, R.string.bt_add_service_failed_message, true);
                break;
            default:
                if (DEBUG) {
                    Log.d(TAG, "Note: BroadcastReceiver.onReceive: unhandled action=" + action);
                }
            }

            mTestAdapter.notifyDataSetChanged();

            if (newAction != null) {
                Log.d(TAG, "Starting " + newAction);
                startIntent.setAction(newAction);

                startService(startIntent);
            }

            if (mPassed == PASS_FLAG_ALL) {
                Log.d(TAG, "All Tests Passed.");
                getPassButton().setEnabled(true);
            }
        }
    };
}
