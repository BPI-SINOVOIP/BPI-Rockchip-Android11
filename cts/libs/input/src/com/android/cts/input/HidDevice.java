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

package com.android.cts.input;

import static android.os.FileUtils.closeQuietly;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.hardware.input.InputManager;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.io.OutputStream;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Represents a virtual HID device registered through /dev/uhid.
 */
public final class HidDevice implements InputManager.InputDeviceListener {
    private static final String TAG = "HidDevice";
    // hid executable expects "-" argument to read from stdin instead of a file
    private static final String HID_COMMAND = "hid -";

    private final int mId; // // initialized from the json file

    private OutputStream mOutputStream;
    private Instrumentation mInstrumentation;

    private volatile CountDownLatch mDeviceAddedSignal; // to wait for onInputDeviceAdded signal

    public HidDevice(Instrumentation instrumentation, int deviceId, String registerCommand) {
        mInstrumentation = instrumentation;
        setupPipes();

        mInstrumentation.runOnMainSync(new Runnable(){
            @Override
            public void run() {
                InputManager inputManager =
                        mInstrumentation.getContext().getSystemService(InputManager.class);
                inputManager.registerInputDeviceListener(HidDevice.this, null);
            }
        });

        mId = deviceId;
        registerInputDevice(registerCommand);
    }

    /**
     * Register an input device. May cause a failure if the device added notification
     * is not received within the timeout period
     *
     * @param registerCommand The full json command that specifies how to register this device
     */
    private void registerInputDevice(String registerCommand) {
        mDeviceAddedSignal = new CountDownLatch(1);
        writeHidCommands(registerCommand.getBytes());
        try {
            // Found that in kernel 3.10, the device registration takes a very long time
            // The wait can be decreased to 2 seconds after kernel 3.10 is no longer supported
            mDeviceAddedSignal.await(20L, TimeUnit.SECONDS);
            if (mDeviceAddedSignal.getCount() != 0) {
                throw new RuntimeException("Did not receive device added notification in time");
            }
        } catch (InterruptedException ex) {
            throw new RuntimeException(
                    "Unexpectedly interrupted while waiting for device added notification.");
        }
        // Even though the device has been added, it still may not be ready to process the events
        // right away. This seems to be a kernel bug.
        // Add a small delay here to ensure device is "ready".
        SystemClock.sleep(500);
    }

    /**
     * Add a delay between processing events.
     *
     * @param milliSeconds The delay in milliseconds.
     */
    public void delay(int milliSeconds) {
        JSONObject json = new JSONObject();
        try {
            json.put("command", "delay");
            json.put("id", mId);
            json.put("duration", milliSeconds);
        } catch (JSONException e) {
            throw new RuntimeException(
                    "Could not create JSON object to delay " + milliSeconds + " milliseconds");
        }
        writeHidCommands(json.toString().getBytes());
    }

    /**
     * Send a HID report to the device. The report should follow the report descriptor
     * that was specified during device registration.
     * An example report:
     * String report = "[0x01, 0x00, 0x00, 0x02]";
     *
     * @param report The report to send (a JSON-formatted array of hex)
     */
    public void sendHidReport(String report) {
        JSONObject json = new JSONObject();
        try {
            json.put("command", "report");
            json.put("id", mId);
            json.put("report", new JSONArray(report));
        } catch (JSONException e) {
            throw new RuntimeException("Could not process HID report: " + report);
        }
        writeHidCommands(json.toString().getBytes());
    }

    /**
     * Close the device, which would cause the associated input device to unregister.
     */
    public void close() {
        closeQuietly(mOutputStream);
    }

    private void setupPipes() {
        UiAutomation ui = mInstrumentation.getUiAutomation();
        ParcelFileDescriptor[] pipes = ui.executeShellCommandRw(HID_COMMAND);

        mOutputStream = new ParcelFileDescriptor.AutoCloseOutputStream(pipes[1]);
        closeQuietly(pipes[0]); // hid command is write-only
    }

    private void writeHidCommands(byte[] bytes) {
        try {
            mOutputStream.write(bytes);
            mOutputStream.flush();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    // InputManager.InputDeviceListener functions
    @Override
    public void onInputDeviceAdded(int deviceId) {
        mDeviceAddedSignal.countDown();
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
    }
}
