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
import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.assertTextIsSanitized;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.Helper.getContext;
import static android.autofillservice.cts.Timeouts.MOCK_IME_TIMEOUT_MS;
import static android.autofillservice.cts.inline.InstrumentedAutoFillServiceInlineEnabled.SERVICE_NAME;

import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.app.PendingIntent;
import android.autofillservice.cts.CannedFillResponse;
import android.autofillservice.cts.DummyActivity;
import android.autofillservice.cts.Helper;
import android.autofillservice.cts.InstrumentedAutoFillService;
import android.autofillservice.cts.LoginActivityCommonTestCase;
import android.autofillservice.cts.NonAutofillableActivity;
import android.autofillservice.cts.UsernameOnlyActivity;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.FillContext;
import android.support.test.uiautomator.Direction;

import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.MockImeSession;

import org.junit.Test;
import org.junit.rules.TestRule;

public class InlineLoginActivityTest extends LoginActivityCommonTestCase {

    private static final String TAG = "InlineLoginActivityTest";

    @Override
    protected void enableService() {
        Helper.enableAutofillService(getContext(), SERVICE_NAME);
    }

    public InlineLoginActivityTest() {
        super(getInlineUiBot());
    }

    @Override
    protected boolean isInlineMode() {
        return true;
    }

    @Override
    public TestRule getMainTestRule() {
        return InlineUiBot.annotateRule(super.getMainTestRule());
    }

