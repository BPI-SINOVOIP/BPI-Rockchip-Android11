/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.server.wm.backgroundactivity.common;

import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;
import android.server.wm.backgroundactivity.common.CommonComponents.Event;

import java.util.concurrent.TimeoutException;

/**
 * Class used to register for events sent via IPC using {@link #getNotifier()} parcelable.
 *
 * Create an instance for the event of interest, pass {@link #getNotifier()} to the target, either
 * via some AIDL or intent extra, have the caller call {@link ResultReceiver#send(int, Bundle)},
 * then finally wait for the event with {@link #waitForEventOrThrow(long)}.
 */
public class EventReceiver {
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final ConditionVariable mEventReceivedVariable = new ConditionVariable(false);

    @Event
    private final int mEvent;

    public EventReceiver(@Event int event) {
        mEvent = event;
    }

    public ResultReceiver getNotifier() {
        return new ResultReceiver(mHandler) {
            @Override
            protected void onReceiveResult(@Event int event, Bundle data) {
                if (event == mEvent) {
                    mEventReceivedVariable.open();
                }
            }
        };
    }

    /**
     * Waits for registered event or throws {@link TimeoutException}.
     */
    public void waitForEventOrThrow(long timeoutMs) throws TimeoutException {
        // Avoid deadlocks
        if (Thread.currentThread() == mHandler.getLooper().getThread()) {
            throw new IllegalStateException("This method should be called from different thread");
        }

        if (!mEventReceivedVariable.block(timeoutMs)) {
            throw new TimeoutException("Timed out waiting for event " + mEvent);
        }
    }
}
