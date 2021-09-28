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
 * limitations under the License
 */

package org.conscrypt;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * Class for reading a process' signal masks from the /proc filesystem.  Looks for the
 * BLOCKED, CAUGHT, IGNORED and PENDING masks from /proc/self/status, each of which is a
 * 64 bit bitmask with one bit per signal.
 *
 * Maintains a map from SignalMaskInfo.Type to the bitmask.  The {@code isValid} method
 * will only return true if all 4 masks were successfully parsed. Provides lookup
 * methods per signal, e.g. {@code isPending(signum)} which will throw
 * {@code IllegalStateException} if the current data is not valid.
 */
public class SignalMaskInfo {
    private enum Type {
        BLOCKED("SigBlk"),
        CAUGHT("SigCgt"),
        IGNORED("SigIgn"),
        PENDING("SigPnd");

        // The tag for this mask in /proc/self/status
        private final String tag;

        Type(String tag) {
            this.tag = tag + ":\t";
        }

        public String getTag() {
            return tag;
        }

        public static Map<Type, Long> parseProcinfo(String path) {
            Map<Type, Long> map = new HashMap<>();
            try (BufferedReader reader = new BufferedReader(new FileReader(path))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    for (Type mask : values()) {
                        long value = mask.tryToParse(line);
                        if (value >= 0) {
                            map.put(mask, value);
                        }
                    }
                }
            } catch (NumberFormatException | IOException e) {
                // Ignored - the map will end up being invalid instead.
            }
            return map;
        }

        private long tryToParse(String line) {
            if (line.startsWith(tag)) {
                return Long.valueOf(line.substring(tag.length()), 16);
            } else {
                return -1;
            }
        }
    }

    private static final String PROCFS_PATH = "/proc/self/status";
    private Map<Type, Long> maskMap = null;

    SignalMaskInfo() {
        refresh();
    }

    public void refresh() {
        maskMap = Type.parseProcinfo(PROCFS_PATH);
    }

    public boolean isValid() {
        return (maskMap != null && maskMap.size() == Type.values().length);
    }

    public boolean isCaught(int signal) {
        return isSignalInMask(signal, Type.CAUGHT);
    }

    public boolean isBlocked(int signal) {
        return isSignalInMask(signal, Type.BLOCKED);
    }

    public boolean isPending(int signal) {
        return isSignalInMask(signal, Type.PENDING);
    }

    public boolean isIgnored(int signal) {
        return isSignalInMask(signal, Type.IGNORED);
    }

    private void checkValid() {
        if (!isValid()) {
            throw new IllegalStateException();
        }
    }

    private boolean isSignalInMask(int signal, Type mask) {
        long bit = 1L << (signal - 1);
        return (getSignalMask(mask) & bit) != 0;
    }

    private long getSignalMask(Type mask) {
        checkValid();
        Long value = maskMap.get(mask);
        if (value == null) {
            throw new IllegalStateException();
        }
        return value;
    }
}
