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
package android.autofillservice.cts;

import static android.autofillservice.cts.Helper.assertTextAndValue;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.PreSimpleSaveActivity.ID_PRE_INPUT;
import static android.autofillservice.cts.SimpleSaveActivity.ID_INPUT;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_USERNAME;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.app.assist.AssistStructure;
import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.content.ComponentName;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.SaveInfo;
import android.support.test.uiautomator.UiObject2;

import org.junit.Test;

@AppModeFull(reason = "Service-specific test")
public class MultiScreenDifferentActivitiesTest
        extends AutoFillServiceTestCase.ManualActivityLaunch {

    @Test
    public void testActivityNotDelayedIsNotMerged() throws Exception {
        // Set service.
        enableService();

        // Trigger autofill on 1st activity, without using FLAG_DELAY_SAVE
        final PreSimpleSaveActivity activity1 = startPreSimpleSaveActivity();
        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME, ID_PRE_INPUT)
                .build());

        activity1.syncRunOnUiThread(() -> activity1.mPreInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger autofill on 2nd activity
        final SimpleSaveActivity activity2 = startSimpleSaveActivity();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_INPUT)
                .build());
        activity2.syncRunOnUiThread(() -> activity2.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save
        activity2.syncRunOnUiThread(() -> {
            activity2.mInput.setText("ID");
            activity2.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_PASSWORD);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();

        // Make sure only second request is available
        assertThat(saveRequest.contexts).hasSize(1);

        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "ID");
    }

    @Test
    public void testDelayedActivityIsMerged() throws Exception {
        // Set service.
        enableService();

        // Trigger autofill on 1st activity, usingFLAG_DELAY_SAVE
        final PreSimpleSaveActivity activity1 = startPreSimpleSaveActivity();
        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setSaveInfoFlags(SaveInfo.FLAG_DELAY_SAVE)
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME, ID_PRE_INPUT)
                .build());

        activity1.syncRunOnUiThread(() -> activity1.mPreInput.requestFocus());
        sReplier.getNextFillRequest();

        // Fill field but don't finish session yet
        activity1.syncRunOnUiThread(() -> {
            activity1.mPreInput.setText("PRE");
        });

        // Trigger autofill on 2nd activity
        final SimpleSaveActivity activity2 = startSimpleSaveActivity();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_INPUT)
                .build());
        activity2.syncRunOnUiThread(() -> activity2.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save
        activity2.syncRunOnUiThread(() -> {
            activity2.mInput.setText("ID");
            activity2.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_PASSWORD);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();

        // Make sure both requests are available
        assertThat(saveRequest.contexts).hasSize(2);

        // Assert 1st request
        final AssistStructure structure1 = saveRequest.contexts.get(0).getStructure();
        assertWithMessage("no structure for 1st activity").that(structure1).isNotNull();
        assertTextAndValue(findNodeByResourceId(structure1, ID_PRE_INPUT), "PRE");
        assertThat(findNodeByResourceId(structure1, ID_INPUT)).isNull();
        final ComponentName component1 = structure1.getActivityComponent();
        assertThat(component1).isEqualTo(activity1.getComponentName());

        // Assert 2nd request
        final AssistStructure structure2 = saveRequest.contexts.get(1).getStructure();
        assertWithMessage("no structure for 2nd activity").that(structure2).isNotNull();
        assertThat(findNodeByResourceId(structure2, ID_PRE_INPUT)).isNull();
        assertTextAndValue(findNodeByResourceId(structure2, ID_INPUT), "ID");
        final ComponentName component2 = structure2.getActivityComponent();
        assertThat(component2).isEqualTo(activity2.getComponentName());
    }
}
