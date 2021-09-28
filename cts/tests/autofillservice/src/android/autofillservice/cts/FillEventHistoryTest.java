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
 * limitations under the License.
 */

package android.autofillservice.cts;

import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.NULL_DATASET_ID;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetSelected;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetShown;
import static android.autofillservice.cts.Helper.assertFillEventForSaveShown;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.autofillservice.cts.LoginActivity.BACKDOOR_USERNAME;
import static android.autofillservice.cts.LoginActivity.getWelcomeMessage;
import static android.service.autofill.FillEventHistory.Event.TYPE_CONTEXT_COMMITTED;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.FillContext;
import android.service.autofill.FillEventHistory;
import android.service.autofill.FillEventHistory.Event;
import android.service.autofill.FillResponse;
import android.support.test.uiautomator.UiObject2;
import android.view.View;
import android.view.autofill.AutofillId;

import com.google.common.collect.ImmutableMap;

import org.junit.Test;

import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Test that uses {@link LoginActivity} to test {@link FillEventHistory}.
 */
@AppModeFull(reason = "Service-specific test")
public class FillEventHistoryTest extends FillEventHistoryCommonTestCase {

    @Test
    public void testContextCommitted_whenServiceDidntDoAnything() throws Exception {
        enableService();

        sReplier.addResponse(CannedFillResponse.NO_RESPONSE);

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasetsEver();

        // Trigger save
        mActivity.onUsername((v) -> v.setText("malkovich"));
        mActivity.onPassword((v) -> v.setText("malkovich"));
        final String expectedMessage = getWelcomeMessage("malkovich");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // Assert no events where generated
        InstrumentedAutoFillService.assertNoFillEventHistory();
    }

