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

import android.content.Context;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;


/**
 * Parse json resource file that contains the test commands for HidDevice
 *
 * For files containing reports and input events, each entry should be in the following format:
 * <code>
 * {"name": "test case name",
 *  "reports": reports,
 *  "events": input_events
 * }
 * </code>
 *
 * {@code reports} - an array of strings that contain hex arrays.
 * {@code input_events} - an array of dicts in the following format:
 * <code>
 * {"action": "down|move|up", "axes": {"axis_x": x, "axis_y": y}, "keycode": "button_a"}
 * </code>
 * {@code "axes"} should only be defined for motion events, and {@code "keycode"} for key events.
 * Timestamps will not be checked.

 * Example:
 * <code>
 * [{ "name": "press button A",
 *    "reports": ["report1",
 *                "report2",
 *                "report3"
 *               ],
 *    "events": [{"action": "down", "axes": {"axis_y": 0.5, "axis_x": 0.1}},
 *               {"action": "move", "axes": {"axis_y": 0.0, "axis_x": 0.0}}
 *              ]
 *  },
 *  ... more tests like that
 * ]
 * </code>
 */
public class HidJsonParser {
    private static final String TAG = "JsonParser";

    private Context mContext;

    public HidJsonParser(Context context) {
        mContext = context;
    }

    /**
     * Convenience function to create JSONArray from resource.
     * The resource specified should contain JSON array as the top-level structure.
     *
     * @param resourceId The resourceId that contains the json data (typically inside R.raw)
     */
    private JSONArray getJsonArrayFromResource(int resourceId) {
        String data = readRawResource(resourceId);
        try {
            return new JSONArray(data);
        } catch (JSONException e) {
            throw new RuntimeException(
                    "Could not parse resource " + resourceId + ", received: " + data);
        }
    }

    /**
     * Convenience function to read in an entire file as a String.
     *
     * @param id resourceId of the file
     * @return contents of the raw resource file as a String
     */
    private String readRawResource(int id) {
        InputStream inputStream = mContext.getResources().openRawResource(id);
        try {
            return readFully(inputStream);
        } catch (IOException e) {
            throw new RuntimeException("Could not read resource id " + id);
        }
    }

    /**
     * Read register command from raw resource.
     *
     * @param resourceId the raw resource id that contains the command
     * @return the command to register device that can be passed to HidDevice constructor
     */
    public String readRegisterCommand(int resourceId) {
        return readRawResource(resourceId);
    }

    /**
     * Read entire input stream until no data remains.
     *
     * @param inputStream
     * @return content of the input stream
     * @throws IOException
     */
    private String readFully(InputStream inputStream) throws IOException {
        OutputStream baos = new ByteArrayOutputStream();
        byte[] buffer = new byte[1024];
        int read = inputStream.read(buffer);
        while (read >= 0) {
            baos.write(buffer, 0, read);
            read = inputStream.read(buffer);
        }
        return baos.toString();
    }

    /**
     * Extract the device id from the raw resource file. This is needed in order to register
     * a HidDevice.
     *
     * @param resourceId resorce file that contains the register command.
     * @return hid device id
     */
    public int readDeviceId(int resourceId) {
        try {
            JSONObject json = new JSONObject(readRawResource(resourceId));
            return json.getInt("id");
        } catch (JSONException e) {
            throw new RuntimeException("Could not read device id from resource " + resourceId);
        }
    }

