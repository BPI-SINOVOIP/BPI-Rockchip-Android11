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

package com.android.server.wm.flicker;

import androidx.annotation.Nullable;

import com.android.server.wm.flicker.Assertions.Result;
import com.android.server.wm.nano.ActivityRecordProto;
import com.android.server.wm.nano.TaskProto;
import com.android.server.wm.nano.WindowManagerTraceFileProto;
import com.android.server.wm.nano.WindowManagerTraceProto;
import com.android.server.wm.nano.WindowStateProto;
import com.android.server.wm.nano.WindowTokenProto;

import com.google.protobuf.nano.InvalidProtocolBufferNanoException;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/**
 * Contains a collection of parsed WindowManager trace entries and assertions to apply over a single
 * entry.
 *
 * <p>Each entry is parsed into a list of {@link WindowManagerTrace.Entry} objects.
 */
public class WindowManagerTrace {
    private static final int DEFAULT_DISPLAY = 0;
    private final List<Entry> mEntries;
    @Nullable private final Path mSource;

    private WindowManagerTrace(List<Entry> entries, Path source) {
        this.mEntries = entries;
        this.mSource = source;
    }

    /**
     * Parses {@code WindowManagerTraceFileProto} from {@code data} and uses the proto to generates
     * a list of trace entries.
     *
     * @param data binary proto data
     * @param source Path to source of data for additional debug information
     */
    public static WindowManagerTrace parseFrom(byte[] data, Path source) {
        List<Entry> entries = new ArrayList<>();

        WindowManagerTraceFileProto fileProto;
        try {
            fileProto = WindowManagerTraceFileProto.parseFrom(data);
        } catch (InvalidProtocolBufferNanoException e) {
            throw new RuntimeException(e);
        }
        for (WindowManagerTraceProto entryProto : fileProto.entry) {
            entries.add(new Entry(entryProto));
        }
        return new WindowManagerTrace(entries, source);
    }

    public static WindowManagerTrace parseFrom(byte[] data) {
        return parseFrom(data, null);
    }

    public List<Entry> getEntries() {
        return mEntries;
    }

    public Entry getEntry(long timestamp) {
        Optional<Entry> entry =
                mEntries.stream().filter(e -> e.getTimestamp() == timestamp).findFirst();
        if (!entry.isPresent()) {
            throw new RuntimeException("Entry does not exist for timestamp " + timestamp);
        }
        return entry.get();
    }

    public Optional<Path> getSource() {
        return Optional.ofNullable(mSource);
    }

    /** Represents a single WindowManager trace entry. */
    public static class Entry implements ITraceEntry {
        private final WindowManagerTraceProto mProto;

        public Entry(WindowManagerTraceProto proto) {
            mProto = proto;
        }

        private static Result isWindowVisible(
                String windowTitle, WindowTokenProto[] windowTokenProtos) {
            boolean titleFound = false;
            for (WindowTokenProto windowToken : windowTokenProtos) {
                for (WindowStateProto windowState : windowToken.windows) {
                    if (windowState.identifier.title.contains(windowTitle)) {
                        titleFound = true;
                        if (isVisible(windowState)) {
                            return new Result(
                                    true /* success */,
                                    windowState.identifier.title + " is visible");
                        }
                    }
                }
            }

            String reason;
            if (!titleFound) {
                reason = windowTitle + " cannot be found";
            } else {
                reason = windowTitle + " is invisible";
            }
            return new Result(false /* success */, reason);
        }

        private static boolean isVisible(WindowStateProto windowState) {
            return windowState.windowContainer.visible;
        }

        @Override
        public long getTimestamp() {
            return mProto.elapsedRealtimeNanos;
        }

        /** Returns window title of the top most visible app window. */
        private String getTopVisibleAppWindow() {
            TaskProto[] tasks =
                    mProto.windowManagerService
                            .rootWindowContainer
                            .displays[DEFAULT_DISPLAY]
                            .tasks;
            for (TaskProto rootTask : tasks) {
                final String topVisible = getTopVisibleAppWindow(rootTask);
                if (topVisible != null) return topVisible;
            }

            return "";
        }

