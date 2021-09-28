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
 * limitations under the License.
 */

package android.autofillservice.cts;

import static android.autofillservice.cts.CannedFillResponse.DO_NOT_REPLY_RESPONSE;
import static android.autofillservice.cts.CannedFillResponse.NO_RESPONSE;
import static android.autofillservice.cts.CheckoutActivity.ID_CC_NUMBER;
import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.NULL_DATASET_ID;
import static android.autofillservice.cts.Helper.assertDeprecatedClientState;
import static android.autofillservice.cts.Helper.assertFillEventForAuthenticationSelected;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetAuthenticationSelected;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetSelected;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetShown;
import static android.autofillservice.cts.Helper.assertFillEventForSaveShown;
import static android.autofillservice.cts.Helper.assertNoDeprecatedClientState;
import static android.autofillservice.cts.InstrumentedAutoFillService.waitUntilConnected;
import static android.autofillservice.cts.InstrumentedAutoFillService.waitUntilDisconnected;
import static android.autofillservice.cts.LoginActivity.BACKDOOR_USERNAME;
import static android.autofillservice.cts.LoginActivity.getWelcomeMessage;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_GENERIC;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;

import static com.google.common.truth.Truth.assertWithMessage;

import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.inline.InlineFillEventHistoryTest;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.FillEventHistory;
import android.service.autofill.FillEventHistory.Event;
import android.service.autofill.FillResponse;
import android.view.View;

import org.junit.Test;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This is the common test cases with {@link FillEventHistoryTest} and
 * {@link InlineFillEventHistoryTest}.
 */
@AppModeFull(reason = "Service-specific test")
public abstract class FillEventHistoryCommonTestCase extends AbstractLoginActivityTestCase {

    protected FillEventHistoryCommonTestCase() {}

    protected FillEventHistoryCommonTestCase(UiBot inlineUiBot) {
        super(inlineUiBot);
    }

    protected Bundle getBundle(String key, String value) {
        final Bundle bundle = new Bundle();
        bundle.putString(key, value);
        return bundle;
    }