    /**
     * Read json resource, and return a {@code List} of HidTestData, which contains
     * the name of each test, along with the HID reports and the expected input events.
     */
    public List<HidTestData> getTestData(int resourceId) {
        JSONArray json = getJsonArrayFromResource(resourceId);
        List<HidTestData> tests = new ArrayList<HidTestData>();
        for (int testCaseNumber = 0; testCaseNumber < json.length(); testCaseNumber++) {
            HidTestData testData = new HidTestData();

            try {
                JSONObject testcaseEntry = json.getJSONObject(testCaseNumber);
                testData.name = testcaseEntry.getString("name");
                JSONArray reports = testcaseEntry.getJSONArray("reports");

                for (int i = 0; i < reports.length(); i++) {
                    String report = reports.getString(i);
                    testData.reports.add(report);
                }

                final int source = sourceFromString(testcaseEntry.optString("source"));

                JSONArray events = testcaseEntry.getJSONArray("events");
                for (int i = 0; i < events.length(); i++) {
                    JSONObject entry = events.getJSONObject(i);

                    InputEvent event;
                    if (entry.has("keycode")) {
                        event = parseKeyEvent(source, entry);
                    } else if (entry.has("axes")) {
                        event = parseMotionEvent(source, entry);
                    } else {
                        throw new RuntimeException(
                                "Input event is not specified correctly. Received: " + entry);
                    }
                    testData.events.add(event);
                }
                tests.add(testData);
            } catch (JSONException e) {
                throw new RuntimeException("Could not process entry " + testCaseNumber);
            }
        }
        return tests;
    }

    private KeyEvent parseKeyEvent(int source, JSONObject entry) throws JSONException {
        int action = keyActionFromString(entry.getString("action"));
        int keyCode = KeyEvent.keyCodeFromString(entry.getString("keycode"));
        int metaState = metaStateFromString(entry.optString("metaState"));
        // We will only check select fields of the KeyEvent. Times are not checked.
        return new KeyEvent(/* downTime */ 0, /* eventTime */ 0, action, keyCode,
                /* repeat */ 0, metaState, /* deviceId */ 0, /* scanCode */ 0,
                /* flags */ 0, source);
    }

    private MotionEvent parseMotionEvent(int source, JSONObject entry) throws JSONException {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_UNKNOWN;

        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();

        JSONObject axes = entry.getJSONObject("axes");
        Iterator<String> keys = axes.keys();
        while (keys.hasNext()) {
            String axis = keys.next();
            float value = (float) axes.getDouble(axis);
            coords[0].setAxisValue(MotionEvent.axisFromString(axis), value);
        }

        int buttonState = 0;
        JSONArray buttons = entry.optJSONArray("buttonState");
        if (buttons != null) {
            for (int i = 0; i < buttons.length(); ++i) {
                buttonState |= motionButtonFromString(buttons.getString(i));
            }
        }

        int action = motionActionFromString(entry.getString("action"));
        // Only care about axes, action and source here. Times are not checked.
        return MotionEvent.obtain(/* downTime */ 0, /* eventTime */ 0, action,
                /* pointercount */ 1, properties, coords, 0, buttonState, 0f, 0f,
                0, 0, source, 0);
    }

    private static int keyActionFromString(String action) {
        switch (action.toUpperCase()) {
            case "DOWN":
                return KeyEvent.ACTION_DOWN;
            case "UP":
                return KeyEvent.ACTION_UP;
        }
        throw new RuntimeException("Unknown action specified: " + action);
    }

