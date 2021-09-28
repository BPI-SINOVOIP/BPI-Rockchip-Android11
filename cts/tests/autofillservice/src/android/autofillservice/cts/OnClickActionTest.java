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

import static android.autofillservice.cts.CustomDescriptionHelper.ID_HIDE;
import static android.autofillservice.cts.CustomDescriptionHelper.ID_PASSWORD_MASKED;
import static android.autofillservice.cts.CustomDescriptionHelper.ID_PASSWORD_PLAIN;
import static android.autofillservice.cts.CustomDescriptionHelper.ID_SHOW;
import static android.autofillservice.cts.CustomDescriptionHelper.ID_USERNAME_MASKED;
import static android.autofillservice.cts.CustomDescriptionHelper.ID_USERNAME_PLAIN;
import static android.autofillservice.cts.CustomDescriptionHelper.newCustomDescriptionWithHiddenFields;
import static android.autofillservice.cts.Helper.ID_PASSWORD_LABEL;
import static android.autofillservice.cts.Helper.ID_USERNAME_LABEL;
import static android.autofillservice.cts.Helper.assertTextAndValue;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.SimpleSaveActivity.ID_INPUT;
import static android.autofillservice.cts.SimpleSaveActivity.ID_PASSWORD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_GENERIC;

import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.CharSequenceTransformation;
import android.service.autofill.FillContext;
import android.service.autofill.OnClickAction;
import android.service.autofill.VisibilitySetterAction;
import android.support.test.uiautomator.UiObject2;
import android.view.View;
import android.view.autofill.AutofillId;

import org.junit.Test;

import java.util.regex.Pattern;

/**
 * Integration tests for the {@link OnClickAction} implementations.
 */
@AppModeFull(reason = "Service-specific test")
public class OnClickActionTest
        extends AutoFillServiceTestCase.AutoActivityLaunch<SimpleSaveActivity> {

    private static final Pattern MATCH_ALL = Pattern.compile("^(.*)$");

    private SimpleSaveActivity mActivity;

    @Override
    protected AutofillActivityTestRule<SimpleSaveActivity> getActivityRule() {
        return new AutofillActivityTestRule<SimpleSaveActivity>(SimpleSaveActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Test
    public void testHideAndShow() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId usernameId = findAutofillIdByResourceId(context, ID_INPUT);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);

                    final CharSequenceTransformation usernameTrans = new CharSequenceTransformation
                            .Builder(usernameId, MATCH_ALL, "$1").build();
                    final CharSequenceTransformation passwordTrans = new CharSequenceTransformation
                            .Builder(passwordId, MATCH_ALL, "$1").build();
                    builder.setCustomDescription(newCustomDescriptionWithHiddenFields()
                            .addChild(R.id.username_plain, usernameTrans)
                            .addChild(R.id.password_plain, passwordTrans)
                            .addOnClickAction(R.id.show, new VisibilitySetterAction
                                    .Builder(R.id.hide, View.VISIBLE)
                                    .setVisibility(R.id.show, View.GONE)
                                    .setVisibility(R.id.username_plain, View.VISIBLE)
                                    .setVisibility(R.id.password_plain, View.VISIBLE)
                                    .setVisibility(R.id.username_masked, View.GONE)
                                    .setVisibility(R.id.password_masked, View.GONE)
                                    .build())
                            .addOnClickAction(R.id.hide, new VisibilitySetterAction
                                    .Builder(R.id.show, View.VISIBLE)
                                    .setVisibility(R.id.hide, View.GONE)
                                    .setVisibility(R.id.username_masked, View.VISIBLE)
                                    .setVisibility(R.id.password_masked, View.VISIBLE)
                                    .setVisibility(R.id.username_plain, View.GONE)
                                    .setVisibility(R.id.password_plain, View.GONE)
                                    .build())
                            .build());
                })
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("42");
            mActivity.mPassword.setText("108");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Assert initial UI is hidden the password.
        final UiObject2 showButton = assertHidden(saveUi);

        // Then tap SHOW and assert it's showing how
        showButton.click();
        final UiObject2 hideButton = assertShown(saveUi);

        // Hide again
        hideButton.click();
        assertHidden(saveUi);

        // Rinse-and repeat a couple times
        showButton.click(); assertShown(saveUi);
        hideButton.click(); assertHidden(saveUi);
        showButton.click(); assertShown(saveUi);
        hideButton.click(); assertHidden(saveUi);

        // Then save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "42");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "108");
    }

    /**
     * Asserts that the Save UI is in the hiding the password field, returning the {@code SHOW}
     * button.
     */
    private UiObject2 assertHidden(UiObject2 saveUi) throws Exception {
        // Username
        mUiBot.assertChildText(saveUi, ID_USERNAME_LABEL, "User:");
        mUiBot.assertChildText(saveUi, ID_USERNAME_MASKED, "****");
        assertInvisible(saveUi, ID_USERNAME_PLAIN);

        // Password
        mUiBot.assertChildText(saveUi, ID_PASSWORD_LABEL, "Pass:");
        mUiBot.assertChildText(saveUi, ID_PASSWORD_MASKED, "....");
        assertInvisible(saveUi, ID_PASSWORD_PLAIN);

        // Buttons
        assertInvisible(saveUi, ID_HIDE);
        return mUiBot.assertChildText(saveUi, ID_SHOW, "SHOW");
    }

    /**
     * Asserts that the Save UI is in the showing the password field, returning the {@code HIDE}
     * button.
     */
    private UiObject2 assertShown(UiObject2 saveUi) throws Exception {
        // Username
        mUiBot.assertChildText(saveUi, ID_USERNAME_LABEL, "User:");
        mUiBot.assertChildText(saveUi, ID_USERNAME_PLAIN, "42");
        assertInvisible(saveUi, ID_USERNAME_MASKED);

        // Password
        mUiBot.assertChildText(saveUi, ID_PASSWORD_LABEL, "Pass:");
        mUiBot.assertChildText(saveUi, ID_PASSWORD_PLAIN, "108");
        assertInvisible(saveUi, ID_PASSWORD_MASKED);

        // Buttons
        assertInvisible(saveUi, ID_SHOW);
        return mUiBot.assertChildText(saveUi, ID_HIDE, "HIDE");
    }

    private void assertInvisible(UiObject2 saveUi, String resourceId) {
        mUiBot.assertGoneByRelativeId(saveUi, resourceId, Timeouts.UI_TIMEOUT);
    }
}
