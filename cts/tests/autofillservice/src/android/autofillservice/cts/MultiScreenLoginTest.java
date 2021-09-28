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

import static android.autofillservice.cts.CustomDescriptionHelper.newCustomDescriptionWithUsernameAndPassword;
import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_PASSWORD_LABEL;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.ID_USERNAME_LABEL;
import static android.autofillservice.cts.Helper.assertTextAndValue;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_USERNAME;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.app.assist.AssistStructure;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.content.ComponentName;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.CharSequenceTransformation;
import android.service.autofill.SaveInfo;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.view.autofill.AutofillId;

import org.junit.Test;

import java.util.regex.Pattern;

/**
 * Test case for the senario where a login screen is split in multiple activities.
 */
@AppModeFull(reason = "Service-specific test")
public class MultiScreenLoginTest
        extends AutoFillServiceTestCase.AutoActivityLaunch<UsernameOnlyActivity> {

    private static final String TAG = "MultiScreenLoginTest";
    private static final Pattern MATCH_ALL = Pattern.compile("^(.*)$");

    private UsernameOnlyActivity mActivity;

    @Override
    protected AutofillActivityTestRule<UsernameOnlyActivity> getActivityRule() {
        return new AutofillActivityTestRule<UsernameOnlyActivity>(UsernameOnlyActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    /**
     * Tests the "traditional" scenario where the service must save each field (username and
     * password) separately.
     */
    @Test
    public void testSaveEachFieldSeparately() throws Exception {
        // Set service
        enableService();

        // First handle username...

        // Set expectations.
        final Bundle clientState1 = new Bundle();
        clientState1.putString("first", "one");
        clientState1.putString("last", "one");
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME, ID_USERNAME)
                .setExtras(clientState1)
                .build());

        // Trigger autofill
        mActivity.focusOnUsername();
        final FillRequest fillRequest1 = sReplier.getNextFillRequest();
        assertThat(fillRequest1.contexts.size()).isEqualTo(1);
        mUiBot.assertNoDatasetsEver();

        // Trigger save...
        mActivity.setUsername("dude");
        mActivity.next();
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_USERNAME);

        // ..and assert results
        final SaveRequest saveRequest1 = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest1.structure, ID_USERNAME), "dude");
        assertThat(saveRequest1.data.getString("first")).isEqualTo("one");
        assertThat(saveRequest1.data.getString("last")).isEqualTo("one");

        // ...now rinse and repeat for password

        // Get the activity
        final PasswordOnlyActivity activity2 = AutofillTestWatcher
                .getActivity(PasswordOnlyActivity.class);

        // Set expectations.
        final Bundle clientState2 = new Bundle();
        clientState2.putString("second", "two");
        clientState2.putString("last", "two");
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_PASSWORD)
                .setExtras(clientState2)
                .build());

        // Trigger autofill
        activity2.focusOnPassword();
        final FillRequest fillRequest2 = sReplier.getNextFillRequest();
        assertThat(fillRequest2.contexts.size()).isEqualTo(1);
        mUiBot.assertNoDatasetsEver();

        // Trigger save...
        activity2.setPassword("sweet");
        activity2.login();
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_PASSWORD);

        // ..and assert results
        final SaveRequest saveRequest2 = sReplier.getNextSaveRequest();
        assertThat(saveRequest2.data.getString("first")).isNull();
        assertThat(saveRequest2.data.getString("second")).isEqualTo("two");
        assertThat(saveRequest2.data.getString("last")).isEqualTo("two");
        assertTextAndValue(findNodeByResourceId(saveRequest2.structure, ID_PASSWORD), "sweet");
    }

    /**
     * Tests the new scenario introudced on Q where the service can set a multi-screen session,
     * with the service setting the client state just in the first request (so its passed to both
     * the second fill request and the save request.
     */
    @Test
    public void testSaveBothFieldsAtOnceNoClientStateOnSecondRequest() throws Exception {
        // Set service
        enableService();

        // First handle username...

        // Set expectations.
        final Bundle clientState1 = new Bundle();
        clientState1.putString("first", "one");
        clientState1.putString("last", "one");
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setSaveInfoFlags(SaveInfo.FLAG_DELAY_SAVE)
                .setExtras(clientState1)
                .build());

        // Trigger autofill
        mActivity.focusOnUsername();
        final FillRequest fillRequest1 = sReplier.getNextFillRequest();
        assertThat(fillRequest1.contexts.size()).isEqualTo(1);
        final ComponentName component1 = fillRequest1.structure.getActivityComponent();
        assertThat(component1).isEqualTo(mActivity.getComponentName());
        mUiBot.assertNoDatasetsEver();

        // Trigger what would be save...
        mActivity.setUsername("dude");
        mActivity.next();
        mUiBot.assertSaveNotShowing();

        // ...now rinse and repeat for password

        // Get the activity
        final PasswordOnlyActivity passwordActivity = AutofillTestWatcher
                .getActivity(PasswordOnlyActivity.class);

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME | SAVE_DATA_TYPE_PASSWORD,
                        ID_PASSWORD)
                .build());

        // Trigger autofill
        passwordActivity.focusOnPassword();
        final FillRequest fillRequest2 = sReplier.getNextFillRequest();
        assertThat(fillRequest2.contexts.size()).isEqualTo(2);
        // Client state should come from 1st request
        assertThat(fillRequest2.data.getString("first")).isEqualTo("one");
        assertThat(fillRequest2.data.getString("last")).isEqualTo("one");

        final ComponentName component2 = fillRequest2.structure.getActivityComponent();
        assertThat(component2).isEqualTo(passwordActivity.getComponentName());
        mUiBot.assertNoDatasetsEver();

        // Trigger save...
        passwordActivity.setPassword("sweet");
        passwordActivity.login();
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_USERNAME, SAVE_DATA_TYPE_PASSWORD);

        // ..and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        // Client state should come from 1st request
        assertThat(fillRequest2.data.getString("first")).isEqualTo("one");
        assertThat(fillRequest2.data.getString("last")).isEqualTo("one");

        assertThat(saveRequest.contexts.size()).isEqualTo(2);

        // Username is set in the 1st context
        final AssistStructure previousStructure = saveRequest.contexts.get(0).getStructure();
        assertWithMessage("no structure for 1st activity").that(previousStructure).isNotNull();
        assertTextAndValue(findNodeByResourceId(previousStructure, ID_USERNAME), "dude");
        final ComponentName componentPrevious = previousStructure.getActivityComponent();
        assertThat(componentPrevious).isEqualTo(mActivity.getComponentName());

        // Password is set in the 2nd context
        final AssistStructure currentStructure = saveRequest.contexts.get(1).getStructure();
        assertWithMessage("no structure for 2nd activity").that(currentStructure).isNotNull();
        assertTextAndValue(findNodeByResourceId(currentStructure, ID_PASSWORD), "sweet");
        final ComponentName componentCurrent = currentStructure.getActivityComponent();
        assertThat(componentCurrent).isEqualTo(passwordActivity.getComponentName());
    }

    /**
     * Tests the new scenario introudced on Q where the service can set a multi-screen session,
     * with the service setting the client state just on both requests (so the 1st client state is
     * passed to the 2nd request, and the 2nd client state is passed to the save request).
     */
    @Test
    public void testSaveBothFieldsAtOnceWithClientStateOnBothRequests() throws Exception {
        // Set service
        enableService();

        // First handle username...

        // Set expectations.
        final Bundle clientState1 = new Bundle();
        clientState1.putString("first", "one");
        clientState1.putString("last", "one");
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setSaveInfoFlags(SaveInfo.FLAG_DELAY_SAVE)
                .setExtras(clientState1)
                .build());

        // Trigger autofill
        mActivity.focusOnUsername();
        final FillRequest fillRequest1 = sReplier.getNextFillRequest();
        assertThat(fillRequest1.contexts.size()).isEqualTo(1);
        final ComponentName component1 = fillRequest1.structure.getActivityComponent();
        assertThat(component1).isEqualTo(mActivity.getComponentName());
        mUiBot.assertNoDatasetsEver();

        // Trigger what would be save...
        mActivity.setUsername("dude");
        mActivity.next();
        mUiBot.assertSaveNotShowing();

        // ...now rinse and repeat for password

        // Get the activity
        final PasswordOnlyActivity passwordActivity = AutofillTestWatcher
                .getActivity(PasswordOnlyActivity.class);

        // Set expectations.
        final Bundle clientState2 = new Bundle();
        clientState2.putString("second", "two");
        clientState2.putString("last", "two");
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME | SAVE_DATA_TYPE_PASSWORD,
                        ID_PASSWORD)
                .setExtras(clientState2)
                .build());

        // Trigger autofill
        passwordActivity.focusOnPassword();
        final FillRequest fillRequest2 = sReplier.getNextFillRequest();
        assertThat(fillRequest2.contexts.size()).isEqualTo(2);
        // Client state on 2nd request should come from previous (1st) request
        assertThat(fillRequest2.data.getString("first")).isEqualTo("one");
        assertThat(fillRequest2.data.getString("second")).isNull();
        assertThat(fillRequest2.data.getString("last")).isEqualTo("one");

        final ComponentName component2 = fillRequest2.structure.getActivityComponent();
        assertThat(component2).isEqualTo(passwordActivity.getComponentName());
        mUiBot.assertNoDatasetsEver();

        // Trigger save...
        passwordActivity.setPassword("sweet");
        passwordActivity.login();
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_USERNAME, SAVE_DATA_TYPE_PASSWORD);

        // ..and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        // Client state on save request should come from last (2nd) request
        assertThat(saveRequest.data.getString("first")).isNull();
        assertThat(saveRequest.data.getString("second")).isEqualTo("two");
        assertThat(saveRequest.data.getString("last")).isEqualTo("two");

        assertThat(saveRequest.contexts.size()).isEqualTo(2);

        // Username is set in the 1st context
        final AssistStructure previousStructure = saveRequest.contexts.get(0).getStructure();
        assertWithMessage("no structure for 1st activity").that(previousStructure).isNotNull();
        assertTextAndValue(findNodeByResourceId(previousStructure, ID_USERNAME), "dude");
        final ComponentName componentPrevious = previousStructure.getActivityComponent();
        assertThat(componentPrevious).isEqualTo(mActivity.getComponentName());

        // Password is set in the 2nd context
        final AssistStructure currentStructure = saveRequest.contexts.get(1).getStructure();
        assertWithMessage("no structure for 2nd activity").that(currentStructure).isNotNull();
        assertTextAndValue(findNodeByResourceId(currentStructure, ID_PASSWORD), "sweet");
        final ComponentName componentCurrent = currentStructure.getActivityComponent();
        assertThat(componentCurrent).isEqualTo(passwordActivity.getComponentName());
    }

    @Test
    public void testSaveBothFieldsCustomDescription_differentIds() throws Exception {
        saveBothFieldsCustomDescription(false);
    }

    @Test
    public void testSaveBothFieldsCustomDescription_sameIds() throws Exception {
        saveBothFieldsCustomDescription(true);
    }

    private void saveBothFieldsCustomDescription(boolean sameAutofillId) throws Exception {
        // Set service
        enableService();

        // Set ids
        final AutofillId appUsernameId = mActivity.getUsernameAutofillId();
        final AutofillId appPasswordId = sameAutofillId ? appUsernameId
                : mActivity.getAutofillManager().getNextAutofillId();
        mActivity.setPasswordAutofillId(appPasswordId);
        Log.d(TAG, "App: usernameId=" + appUsernameId + ", passwordId=" + appPasswordId);

        // First handle username...

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setSaveInfoFlags(SaveInfo.FLAG_DELAY_SAVE)
                .build());

        // Trigger autofill
        mActivity.focusOnUsername();
        final FillRequest fillRequest1 = sReplier.getNextFillRequest();
        assertThat(fillRequest1.contexts.size()).isEqualTo(1);
        final ComponentName component1 = fillRequest1.structure.getActivityComponent();
        assertThat(component1).isEqualTo(mActivity.getComponentName());
        mUiBot.assertNoDatasetsEver();

        // Trigger what would be save...
        mActivity.setUsername("dude");
        mActivity.next();
        mUiBot.assertSaveNotShowing();

        // ...now rinse and repeat for password

        // Get the activity
        final PasswordOnlyActivity passwordActivity = AutofillTestWatcher
                .getActivity(PasswordOnlyActivity.class);

        // Must get AutofillIds from FillRequest, as they contain the proper session ids
        final AutofillId svcUsernameId = findAutofillIdByResourceId(fillRequest1.contexts.get(0),
                ID_USERNAME);
        Log.d(TAG, "Service: usernameId=" + svcUsernameId);

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setVisitor((contexts, builder) -> {
                    final AutofillId svcPasswordId =
                            findAutofillIdByResourceId(contexts.get(1), ID_PASSWORD);
                    Log.d(TAG, "Service: passwordId=" + svcPasswordId);
                    final CharSequenceTransformation usernameTrans =
                            new CharSequenceTransformation.Builder(svcUsernameId, MATCH_ALL, "$1")
                            .build();
                    final CharSequenceTransformation passwordTrans =
                            new CharSequenceTransformation.Builder(svcPasswordId, MATCH_ALL, "$1")
                            .build();
                    builder.setSaveInfo(new SaveInfo.Builder(
                            SAVE_DATA_TYPE_USERNAME | SAVE_DATA_TYPE_PASSWORD,
                            new AutofillId[] {svcPasswordId})
                            .setCustomDescription(newCustomDescriptionWithUsernameAndPassword()
                                    .addChild(R.id.username, usernameTrans)
                                    .addChild(R.id.password, passwordTrans)
                                    .build())
                            .build());
                })
                .build());

        // Trigger autofill
        passwordActivity.focusOnPassword();
        final FillRequest fillRequest2 = sReplier.getNextFillRequest();
        assertThat(fillRequest2.contexts.size()).isEqualTo(2);

        final ComponentName component2 = fillRequest2.structure.getActivityComponent();
        assertThat(component2).isEqualTo(passwordActivity.getComponentName());
        mUiBot.assertNoDatasetsEver();

        // Trigger save...
        passwordActivity.setPassword("sweet");
        passwordActivity.login();

        // ...and assert UI
        final UiObject2 saveUi = mUiBot.assertSaveShowing(
                SaveInfo.NEGATIVE_BUTTON_STYLE_CANCEL, null, SAVE_DATA_TYPE_USERNAME,
                SAVE_DATA_TYPE_PASSWORD);

        mUiBot.assertChildText(saveUi, ID_USERNAME_LABEL, "User:");
        mUiBot.assertChildText(saveUi, ID_USERNAME, "dude");
        mUiBot.assertChildText(saveUi, ID_PASSWORD_LABEL, "Pass:");
        mUiBot.assertChildText(saveUi, ID_PASSWORD, "sweet");
    }

    // TODO(b/113281366): add test cases for more scenarios such as:
    // - make sure that activity not marked with keepAlive is not sent in the 2nd request
    // - somehow verify that the first activity's session is gone
    // - WebView
}