        private String getTopVisibleAppWindow(TaskProto task) {
            for (ActivityRecordProto activity : task.activities) {
                for (WindowStateProto windowState : activity.windowToken.windows) {
                    if (windowState.windowContainer.visible) {
                        return task.activities[0].name;
                    }
                }
            }

            for (TaskProto childTask : task.tasks) {
                final String topVisible = getTopVisibleAppWindow(childTask);
                if (topVisible != null) return topVisible;
            }
            return null;
        }

        /** Checks if aboveAppWindow with {@code windowTitle} is visible. */
        public Result isAboveAppWindowVisible(String windowTitle) {
            WindowTokenProto[] windowTokenProtos =
                    mProto.windowManagerService
                            .rootWindowContainer
                            .displays[DEFAULT_DISPLAY]
                            .aboveAppWindows;
            Result result = isWindowVisible(windowTitle, windowTokenProtos);
            return new Result(result.success, getTimestamp(), "showsAboveAppWindow", result.reason);
        }

        /** Checks if belowAppWindow with {@code windowTitle} is visible. */
        public Result isBelowAppWindowVisible(String windowTitle) {
            WindowTokenProto[] windowTokenProtos =
                    mProto.windowManagerService
                            .rootWindowContainer
                            .displays[DEFAULT_DISPLAY]
                            .belowAppWindows;
            Result result = isWindowVisible(windowTitle, windowTokenProtos);
            return new Result(
                    result.success, getTimestamp(), "isBelowAppWindowVisible", result.reason);
        }

        /** Checks if imeWindow with {@code windowTitle} is visible. */
        public Result isImeWindowVisible(String windowTitle) {
            WindowTokenProto[] windowTokenProtos =
                    mProto.windowManagerService
                            .rootWindowContainer
                            .displays[DEFAULT_DISPLAY]
                            .imeWindows;
            Result result = isWindowVisible(windowTitle, windowTokenProtos);
            return new Result(result.success, getTimestamp(), "isImeWindowVisible", result.reason);
        }

        /** Checks if app window with {@code windowTitle} is on top. */
        public Result isVisibleAppWindowOnTop(String windowTitle) {
            String topAppWindow = getTopVisibleAppWindow();
            boolean success = topAppWindow.contains(windowTitle);
            String reason = "wanted=" + windowTitle + " found=" + topAppWindow;
            return new Result(success, getTimestamp(), "isAppWindowOnTop", reason);
        }

        /** Checks if app window with {@code windowTitle} is visible. */
        public Result isAppWindowVisible(String windowTitle) {
            final String assertionName = "isAppWindowVisible";
            boolean[] titleFound = { false };
            TaskProto[] tasks =
                    mProto.windowManagerService
                            .rootWindowContainer
                            .displays[DEFAULT_DISPLAY]
                            .tasks;
            for (TaskProto task : tasks) {
                final Result result = isAppWindowVisible(
                        windowTitle, assertionName, titleFound, task);
                if (result != null) return result;
            }
            String reason;
            if (!titleFound[0]) {
                reason = "Window " + windowTitle + " cannot be found";
            } else {
                reason = "Window " + windowTitle + " is invisible";
            }
            return new Result(false /* success */, getTimestamp(), assertionName, reason);
        }

        private Result isAppWindowVisible(String windowTitle, String assertionName,
                boolean[] titleFound, TaskProto task) {
            for (ActivityRecordProto activity : task.activities) {
                if (activity.name.contains(windowTitle)) {
                    titleFound[0] = true;
                    for (WindowStateProto windowState : activity.windowToken.windows) {
                        if (windowState.windowContainer.visible) {
                            return new Result(
                                    true /* success */,
                                    getTimestamp(),
                                    assertionName,
                                    "Window " + activity.name + "is visible");
                        }
                    }
                }
            }

            for (TaskProto childTask : task.tasks) {
                final Result result = isAppWindowVisible(
                        windowTitle, assertionName, titleFound, childTask);
                if (result != null) return result;
            }
            return null;
        }
    }
}
