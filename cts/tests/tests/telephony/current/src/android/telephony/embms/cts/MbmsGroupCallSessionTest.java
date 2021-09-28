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
import static org.junit.Assert.fail;

import android.telephony.MbmsGroupCallSession;
import android.telephony.cts.embmstestapp.CtsGroupCallService;
import android.telephony.mbms.GroupCallCallback;
import android.telephony.mbms.MbmsErrors;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.junit.Test;

public class MbmsGroupCallSessionTest extends MbmsGroupCallTestBase {
    @Test
    public void testDuplicateSession() throws Exception {
        try {
            MbmsGroupCallSession failure = MbmsGroupCallSession.create(
                    mContext, mCallbackExecutor, mCallback);
            fail("Duplicate create should've thrown an exception");
        } catch (IllegalStateException e) {
            // Succeed
        }
    }

    @Test
    public void testClose() throws Exception {
        mGroupCallSession.close();

        // Make sure we can't use it anymore
        try {
            mGroupCallSession.startGroupCall(0, Collections.emptyList(), Collections.emptyList(),
                    mCallbackExecutor, new GroupCallCallback() {});
            fail("GroupCall session should not be usable after close");
        } catch (IllegalStateException e) {
            // Succeed
        }

        // Make sure that the middleware got the call to close
        List<List<Object>> closeCalls = getMiddlewareCalls(CtsGroupCallService.METHOD_CLOSE);
        assertEquals(1, closeCalls.size());
    }

    @Test
    public void testErrorDelivery() throws Exception {
        mMiddlewareControl.forceErrorCode(
                MbmsErrors.GeneralErrors.ERROR_MIDDLEWARE_TEMPORARILY_UNAVAILABLE);
        mGroupCallSession.startGroupCall(0, Collections.emptyList(), Collections.emptyList(),
                mCallbackExecutor, new GroupCallCallback() {});
        assertEquals(MbmsErrors.GeneralErrors.ERROR_MIDDLEWARE_TEMPORARILY_UNAVAILABLE,
                mCallback.waitOnError().arg1);
    }

    @Test
    public void testCallbacks() throws Exception {
        List<Integer> expectCurrentSais = Arrays.asList(10, 14, 17);
        List<List<Integer>> expectAvailableSais = new ArrayList<List<Integer>>() {{
            add(expectCurrentSais);
            add(Arrays.asList(11, 15, 17));
        }};
        mMiddlewareControl.fireAvailableSaisUpdated(expectCurrentSais, expectAvailableSais);
        SomeArgs callbackResult = mCallback.waitOnAvailableSaisUpdatedCalls();
        assertEquals(callbackResult.arg1, expectCurrentSais);
        assertEquals(callbackResult.arg2, expectAvailableSais);

        String interfaceName = "TEST";
        int index = 10;
        mMiddlewareControl.fireServiceInterfaceAvailable(interfaceName, index);
        callbackResult = mCallback.waitOnServiceInterfaceAvailableCalls();
        assertEquals(interfaceName, callbackResult.arg1);
        assertEquals(index, callbackResult.arg2);
    }
}
