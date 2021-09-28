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

package android.autofillservice.cts;

import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_PASSWORD_LABEL;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.ID_USERNAME_LABEL;
import static android.autofillservice.cts.Helper.assertHintFromResources;
import static android.autofillservice.cts.Helper.assertTextAndValue;
import static android.autofillservice.cts.Helper.assertTextFromResources;
import static android.autofillservice.cts.Helper.assertTextIsSanitized;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.InstrumentedAutoFillService.waitUntilConnected;
import static android.autofillservice.cts.LoginActivity.AUTHENTICATION_MESSAGE;
import static android.autofillservice.cts.LoginActivity.getWelcomeMessage;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.app.assist.AssistStructure.ViewNode;
import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.view.View;

import org.junit.Test;

@AppModeFull(reason = "LoginActivityTest is enough")
public class LoginWithStringsActivityTest
        extends AutoFillServiceTestCase.AutoActivityLaunch<LoginWithStringsActivity> {

    private LoginWithStringsActivity mActivity;


    @Override
    protected AutofillActivityTestRule<LoginWithStringsActivity> getActivityRule() {
        return new AutofillActivityTestRule<LoginWithStringsActivity>(
                LoginWithStringsActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Test
    public void testAutoFillOneDatasetAndSave() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        final Bundle extras = new Bundle();
        extras.putString("numbers", "4815162342");

        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setId("I'm the alpha and the omega")
                        .setField(ID_USERNAME, "dude")
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Dude"))
                        .build())
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_USERNAME, ID_PASSWORD)
                .setExtras(extras)
                .build());
        mActivity.expectAutoFill("dude", "sweet");

        // Trigger auto-fill.
        mActivity.onUsername(View::requestFocus);
        waitUntilConnected();

        final FillRequest fillRequest = sReplier.getNextFillRequest();

        // Make sure input was sanitized.
        assertTextIsSanitized(fillRequest.structure, ID_USERNAME);
        assertTextIsSanitized(fillRequest.structure, ID_PASSWORD);

        // Make sure labels were not sanitized
        assertTextFromResources(fillRequest.structure, ID_USERNAME_LABEL, "Username", false,
                "username_string");
        assertTextFromResources(fillRequest.structure, ID_PASSWORD_LABEL, "Password", false,
                "password_string");

        // Check text hints
        assertHintFromResources(fillRequest.structure, ID_USERNAME, "Hint for username",
                "username_hint");
        assertHintFromResources(fillRequest.structure, ID_PASSWORD, "Hint for password",
                "password_hint");

        // Auto-fill it.
        mUiBot.selectDataset("The Dude");

        // Check the results.
        mActivity.assertAutoFilled();

        // Try to login, it will fail.
        final String loginMessage = mActivity.tapLogin();

        assertWithMessage("Wrong login msg").that(loginMessage).isEqualTo(AUTHENTICATION_MESSAGE);

        // Set right password...
        mActivity.onPassword((v) -> v.setText("dude"));

        // ... and try again
        final String expectedMessage = getWelcomeMessage("dude");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);

        // Assert the snack bar is shown and tap "Save".
        mUiBot.updateForAutofill(true, SAVE_DATA_TYPE_PASSWORD);

        final SaveRequest saveRequest = sReplier.getNextSaveRequest();

        assertThat(saveRequest.datasetIds).containsExactly("I'm the alpha and the omega");

        // Assert value of expected fields - should not be sanitized.
        final ViewNode username = findNodeByResourceId(saveRequest.structure, ID_USERNAME);
        assertTextAndValue(username, "dude");
        final ViewNode password = findNodeByResourceId(saveRequest.structure, ID_USERNAME);
        assertTextAndValue(password, "dude");

        // Make sure labels were not sanitized
        assertTextFromResources(saveRequest.structure, ID_USERNAME_LABEL, "Username", false,
                "username_string");
        assertTextFromResources(saveRequest.structure, ID_PASSWORD_LABEL, "Password", false,
                "password_string");

        // Check text hints
        assertHintFromResources(fillRequest.structure, ID_USERNAME, "Hint for username",
                "username_hint");
        assertHintFromResources(fillRequest.structure, ID_PASSWORD, "Hint for password",
                "password_hint");

        // Make sure extras were passed back on onSave()
        assertThat(saveRequest.data).isNotNull();
        final String extraValue = saveRequest.data.getString("numbers");
        assertWithMessage("extras not passed on save").that(extraValue).isEqualTo("4815162342");
    }
}