    @Test
    public void testAutofill_disjointDatasets() throws Exception {
        // Set service.
        enableService();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(createInlinePresentation("The Username"))
                        .build())
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Password"))
                        .setInlinePresentation(createInlinePresentation("The Password"))
                        .build())
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_PASSWORD, "lollipop")
                        .setPresentation(createPresentation("The Password2"))
                        .setInlinePresentation(createInlinePresentation("The Password2"))
                        .build());

        sReplier.addResponse(builder.build());
        mActivity.expectAutoFill("dude");

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Username");

        // Switch focus to password
        mUiBot.selectByRelativeId(ID_PASSWORD);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Password", "The Password2");

        // Switch focus back to username
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Username");
        mUiBot.selectDataset("The Username");
        mUiBot.waitForIdleSync();

        // Check the results.
        mActivity.assertAutoFilled();

        // Make sure input was sanitized.
        final InstrumentedAutoFillService.FillRequest request = sReplier.getNextFillRequest();
        assertWithMessage("CancelationSignal is null").that(request.cancellationSignal).isNotNull();
        assertTextIsSanitized(request.structure, ID_PASSWORD);
        final FillContext fillContext = request.contexts.get(request.contexts.size() - 1);
        assertThat(fillContext.getFocusedId())
                .isEqualTo(findAutofillIdByResourceId(fillContext, ID_USERNAME));

        // Make sure initial focus was properly set.
        assertWithMessage("Username node is not focused").that(
                findNodeByResourceId(request.structure, ID_USERNAME).isFocused()).isTrue();
        assertWithMessage("Password node is focused").that(
                findNodeByResourceId(request.structure, ID_PASSWORD).isFocused()).isFalse();
    }

    @Test
    public void testAutofill_SwitchToAutofillableActivity() throws Exception {
        assertAutofill_SwitchActivity(UsernameOnlyActivity.class, /* autofillable */ true);
    }

    @Test
    public void testAutofill_SwitchToNonAutofillableActivity() throws Exception {
        assertAutofill_SwitchActivity(NonAutofillableActivity.class, /* autofillable */ false);
    }

    private void assertAutofill_SwitchActivity(Class<?> clazz, boolean autofillable)
            throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setField(ID_PASSWORD, "password")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(createInlinePresentation("The Username"))
                        .build());
        sReplier.addResponse(builder.build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        sReplier.getNextFillRequest();
        // Make sure the suggestion is shown.
        mUiBot.assertDatasets("The Username");

        mUiBot.pressHome();
        mUiBot.waitForIdle();

        // Switch to another Activity
        startActivity(clazz);
        mUiBot.waitForIdle();

        // Trigger input method show.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        if (autofillable) {
            sReplier.addResponse(NO_RESPONSE);
            sReplier.getNextFillRequest();
        }
        // Make sure suggestion is not shown.
        mUiBot.assertNoDatasets();
    }

    protected final void startActivity(Class<?> clazz) {
        final Intent intent = new Intent(mContext, clazz);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    @Test
    public void testAutofill_selectDatasetThenHideInlineSuggestion() throws Exception {
        // Set service.
        enableService();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(createInlinePresentation("The Username"))
                        .build());

        sReplier.addResponse(builder.build());
        mActivity.expectAutoFill("dude");

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Username");

        mUiBot.selectDataset("The Username");
        mUiBot.waitForIdleSync();

        mUiBot.assertNoDatasets();

        // Make sure input was sanitized.
        final InstrumentedAutoFillService.FillRequest request = sReplier.getNextFillRequest();
        assertWithMessage("CancelationSignal is null").that(request.cancellationSignal).isNotNull();
        assertTextIsSanitized(request.structure, ID_PASSWORD);
        final FillContext fillContext = request.contexts.get(request.contexts.size() - 1);
        assertThat(fillContext.getFocusedId())
                .isEqualTo(findAutofillIdByResourceId(fillContext, ID_USERNAME));

        // Make sure initial focus was properly set.
        assertWithMessage("Username node is not focused").that(
                findNodeByResourceId(request.structure, ID_USERNAME).isFocused()).isTrue();
        assertWithMessage("Password node is focused").that(
                findNodeByResourceId(request.structure, ID_PASSWORD).isFocused()).isFalse();
    }

    @Test
    public void testLongClickAttribution() throws Exception {
        // Set service.
        enableService();

        Intent intent = new Intent(mContext, DummyActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(
                                createInlinePresentation("The Username", pendingIntent))
                        .build());

        sReplier.addResponse(builder.build());
        mActivity.expectAutoFill("dude");

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Username");

        // Long click on suggestion
        mUiBot.longPressSuggestion("The Username");
        mUiBot.waitForIdleSync();

        // Make sure the attribution showed worked
        mUiBot.selectByText("foo");

        // Go back to the filled app.
        mUiBot.pressBack();

        sReplier.getNextFillRequest();
        mUiBot.waitForIdleSync();
    }

    @Test
    public void testAutofill_noInvalid() throws Exception {
        final String keyInvalid = "invalid";
        final String keyValid = "valid";
        final String message = "Passes valid message to the remote service";
        final Bundle bundle = new Bundle();
        bundle.putBinder(keyInvalid, new Binder());
        bundle.putString(keyValid, message);

        // Set service.
        enableService();
        final MockImeSession mockImeSession = sMockImeSessionRule.getMockImeSession();
        assumeTrue("MockIME not available", mockImeSession != null);

        mockImeSession.callSetInlineSuggestionsExtras(bundle);

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(createInlinePresentation("The Username"))
                        .build());

        sReplier.addResponse(builder.build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Username");

        final InstrumentedAutoFillService.FillRequest request = sReplier.getNextFillRequest();
        final Bundle extras = request.inlineRequest.getExtras();
        assertThat(extras.get(keyInvalid)).isNull();
        assertThat(extras.getString(keyValid)).isEqualTo(message);

        final Bundle style = request.inlineRequest.getInlinePresentationSpecs().get(0).getStyle();
        assertThat(style.get(keyInvalid)).isNull();
        assertThat(style.getString(keyValid)).isEqualTo(message);

        final Bundle style2 = request.inlineRequest.getInlinePresentationSpecs().get(1).getStyle();
        assertThat(style2.get(keyInvalid)).isNull();
        assertThat(style2.getString(keyValid)).isEqualTo(message);
    }

    @Test
    @AppModeFull(reason = "WRITE_SECURE_SETTING permission can't be grant to instant apps")
    public void testSwitchInputMethod() throws Exception {
        // Set service
        enableService();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(createInlinePresentation("The Username"))
                        .build());

        sReplier.addResponse(builder.build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertDatasets("The Username");

        sReplier.getNextFillRequest();

        // Trigger IME switch event
        Helper.mockSwitchInputMethod(sContext);
        mUiBot.waitForIdleSync();

        final CannedFillResponse.Builder builder2 = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude2")
                        .setPresentation(createPresentation("The Username 2"))
                        .setInlinePresentation(createInlinePresentation("The Username 2"))
                        .build());

        sReplier.addResponse(builder2.build());

        // Trigger auto-fill
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        // Confirm new suggestion
        mUiBot.assertDatasets("The Username 2");

        // Confirm new fill request
        sReplier.getNextFillRequest();
    }

    @Test
    public void testScrollSuggestionView() throws Exception {
        // Set service.
        enableService();

        final int firstDataset = 1;
        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder();
        for (int i = firstDataset; i <= 20; i++) {
            builder.addDataset(new CannedFillResponse.CannedDataset.Builder()
                    .setField(ID_USERNAME, "dude" + i)
                    .setPresentation(createPresentation("Username" + i))
                    .setInlinePresentation(createInlinePresentation("Username" + i))
                    .build());
        }

        sReplier.addResponse(builder.build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();

        mUiBot.assertSuggestion("Username" + firstDataset);

        // Scroll the suggestion view
        mUiBot.scrollSuggestionView(Direction.RIGHT, /* speed */ 5000);
        mUiBot.waitForIdleSync();

        mUiBot.assertNoSuggestion("Username" + firstDataset);

        sReplier.getNextFillRequest();
        mUiBot.waitForIdleSync();
    }

    @Test
    public void testClickEventPassToIme() throws Exception {
        testTouchEventPassToIme(/* longPress */ false);
    }

    @Test
    public void testLongClickEventPassToIme() throws Exception {
        testTouchEventPassToIme(/* longPress */ true);
    }

    private void testTouchEventPassToIme(boolean longPress) throws Exception {
        final MockImeSession mockImeSession = sMockImeSessionRule.getMockImeSession();
        assumeTrue("MockIME not available", mockImeSession != null);

        // Set service.
        enableService();

        Intent intent = new Intent(mContext, DummyActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(longPress
                                ? createInlinePresentation("The Username", pendingIntent)
                                : createInlinePresentation("The Username"))
                        .build());

        sReplier.addResponse(builder.build());

        final ImeEventStream stream = mockImeSession.openEventStream();

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        sReplier.getNextFillRequest();

        mUiBot.assertDatasets("The Username");

        if (longPress) {
            // Long click on suggestion
            mUiBot.longPressSuggestion("The Username");

            expectEvent(stream,
                    event -> "onInlineSuggestionLongClickedEvent".equals(event.getEventName()),
                    MOCK_IME_TIMEOUT_MS);
        } else {
            // Click on suggestion
            mUiBot.selectDataset("The Username");

            expectEvent(stream,
                    event -> "onInlineSuggestionClickedEvent".equals(event.getEventName()),
                    MOCK_IME_TIMEOUT_MS);
        }
    }

    @Test
    public void testInlineSuggestionViewReleased() throws Exception {
        // Set service
        enableService();

        // Prepare the autofill response
        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setPresentation(createPresentation("The Username"))
                        .setInlinePresentation(createInlinePresentation("The Username"))
                        .build())
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Password"))
                        .setInlinePresentation(createInlinePresentation("The Password"))
                        .build())
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_PASSWORD, "lollipop")
                        .setPresentation(createPresentation("The Password2"))
                        .setInlinePresentation(createInlinePresentation("The Password2"))
                        .build());
        sReplier.addResponse(builder.build());

        // Trigger auto-fill on username field
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("The Username");
        Helper.assertActiveViewCountFromInlineSuggestionRenderService(1);

        // Switch focus to password
        mUiBot.selectByRelativeId(ID_PASSWORD);
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("The Password", "The Password2");
        Helper.assertActiveViewCountFromInlineSuggestionRenderService(2);

        // Switch focus back to username
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("The Username");
        Helper.assertActiveViewCountFromInlineSuggestionRenderService(1);

        // Select the autofill suggestion on username, then check the results
        mActivity.expectAutoFill("dude");
        mUiBot.selectDataset("The Username");
        mUiBot.waitForIdleSync();
        mActivity.assertAutoFilled();
        sReplier.getNextFillRequest();

        // Sleep for a while for the wait in {@link com.android.server.autofill.ui
        // .RemoteInlineSuggestionUi} to timeout.
        SystemClock.sleep(500);
        Helper.assertActiveViewCountFromInlineSuggestionRenderService(0);
    }
}
