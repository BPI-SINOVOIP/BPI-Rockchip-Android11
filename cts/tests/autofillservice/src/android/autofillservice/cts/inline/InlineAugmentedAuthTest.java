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

import static android.app.Activity.RESULT_CANCELED;
import static android.app.Activity.RESULT_OK;
import static android.autofillservice.cts.CannedFillResponse.NO_RESPONSE;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.augmented.AugmentedHelper.assertBasicRequestInfo;

import static com.google.common.truth.Truth.assertThat;

import android.autofillservice.cts.AutofillActivityTestRule;
import android.autofillservice.cts.augmented.AugmentedAuthActivity;
import android.autofillservice.cts.augmented.AugmentedAutofillAutoActivityLaunchTestCase;
import android.autofillservice.cts.augmented.AugmentedLoginActivity;
import android.autofillservice.cts.augmented.CannedAugmentedFillResponse;
import android.autofillservice.cts.augmented.CtsAugmentedAutofillService;
import android.content.IntentSender;
import android.service.autofill.Dataset;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.EditText;

import org.junit.Test;

public class InlineAugmentedAuthTest
        extends AugmentedAutofillAutoActivityLaunchTestCase<AugmentedLoginActivity> {

    protected AugmentedLoginActivity mActivity;

    public InlineAugmentedAuthTest() {
        super(getInlineUiBot());
    }

    @Override
    protected AutofillActivityTestRule<AugmentedLoginActivity> getActivityRule() {
        return new AutofillActivityTestRule<AugmentedLoginActivity>(AugmentedLoginActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Test
    public void testDatasetAuth_resultOk_validDataset() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText unField = mActivity.getUsername();
        final AutofillId unFieldId = unField.getAutofillId();
        final AutofillValue unValue = unField.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        Dataset authResult = new Dataset.Builder(createInlinePresentation("auth"))
                .setId("dummyId")
                .setValue(unFieldId, AutofillValue.forText("Auth Result"))
                .build();
        IntentSender authAction = AugmentedAuthActivity.createSender(mContext, 1,
                authResult, null, RESULT_OK);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("bla").build(),
                        unFieldId)
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("inline")
                        .setField(unFieldId, "John Smith", createInlinePresentation("John"))
                        .setAuthentication(authAction)
                        .build())
                .build());

        // Trigger autofill request
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        CtsAugmentedAutofillService.AugmentedFillRequest request1 =
                sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, unFieldId, unValue);

        // Confirm suggestions
        mUiBot.assertDatasets("John");

        // Tap on suggestion
        mUiBot.selectDataset("John");
        mUiBot.waitForIdle();

        // Tap on the auth activity button and assert that the dataset from the auth activity is
        // filled into the field.
        mActivity.expectAutoFill("Auth Result");
        mUiBot.selectByRelativeId(AugmentedAuthActivity.ID_AUTH_ACTIVITY_BUTTON);
        mUiBot.waitForIdle();
        mActivity.assertAutoFilled();
        assertThat(unField.getText().toString()).isEqualTo("Auth Result");
        mUiBot.assertNoDatasets();
    }

    @Test
    public void testDatasetAuth_resultOk_nullDataset() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText unField = mActivity.getUsername();
        final AutofillId unFieldId = unField.getAutofillId();
        final AutofillValue unValue = unField.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        IntentSender authAction = AugmentedAuthActivity.createSender(mContext, 1,
                null, null, RESULT_OK);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("bla").build(),
                        unFieldId)
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("inline")
                        .setField(unFieldId, "John Smith", createInlinePresentation("John"))
                        .setAuthentication(authAction)
                        .build())
                .build());

        // Trigger autofill request
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        CtsAugmentedAutofillService.AugmentedFillRequest request1 =
                sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, unFieldId, unValue);

        // Confirm suggestions
        mUiBot.assertDatasets("John");

        // Tap on suggestion
        mUiBot.selectDataset("John");
        mUiBot.waitForIdle();

        // Tap on the auth activity button and assert that the field is left unchanged (since the
        // dataset returned from the auth activity is null).
        mUiBot.selectByRelativeId(AugmentedAuthActivity.ID_AUTH_ACTIVITY_BUTTON);
        mUiBot.waitForIdle();
        assertThat(unField.getText().toString()).isEqualTo("");
    }

    @Test
    public void testDatasetAuth_resultCancel() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText unField = mActivity.getUsername();
        final AutofillId unFieldId = unField.getAutofillId();
        final AutofillValue unValue = unField.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        Dataset authResult = new Dataset.Builder(createInlinePresentation("auth"))
                .setId("dummyId")
                .setValue(unFieldId, AutofillValue.forText("Auth Result"))
                .build();
        IntentSender authAction = AugmentedAuthActivity.createSender(mContext, 1,
                authResult, null, RESULT_CANCELED);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("bla").build(),
                        unFieldId)
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("inline")
                        .setField(unFieldId, "John Smith", createInlinePresentation("John"))
                        .setAuthentication(authAction)
                        .build())
                .build());

        // Trigger autofill request
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();
        CtsAugmentedAutofillService.AugmentedFillRequest request1 =
                sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, unFieldId, unValue);

        // Confirm suggestions
        mUiBot.assertDatasets("John");

        // Tap on suggestion
        mUiBot.selectDataset("John");
        mUiBot.waitForIdle();

        // Tap on the auth activity button and assert that the field is left unchanged (since the
        // result code returned by the auth activity is RESULT_CANCELED).
        mUiBot.selectByRelativeId(AugmentedAuthActivity.ID_AUTH_ACTIVITY_BUTTON);
        mUiBot.waitForIdle();
        assertThat(unField.getText().toString()).isEqualTo("");
    }
}
