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

import static android.autofillservice.cts.CannedFillResponse.NO_MOAR_RESPONSES;
import static android.autofillservice.cts.CannedFillResponse.NO_RESPONSE;
import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.assertTextIsSanitized;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.InstrumentedAutoFillService.waitUntilConnected;
import static android.autofillservice.cts.InstrumentedAutoFillService.waitUntilDisconnected;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.autofillservice.cts.inline.InlineLoginActivityTest;
import android.service.autofill.FillContext;
import android.view.View;

import org.junit.Test;

/**
 * This is the common test cases with {@link LoginActivityTest} and {@link InlineLoginActivityTest}.
 */
public abstract class LoginActivityCommonTestCase extends AbstractLoginActivityTestCase {

    protected LoginActivityCommonTestCase() {}

    protected LoginActivityCommonTestCase(UiBot inlineUiBot) {
        super(inlineUiBot);
    }

    @Test
    public void testAutoFillNoDatasets() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(NO_RESPONSE);

        // Trigger autofill.
        mUiBot.selectByRelativeId(ID_USERNAME);

        // Make sure a fill request is called but don't check for connected() - as we're returning
        // a null response, the service might have been disconnected already by the time we assert
        // it.
        sReplier.getNextFillRequest();

        // Make sure UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Test connection lifecycle.
        waitUntilDisconnected();
    }

    @Test
    public void testAutoFillNoDatasets_multipleFields_alwaysNull() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(NO_RESPONSE)
                .addResponse(NO_MOAR_RESPONSES);

        // Trigger autofill
        mUiBot.selectByRelativeId(ID_USERNAME);
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasetsEver();

        // Tap back and forth to make sure no more requests are shown

        mActivity.onPassword(View::requestFocus);
        mUiBot.assertNoDatasetsEver();

        mActivity.onUsername(View::requestFocus);
        mUiBot.assertNoDatasetsEver();

        mActivity.onPassword(View::requestFocus);
        mUiBot.assertNoDatasetsEver();
    }


    @Test
    public void testAutofill_oneDataset() throws Exception {
        testBasicLoginAutofill(/* numDatasets= */ 1, /* selectedDatasetIndex= */ 0);
    }

    @Test
    public void testAutofill_twoDatasets_selectFirstDataset() throws Exception {
        testBasicLoginAutofill(/* numDatasets= */ 2, /* selectedDatasetIndex= */ 0);

    }

    @Test
    public void testAutofill_twoDatasets_selectSecondDataset() throws Exception {
        testBasicLoginAutofill(/* numDatasets= */ 2, /* selectedDatasetIndex= */ 1);
    }

    private void testBasicLoginAutofill(int numDatasets, int selectedDatasetIndex)
            throws Exception {
        // Set service.
        enableService();

        final MyAutofillCallback callback = mActivity.registerCallback();
        final View username = mActivity.getUsername();
        final View password = mActivity.getPassword();

        String[] expectedDatasets = new String[numDatasets];
        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder();
        for (int i = 0; i < numDatasets; i++) {
            builder.addDataset(new CannedFillResponse.CannedDataset.Builder()
                    .setField(ID_USERNAME, "dude" + i)
                    .setField(ID_PASSWORD, "sweet" + i)
                    .setPresentation("The Dude" + i, isInlineMode())
                    .build());
            expectedDatasets[i] = "The Dude" + i;
        }

        sReplier.addResponse(builder.build());
        mActivity.expectAutoFill("dude" + selectedDatasetIndex, "sweet" + selectedDatasetIndex);

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();

        mUiBot.assertDatasets(expectedDatasets);
        callback.assertUiShownEvent(username);

        mUiBot.selectDataset(expectedDatasets[selectedDatasetIndex]);

        // Check the results.
        mActivity.assertAutoFilled();
        callback.assertUiHiddenEvent(username);

        // Make sure input was sanitized.
        final InstrumentedAutoFillService.FillRequest request = sReplier.getNextFillRequest();
        assertWithMessage("CancelationSignal is null").that(request.cancellationSignal).isNotNull();
        assertTextIsSanitized(request.structure, ID_PASSWORD);
        final FillContext fillContext = request.contexts.get(request.contexts.size() - 1);
        assertThat(fillContext.getFocusedId())
                .isEqualTo(findAutofillIdByResourceId(fillContext, ID_USERNAME));
        if (isInlineMode()) {
            assertThat(request.inlineRequest).isNotNull();
        } else {
            assertThat(request.inlineRequest).isNull();
        }

        // Make sure initial focus was properly set.
        assertWithMessage("Username node is not focused").that(
                findNodeByResourceId(request.structure, ID_USERNAME).isFocused()).isTrue();
        assertWithMessage("Password node is focused").that(
                findNodeByResourceId(request.structure, ID_PASSWORD).isFocused()).isFalse();
    }

    @Test
    public void testClearFocusBeforeRespond() throws Exception {
        // Set service
        enableService();

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        waitUntilConnected();

        // Clear focus before responded
        mActivity.onUsername(View::clearFocus);
        mUiBot.waitForIdleSync();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation("The Dude", isInlineMode())
                        .build());
        sReplier.addResponse(builder.build());
        sReplier.getNextFillRequest();

        // Confirm no datasets shown
        mUiBot.assertNoDatasetsEver();
    }

    @Test
    public void testSwitchFocusBeforeResponse() throws Exception {
        // Set service
        enableService();

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        waitUntilConnected();

        // Trigger second fill request
        mUiBot.selectByRelativeId(ID_PASSWORD);
        mUiBot.waitForIdleSync();

        // Respond for username
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation("The Dude", isInlineMode())
                        .build())
                .build());
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();

        // Set expectations and respond for password
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation("The Password", isInlineMode())
                        .build())
                .build());
        sReplier.getNextFillRequest();

        // confirm second response shown
        mUiBot.assertDatasets("The Password");
    }

    @Test
    public void testManualRequestWhileFirstResponseDelayed() throws Exception {
        // Set service
        enableService();

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        waitUntilConnected();

        // Trigger second fill request
        mActivity.forceAutofillOnUsername();
        mUiBot.waitForIdleSync();

        // Respond for first request
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation("The Dude", isInlineMode())
                        .build())
                .build());
        sReplier.getNextFillRequest();

        // Set expectations and respond for second request
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude2")
                        .setPresentation("The Dude 2", isInlineMode())
                        .build()).build());
        sReplier.getNextFillRequest();

        // confirm second response shown
        mUiBot.assertDatasets("The Dude 2");
    }

    @Test
    public void testResponseFirstAfterResponseSecond() throws Exception {
        // Set service
        enableService();

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        waitUntilConnected();

        // Trigger second fill request
        mActivity.forceAutofillOnUsername();
        mUiBot.waitForIdleSync();

        // Respond for first request
        sReplier.addResponse(new CannedFillResponse.Builder(CannedFillResponse.ResponseType.DELAY)
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation("The Dude", isInlineMode())
                        .build())
                .build());
        sReplier.getNextFillRequest();

        // Set expectations and respond for second request
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude2")
                        .setPresentation("The Dude 2", isInlineMode())
                        .build()).build());
        sReplier.getNextFillRequest();

        // confirm second response shown
        mUiBot.assertDatasets("The Dude 2");

        // Wait first response was sent
        sReplier.getNextFillRequest();

        // confirm second response still shown
        mUiBot.assertDatasets("The Dude 2");
    }
}