    @Test
    public void testDatasetAuthenticationSelected() throws Exception {
        enableService();

        // Set up FillResponse with dataset authentication
        Bundle clientState = new Bundle();
        clientState.putCharSequence("clientStateKey", "clientStateValue");

        // Prepare the authenticated response
        final IntentSender authentication = AuthenticationActivity.createSender(mContext, 1,
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation("Dataset", isInlineMode())
                        .build());

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username")
                        .setId("name")
                        .setPresentation("authentication", isInlineMode())
                        .setAuthentication(authentication)
                        .build())
                .setExtras(clientState).build());
        mActivity.expectAutoFill("dude", "sweet");

        // Trigger autofill and IME.
        mUiBot.focusByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();

        // Authenticate
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("authentication");
        mActivity.assertAutoFilled();

        // Verify fill selection
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
        assertFillEventForDatasetShown(events.get(0), "clientStateKey", "clientStateValue");
        assertFillEventForDatasetAuthenticationSelected(events.get(1), "name",
                "clientStateKey", "clientStateValue");
    }

    @Test
    public void testAuthenticationSelected() throws Exception {
        enableService();

        // Set up FillResponse with response wide authentication
        Bundle clientState = new Bundle();
        clientState.putCharSequence("clientStateKey", "clientStateValue");

        // Prepare the authenticated response
        final IntentSender authentication = AuthenticationActivity.createSender(mContext, 1,
                new CannedFillResponse.Builder().addDataset(
                        new CannedDataset.Builder()
                                .setField(ID_USERNAME, "username")
                                .setId("name")
                                .setPresentation("dataset", isInlineMode())
                                .build())
                        .setExtras(clientState).build());

        sReplier.addResponse(new CannedFillResponse.Builder().setExtras(clientState)
                .setPresentation("authentication", isInlineMode())
                .setAuthentication(authentication, ID_USERNAME)
                .build());

        // Trigger autofill and IME.
        mUiBot.focusByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();

        // Authenticate
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("authentication");
        mUiBot.waitForIdle();
        mUiBot.selectDataset("dataset");
        mUiBot.waitForIdle();

        // Verify fill selection
        final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(4);
        assertDeprecatedClientState(selection, "clientStateKey", "clientStateValue");
        List<Event> events = selection.getEvents();
        assertFillEventForDatasetShown(events.get(0), "clientStateKey", "clientStateValue");
        assertFillEventForAuthenticationSelected(events.get(1), NULL_DATASET_ID,
                "clientStateKey", "clientStateValue");
        assertFillEventForDatasetShown(events.get(2), "clientStateKey", "clientStateValue");
        assertFillEventForDatasetSelected(events.get(3), "name",
                "clientStateKey", "clientStateValue");
    }

    @Test
    public void testDatasetSelected_twoResponses() throws Exception {
        enableService();

        // Set up first partition with an anonymous dataset
        Bundle clientState1 = new Bundle();
        clientState1.putCharSequence("clientStateKey", "Value1");

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username")
                        .setPresentation("dataset1", isInlineMode())
                        .build())
                .setExtras(clientState1)
                .build());
        mActivity.expectAutoFill("username");

        // Trigger autofill and IME.
        mUiBot.focusByRelativeId(ID_USERNAME);
        waitUntilConnected();
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset1");
        mUiBot.waitForIdle();
        mActivity.assertAutoFilled();

        {
            // Verify fill selection
            final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(2);
            assertDeprecatedClientState(selection, "clientStateKey", "Value1");
            final List<Event> events = selection.getEvents();
            assertFillEventForDatasetShown(events.get(0), "clientStateKey", "Value1");
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID,
                    "clientStateKey", "Value1");
        }

        // Set up second partition with a named dataset
        Bundle clientState2 = new Bundle();
        clientState2.putCharSequence("clientStateKey", "Value2");

        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(
                        new CannedDataset.Builder()
                                .setField(ID_PASSWORD, "password2")
                                .setPresentation("dataset2", isInlineMode())
                                .setId("name2")
                                .build())
                .addDataset(
                        new CannedDataset.Builder()
                                .setField(ID_PASSWORD, "password3")
                                .setPresentation("dataset3", isInlineMode())
                                .setId("name3")
                                .build())
                .setExtras(clientState2)
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_PASSWORD).build());
        mActivity.expectPasswordAutoFill("password3");

        // Trigger autofill on password
        mActivity.onPassword(View::requestFocus);
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset3");
        mUiBot.waitForIdle();
        mActivity.assertAutoFilled();

        {
            // Verify fill selection
            final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(2);
            assertDeprecatedClientState(selection, "clientStateKey", "Value2");
            final List<Event> events = selection.getEvents();
            assertFillEventForDatasetShown(events.get(0), "clientStateKey", "Value2");
            assertFillEventForDatasetSelected(events.get(1), "name3",
                    "clientStateKey", "Value2");
        }

        mActivity.onPassword((v) -> v.setText("new password"));
        mActivity.syncRunOnUiThread(() -> mActivity.finish());
        waitUntilDisconnected();

        {
            // Verify fill selection
            final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(4);
            assertDeprecatedClientState(selection, "clientStateKey", "Value2");

            final List<Event> events = selection.getEvents();
            assertFillEventForDatasetShown(events.get(0), "clientStateKey", "Value2");
            assertFillEventForDatasetSelected(events.get(1), "name3",
                    "clientStateKey", "Value2");
            assertFillEventForDatasetShown(events.get(2), "clientStateKey", "Value2");
            assertFillEventForSaveShown(events.get(3), NULL_DATASET_ID,
                    "clientStateKey", "Value2");
        }
    }

    @Test
    public void testNoEvents_whenServiceReturnsNullResponse() throws Exception {
        enableService();

        // First reset
        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username")
                        .setPresentation("dataset1", isInlineMode())
                        .build())
                .build());
        mActivity.expectAutoFill("username");

        // Trigger autofill and IME.
        mUiBot.focusByRelativeId(ID_USERNAME);
        waitUntilConnected();
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset1");
        mUiBot.waitForIdleSync();
        mActivity.assertAutoFilled();

        {
            // Verify fill selection
            final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(2);
            assertNoDeprecatedClientState(selection);
            final List<Event> events = selection.getEvents();
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);
        }

        // Second request
        sReplier.addResponse(NO_RESPONSE);
        mActivity.onPassword(View::requestFocus);
        mUiBot.waitForIdleSync();
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasets();
        waitUntilDisconnected();

        InstrumentedAutoFillService.assertNoFillEventHistory();
    }

    @Test
    public void testNoEvents_whenServiceReturnsFailure() throws Exception {
        enableService();

        // First reset
        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username")
                        .setPresentation("dataset1", isInlineMode())
                        .build())
                .build());
        mActivity.expectAutoFill("username");

        // Trigger autofill and IME.
        mUiBot.focusByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        waitUntilConnected();
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset1");
        mUiBot.waitForIdleSync();
        mActivity.assertAutoFilled();

        {
            // Verify fill selection
            final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(2);
            assertNoDeprecatedClientState(selection);
            final List<Event> events = selection.getEvents();
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);
        }

        // Second request
        sReplier.addResponse(new CannedFillResponse.Builder().returnFailure("D'OH!").build());
        mActivity.onPassword(View::requestFocus);
        mUiBot.waitForIdleSync();
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasets();
        waitUntilDisconnected();

        InstrumentedAutoFillService.assertNoFillEventHistory();
    }

    @Test
    public void testNoEvents_whenServiceTimesout() throws Exception {
        enableService();

        // First reset
        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username")
                        .setPresentation("dataset1", isInlineMode())
                        .build())
                .build());
        mActivity.expectAutoFill("username");

        // Trigger autofill and IME.
        mUiBot.focusByRelativeId(ID_USERNAME);
        waitUntilConnected();
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset1");
        mActivity.assertAutoFilled();

        {
            // Verify fill selection
            final FillEventHistory selection = InstrumentedAutoFillService.getFillEventHistory(2);
            assertNoDeprecatedClientState(selection);
            final List<Event> events = selection.getEvents();
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);
        }

        // Second request
        sReplier.addResponse(DO_NOT_REPLY_RESPONSE);
        mActivity.onPassword(View::requestFocus);
        sReplier.getNextFillRequest();
        waitUntilDisconnected();

        InstrumentedAutoFillService.assertNoFillEventHistory();
    }

    /**
     * Tests the following scenario:
     *
     * <ol>
     *    <li>Activity A is launched.
     *    <li>Activity A triggers autofill.
     *    <li>Activity B is launched.
     *    <li>Activity B triggers autofill.
     *    <li>User goes back to Activity A.
     *    <li>Activity A triggers autofill.
     *    <li>User triggers save on Activity A - at this point, service should have stats of
     *        activity A.
     * </ol>
     */
    @Test
    public void testEventsFromPreviousSessionIsDiscarded() throws Exception {
        enableService();

        // Launch activity A
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setExtras(getBundle("activity", "A"))
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_USERNAME, ID_PASSWORD)
                .build());

        // Trigger autofill and IME on activity A.
        mUiBot.focusByRelativeId(ID_USERNAME);
        waitUntilConnected();
        sReplier.getNextFillRequest();

        // Verify fill selection for Activity A
        final FillEventHistory selectionA = InstrumentedAutoFillService.getFillEventHistory(0);
        assertDeprecatedClientState(selectionA, "activity", "A");

        // Launch activity B
        mActivity.startActivity(new Intent(mActivity, CheckoutActivity.class));
        mUiBot.assertShownByRelativeId(ID_CC_NUMBER);

        // Trigger autofill on activity B
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setExtras(getBundle("activity", "B"))
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_CC_NUMBER, "4815162342")
                        .setPresentation("datasetB", isInlineMode())
                        .build())
                .build());
        mUiBot.focusByRelativeId(ID_CC_NUMBER);
        sReplier.getNextFillRequest();

        // Verify fill selection for Activity B
        final FillEventHistory selectionB = InstrumentedAutoFillService.getFillEventHistory(1);
        assertDeprecatedClientState(selectionB, "activity", "B");
        assertFillEventForDatasetShown(selectionB.getEvents().get(0), "activity", "B");

        // Set response for back to activity A
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setExtras(getBundle("activity", "A"))
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_USERNAME, ID_PASSWORD)
                .build());

        // Now switch back to A...
        mUiBot.pressBack(); // dismiss autofill
        mUiBot.pressBack(); // dismiss keyboard (or task, if there was no keyboard)
        final AtomicBoolean focusOnA = new AtomicBoolean();
        mActivity.syncRunOnUiThread(() -> focusOnA.set(mActivity.hasWindowFocus()));
        if (!focusOnA.get()) {
            mUiBot.pressBack(); // dismiss task, if the last pressBack dismissed only the keyboard
        }
        mUiBot.assertShownByRelativeId(ID_USERNAME);
        assertWithMessage("root window has no focus")
                .that(mActivity.getWindow().getDecorView().hasWindowFocus()).isTrue();

        sReplier.getNextFillRequest();

        // ...and trigger save
        // Set credentials...
        mActivity.onUsername((v) -> v.setText("malkovich"));
        mActivity.onPassword((v) -> v.setText("malkovich"));
        final String expectedMessage = getWelcomeMessage("malkovich");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_PASSWORD);
        sReplier.getNextSaveRequest();

        // Finally, make sure history is right
        final FillEventHistory finalSelection = InstrumentedAutoFillService.getFillEventHistory(1);
        assertDeprecatedClientState(finalSelection, "activity", "A");
        assertFillEventForSaveShown(finalSelection.getEvents().get(0), NULL_DATASET_ID, "activity",
                "A");
    }

    @Test
    public void testContextCommitted_withoutFlagOnLastResponse() throws Exception {
        enableService();
        // Trigger 1st autofill request
        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, BACKDOOR_USERNAME)
                        .setPresentation("dataset1", isInlineMode())
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill(BACKDOOR_USERNAME);
        // Trigger autofill and IME on username.
        mUiBot.focusByRelativeId(ID_USERNAME);
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset1");
        mActivity.assertAutoFilled();
        // Verify fill history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
        }

        // Trigger 2nd autofill request (which will clear the fill event history)
        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_PASSWORD, "whatever")
                        .setPresentation("dataset2", isInlineMode())
                        .build())
                // don't set flags
                .build());
        mActivity.expectPasswordAutoFill("whatever");
        mActivity.onPassword(View::requestFocus);
        sReplier.getNextFillRequest();
        mUiBot.selectDataset("dataset2");
        mActivity.assertAutoFilled();
        // Verify fill history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id2");
        }

        // Finish the context by login in
        final String expectedMessage = getWelcomeMessage(BACKDOOR_USERNAME);
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id2");
        }
    }
}
