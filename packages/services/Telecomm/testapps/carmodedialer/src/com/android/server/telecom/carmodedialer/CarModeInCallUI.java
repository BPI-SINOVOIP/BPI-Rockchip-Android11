/*
 * Copyright (C) 2019 Android Open Source Project
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

package com.android.server.telecom.carmodedialer;

import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.os.Bundle;
import android.telecom.Call;
import android.telecom.CallAudioState;
import android.telecom.VideoProfile;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Collection;

public class CarModeInCallUI extends Activity {
    private class BluetoothDeviceAdapter extends ArrayAdapter<BluetoothDevice> {
        public BluetoothDeviceAdapter() {
            super(com.android.server.telecom.carmodedialer.CarModeInCallUI.this, android.R.layout.simple_spinner_item);
            setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            BluetoothDevice info = getItem(position);
            TextView result = new TextView(com.android.server.telecom.carmodedialer.CarModeInCallUI.this);
            result.setText(info.getName());
            return result;
        }

        public void update(Collection<BluetoothDevice> devices) {
            clear();
            addAll(devices);
        }
    }

    public static com.android.server.telecom.carmodedialer.CarModeInCallUI sInstance;
    private ListView mListView;
    private CarModeCallList mCallList;
    private Spinner mBtDeviceList;
    private BluetoothDeviceAdapter mBluetoothDeviceAdapter;
    private TextView mCurrentRouteDisplay;

    /** ${inheritDoc} */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;

        setContentView(R.layout.incall_screen);

        mListView = (ListView) findViewById(R.id.callListView);
        mListView.setAdapter(new CallListAdapter(this));
        mListView.setVisibility(View.VISIBLE);

        mCallList = CarModeCallList.getInstance();
        mCallList.addListener(new CarModeCallList.Listener() {
            @Override
            public void onCallRemoved(Call call) {
                if (mCallList.size() == 0) {
                    Log.i(CarModeInCallUI.class.getSimpleName(), "Ending the incall UI");
                    finish();
                }
            }

            @Override
            public void onRttStarted(Call call) {
                Toast.makeText(CarModeInCallUI.this, "RTT now enabled", Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onRttStopped(Call call) {
                Toast.makeText(CarModeInCallUI.this, "RTT now disabled", Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onRttInitiationFailed(Call call, int reason) {
                Toast.makeText(CarModeInCallUI.this, String.format("RTT failed to init: %d", reason),
                        Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onRttRequest(Call call, int id) {
                Toast.makeText(CarModeInCallUI.this, String.format("RTT request: %d", id),
                        Toast.LENGTH_SHORT).show();
            }
        });

        View endCallButton = findViewById(R.id.end_call_button);
        View holdButton = findViewById(R.id.hold_button);
        View muteButton = findViewById(R.id.mute_button);
        View answerButton = findViewById(R.id.answer_button);
        View setBtDeviceButton = findViewById(R.id.set_bt_device_button);
        View earpieceButton = findViewById(R.id.earpiece_button);
        View speakerButton = findViewById(R.id.speaker_button);
        mBtDeviceList = findViewById(R.id.available_bt_devices);
        mBluetoothDeviceAdapter = new BluetoothDeviceAdapter();
        mBtDeviceList.setAdapter(mBluetoothDeviceAdapter);
        mCurrentRouteDisplay = findViewById(R.id.current_audio_route);

        endCallButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                Call call = mCallList.getCall(0);
                if (call != null) {
                    call.disconnect();
                }
            }
        });
        holdButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                Call call = mCallList.getCall(0);
                if (call != null) {
                    if (call.getState() == Call.STATE_HOLDING) {
                        call.unhold();
                    } else {
                        call.hold();
                    }
                }
            }
        });
        muteButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                Call call = mCallList.getCall(0);
                if (call != null) {

                }
            }
        });

        answerButton.setOnClickListener(view -> {
            Call call = mCallList.getCall(0);
            if (call.getState() == Call.STATE_RINGING) {
                call.answer(VideoProfile.STATE_AUDIO_ONLY);
            }
        });

        earpieceButton.setOnClickListener(view -> {
            CarModeInCallServiceImpl.sInstance.setAudioRoute(CallAudioState.ROUTE_WIRED_OR_EARPIECE);
        });

        speakerButton.setOnClickListener(view -> {
            CarModeInCallServiceImpl.sInstance.setAudioRoute(CallAudioState.ROUTE_SPEAKER);
        });

        setBtDeviceButton.setOnClickListener(view -> {
            if (mBtDeviceList.getSelectedItem() != null
                    && CarModeInCallServiceImpl.sInstance != null) {
                CarModeInCallServiceImpl.sInstance.requestBluetoothAudio(
                        (BluetoothDevice) mBtDeviceList.getSelectedItem());
            }
        });

    }

    public void updateCallAudioState(CallAudioState cas) {
        mBluetoothDeviceAdapter.update(cas.getSupportedBluetoothDevices());
        String routeText;
        switch (cas.getRoute()) {
            case CallAudioState.ROUTE_EARPIECE:
                routeText = "Earpiece";
                break;
            case CallAudioState.ROUTE_SPEAKER:
                routeText = "Speaker";
                break;
            case CallAudioState.ROUTE_WIRED_HEADSET:
                routeText = "Wired";
                break;
            case CallAudioState.ROUTE_BLUETOOTH:
                BluetoothDevice activeDevice = cas.getActiveBluetoothDevice();
                routeText = activeDevice == null ? "null bt" : activeDevice.getName();
                break;
            default:
                routeText = "unknown: " + cas.getRoute();
        }
        mCurrentRouteDisplay.setText(routeText);
    }

    /** ${inheritDoc} */
    @Override
    protected void onDestroy() {
        sInstance = null;
        super.onDestroy();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }
}
