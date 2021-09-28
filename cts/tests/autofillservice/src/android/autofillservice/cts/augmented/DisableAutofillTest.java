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

import static android.autofillservice.cts.augmented.AugmentedHelper.assertBasicRequestInfo;
import static android.autofillservice.cts.augmented.AugmentedHelper.resetAugmentedService;

import android.autofillservice.cts.CannedFillResponse;
import android.autofillservice.cts.PreSimpleSaveActivity;
import android.autofillservice.cts.SimpleSaveActivity;
import android.autofillservice.cts.augmented.CtsAugmentedAutofillService.AugmentedFillRequest;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiObject2;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.EditText;

import org.junit.Test;

@AppModeFull(reason = "AugmentedLoginActivityTest is enough")
public class DisableAutofillTest extends AugmentedAutofillManualActivityLaunchTestCase {

    @Test
    public void testAugmentedAutofill_standardAutofillDisabledByService_sameActivity()
            throws Exception {
        // Enable services
        enableService();
        enableAugmentedService();

        // Set expectations
        sReplier.addResponse(
                new CannedFillResponse.Builder().disableAutofill(Long.MAX_VALUE).build());
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setOnlyDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setOnlyField("dude")
                        .build())
                .build());

        final PreSimpleSaveActivity preSimpleActivity = startPreSimpleSaveActivity();
        final EditText preInput = preSimpleActivity.getPreInput();
        preSimpleActivity.syncRunOnUiThread(() -> preInput.requestFocus());
        sReplier.getNextFillRequest();

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Assert augmented autofill request
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request, preSimpleActivity, preInput.getAutofillId(),
                preInput.getAutofillValue());

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(preInput.getAutofillId(), "Augment Me");

        // Then autofill it
        final PreSimpleSaveActivity.FillExpectation autofillExpectation = preSimpleActivity
                .expectAutoFill("dude");
        ui.click();
        autofillExpectation.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();

        // Sanity check - make sure standard autofill was not requested
        sReplier.assertNoUnhandledFillRequests();
    }

    @Test
    public void testAugmentedAutofill_standardAutofillDisabledByService_otherActivity()
            throws Exception {
        // Set standard service only, so we don't trigger augmented autofill when we launch the
        // first activity
        enableService();
        resetAugmentedService();

        // Use PreSimpleSaveActivity disable standard autofill
        sReplier.addResponse(
                new CannedFillResponse.Builder().disableAutofill(Long.MAX_VALUE).build());
        final PreSimpleSaveActivity preSimpleActivity = startPreSimpleSaveActivity();
        final EditText preInput = preSimpleActivity.getPreInput();
        preSimpleActivity.syncRunOnUiThread(() -> preInput.requestFocus());
        sReplier.getNextFillRequest();

        // Then use SimpleSaveActivity for the main test
        enableAugmentedService();
        final SimpleSaveActivity simpleActivity = startSimpleSaveActivity();
        final EditText input = simpleActivity.getInput();


        // Set expectations
        final AutofillId inputId = input.getAutofillId();
        final AutofillValue inputValue = input.getAutofillValue();
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .setDataset(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(inputId, "dude")
                        .build(), inputId)
                .build());

        simpleActivity.syncRunOnUiThread(() -> input.requestFocus());

        // Make sure standard Autofill UI is not shown.
        mUiBot.assertNoDatasetsEver();

        // Assert augmented autofill request
        final AugmentedFillRequest request = sAugmentedReplier.getNextFillRequest();
        assertBasicRequestInfo(request, simpleActivity, inputId, inputValue);

        // Make sure Augmented Autofill UI is shown.
        final UiObject2 ui = mAugmentedUiBot.assertUiShown(inputId, "Augment Me");

        // Then autofill it
        final SimpleSaveActivity.FillExpectation autofillExpectation = simpleActivity
                .expectAutoFill("dude");
        ui.click();
        autofillExpectation.assertAutoFilled();
        mAugmentedUiBot.assertUiGone();

        // Sanity check - make sure standard autofill was not requested
        sReplier.assertNoUnhandledFillRequests();
    }
}
