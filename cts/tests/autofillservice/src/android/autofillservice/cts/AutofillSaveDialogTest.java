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

import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_USERNAME;

import android.content.Context;
import android.content.Intent;
import android.view.View;

import org.junit.Test;

/**
 * Tests whether autofill save dialog is shown as expected.
 */
public class AutofillSaveDialogTest extends AutoFillServiceTestCase.ManualActivityLaunch {

    @Test
    public void testShowSaveUiWhenLaunchActivityWithFlagClearTopAndSingleTop() throws Exception {
        // Set service.
        enableService();

        // Start SimpleBeforeLoginActivity before login activity.
        startActivityWithFlag(mContext, SimpleBeforeLoginActivity.class,
                Intent.FLAG_ACTIVITY_NEW_TASK);
        mUiBot.assertShownByRelativeId(SimpleBeforeLoginActivity.ID_BEFORE_LOGIN);

        // Start LoginActivity.
        startActivityWithFlag(SimpleBeforeLoginActivity.getCurrentActivity(), LoginActivity.class,
                /* flags= */ 0);
        mUiBot.assertShownByRelativeId(LoginActivity.ID_USERNAME_CONTAINER);

        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME, ID_USERNAME)
                .build());

        // Trigger autofill on username.
        LoginActivity loginActivity = LoginActivity.getCurrentActivity();
        loginActivity.onUsername(View::requestFocus);

        // Wait for fill request to be processed.
        sReplier.getNextFillRequest();

        // Set data.
        loginActivity.onUsername((v) -> v.setText("test"));

        // Start SimpleAfterLoginActivity after login activity.
        startActivityWithFlag(loginActivity, SimpleAfterLoginActivity.class, /* flags= */ 0);
        mUiBot.assertShownByRelativeId(SimpleAfterLoginActivity.ID_AFTER_LOGIN);

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_USERNAME);

        // Restart SimpleBeforeLoginActivity with CLEAR_TOP and SINGLE_TOP.
        startActivityWithFlag(SimpleAfterLoginActivity.getCurrentActivity(),
                SimpleBeforeLoginActivity.class,
                Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        mUiBot.assertShownByRelativeId(SimpleBeforeLoginActivity.ID_BEFORE_LOGIN);

        // Verify save ui dialog.
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_USERNAME);
    }

    @Test
    public void testShowSaveUiWhenLaunchActivityWithFlagClearTaskAndNewTask() throws Exception {
        // Set service.
        enableService();

        // Start SimpleBeforeLoginActivity before login activity.
        startActivityWithFlag(mContext, SimpleBeforeLoginActivity.class,
                Intent.FLAG_ACTIVITY_NEW_TASK);
        mUiBot.assertShownByRelativeId(SimpleBeforeLoginActivity.ID_BEFORE_LOGIN);

        // Start LoginActivity.
        startActivityWithFlag(SimpleBeforeLoginActivity.getCurrentActivity(), LoginActivity.class,
                /* flags= */ 0);
        mUiBot.assertShownByRelativeId(LoginActivity.ID_USERNAME_CONTAINER);

        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME, ID_USERNAME)
                .build());

        // Trigger autofill on username.
        LoginActivity loginActivity = LoginActivity.getCurrentActivity();
        loginActivity.onUsername(View::requestFocus);

        // Wait for fill request to be processed.
        sReplier.getNextFillRequest();

        // Set data.
        loginActivity.onUsername((v) -> v.setText("test"));

        // Start SimpleAfterLoginActivity with CLEAR_TASK and NEW_TASK after login activity.
        startActivityWithFlag(loginActivity, SimpleAfterLoginActivity.class,
                Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        mUiBot.assertShownByRelativeId(SimpleAfterLoginActivity.ID_AFTER_LOGIN);

        // Verify save ui dialog.
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_USERNAME);
    }

    private void startActivityWithFlag(Context context, Class<?> clazz, int flags) {
        final Intent intent = new Intent(context, clazz);
        intent.setFlags(flags);
        context.startActivity(intent);
    }
}
