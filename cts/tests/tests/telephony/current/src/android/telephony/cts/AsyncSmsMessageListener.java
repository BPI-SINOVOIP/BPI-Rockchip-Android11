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
package android.telephony.cts;

import android.content.Intent;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class AsyncSmsMessageListener {

    private static final AsyncSmsMessageListener sInstance = new AsyncSmsMessageListener();

    public static final AsyncSmsMessageListener getInstance() {
        return sInstance;
    }

    private final LinkedBlockingQueue<String> mMessages = new LinkedBlockingQueue<>(1);
    private final LinkedBlockingQueue<Intent> mSentMessageResults = new LinkedBlockingQueue<>(1);
    private final LinkedBlockingQueue<Intent> mDeliveredMessageResults =
            new LinkedBlockingQueue<>(1);

    /**
     * Offer a SMS message to the queue of SMS messages waiting to be processed.
     */
    public void offerSmsMessage(String smsMessage) {
        mMessages.offer(smsMessage);
    }

    /**
     * Wait for timeoutMs for a incoming SMS message to be received and return that SMS message,
     * or null if the SMS message was not received before the timeout.
     */
    public String waitForSmsMessage(int timeoutMs) {
        try {
            return mMessages.poll(timeoutMs, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return null;
        }
    }

    public void offerMessageSentIntent(Intent intent) {
        mSentMessageResults.offer(intent);
    }

    public Intent waitForMessageSentIntent(int timeoutMs) {
        try {
            return mSentMessageResults.poll(timeoutMs, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return null;
        }
    }

    public void offerMessageDeliveredIntent(Intent intent) {
        mDeliveredMessageResults.offer(intent);
    }

    public Intent waitForMessageDeliveredIntent(int timeoutMs) {
        try {
            return mDeliveredMessageResults.poll(timeoutMs, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            return null;
        }
    }
}
