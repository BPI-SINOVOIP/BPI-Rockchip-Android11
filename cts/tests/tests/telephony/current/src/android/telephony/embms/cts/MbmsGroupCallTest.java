/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.telephony.embms.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.annotation.Nullable;
import android.telephony.cts.embmstestapp.CtsGroupCallService;
import android.telephony.mbms.GroupCallCallback;
import android.telephony.mbms.MbmsErrors;
import android.telephony.mbms.GroupCall;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import org.junit.Test;

public class MbmsGroupCallTest extends MbmsGroupCallTestBase {
    private class TestGroupCallCallback implements GroupCallCallback {
        private final BlockingQueue<SomeArgs> mErrorCalls = new LinkedBlockingQueue<>();
        private final BlockingQueue<SomeArgs> mGroupCallStateChangedCalls=
                new LinkedBlockingQueue<>();
        private final BlockingQueue<SomeArgs> mBroadcastSignalStrengthUpdatedCalls =
                new LinkedBlockingQueue<>();

        @Override
        public void onError(int errorCode, @Nullable String message) {
            GroupCallCallback.super.onError(errorCode, message);
            SomeArgs args = SomeArgs.obtain();
            args.arg1 = errorCode;
            args.arg2 = message;
            mErrorCalls.add(args);
        }

        @Override
        public void onGroupCallStateChanged(int state, int reason) {
            GroupCallCallback.super.onGroupCallStateChanged(state, reason);
            SomeArgs args = SomeArgs.obtain();
            args.arg1 = state;
            args.arg2 = reason;
            mGroupCallStateChangedCalls.add(args);
        }

        @Override
        public void onBroadcastSignalStrengthUpdated(int signalStrength) {
            GroupCallCallback.super.onBroadcastSignalStrengthUpdated(signalStrength);
            SomeArgs args = SomeArgs.obtain();
            args.arg1 = signalStrength;
            mBroadcastSignalStrengthUpdatedCalls.add(args);
        }

        public SomeArgs waitOnError() {
            try {
                return mErrorCalls.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return null;
            }
        }

        public SomeArgs waitOnGroupCallStateChanged() {
            try {
                return mGroupCallStateChangedCalls.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return null;
            }
        }

        public SomeArgs waitOnBroadcastSignalStrengthUpdated() {
            try {
                return mBroadcastSignalStrengthUpdatedCalls.poll(
                        ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return null;
            }
        }
    }

    private static final long TMGI = 568734963245L;
    private static final List<Integer> SAI_LIST = Arrays.asList(16, 24, 46, 76);
    private static final List<Integer> FREQUENCY_LIST = Arrays.asList(2075, 2050, 1865);

    private TestGroupCallCallback mGroupCallCallback =
            new TestGroupCallCallback();

    @Test
    public void testStartGroupCall() throws Exception {
        GroupCall groupCall = mGroupCallSession.startGroupCall(TMGI, SAI_LIST, FREQUENCY_LIST,
                mCallbackExecutor, mGroupCallCallback
        );
        assertNotNull(groupCall);
        assertEquals(TMGI, groupCall.getTmgi());

        SomeArgs args = mGroupCallCallback.waitOnGroupCallStateChanged();
        assertEquals(GroupCall.STATE_STARTED, args.arg1);
        assertEquals(GroupCall.REASON_BY_USER_REQUEST, args.arg2);

        List<List<Object>> startGroupCallCalls =
                getMiddlewareCalls(CtsGroupCallService.METHOD_START_GROUP_CALL);
        assertEquals(1, startGroupCallCalls.size());
        List<Object> startGroupCallCall = startGroupCallCalls.get(0);
        assertEquals(TMGI, startGroupCallCall.get(2));
        assertEquals(SAI_LIST, startGroupCallCall.get(3));
        assertEquals(FREQUENCY_LIST, startGroupCallCall.get(4));
    }

    @Test
    public void testUpdateGroupCall() throws Exception {
        GroupCall groupCall = mGroupCallSession.startGroupCall(TMGI, SAI_LIST, FREQUENCY_LIST,
                mCallbackExecutor, mGroupCallCallback
        );
        List<Integer> newSais = Collections.singletonList(16);
        List<Integer> newFreqs = Collections.singletonList(2025);
        groupCall.updateGroupCall(newSais, newFreqs);

        List<List<Object>> updateGroupCallCalls =
                getMiddlewareCalls(CtsGroupCallService.METHOD_UPDATE_GROUP_CALL);
        assertEquals(1, updateGroupCallCalls.size());
        List<Object> updateGroupCallCall = updateGroupCallCalls.get(0);
        assertEquals(TMGI, updateGroupCallCall.get(2));
        assertEquals(newSais, updateGroupCallCall.get(3));
        assertEquals(newFreqs, updateGroupCallCall.get(4));
    }

    @Test
    public void testStopGroupCall() throws Exception {
        GroupCall groupCall = mGroupCallSession.startGroupCall(TMGI, SAI_LIST, FREQUENCY_LIST,
                mCallbackExecutor, mGroupCallCallback
        );
        groupCall.close();
        List<List<Object>> stopGroupCallCalls =
                getMiddlewareCalls(CtsGroupCallService.METHOD_STOP_GROUP_CALL);
        assertEquals(1, stopGroupCallCalls.size());
        assertEquals(TMGI, stopGroupCallCalls.get(0).get(2));
    }

    @Test
    public void testGroupCallCallbacks() throws Exception {
        mGroupCallSession.startGroupCall(TMGI, SAI_LIST, FREQUENCY_LIST, mCallbackExecutor,
                mGroupCallCallback
        );
        mMiddlewareControl.fireErrorOnGroupCall(MbmsErrors.GeneralErrors.ERROR_IN_E911,
                MbmsGroupCallTest.class.getSimpleName());
        SomeArgs groupCallErrorArgs = mGroupCallCallback.waitOnError();
        assertEquals(MbmsErrors.GeneralErrors.ERROR_IN_E911, groupCallErrorArgs.arg1);
        assertEquals(MbmsGroupCallTest.class.getSimpleName(), groupCallErrorArgs.arg2);

        int broadcastSignalStrength = 3;
        mMiddlewareControl.fireBroadcastSignalStrengthUpdated(broadcastSignalStrength);
        assertEquals(broadcastSignalStrength,
                mGroupCallCallback.waitOnBroadcastSignalStrengthUpdated().arg1);
    }

    @Test
    public void testStartGroupCallFailure() throws Exception {
        mMiddlewareControl.forceErrorCode(
                MbmsErrors.GeneralErrors.ERROR_MIDDLEWARE_TEMPORARILY_UNAVAILABLE);
        mGroupCallSession.startGroupCall(TMGI, SAI_LIST, FREQUENCY_LIST, mCallbackExecutor,
                mGroupCallCallback
        );
        assertEquals(MbmsErrors.GeneralErrors.ERROR_MIDDLEWARE_TEMPORARILY_UNAVAILABLE,
                mCallback.waitOnError().arg1);
    }
}
