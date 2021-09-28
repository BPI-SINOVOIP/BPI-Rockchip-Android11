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

package android.autofillservice.cts.augmented;

import static android.autofillservice.cts.CannedFillResponse.NO_RESPONSE;
import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.assertHasFlags;
import static android.autofillservice.cts.Helper.assertTextAndValue;
import static android.autofillservice.cts.Helper.assertViewAutofillState;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.LoginActivity.getWelcomeMessage;
import static android.autofillservice.cts.UiBot.LANDSCAPE;
import static android.autofillservice.cts.UiBot.PORTRAIT;
import static android.autofillservice.cts.augmented.AugmentedHelper.assertBasicRequestInfo;
import static android.autofillservice.cts.augmented.AugmentedTimeouts.AUGMENTED_FILL_TIMEOUT;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.DO_NOT_REPLY_AUGMENTED_RESPONSE;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.NO_AUGMENTED_RESPONSE;
import static android.service.autofill.FillRequest.FLAG_MANUAL_REQUEST;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;
import static org.testng.Assert.assertThrows;

import android.app.assist.AssistStructure.ViewNode;
import android.autofillservice.cts.AutofillActivityTestRule;
import android.autofillservice.cts.CannedFillResponse;
import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.Helper;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.autofillservice.cts.LoginActivity;
import android.autofillservice.cts.MyAutofillCallback;
import android.autofillservice.cts.OneTimeCancellationSignalListener;
import android.autofillservice.cts.augmented.CtsAugmentedAutofillService.AugmentedFillRequest;
import android.content.ComponentName;
import android.os.CancellationSignal;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiObject2;
import android.util.ArraySet;
import android.view.View;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;
import android.widget.EditText;

import org.junit.Test;

import java.util.Set;

