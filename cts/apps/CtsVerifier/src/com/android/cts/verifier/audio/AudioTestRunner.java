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

package com.android.cts.verifier.audio;

import android.os.Handler;
import android.os.Message;
import android.util.Log;

public class AudioTestRunner implements Runnable {

    public static final int TEST_STARTED = 900;
    public static final int TEST_MESSAGE = 903;
    public static final int TEST_ENDED_OK = 904;
    public static final int TEST_ENDED_ERROR = 905;

    private final String mTag;
    public final int mTestId;
    private final Handler mHandler;
    AudioTestRunner(String tag, int testId, Handler handler) {
        mTag = tag;
        mTestId = testId;
        mHandler = handler;
        Log.v(mTag,"New AudioTestRunner test: " + mTestId);
    }
    public void run() {
        sendMessage(mTestId, TEST_STARTED,"");
    };

    public void sleep(int durationMs) {
        try {
            Thread.sleep(durationMs);
        } catch (InterruptedException e) {
            e.printStackTrace();
            //restore interrupted status
            Thread.currentThread().interrupt();
        }
    }

    public void sendMessage(int msgType, String str) {
        sendMessage(mTestId, msgType, str);
    }

    public void sendMessage(int testId, int msgType, String str) {
        Message msg = Message.obtain();
        msg.what = msgType;
        msg.obj = str;
        msg.arg1 = testId;
        mHandler.sendMessage(msg);
    }

    public static class AudioTestRunnerMessageHandler extends Handler {
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            int testId = msg.arg1; //testId
            String str = (String) msg.obj;
            switch (msg.what) {
                case TEST_STARTED:
                    testStarted(testId, str);
                    break;
                case TEST_MESSAGE:
                    testMessage(testId, str);
                    break;
                case TEST_ENDED_OK:
                    testEndedOk(testId, str);
                    break;
                case TEST_ENDED_ERROR:
                    testEndedError(testId, str);
                    break;
                default:
                    Log.e("TestHandler", String.format("Unknown message: %d", msg.what));
            }
        }

        public void testStarted(int testId, String str) {
            //replace with your code
        }

        public void testMessage(int testId, String str) {
            //replace with your code
        }

        public void testEndedOk(int testId, String str) {
            //replace with your code
        }

        public void testEndedError(int testId, String str) {
            //replace with your code
        }
    }
}