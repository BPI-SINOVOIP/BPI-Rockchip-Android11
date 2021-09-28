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

package android.autofillservice.cts.inline;

import static android.autofillservice.cts.CannedFillResponse.NO_RESPONSE;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.NULL_DATASET_ID;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetSelected;
import static android.autofillservice.cts.Helper.assertFillEventForDatasetShown;
import static android.autofillservice.cts.augmented.AugmentedHelper.assertBasicRequestInfo;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.CLIENT_STATE_KEY;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.CLIENT_STATE_VALUE;

import static com.google.common.truth.Truth.assertThat;

import android.autofillservice.cts.AutofillActivityTestRule;
import android.autofillservice.cts.Helper;
import android.autofillservice.cts.MyAutofillCallback;
import android.autofillservice.cts.augmented.AugmentedAutofillAutoActivityLaunchTestCase;
import android.autofillservice.cts.augmented.AugmentedLoginActivity;
import android.autofillservice.cts.augmented.CannedAugmentedFillResponse;
import android.autofillservice.cts.augmented.CtsAugmentedAutofillService;
import android.autofillservice.cts.augmented.CtsAugmentedAutofillService.AugmentedFillRequest;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.FillEventHistory;
import android.service.autofill.FillEventHistory.Event;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.EditText;

import org.junit.Test;
import org.junit.rules.TestRule;

import java.util.List;

public class InlineAugmentedLoginActivityTest
        extends AugmentedAutofillAutoActivityLaunchTestCase<AugmentedLoginActivity> {

    protected AugmentedLoginActivity mActivity;

    public InlineAugmentedLoginActivityTest() {
        super(getInlineUiBot());
    }

    @Override
    protected AutofillActivityTestRule<AugmentedLoginActivity> getActivityRule() {
        return new AutofillActivityTestRule<AugmentedLoginActivity>(
                AugmentedLoginActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Override
    public TestRule getMainTestRule() {
        return InlineUiBot.annotateRule(super.getMainTestRule());
    }

    @Test
    public void testAugmentedAutoFill_oneDatasetThenFilled() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        testBasicLoginAutofill();
    }

    @Test
    public void testAugmentedAutofillFillHistory_oneDatasetThenFilled() throws Exception {
        // Set services
        enableService();
        final CtsAugmentedAutofillService augmentedService = enableAugmentedService();

        testBasicLoginAutofill();

        // Verify events history
        final FillEventHistory history = augmentedService.getFillEventHistory(2);
        assertThat(history).isNotNull();
        final List<Event> events = history.getEvents();
        assertFillEventForDatasetShown(events.get(0), CLIENT_STATE_KEY, CLIENT_STATE_VALUE);
        assertFillEventForDatasetSelected(events.get(1), NULL_DATASET_ID, CLIENT_STATE_KEY,
                CLIENT_STATE_VALUE);
    }

    @Test
    public void testAugmentedAutoFill_noFiltering() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final EditText password = mActivity.getPassword();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillId passwordId = password.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .setField(passwordId, "sweet", createInlinePresentation("sweet"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        sReplier.getNextFillRequest();
        sAugmentedReplier.getNextFillRequest();

        // Assert suggestion
        mUiBot.assertDatasets("dude");

        // Suggestion was not shown.
        mActivity.onUsername((v) -> v.setText("d"));
        mUiBot.assertNoDatasetsEver();
    }

    @Test
    public void testAugmentedAutoFill_twoDatasetThenFilledSecond() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final EditText password = mActivity.getPassword();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillId passwordId = password.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .setField(passwordId, "sweet", createInlinePresentation("sweet"))
                        .build())
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me2")
                        .setField(usernameId, "DUDE", createInlinePresentation("DUDE"))
                        .setField(passwordId, "SWEET", createInlinePresentation("SWEET"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, usernameId, usernameValue);

        // Confirm one suggestion
        mUiBot.assertDatasets("dude", "DUDE");

        mActivity.expectAutoFill("DUDE", "SWEET");

        // Select suggestion
        mUiBot.selectDataset("DUDE");
        mUiBot.waitForIdle();

        mActivity.assertAutoFilled();
    }

    private void testBasicLoginAutofill() throws Exception {

        final MyAutofillCallback callback = mActivity.registerCallback();
        // Set expectations
        final EditText username = mActivity.getUsername();
        final EditText password = mActivity.getPassword();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillId passwordId = password.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .setField(passwordId, "sweet", createInlinePresentation("sweet"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, usernameId, usernameValue);

        // Confirm two suggestion
        mUiBot.assertDatasets("dude");
        callback.assertUiShownEvent(username);

        mActivity.expectAutoFill("dude", "sweet");

        // Select suggestion
        mUiBot.selectDataset("dude");
        mUiBot.waitForIdle();

        mActivity.assertAutoFilled();
        mUiBot.assertNoDatasetsEver();
        callback.assertUiHiddenEvent(username);
    }

    @Test
    public void testAugmentedAutoFill_selectDatasetThenHideInlineSuggestion() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final EditText password = mActivity.getPassword();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillId passwordId = password.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .setField(passwordId, "sweet", createInlinePresentation("sweet"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        mUiBot.assertDatasets("dude");

        mActivity.expectAutoFill("dude", "sweet");

        mUiBot.selectDataset("dude");
        mUiBot.waitForIdle();

        mUiBot.assertNoDatasets();
    }

    @Test
    public void testAugmentedAutoFill_startTypingThenHideInlineSuggestion() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        sAugmentedReplier.getNextFillRequest();

        mUiBot.assertDatasets("dude");

        // Now pretend user typing something by updating the value in the input field.
        mActivity.onUsername((v) -> v.setText("d"));
        mUiBot.waitForIdle();

        // Expect the inline suggestion to disappear.
        mUiBot.assertNoDatasets();
    }

    @Test
    @AppModeFull(reason = "WRITE_SECURE_SETTING permission can't be grant to instant apps")
    public void testSwitchInputMethod() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final AutofillId usernameId = mActivity.getUsername().getAutofillId();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        sAugmentedReplier.getNextFillRequest();

        // Confirm suggestion
        mUiBot.assertDatasets("dude");

        // Trigger IME switch event
        Helper.mockSwitchInputMethod(sContext);
        mUiBot.waitForIdleSync();

        // Set new expectations
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 2")
                        .setField(usernameId, "dude2", createInlinePresentation("dude2"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req2")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        sAugmentedReplier.getNextFillRequest();

        // Confirm new suggestion
        mUiBot.assertDatasets("dude2");
    }

    @Test
    @AppModeFull(reason = "WRITE_SECURE_SETTING permission can't be grant to instant apps")
    public void testSwitchInputMethod_mainServiceDisabled() throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        enableAugmentedService();

        // Set expectations
        final AutofillId usernameId = mActivity.getUsername().getAutofillId();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sAugmentedReplier.getNextFillRequest();

        // Confirm suggestion
        mUiBot.assertDatasets("dude");

        // Trigger IME switch event
        Helper.mockSwitchInputMethod(sContext);
        mUiBot.waitForIdleSync();

        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 2")
                        .setField(usernameId, "dude2", createInlinePresentation("dude2"))
                        .build())
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req2")
                        .build(), usernameId)
                .build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();

        // Confirm new fill request
        sAugmentedReplier.getNextFillRequest();

        // Confirm new suggestion
        mUiBot.assertDatasets("dude2");
    }
}
