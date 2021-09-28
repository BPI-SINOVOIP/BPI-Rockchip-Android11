/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcp;

import android.util.Log;

import com.android.bluetooth.Utils;

import com.google.common.collect.EvictingQueue;


// This class is to store logs for Avrcp for given size.
public class AvrcpEventLogger {
    private final String mTitle;
    private final EvictingQueue<Event> mEvents;

    // Event class contain timestamp and log context.
    private class Event {
        private final String mTimeStamp;
        private final String mMsg;

        Event(String msg) {
            mTimeStamp = Utils.getLocalTimeString();
            mMsg = msg;
        }

        public String toString() {
            return (new StringBuilder(mTimeStamp)
                    .append(" ").append(mMsg).toString());
        }
    }

    AvrcpEventLogger(int size, String title) {
        mEvents = EvictingQueue.create(size);
        mTitle = title;
    }

    synchronized void add(String msg) {
        Event event = new Event(msg);
        mEvents.add(event);
    }

    synchronized void logv(String tag, String msg) {
        add(msg);
        Log.v(tag, msg);
    }

    synchronized void logd(String tag, String msg) {
        logd(true, tag, msg);
    }

    synchronized void logd(boolean debug, String tag, String msg) {
        add(msg);
        if (debug) {
            Log.d(tag, msg);
        }
    }

    synchronized void dump(StringBuilder sb) {
        sb.append("Avrcp ").append(mTitle).append(":\n");
        for (Event event : mEvents) {
            sb.append("  ").append(event.toString()).append("\n");
        }
    }
}
