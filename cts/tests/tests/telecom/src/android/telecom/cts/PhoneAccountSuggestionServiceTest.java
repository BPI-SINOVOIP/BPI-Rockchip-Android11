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
package android.telecom.cts;

import static android.telecom.cts.TestUtils.TEST_PHONE_ACCOUNT_HANDLE;
import static android.telecom.cts.TestUtils.shouldTestTelecom;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.content.ComponentName;
import android.net.Uri;
import android.os.Bundle;
import android.telecom.Call;
import android.telecom.PhoneAccountHandle;
import android.telecom.PhoneAccountSuggestion;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

public class PhoneAccountSuggestionServiceTest extends BaseTelecomTestWithMockServices {
    private static final long TEST_TIMEOUT = 5000;
    private PhoneAccountHandle mCachedDefaultHandle;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        if (shouldTestTelecom(mContext)) {
            TestUtils.setCtsPhoneAccountSuggestionService(getInstrumentation(),
                    new ComponentName(mContext, CtsPhoneAccountSuggestionService.class));
            mTelecomManager.registerPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT);
            mTelecomManager.registerPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT_2);
            mCachedDefaultHandle = mTelecomManager.getUserSelectedOutgoingPhoneAccount();
            runWithShellPermissionIdentity(
                    () -> mTelecomManager.setUserSelectedOutgoingPhoneAccount(null));
            TestUtils.enablePhoneAccount(getInstrumentation(), TestUtils.TEST_PHONE_ACCOUNT_HANDLE);
            TestUtils.enablePhoneAccount(
                    getInstrumentation(), TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2);
            CtsPhoneAccountSuggestionService.enableService(mContext);
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (shouldTestTelecom(mContext)) {
            if (mInCallCallbacks.getService().getLastCall() != null) {
                mInCallCallbacks.getService().getLastCall().disconnect();
            }
            TestUtils.setCtsPhoneAccountSuggestionService(getInstrumentation(), null);
            mTelecomManager.unregisterPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT_HANDLE);
            mTelecomManager.unregisterPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2);
            runWithShellPermissionIdentity(() ->
                    mTelecomManager.setUserSelectedOutgoingPhoneAccount(mCachedDefaultHandle));
            CtsPhoneAccountSuggestionService.disableService(mContext);
        }
        super.tearDown();
    }

    public void testSuggestionFlow() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CtsPhoneAccountSuggestionService.sSuggestionsToProvide =
                new ArrayList<PhoneAccountSuggestion>() {{
                    add(new PhoneAccountSuggestion(TestUtils.TEST_PHONE_ACCOUNT_HANDLE,
                            PhoneAccountSuggestion.REASON_NONE,
                            false));

                    add(new PhoneAccountSuggestion(TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2,
                            PhoneAccountSuggestion.REASON_FREQUENT,
                            true));
                }};

        Uri number = createTestNumber();
        mTelecomManager.placeCall(number, new Bundle());

        assertEquals(number.getSchemeSpecificPart(),
                CtsPhoneAccountSuggestionService.sSuggestionRequestQueue.poll(
                        TEST_TIMEOUT, TimeUnit.MILLISECONDS));

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        final MockInCallService inCallService = mInCallCallbacks.getService();
        Call phoneAcctSelectCall = inCallService.getLastCall();
        assertEquals(Call.STATE_SELECT_PHONE_ACCOUNT, phoneAcctSelectCall.getState());
        List<PhoneAccountSuggestion> receivedSuggestions =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.EXTRA_SUGGESTED_PHONE_ACCOUNTS);
        assertTrue(CtsPhoneAccountSuggestionService.sSuggestionsToProvide.size()
                <= receivedSuggestions.size());
        assertTrue(receivedSuggestions.containsAll(
                CtsPhoneAccountSuggestionService.sSuggestionsToProvide));
    }

    public void testSuggestionTimeout() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CtsPhoneAccountSuggestionService.sSuggestionsToProvide =
                new ArrayList<PhoneAccountSuggestion>() {{
                    add(new PhoneAccountSuggestion(TestUtils.TEST_PHONE_ACCOUNT_HANDLE,
                            PhoneAccountSuggestion.REASON_NONE,
                            false));

                    add(new PhoneAccountSuggestion(TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2,
                            PhoneAccountSuggestion.REASON_FREQUENT,
                            true));
                }};

        // Force a Telecom time out
        CtsPhoneAccountSuggestionService.sSuggestionWaitTime =
                2 * 1000 * TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S;

        Uri number = createTestNumber();
        mTelecomManager.placeCall(number, new Bundle());

        assertEquals(number.getSchemeSpecificPart(),
                CtsPhoneAccountSuggestionService.sSuggestionRequestQueue.poll(
                        TEST_TIMEOUT, TimeUnit.MILLISECONDS));

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        final MockInCallService inCallService = mInCallCallbacks.getService();
        Call phoneAcctSelectCall = inCallService.getLastCall();
        assertEquals(Call.STATE_SELECT_PHONE_ACCOUNT, phoneAcctSelectCall.getState());
        List<PhoneAccountSuggestion> receivedSuggestions =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.EXTRA_SUGGESTED_PHONE_ACCOUNTS);

        List<PhoneAccountHandle> receivedAccounts =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.AVAILABLE_PHONE_ACCOUNTS);
        // We don't need to assert anything about the contents, just make sure that we get
        // some default suggestions that match up with the available accounts.
        assertEquals(receivedAccounts.size(), receivedSuggestions.size());
        assertTrue(receivedAccounts.containsAll(getHandlesFromSuggestions(receivedSuggestions)));
    }

    public void testEmptySuggestions() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CtsPhoneAccountSuggestionService.sSuggestionsToProvide = Collections.emptyList();

        Uri number = createTestNumber();
        mTelecomManager.placeCall(number, new Bundle());

        assertEquals(number.getSchemeSpecificPart(),
                CtsPhoneAccountSuggestionService.sSuggestionRequestQueue.poll(
                        TEST_TIMEOUT, TimeUnit.MILLISECONDS));

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        final MockInCallService inCallService = mInCallCallbacks.getService();
        Call phoneAcctSelectCall = inCallService.getLastCall();
        assertEquals(Call.STATE_SELECT_PHONE_ACCOUNT, phoneAcctSelectCall.getState());
        List<PhoneAccountSuggestion> receivedSuggestions =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.EXTRA_SUGGESTED_PHONE_ACCOUNTS);
        List<PhoneAccountHandle> receivedAccounts =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.AVAILABLE_PHONE_ACCOUNTS);
        assertEquals(receivedAccounts.size(), receivedSuggestions.size());
        assertTrue(receivedAccounts.containsAll(getHandlesFromSuggestions(receivedSuggestions)));
    }

    public void testPartialSuggestions() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CtsPhoneAccountSuggestionService.sSuggestionsToProvide =
                new ArrayList<PhoneAccountSuggestion>() {{
                    add(new PhoneAccountSuggestion(TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2,
                            PhoneAccountSuggestion.REASON_FREQUENT,
                            true));
                }};

        Uri number = createTestNumber();
        mTelecomManager.placeCall(number, new Bundle());

        assertEquals(number.getSchemeSpecificPart(),
                CtsPhoneAccountSuggestionService.sSuggestionRequestQueue.poll(
                        TEST_TIMEOUT, TimeUnit.MILLISECONDS));

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        final MockInCallService inCallService = mInCallCallbacks.getService();
        Call phoneAcctSelectCall = inCallService.getLastCall();
        assertEquals(Call.STATE_SELECT_PHONE_ACCOUNT, phoneAcctSelectCall.getState());
        List<PhoneAccountSuggestion> receivedSuggestions =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.EXTRA_SUGGESTED_PHONE_ACCOUNTS);
        List<PhoneAccountHandle> receivedAccounts =
                phoneAcctSelectCall.getDetails().getIntentExtras()
                        .getParcelableArrayList(Call.AVAILABLE_PHONE_ACCOUNTS);
        assertEquals(receivedAccounts.size(), receivedSuggestions.size());
        // Make sure the received list contains the one that we provided.
        assertTrue(receivedSuggestions
                .containsAll(CtsPhoneAccountSuggestionService.sSuggestionsToProvide));
        // Make sure the framework filled in the other one.
        assertEquals(1, receivedSuggestions.stream()
                .filter(s -> TEST_PHONE_ACCOUNT_HANDLE.equals(s.getPhoneAccountHandle()))
                .count());
    }

    private static List<PhoneAccountHandle> getHandlesFromSuggestions(
            List<PhoneAccountSuggestion> s) {
        return s.stream().map(PhoneAccountSuggestion::getPhoneAccountHandle)
                .collect(Collectors.toList());
    }
}