    private static int metaStateFromString(String metaStateString) {
        int metaState = 0;
        if (metaStateString.isEmpty()) {
            return metaState;
        }
        final String[] metaKeys = metaStateString.split("\\|");
        for (final String metaKeyString : metaKeys) {
            final String trimmedKeyString = metaKeyString.trim();
            switch (trimmedKeyString.toUpperCase()) {
                case "SHIFT_LEFT":
                    metaState |= KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_LEFT_ON;
                    break;
                case "SHIFT_RIGHT":
                    metaState |= KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_RIGHT_ON;
                    break;
                case "CTRL_LEFT":
                    metaState |= KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_LEFT_ON;
                    break;
                case "CTRL_RIGHT":
                    metaState |= KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_RIGHT_ON;
                    break;
                case "ALT_LEFT":
                    metaState |= KeyEvent.META_ALT_ON | KeyEvent.META_ALT_LEFT_ON;
                    break;
                case "ALT_RIGHT":
                    metaState |= KeyEvent.META_ALT_ON | KeyEvent.META_ALT_RIGHT_ON;
                    break;
                case "META_LEFT":
                    metaState |= KeyEvent.META_META_ON | KeyEvent.META_META_LEFT_ON;
                    break;
                case "META_RIGHT":
                    metaState |= KeyEvent.META_META_ON | KeyEvent.META_META_RIGHT_ON;
                    break;
                case "CAPS_LOCK":
                    metaState |= KeyEvent.META_CAPS_LOCK_ON;
                    break;
                case "NUM_LOCK":
                    metaState |= KeyEvent.META_NUM_LOCK_ON;
                    break;
                case "SCROLL_LOCK":
                    metaState |= KeyEvent.META_SCROLL_LOCK_ON;
                    break;
                default:
                    throw new RuntimeException("Unknown meta state chunk: " + trimmedKeyString
                            + " in meta state string: " + metaStateString);
            }
        }
        return metaState;
    }

    private static int motionActionFromString(String action) {
        switch (action.toUpperCase()) {
            case "DOWN":
                return MotionEvent.ACTION_DOWN;
            case "MOVE":
                return MotionEvent.ACTION_MOVE;
            case "UP":
                return MotionEvent.ACTION_UP;
            case "BUTTON_PRESS":
                return MotionEvent.ACTION_BUTTON_PRESS;
            case "BUTTON_RELEASE":
                return MotionEvent.ACTION_BUTTON_RELEASE;
            case "HOVER_ENTER":
                return MotionEvent.ACTION_HOVER_ENTER;
            case "HOVER_MOVE":
                return MotionEvent.ACTION_HOVER_MOVE;
            case "HOVER_EXIT":
                return MotionEvent.ACTION_HOVER_EXIT;
        }
        throw new RuntimeException("Unknown action specified: " + action);
    }

    private static int sourceFromString(String sourceString) {
        if (sourceString.isEmpty()) {
            return InputDevice.SOURCE_UNKNOWN;
        }
        int source = 0;
        final String[] sourceEntries = sourceString.split("\\|");
        for (final String sourceEntry : sourceEntries) {
            final String trimmedSourceEntry = sourceEntry.trim();
            switch (trimmedSourceEntry.toUpperCase()) {
                case "MOUSE_RELATIVE":
                    source |= InputDevice.SOURCE_MOUSE_RELATIVE;
                    break;
                case "JOYSTICK":
                    source |= InputDevice.SOURCE_JOYSTICK;
                    break;
                case "KEYBOARD":
                    source |= InputDevice.SOURCE_KEYBOARD;
                    break;
                case "GAMEPAD":
                    source |= InputDevice.SOURCE_GAMEPAD;
                    break;
                case "DPAD":
                    source |= InputDevice.SOURCE_DPAD;
                    break;
                default:
                    throw new RuntimeException("Unknown source chunk: " + trimmedSourceEntry
                            + " in source string: " + sourceString);
            }
        }
        return source;
    }

    private static int motionButtonFromString(String button) {
        switch (button.toUpperCase()) {
            case "BACK":
                return MotionEvent.BUTTON_BACK;
            case "FORWARD":
                return MotionEvent.BUTTON_FORWARD;
            case "PRIMARY":
                return MotionEvent.BUTTON_PRIMARY;
            case "SECONDARY":
                return MotionEvent.BUTTON_SECONDARY;
            case "STYLUS_PRIMARY":
                return MotionEvent.BUTTON_STYLUS_PRIMARY;
            case "STYLUS_SECONDARY":
                return MotionEvent.BUTTON_STYLUS_SECONDARY;
            case "TERTIARY":
                return MotionEvent.BUTTON_TERTIARY;
        }
        throw new RuntimeException("Unknown button specified: " + button);
    }
}