    @Test
    public void textContextCommitted_withoutDatasets() throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_USERNAME, ID_PASSWORD)
                .build());

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasetsEver();

        // Trigger save
        mActivity.onUsername((v) -> v.setText("malkovich"));
        mActivity.onPassword((v) -> v.setText("malkovich"));
        final String expectedMessage = getWelcomeMessage("malkovich");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_PASSWORD);
        sReplier.getNextSaveRequest();

        // Assert it
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForSaveShown(events.get(0), NULL_DATASET_ID);
    }


    @Test
    public void testContextCommitted_idlessDatasets() throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username1")
                        .setField(ID_PASSWORD, "password1")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username2")
                        .setField(ID_PASSWORD, "password2")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill("username1", "password1");

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();

        final UiObject2 datasetPicker = mUiBot.assertDatasets("dataset1", "dataset2");
        mUiBot.selectDataset(datasetPicker, "dataset1");
        mActivity.assertAutoFilled();

        // Verify dataset selection
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);
        }

        // Finish the context by login in
        mActivity.onUsername((v) -> v.setText("USERNAME"));
        mActivity.onPassword((v) -> v.setText("USERNAME"));

        final String expectedMessage = getWelcomeMessage("USERNAME");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // ...and check again
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(3);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);
            assertFillEventForDatasetShown(events.get(2));
        }
    }

    @Test
    public void testContextCommitted_idlessDatasetSelected_datasetWithIdIgnored()
            throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username1")
                        .setField(ID_PASSWORD, "password1")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_USERNAME, "username2")
                        .setField(ID_PASSWORD, "password2")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill("username1", "password1");

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        final FillRequest request = sReplier.getNextFillRequest();

        final UiObject2 datasetPicker = mUiBot.assertDatasets("dataset1", "dataset2");
        mUiBot.selectDataset(datasetPicker, "dataset1");
        mActivity.assertAutoFilled();

        // Verify dataset selection
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);
        }

        // Finish the context by login in
        mActivity.onPassword((v) -> v.setText("username1"));

        final String expectedMessage = getWelcomeMessage("username1");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // ...and check again
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(3);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID);

            FillEventHistory.Event event2 = events.get(2);
            assertThat(event2.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event2.getDatasetId()).isNull();
            assertThat(event2.getClientState()).isNull();
            assertThat(event2.getSelectedDatasetIds()).isEmpty();
            assertThat(event2.getIgnoredDatasetIds()).containsExactly("id2");
            final AutofillId passwordId = findAutofillIdByResourceId(request.contexts.get(0),
                    ID_PASSWORD);
            final Map<AutofillId, String> changedFields = event2.getChangedFields();
            assertThat(changedFields).containsExactly(passwordId, "id2");
            assertThat(event2.getManuallyEnteredField()).isEmpty();
        }
    }

    @Test
    public void testContextCommitted_idlessDatasetIgnored_datasetWithIdSelected()
            throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "username1")
                        .setField(ID_PASSWORD, "password1")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_USERNAME, "username2")
                        .setField(ID_PASSWORD, "password2")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill("username2", "password2");

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        final FillRequest request = sReplier.getNextFillRequest();

        final UiObject2 datasetPicker = mUiBot.assertDatasets("dataset1", "dataset2");
        mUiBot.selectDataset(datasetPicker, "dataset2");
        mActivity.assertAutoFilled();

        // Verify dataset selection
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id2");
        }

        // Finish the context by login in
        mActivity.onPassword((v) -> v.setText("username2"));

        final String expectedMessage = getWelcomeMessage("username2");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // ...and check again
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(3);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id2");

            final FillEventHistory.Event event2 = events.get(2);
            assertThat(event2.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event2.getDatasetId()).isNull();
            assertThat(event2.getClientState()).isNull();
            assertThat(event2.getSelectedDatasetIds()).containsExactly("id2");
            assertThat(event2.getIgnoredDatasetIds()).isEmpty();
            final AutofillId passwordId = findAutofillIdByResourceId(request.contexts.get(0),
                    ID_PASSWORD);
            final Map<AutofillId, String> changedFields = event2.getChangedFields();
            assertThat(changedFields).containsExactly(passwordId, "id2");
            assertThat(event2.getManuallyEnteredField()).isEmpty();
        }
    }

    /**
     * Tests scenario where the context was committed, no dataset was selected by the user,
     * neither the user entered values that were present in these datasets.
     */
    @Test
    public void testContextCommitted_noDatasetSelected_valuesNotManuallyEntered() throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, "username1")
                        .setField(ID_PASSWORD, "password1")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_USERNAME, "username2")
                        .setField(ID_PASSWORD, "password2")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        mUiBot.assertDatasets("dataset1", "dataset2");

        // Verify history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
            assertFillEventForDatasetShown(events.get(0));
        }
        // Enter values not present at the datasets
        mActivity.onUsername((v) -> v.setText("USERNAME"));
        mActivity.onPassword((v) -> v.setText("USERNAME"));

        // Finish the context by login in
        final String expectedMessage = getWelcomeMessage("USERNAME");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // Verify history again
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            final Event event = events.get(1);
            assertThat(event.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event.getDatasetId()).isNull();
            assertThat(event.getClientState()).isNull();
            assertThat(event.getIgnoredDatasetIds()).containsExactly("id1", "id2");
            assertThat(event.getChangedFields()).isEmpty();
            assertThat(event.getManuallyEnteredField()).isEmpty();
        }
    }

    /**
     * Tests scenario where the context was committed, just one dataset was selected by the user,
     * and the user changed the values provided by the service.
     */
    @Test
    public void testContextCommitted_oneDatasetSelected() throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, "username1")
                        .setField(ID_PASSWORD, "password1")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_USERNAME, "username2")
                        .setField(ID_PASSWORD, "password2")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill("username1", "password1");

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        final FillRequest request = sReplier.getNextFillRequest();

        final UiObject2 datasetPicker = mUiBot.assertDatasets("dataset1", "dataset2");
        mUiBot.selectDataset(datasetPicker, "dataset1");
        mActivity.assertAutoFilled();

        // Verify dataset selection
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
        }

        // Finish the context by login in
        mActivity.onUsername((v) -> v.setText("USERNAME"));
        mActivity.onPassword((v) -> v.setText("USERNAME"));

        final String expectedMessage = getWelcomeMessage("USERNAME");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // ...and check again
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(4);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");

            assertFillEventForDatasetShown(events.get(2));
            final FillEventHistory.Event event2 = events.get(3);
            assertThat(event2.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event2.getDatasetId()).isNull();
            assertThat(event2.getClientState()).isNull();
            assertThat(event2.getSelectedDatasetIds()).containsExactly("id1");
            assertThat(event2.getIgnoredDatasetIds()).containsExactly("id2");
            final Map<AutofillId, String> changedFields = event2.getChangedFields();
            final FillContext context = request.contexts.get(0);
            final AutofillId usernameId = findAutofillIdByResourceId(context, ID_USERNAME);
            final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);

            assertThat(changedFields).containsExactlyEntriesIn(
                    ImmutableMap.of(usernameId, "id1", passwordId, "id1"));
            assertThat(event2.getManuallyEnteredField()).isEmpty();
        }
    }

    /**
     * Tests scenario where the context was committed, both datasets were selected by the user,
     * and the user changed the values provided by the service.
     */
    @Test
    public void testContextCommitted_multipleDatasetsSelected() throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, "username")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_PASSWORD, "password")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill("username");

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        final FillRequest request = sReplier.getNextFillRequest();

        // Autofill username
        mUiBot.selectDataset("dataset1");
        mActivity.assertAutoFilled();
        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
        }

        // Autofill password
        mActivity.expectPasswordAutoFill("password");

        mActivity.onPassword(View::requestFocus);
        mUiBot.selectDataset("dataset2");
        mActivity.assertAutoFilled();

        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(4);

            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
            assertFillEventForDatasetShown(events.get(2));
            assertFillEventForDatasetSelected(events.get(3), "id2");
        }

        // Finish the context by login in
        mActivity.onPassword((v) -> v.setText("username"));

        final String expectedMessage = getWelcomeMessage("username");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(6);

            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
            assertFillEventForDatasetShown(events.get(2));
            assertFillEventForDatasetSelected(events.get(3), "id2");

            assertFillEventForDatasetShown(events.get(4));
            final FillEventHistory.Event event3 = events.get(5);
            assertThat(event3.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event3.getDatasetId()).isNull();
            assertThat(event3.getClientState()).isNull();
            assertThat(event3.getSelectedDatasetIds()).containsExactly("id1", "id2");
            assertThat(event3.getIgnoredDatasetIds()).isEmpty();
            final Map<AutofillId, String> changedFields = event3.getChangedFields();
            final AutofillId passwordId = findAutofillIdByResourceId(request.contexts.get(0),
                    ID_PASSWORD);
            assertThat(changedFields).containsExactly(passwordId, "id2");
            assertThat(event3.getManuallyEnteredField()).isEmpty();
        }
    }

    /**
     * Tests scenario where the context was committed, both datasets were selected by the user,
     * and the user didn't change the values provided by the service.
     */
    @Test
    public void testContextCommitted_multipleDatasetsSelected_butNotChanged() throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, BACKDOOR_USERNAME)
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_PASSWORD, "whatever")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill(BACKDOOR_USERNAME);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();

        // Autofill username
        mUiBot.selectDataset("dataset1");
        mActivity.assertAutoFilled();
        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
        }

        // Autofill password
        mActivity.expectPasswordAutoFill("whatever");

        mActivity.onPassword(View::requestFocus);
        mUiBot.selectDataset("dataset2");
        mActivity.assertAutoFilled();

        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(4);

            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
            assertFillEventForDatasetShown(events.get(2));
            assertFillEventForDatasetSelected(events.get(3), "id2");
        }

        // Finish the context by login in
        final String expectedMessage = getWelcomeMessage(BACKDOOR_USERNAME);
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        {
            // Verify fill history
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(5);

            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
            assertFillEventForDatasetShown(events.get(2));
            assertFillEventForDatasetSelected(events.get(3), "id2");

            final FillEventHistory.Event event3 = events.get(4);
            assertThat(event3.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event3.getDatasetId()).isNull();
            assertThat(event3.getClientState()).isNull();
            assertThat(event3.getSelectedDatasetIds()).containsExactly("id1", "id2");
            assertThat(event3.getIgnoredDatasetIds()).isEmpty();
            assertThat(event3.getChangedFields()).isEmpty();
            assertThat(event3.getManuallyEnteredField()).isEmpty();
        }
    }

    /**
     * Tests scenario where the context was committed, the user selected the dataset, than changed
     * the autofilled values, but then change the values again so they match what was provided by
     * the service.
     */
    @Test
    public void testContextCommitted_oneDatasetSelected_Changed_thenChangedBack()
            throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, "username")
                        .setField(ID_PASSWORD, "username")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        mActivity.expectAutoFill("username", "username");

        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();

        mUiBot.selectDataset("dataset1");
        mActivity.assertAutoFilled();

        // Verify dataset selection
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
        }

        // Change the fields to different values from0 datasets
        mActivity.onUsername((v) -> v.setText("USERNAME"));
        mActivity.onPassword((v) -> v.setText("USERNAME"));

        // Then change back to dataset values
        mActivity.onUsername((v) -> v.setText("username"));
        mActivity.onPassword((v) -> v.setText("username"));

        final String expectedMessage = getWelcomeMessage("username");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // ...and check again
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(4);
            assertFillEventForDatasetShown(events.get(0));
            assertFillEventForDatasetSelected(events.get(1), "id1");
            assertFillEventForDatasetShown(events.get(2));

            FillEventHistory.Event event4 = events.get(3);
            assertThat(event4.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event4.getDatasetId()).isNull();
            assertThat(event4.getClientState()).isNull();
            assertThat(event4.getSelectedDatasetIds()).containsExactly("id1");
            assertThat(event4.getIgnoredDatasetIds()).isEmpty();
            assertThat(event4.getChangedFields()).isEmpty();
            assertThat(event4.getManuallyEnteredField()).isEmpty();
        }
    }

    /**
     * Tests scenario where the context was committed, the user did not selected any dataset, but
     * the user manually entered values that match what was provided by the service.
     */
    @Test
    public void testContextCommitted_noDatasetSelected_butManuallyEntered()
            throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, BACKDOOR_USERNAME)
                        .setField(ID_PASSWORD, "NotUsedPassword")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_USERNAME, "NotUserUsername")
                        .setField(ID_PASSWORD, "whatever")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        final FillRequest request = sReplier.getNextFillRequest();
        mUiBot.assertDatasets("dataset1", "dataset2");

        // Verify history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
            assertFillEventForDatasetShown(events.get(0));
        }

        // Enter values present at the datasets
        mActivity.onUsername((v) -> v.setText(BACKDOOR_USERNAME));
        mActivity.onPassword((v) -> v.setText("whatever"));

        // Finish the context by login in
        final String expectedMessage = getWelcomeMessage(BACKDOOR_USERNAME);
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // Verify history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));
            FillEventHistory.Event event = events.get(1);
            assertThat(event.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event.getDatasetId()).isNull();
            assertThat(event.getClientState()).isNull();
            assertThat(event.getSelectedDatasetIds()).isEmpty();
            assertThat(event.getIgnoredDatasetIds()).containsExactly("id1", "id2");
            assertThat(event.getChangedFields()).isEmpty();
            final FillContext context = request.contexts.get(0);
            final AutofillId usernameId = findAutofillIdByResourceId(context, ID_USERNAME);
            final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);

            final Map<AutofillId, Set<String>> manuallyEnteredFields =
                    event.getManuallyEnteredField();
            assertThat(manuallyEnteredFields).isNotNull();
            assertThat(manuallyEnteredFields.size()).isEqualTo(2);
            assertThat(manuallyEnteredFields.get(usernameId)).containsExactly("id1");
            assertThat(manuallyEnteredFields.get(passwordId)).containsExactly("id2");
        }
    }

    /**
     * Tests scenario where the context was committed, the user did not selected any dataset, but
     * the user manually entered values that match what was provided by the service on different
     * datasets.
     */
    @Test
    public void testContextCommitted_noDatasetSelected_butManuallyEntered_matchingMultipleDatasets()
            throws Exception {
        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder().addDataset(
                new CannedDataset.Builder()
                        .setId("id1")
                        .setField(ID_USERNAME, BACKDOOR_USERNAME)
                        .setField(ID_PASSWORD, "NotUsedPassword")
                        .setPresentation(createPresentation("dataset1"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id2")
                        .setField(ID_USERNAME, "NotUserUsername")
                        .setField(ID_PASSWORD, "whatever")
                        .setPresentation(createPresentation("dataset2"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("id3")
                        .setField(ID_USERNAME, BACKDOOR_USERNAME)
                        .setField(ID_PASSWORD, "whatever")
                        .setPresentation(createPresentation("dataset3"))
                        .build())
                .setFillResponseFlags(FillResponse.FLAG_TRACK_CONTEXT_COMMITED)
                .build());
        // Trigger autofill on username
        mActivity.onUsername(View::requestFocus);
        final FillRequest request = sReplier.getNextFillRequest();
        mUiBot.assertDatasets("dataset1", "dataset2", "dataset3");

        // Verify history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
            assertFillEventForDatasetShown(events.get(0));
        }

        // Enter values present at the datasets
        mActivity.onUsername((v) -> v.setText(BACKDOOR_USERNAME));
        mActivity.onPassword((v) -> v.setText("whatever"));

        // Finish the context by login in
        final String expectedMessage = getWelcomeMessage(BACKDOOR_USERNAME);
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);

        // Verify history
        {
            final List<Event> events = InstrumentedAutoFillService.getFillEvents(2);
            assertFillEventForDatasetShown(events.get(0));

            final FillEventHistory.Event event = events.get(1);
            assertThat(event.getType()).isEqualTo(TYPE_CONTEXT_COMMITTED);
            assertThat(event.getDatasetId()).isNull();
            assertThat(event.getClientState()).isNull();
            assertThat(event.getSelectedDatasetIds()).isEmpty();
            assertThat(event.getIgnoredDatasetIds()).containsExactly("id1", "id2", "id3");
            assertThat(event.getChangedFields()).isEmpty();
            final FillContext context = request.contexts.get(0);
            final AutofillId usernameId = findAutofillIdByResourceId(context, ID_USERNAME);
            final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);

            final Map<AutofillId, Set<String>> manuallyEnteredFields =
                    event.getManuallyEnteredField();
            assertThat(manuallyEnteredFields.size()).isEqualTo(2);
            assertThat(manuallyEnteredFields.get(usernameId)).containsExactly("id1", "id3");
            assertThat(manuallyEnteredFields.get(passwordId)).containsExactly("id2", "id3");
        }
    }
}