public class AugmentedLoginActivityTest
        extends AugmentedAutofillAutoActivityLaunchTestCase<AugmentedLoginActivity> {

    protected AugmentedLoginActivity mActivity;

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

    @Test
    public void testServiceLifecycle() throws Exception {
        enableService();
        CtsAugmentedAutofillService augmentedService = enableAugmentedService();

        AugmentedHelper.resetAugmentedService();
        augmentedService.waitUntilDisconnected();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAutoFill_neitherServiceCanAutofill() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        final AutofillId usernameId = username.getAutofillId();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest augmentedRequest = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(augmentedRequest, mActivity, usernameId, expectedFocusedValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAutoFill_neitherServiceCanAutofill_manualRequest() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        final AutofillId usernameId = username.getAutofillId();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill
        mActivity.forceAutofillOnUsername();
        sReplier.getNextFillRequest();
        final AugmentedFillRequest augmentedRequest = sAugmentedReplier.getNextFillRequest();

        // Assert request
        // No inline request because didn't focus on any view.
        assertBasicRequestInfo(augmentedRequest, mActivity, usernameId, expectedFocusedValue,
                /* hasInlineRequest */ false);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAutoFill_neitherServiceCanAutofill_thenManualRequest() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        final AutofillId expectedFocusedId = username.getAutofillId();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, expectedFocusedId, expectedFocusedValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();

        // Try again, forcing it
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_USERNAME, ID_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Dude"))
                        .build())
                .build());
        mActivity.expectAutoFill("dude", "sweet");
        mActivity.forceAutofillOnUsername();
        final FillRequest fillRequest = sReplier.getNextFillRequest();
        assertHasFlags(fillRequest.flags, FLAG_MANUAL_REQUEST);
        mAugmentedUiBot.assertUiNeverShown();

        mUiBot.selectDataset("The Dude");
        mActivity.assertAutoFilled();

        // Now force save to make sure the values changes are notified
        mActivity.onUsername((v) -> v.setText("malkovich"));
        mActivity.onPassword((v) -> v.setText("malkovich"));
        final String expectedMessage = getWelcomeMessage("malkovich");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);

        // Assert the snack bar is shown and tap "Save".
        mUiBot.updateForAutofill(true, SAVE_DATA_TYPE_PASSWORD);
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        sReplier.assertNoUnhandledSaveRequests();
        assertThat(saveRequest.datasetIds).isNull();

        // Assert value of expected fields - should not be sanitized.
        final ViewNode usernameNode = findNodeByResourceId(saveRequest.structure, ID_USERNAME);
        assertTextAndValue(usernameNode, "malkovich");
        final ViewNode passwordNode = findNodeByResourceId(saveRequest.structure, ID_PASSWORD);
        assertTextAndValue(passwordNode, "malkovich");
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAutoFill_notImportantForAutofill_thenManualRequest() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set IMPORTANT_FOR_AUTOFILL_NO
        mActivity.onUsername((v) -> v.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO));

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        final AutofillId expectedFocusedId = username.getAutofillId();
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.assertOnFillRequestNotCalled();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, expectedFocusedId, expectedFocusedValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();

        // Try again, forcing it
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_USERNAME, ID_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Dude"))
                        .build())
                .build());
        mActivity.expectAutoFill("dude", "sweet");
        mActivity.forceAutofillOnUsername();
        final FillRequest fillRequest = sReplier.getNextFillRequest();
        assertHasFlags(fillRequest.flags, FLAG_MANUAL_REQUEST);
        mAugmentedUiBot.assertUiNeverShown();

        mUiBot.selectDataset("The Dude");
        mActivity.assertAutoFilled();

        // Now force save to make sure the values changes are notified
        mActivity.onUsername((v) -> v.setText("malkovich"));
        mActivity.onPassword((v) -> v.setText("malkovich"));
        final String expectedMessage = getWelcomeMessage("malkovich");
        final String actualMessage = mActivity.tapLogin();
        assertWithMessage("Wrong welcome msg").that(actualMessage).isEqualTo(expectedMessage);

        // Assert the snack bar is shown and tap "Save".
        mUiBot.updateForAutofill(true, SAVE_DATA_TYPE_PASSWORD);
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        sReplier.assertNoUnhandledSaveRequests();
        assertThat(saveRequest.datasetIds).isNull();

        // Assert value of expected fields - should not be sanitized.
        final ViewNode usernameNode = findNodeByResourceId(saveRequest.structure, ID_USERNAME);
        assertTextAndValue(usernameNode, "malkovich");
        final ViewNode passwordNode = findNodeByResourceId(saveRequest.structure, ID_PASSWORD);
        assertTextAndValue(passwordNode, "malkovich");
    }

    @Test
    public void testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());
        mActivity.expectAutoFill("dude");

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, usernameId, expectedFocusedValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // Autofill
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    public void testAutoFill_augmentedFillRequestCancelled() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .setDelay(AUGMENTED_FILL_TIMEOUT.ms() + 6000)
                .build());

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        sAugmentedReplier.getNextFillRequest();

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        mAugmentedUiBot.assertUiNeverShown();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAutoFill_mainServiceReturnedNull_augmentedAutofillTwoFields() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .setField(mActivity.getPassword().getAutofillId(), "sweet")
                        .build(), usernameId)
                .build());
        mActivity.expectAutoFill("dude", "sweet");

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, usernameId, expectedFocusedValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // Autofill
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testCancellationSignalCalledAfterTimeout() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(DO_NOT_REPLY_AUGMENTED_RESPONSE);
        final OneTimeCancellationSignalListener listener =
                new OneTimeCancellationSignalListener(AUGMENTED_FILL_TIMEOUT.ms() + 5000);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();

        final CancellationSignal cancellationSignal = sAugmentedReplier.getNextFillRequest()
                .cancellationSignal;

        assertThat(cancellationSignal).isNotNull();
        cancellationSignal.setOnCancelListener(listener);

        // Assert results
        listener.assertOnCancelCalled();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testCancellationSignalCalled_retriggerAugmentedAutofill() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .setField(mActivity.getPassword().getAutofillId(), "sweet")
                        .build(), usernameId)
                .build());

        final OneTimeCancellationSignalListener listener =
                new OneTimeCancellationSignalListener(AUGMENTED_FILL_TIMEOUT.ms() + 5000);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, usernameId, expectedFocusedValue);
        mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        final CancellationSignal cancellationSignal = request.cancellationSignal;

        assertThat(cancellationSignal).isNotNull();
        cancellationSignal.setOnCancelListener(listener);

        // Move focus away to make sure Augmented Autofill UI is gone.
        mActivity.clearFocus();
        mAugmentedUiBot.assertUiGone();

        // Set expectations for username again.
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .setField(mActivity.getPassword().getAutofillId(), "sweet")
                        .build(), usernameId)
                .build());

        // Tap on username again
        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request2 = sAugmentedReplier.getNextFillRequest();

        // Assert first request cancelled
        listener.assertOnCancelCalled();

        // Assert request
        assertBasicRequestInfo(request2, mActivity, usernameId, expectedFocusedValue);
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // ...and autofill this time
        mActivity.expectAutoFill("dude", "sweet");
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_multipleRequests() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, usernameId, usernameValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        mAugmentedUiBot.assertUiShown(usernameId, "req1");

        // Move focus away to make sure Augmented Autofill UI is gone.
        mActivity.onLogin(View::requestFocus);
        mAugmentedUiBot.assertUiGone();

        // Tap on password field
        final EditText password = mActivity.getPassword();
        final AutofillId passwordId = password.getAutofillId();
        final AutofillValue passwordValue = password.getAutofillValue();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req2")
                        .build(), passwordId)
                .build());
        mActivity.onPassword(View::requestFocus);
        mUiBot.assertNoDatasetsEver();

        // (TODO: b/141703197) password request temp disabled.
        mAugmentedUiBot.assertUiGone();
        sAugmentedReplier.reset();

        // Tap on username again...
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .setField(passwordId, "sweet")
                        .build(), usernameId)
                .build());

        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request3 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request3, mActivity, usernameId, usernameValue);
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // ...and autofill this time
        mActivity.expectAutoFill("dude", "sweet");
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_thenEditField() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue expectedFocusedValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());
        mActivity.expectAutoFill("dude");

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, usernameId, expectedFocusedValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // Autofill
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
        assertViewAutofillState(mActivity.getUsername(), true);

        // Now change value and make sure autofill status is changed
        mActivity.onUsername((v) -> v.setText("I AM GROOT"));
        assertViewAutofillState(mActivity.getUsername(), false);
    }

    @Test
    public void testAugmentedAutoFill_callback() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .build(), usernameId)
                .build());

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, usernameId, usernameValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        callback.assertUiShownEvent(username);
        mAugmentedUiBot.assertUiShown(usernameId, "req1");

        // Move focus away to make sure Augmented Autofill UI is gone.
        mActivity.onLogin(View::requestFocus);
        mAugmentedUiBot.assertUiGone();
        callback.assertUiHiddenEvent(username);

        // Tap on password field
        final EditText password = mActivity.getPassword();
        final AutofillId passwordId = password.getAutofillId();
        final AutofillValue passwordValue = password.getAutofillValue();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("req2")
                        .build(), passwordId)
                .build());
        mActivity.onPassword(View::requestFocus);
        mUiBot.assertNoDatasetsEver();

        // (TODO: b/141703197) password request temp disabled.
        callback.assertNotCalled();
        mAugmentedUiBot.assertUiGone();
        sAugmentedReplier.reset();

        // Tap on username again...
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .setField(passwordId, "sweet")
                        .build(), usernameId)
                .build());

        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request3 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request3, mActivity, usernameId, usernameValue);
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");
        callback.assertUiShownEvent(username);

        // ...and autofill this time
        mActivity.expectAutoFill("dude", "sweet");
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
        callback.assertUiHiddenEvent(username);
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_rotateDevice() throws Exception {
        assumeTrue("Rotation is supported", Helper.isRotationSupported(mContext));

        // Set services
        enableService();
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sReplier.addResponse(NO_RESPONSE);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 1")
                        .setField(usernameId, "dude1")
                        .build(), usernameId)
                .build());

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        AugmentedLoginActivity currentActivity = mActivity;

        // Assert request
        assertBasicRequestInfo(request1, currentActivity, usernameId, usernameValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        mAugmentedUiBot.assertUiShown(usernameId, "Augment Me 1");

        // 1st landscape rotation
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 2")
                        .setField(usernameId, "dude2")
                        .build(), usernameId)
                .build());
        mUiBot.setScreenOrientation(LANDSCAPE);
        mUiBot.assertNoDatasetsEver();

        // Must update currentActivity after each rotation because it generates a new instance
        currentActivity = LoginActivity.getCurrentActivity();

        final AugmentedFillRequest request2 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request2, currentActivity, usernameId, usernameValue);
        mAugmentedUiBot.assertUiShown(usernameId, "Augment Me 2");

        // Rotate back
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 3")
                        .setField(usernameId, "dude3")
                        .build(), usernameId)
                .build());
        mUiBot.setScreenOrientation(PORTRAIT);
        mUiBot.assertNoDatasetsEver();
        currentActivity = LoginActivity.getCurrentActivity();

        final AugmentedFillRequest request3 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request3, currentActivity, usernameId, usernameValue);
        mAugmentedUiBot.assertUiShown(usernameId, "Augment Me 3");

        // 2nd landscape rotation
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 4")
                        .setField(usernameId, "dude4")
                        .build(), usernameId)
                .build());
        mUiBot.setScreenOrientation(LANDSCAPE);
        mUiBot.assertNoDatasetsEver();
        currentActivity = LoginActivity.getCurrentActivity();

        final AugmentedFillRequest request4 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request4, currentActivity, usernameId, usernameValue);
        mAugmentedUiBot.assertUiShown(usernameId, "Augment Me 4");

        // Final rotation - should be enough....
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me 5")
                        .setField(usernameId, "dude5")
                        .build(), usernameId)
                .build());
        mUiBot.setScreenOrientation(PORTRAIT);
        mUiBot.assertNoDatasetsEver();
        currentActivity = LoginActivity.getCurrentActivity();

        final AugmentedFillRequest request5 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request5, mActivity, usernameId, usernameValue);
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me 5");

        // ..then autofill

        // Must get the latest activity because each rotation creates a new object.
        currentActivity.expectAutoFill("dude5");
        ui.click();
        mAugmentedUiBot.assertUiGone();
        currentActivity.assertAutoFilled();
    }

    @Test
    public void testAugmentedAutoFill_noPreviousRequest_requestAutofill() throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        final CtsAugmentedAutofillService service = enableAugmentedService();

        // Request requestAutofill without any existing request
        final AutofillId usernameId = mActivity.getUsername().getAutofillId();
        final ComponentName componentName = mActivity.getComponentName();
        final boolean requestResult = service.requestAutofill(componentName, usernameId);

        assertThat(requestResult).isFalse();
    }

    @Test
    public void testAugmentedAutoFill_hasPreviousRequestViewFocused_requestAutofill()
            throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        final CtsAugmentedAutofillService service = enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        mUiBot.waitForIdleSync();
        sAugmentedReplier.getNextFillRequest();

        // Set expectations for username again
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());
        // Service requests requestAutofill() for same focused view
        final ComponentName componentName = mActivity.getComponentName();
        final boolean requestResult = service.requestAutofill(componentName, usernameId);
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertThat(requestResult).isTrue();
        assertBasicRequestInfo(request, mActivity, usernameId, usernameValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");
    }

    @Test
    public void testAugmentedAutoFill_hasPreviousRequestViewNotFocused_requestAutofill()
            throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        final CtsAugmentedAutofillService service = enableAugmentedService();

        // Set expectations
        final AutofillId usernameId = mActivity.getUsername().getAutofillId();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sAugmentedReplier.getNextFillRequest();

        // Clear focus
        mActivity.clearFocus();

        // Service requests requestAutofill() for non-focused view
        final ComponentName componentName = mActivity.getComponentName();
        final boolean requestResult = service.requestAutofill(componentName, usernameId);

        assertThat(requestResult).isFalse();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_mainServiceDisabled() throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());
        mActivity.expectAutoFill("dude");

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request, mActivity, usernameId, usernameValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // Autofill
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_mainServiceDisabled_manualRequest() throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());
        mActivity.expectAutoFill("dude");

        // Trigger autofill
        mActivity.forceAutofillOnUsername();
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();

        // Assert request
        // No inline request because didn't focus on any view.
        assertBasicRequestInfo(request, mActivity, usernameId, usernameValue,
                /* hasInlineRequest */ false);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // Autofill
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_mainServiceDisabled_autoThenManualRequest() throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue usernameValue = username.getAutofillValue();
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request1, mActivity, usernameId, usernameValue);

        // Make sure standard no UI is not shown.
        mUiBot.assertNoDatasetsEver();
        mAugmentedUiBot.assertUiNeverShown();


        // Trigger 2nd request, manually
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude")
                        .build(), usernameId)
                .build());
        mActivity.expectAutoFill("dude");
        mActivity.forceAutofillOnUsername();
        final AugmentedFillRequest request2 = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request2, mActivity, usernameId, usernameValue);

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(usernameId, "Augment Me");

        // Autofill
        ui.click();
        mActivity.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_mainServiceDisabled_valueChangedOnSecondRequest()
            throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue initialValue = username.getAutofillValue();
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, usernameId, initialValue);

        // Make sure UIs were not shown
        mUiBot.assertNoDatasetsEver();
        mAugmentedUiBot.assertUiNeverShown();

        // Change field value
        mActivity.onUsername((v) -> v.setText("DOH"));

        // Trigger autofill again, by forcing a manual autofill request
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);
        mActivity.forceAutofillOnUsername();
        final AugmentedFillRequest request2 = sAugmentedReplier.getNextFillRequest();

        // Assert 2nd request
        assertBasicRequestInfo(request2, mActivity, usernameId, "DOH");

        // Make sure UIs were not shown
        mUiBot.assertNoDatasetsEver();
        mAugmentedUiBot.assertUiNeverShown();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutoFill_mainServiceDisabled_tappingSecondTimeNotTrigger()
            throws Exception {
        // Set services
        Helper.disableAutofillService(sContext);
        enableAugmentedService();

        // Set expectations
        final EditText username = mActivity.getUsername();
        final AutofillId usernameId = username.getAutofillId();
        final AutofillValue initialValue = username.getAutofillValue();
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill by focusing on the field
        mActivity.onUsername(View::requestFocus);
        final AugmentedFillRequest request1 = sAugmentedReplier.getNextFillRequest();

        // Assert request
        assertBasicRequestInfo(request1, mActivity, usernameId, initialValue);

        // Make sure UIs were not shown
        mUiBot.assertNoDatasetsEver();
        mAugmentedUiBot.assertUiNeverShown();

        // Change field value
        mActivity.onUsername((v) -> v.setText("DOH"));

        // Tap on the field again
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);
        mActivity.onUsername(View::performClick);

        // Assert no fill requests
        sAugmentedReplier.assertNoUnhandledFillRequests();

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testSetAugmentedAutofillWhitelist_noStandardServiceSet() throws Exception {
        final AutofillManager mgr = mActivity.getAutofillManager();
        assertThrows(SecurityException.class,
                () -> mgr.setAugmentedAutofillWhitelist((Set<String>) null,
                        (Set<ComponentName>) null));
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testSetAugmentedAutofillWhitelist_notAugmentedService() throws Exception {
        enableService();
        final AutofillManager mgr = mActivity.getAutofillManager();
        assertThrows(SecurityException.class,
                () -> mgr.setAugmentedAutofillWhitelist((Set<String>) null,
                        (Set<ComponentName>) null));
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutofill_packageNotWhitelisted() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        final AutofillManager mgr = mActivity.getAutofillManager();
        mgr.setAugmentedAutofillWhitelist((Set) null, (Set) null);

        // Set expectations
        sReplier.addResponse(NO_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();

        // Assert no fill requests
        sAugmentedReplier.assertNoUnhandledFillRequests();

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();
    }

    @Test
    @AppModeFull(reason = "testAutoFill_mainServiceReturnedNull_augmentedAutofillOneField enough")
    public void testAugmentedAutofill_activityNotWhitelisted() throws Exception {
        // Set services
        enableService();
        enableAugmentedService();

        final AutofillManager mgr = mActivity.getAutofillManager();
        final ArraySet<ComponentName> components = new ArraySet<>();
        components.add(new ComponentName(Helper.MY_PACKAGE, "some.activity"));
        mgr.setAugmentedAutofillWhitelist(null, components);

        // Set expectations
        sReplier.addResponse(NO_RESPONSE);

        // Trigger autofill
        mActivity.onUsername(View::requestFocus);
        sReplier.getNextFillRequest();

        // Assert no fill requests
        sAugmentedReplier.assertNoUnhandledFillRequests();

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Make sure Augmented Autofill UI is not shown.
        mAugmentedUiBot.assertUiNeverShown();
    }

    /*
     * TODO(b/123542344) - add moarrrr tests:
     *
     * - Augmented service returned null
     * - Focus back and forth between username and passwod
     *   - When Augmented service shows UI on one field (like username) but not other.
     *   - When Augmented service shows UI on one field (like username) on both.
     * - Tap back
     * - Tap home (then bring activity back)
     * - Acitivy is killed (and restored)
     * - Main service returns non-null response that doesn't show UI (for example, has just
     *   SaveInfo)
     *   - Augmented autofill show UI, user fills, Save UI is shown
     *   - etc ...
     * - No augmented autofill calls when the main service is not set.
     * - etc...
     */
}
