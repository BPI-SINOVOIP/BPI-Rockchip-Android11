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
package com.android.tradefed.targetprep;

import com.android.helper.aoa.AoaDevice;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.RegexTrie;

import com.google.common.annotations.VisibleForTesting;

import java.awt.Point;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.BiConsumer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * {@link ITargetPreparer} that executes a series of actions (e.g. clicks and swipes) using the
 * Android Open Accessory (AOAv2) protocol. This allows controlling an Android device without
 * enabling USB debugging.
 *
 * <p>Accepts a list of strings which correspond to {@link AoaDevice} methods:
 *
 * <ul>
 *   <li>Click using x and y coordinates, e.g. "click 0 0" or "longClick 360 640".
 *   <li>Swipe between two sets of coordinates in a specified number of milliseconds, e.g. "swipe 0
 *       0 100 360 640" to swipe from (0, 0) to (360, 640) in 100 milliseconds.
 *   <li>Write a string of alphanumeric text, e.g. "write hello world".
 *   <li>Press a combination of keys, e.g. "key RIGHT 2*TAB ENTER".
 *   <li>Wake up the device with "wake".
 *   <li>Press the home button with "home".
 *   <li>Press the back button with "back".
 *   <li>Wait for specified number of milliseconds, e.g. "sleep 1000" to wait for 1000 milliseconds.
 * </ul>
 */
@OptionClass(alias = "aoa-preparer")
public class AoaTargetPreparer extends BaseTargetPreparer {

    private static final String POINT = "(\\d{1,3}) (\\d{1,3})";
    private static final Pattern KEY = Pattern.compile("\\s+(?:(\\d+)\\*)?([a-zA-Z0-9]+)");

    @FunctionalInterface
    private interface Action extends BiConsumer<AoaDevice, List<String>> {}

    // Trie of possible actions, parses the input string and determines the operation to execute
    private static final RegexTrie<Action> ACTIONS = new RegexTrie<>();

    static {
        // clicks
        ACTIONS.put(
                (device, args) -> device.click(parsePoint(args.get(0), args.get(1))),
                String.format("click %s", POINT));
        ACTIONS.put(
                (device, args) -> device.longClick(parsePoint(args.get(0), args.get(1))),
                String.format("longClick %s", POINT));

        // swipes
        ACTIONS.put(
                (device, args) -> {
                    Point from = parsePoint(args.get(0), args.get(1));
                    Duration duration = parseMillis(args.get(2));
                    Point to = parsePoint(args.get(3), args.get(4));
                    device.swipe(from, to, duration);
                },
                String.format("swipe %s (\\d+) %s", POINT, POINT));

        // keyboard
        ACTIONS.put(
                (device, args) -> {
                    List<Integer> keys =
                            Stream.of(args.get(0).split(""))
                                    .map(AoaTargetPreparer::parseKeycode)
                                    .collect(Collectors.toList());
                    device.pressKeys(keys);
                },
                "write ([a-zA-Z0-9\\s]+)");
        ACTIONS.put(
                (device, args) -> {
                    List<Integer> keys = new ArrayList<>();
                    Matcher matcher = KEY.matcher(args.get(0));
                    while (matcher.find()) {
                        int count = matcher.group(1) == null ? 1 : Integer.decode(matcher.group(1));
                        Integer keycode = parseKeycode(matcher.group(2));
                        keys.addAll(Collections.nCopies(count, keycode));
                    }
                    device.pressKeys(keys);
                },
                "key((?: (?:\\d+\\*)?[a-zA-Z0-9]+)+)");

        // other
        ACTIONS.put((device, args) -> device.wakeUp(), "wake");
        ACTIONS.put((device, args) -> device.goHome(), "home");
        ACTIONS.put((device, args) -> device.goBack(), "back");
        ACTIONS.put(
                (device, args) -> {
                    Duration duration = parseMillis(args.get(0));
                    device.sleep(duration);
                },
                "sleep (\\d+)");
    }

    @Option(name = "device-timeout", description = "Maximum time to wait for device")
    private Duration mDeviceTimeout = Duration.ofMinutes(1L);

    @Option(
        name = "wait-for-device-online",
        description = "Checks whether the device is online after preparation."
    )
    private boolean mWaitForDeviceOnline = true;

