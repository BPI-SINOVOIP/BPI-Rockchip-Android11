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

package com.android.tradefed.util;

import com.android.ddmlib.MultiLineReceiver;
import com.android.loganalysis.item.LogcatItem;
import com.android.loganalysis.item.MiscLogcatItem;
import com.android.loganalysis.parser.LogcatParser;
import com.android.tradefed.device.BackgroundDeviceAction;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;

import com.google.common.annotations.VisibleForTesting;

import java.io.Closeable;
import java.util.AbstractMap;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Parse logcat input for events.
 *
 * <p>This class interprets logcat messages and can inform the listener of events in both a blocking
 * and polling fashion.
 */
public class LogcatEventParser implements Closeable {

    // define a custom logcat parser category for events of interest
    private static final String CUSTOM_CATEGORY = "parserEvent";
    private static final String LOGCAT_DESC = "LogcatEventParser-logcat";
    private static final String BASE_LOGCAT_CMD = "logcat -v threadtime -s";

    private final EventTriggerMap mEventTriggerMap = new EventTriggerMap();
    private final MultiLineReceiver mLogcatReceiver = new EventReceiver();
    private final LogcatParser mInternalParser = new LogcatParser();
    private final Set<String> mLogcatTags = new HashSet<>();
    private final LinkedBlockingQueue<LogcatEvent> mEventQueue = new LinkedBlockingQueue<>();
    private BackgroundDeviceAction mDeviceAction;
    private ITestDevice mDevice;
    private boolean mCancelled = false;

    /** Struct to hold a logcat event with the event type and triggering logcat message */
    public static class LogcatEvent {
        private LogcatEventType eventType;
        private String msg;

        private LogcatEvent(LogcatEventType eventType, String msg) {
            this.eventType = eventType;
            this.msg = msg;
        }

        public LogcatEventType getEventType() {
            return eventType;
        }

        public String getMessage() {
            return msg;
        }
    }

    /**
     * Helper class to store (logcat tag, message) with the corresponding event Enum.
     *
     * <p>The message registered with {@link #put(String, String, LogcatEventType)} may be a
     * substring of the actual logcat message.
     */
    private class EventTriggerMap {
        // a map that each tag corresponds to multiple entry of (partial message, response)
        private MultiMap<String, AbstractMap.SimpleEntry<String, LogcatEventType>> mResponses =
                new MultiMap<>();

        /** Register a event Enum with a specific logcat tag and message. */
        private void put(String tag, String partialMessage, LogcatEventType response) {
            mResponses.put(tag, new AbstractMap.SimpleEntry<>(partialMessage, response));
        }

        /** Return an event Enum with exactly matching tag and partially matching message. */
        private LogcatEventType get(String tag, String message) {
            for (Map.Entry<String, LogcatEventType> entry : mResponses.get(tag)) {
                if (message.contains(entry.getKey())) {
                    return entry.getValue();
                }
            }
            return null;
        }
    }

    /** Receives output from a shell command and forwards it to the outer class line by line. */
    private class EventReceiver extends MultiLineReceiver {
        @Override
        public void processNewLines(String[] strings) {
            parseEvents(strings);
        }

        @Override
        public boolean isCancelled() {
            return mCancelled;
        }
    }

    /**
     * Instantiates a new LogcatEventParser
     *
     * @param device to read logcat from
     */
    public LogcatEventParser(ITestDevice device) {
        mDevice = device;
    }

    /**
     * Register an event of given logcat tag and message with the desired response. Message may be
     * partial.
     */
    public void registerEventTrigger(String tag, String msg, LogcatEventType response) {
        mEventTriggerMap.put(tag, msg, response);
        if (mLogcatTags.add(tag)) {
            // Pattern regex is null because we will rely on EventTriggerMap to parse events.
            // Logcat messages must all be of INFO level TODO: generalize this for all levels
            mInternalParser.addPattern(null, "I", tag, CUSTOM_CATEGORY);
        }
    }

    /**
     * Blocks until it receives an event.
     *
     * @param timeoutMs Time to wait in milliseconds
     * @return The event or {@code null} if the timeout is reached
     */
    public LogcatEvent waitForEvent(long timeoutMs) throws InterruptedException {
        return mEventQueue.poll(timeoutMs, TimeUnit.MILLISECONDS);
    }

    /**
     * Polls the event queue. Returns immediately.
     *
     * @return The event or {@code null} if no matching event is found
     */
    public LogcatEvent pollForEvent() {
        return mEventQueue.poll();
    }

    /**
     * Parse logcat lines and add any captured events (that were registered with {@link
     * #registerEventTrigger(String, String, LogcatEventType)}) to the event queue.
     */
    @VisibleForTesting
    public void parseEvents(String[] lines) {
        if (lines.length == 0) {
            return;
        }
        CLog.v(String.join("\n", lines));
        LogcatItem items;
        // LogcatParser maintains a state
        synchronized (mInternalParser) {
            items = mInternalParser.parse(Arrays.asList(lines));
            mInternalParser.clear();
        }
        if (items == null) {
            return;
        }
        for (MiscLogcatItem item : items.getEvents()) {
            LogcatEventType eventType = mEventTriggerMap.get(item.getTag(), item.getStack());
            if (eventType != null) {
                mEventQueue.add(new LogcatEvent(eventType, item.getStack()));
            }
        }
    }

    /** Start listening to logcat and parsing events. */
    public void start() {
        if (mLogcatTags.isEmpty()) {
            throw new RuntimeException("LogcatEventParser started with no event triggers.");
        }
        StringBuilder logcatCmdBuilder = new StringBuilder(BASE_LOGCAT_CMD);
        for (String tag : mLogcatTags) {
            logcatCmdBuilder.append(" ").append(tag).append(":I");
        }
        String logcatCmd = logcatCmdBuilder.toString();
        CLog.i("Starting logcat with command: %s", logcatCmd);
        mDeviceAction =
                new BackgroundDeviceAction(logcatCmd, LOGCAT_DESC, mDevice, mLogcatReceiver, 0);
        mDeviceAction.start();
    }

    /** Stop listening to logcat. */
    @Override
    public void close() {
        mCancelled = true;
        if (mDeviceAction != null) {
            mDeviceAction.cancel();
        }
    }
}