    @Option(name = "action", description = "AOAv2 action to perform. Can be repeated.")
    private List<String> mActions = new ArrayList<>();

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        if (mActions.isEmpty()) {
            return;
        }
        ITestDevice device = testInfo.getDevice();
        try {
            configure(device.getSerialNumber());
        } catch (RuntimeException e) {
            throw new TargetSetupError(e.getMessage(), e, device.getDeviceDescriptor());
        }

        if (mWaitForDeviceOnline) {
            // Verify that the device is online after executing AOA actions
            device.waitForDeviceOnline();
        }
    }

    // Connect to device using its serial number and perform actions
    private void configure(String serialNumber) throws DeviceNotAvailableException {
        try (UsbHelper usb = getUsbHelper();
                AoaDevice device = usb.getAoaDevice(serialNumber, mDeviceTimeout)) {
            if (device == null) {
                throw new DeviceNotAvailableException(
                        "AOAv2-compatible device not found", serialNumber);
            }
            CLog.d("Performing %d actions on device %s", mActions.size(), serialNumber);
            mActions.forEach(action -> execute(device, action));
        }
    }

    @VisibleForTesting
    UsbHelper getUsbHelper() {
        return new UsbHelper();
    }

    // Parse and execute an action
    @VisibleForTesting
    void execute(AoaDevice device, String input) {
        CLog.v("Executing '%s' on %s", input, device.getSerialNumber());
        List<List<String>> args = new ArrayList<>();
        Action action = ACTIONS.retrieve(args, input);
        if (action == null) {
            throw new IllegalArgumentException(String.format("Invalid action %s", input));
        }
        action.accept(device, args.get(0));
    }

    // Construct point from string coordinates
    private static Point parsePoint(String x, String y) {
        return new Point(Integer.decode(x), Integer.decode(y));
    }

    // Construct duration from string milliseconds
    private static Duration parseMillis(String millis) {
        return Duration.ofMillis(Long.parseLong(millis));
    }

    // Convert a string value into a HID keycode
    private static Integer parseKeycode(String key) {
        if (key == null || key.isEmpty()) {
            return null;
        }
        if (key.matches("\\s+")) {
            return 0x2C; // Convert whitespace to the space character
        }
        // Lookup keycode or try to parse into an integer
        Integer keycode = KEYCODES.get(key.toLowerCase());
        return keycode != null ? keycode : Integer.decode(key);
    }

    // Map of characters to HID keycodes
    private static final Map<String, Integer> KEYCODES = new HashMap<>();

    static {
        // Letters
        KEYCODES.put("a", 0x04);
        KEYCODES.put("b", 0x05);
        KEYCODES.put("c", 0x06);
        KEYCODES.put("d", 0x07);
        KEYCODES.put("e", 0x08);
        KEYCODES.put("f", 0x09);
        KEYCODES.put("g", 0x0A);
        KEYCODES.put("h", 0x0B);
        KEYCODES.put("i", 0x0C);
        KEYCODES.put("j", 0x0D);
        KEYCODES.put("k", 0x0E);
        KEYCODES.put("l", 0x0F);
        KEYCODES.put("m", 0x10);
        KEYCODES.put("n", 0x11);
        KEYCODES.put("o", 0x12);
        KEYCODES.put("p", 0x13);
        KEYCODES.put("q", 0x14);
        KEYCODES.put("r", 0x15);
        KEYCODES.put("s", 0x16);
        KEYCODES.put("t", 0x17);
        KEYCODES.put("u", 0x18);
        KEYCODES.put("v", 0x19);
        KEYCODES.put("w", 0x1A);
        KEYCODES.put("x", 0x1B);
        KEYCODES.put("y", 0x1C);
        KEYCODES.put("z", 0x1D);
        // Numbers
        KEYCODES.put("1", 0x1E);
        KEYCODES.put("2", 0x1F);
        KEYCODES.put("3", 0x20);
        KEYCODES.put("4", 0x21);
        KEYCODES.put("5", 0x22);
        KEYCODES.put("6", 0x23);
        KEYCODES.put("7", 0x24);
        KEYCODES.put("8", 0x25);
        KEYCODES.put("9", 0x26);
        KEYCODES.put("0", 0x27);
        // Additional keys
        KEYCODES.put("enter", 0x28);
        KEYCODES.put("tab", 0x2B);
        KEYCODES.put("space", 0x2C);
        KEYCODES.put("right", 0x4F);
        KEYCODES.put("left", 0x50);
        KEYCODES.put("down", 0x51);
        KEYCODES.put("up", 0x52);
    }
}
