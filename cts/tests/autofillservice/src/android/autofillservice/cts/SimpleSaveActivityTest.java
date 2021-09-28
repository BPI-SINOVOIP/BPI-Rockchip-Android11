/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.autofillservice.cts.AntiTrimmerTextWatcher.TRIMMER_PATTERN;
import static android.autofillservice.cts.Helper.ID_STATIC_TEXT;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.LARGE_STRING;
import static android.autofillservice.cts.Helper.assertTextAndValue;
import static android.autofillservice.cts.Helper.assertTextValue;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.autofillservice.cts.Helper.findNodeByResourceId;
import static android.autofillservice.cts.LoginActivity.ID_USERNAME_CONTAINER;
import static android.autofillservice.cts.SimpleSaveActivity.ID_COMMIT;
import static android.autofillservice.cts.SimpleSaveActivity.ID_INPUT;
import static android.autofillservice.cts.SimpleSaveActivity.ID_LABEL;
import static android.autofillservice.cts.SimpleSaveActivity.ID_PASSWORD;
import static android.autofillservice.cts.SimpleSaveActivity.TEXT_LABEL;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_GENERIC;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_USERNAME;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.app.assist.AssistStructure;
import android.app.assist.AssistStructure.ViewNode;
import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.autofillservice.cts.SimpleSaveActivity.FillExpectation;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.BatchUpdates;
import android.service.autofill.CustomDescription;
import android.service.autofill.FillContext;
import android.service.autofill.FillEventHistory;
import android.service.autofill.RegexValidator;
import android.service.autofill.SaveInfo;
import android.service.autofill.TextValueSanitizer;
import android.service.autofill.Validator;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiObject2;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.URLSpan;
import android.view.View;
import android.view.autofill.AutofillId;
import android.widget.RemoteViews;

import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

import java.util.regex.Pattern;

public class SimpleSaveActivityTest extends CustomDescriptionWithLinkTestCase<SimpleSaveActivity> {

    private static final AutofillActivityTestRule<SimpleSaveActivity> sActivityRule =
            new AutofillActivityTestRule<SimpleSaveActivity>(SimpleSaveActivity.class, false);

    private static final AutofillActivityTestRule<WelcomeActivity> sWelcomeActivityRule =
            new AutofillActivityTestRule<WelcomeActivity>(WelcomeActivity.class, false);

    public SimpleSaveActivityTest() {
        super(SimpleSaveActivity.class);
    }

    @Override
    protected AutofillActivityTestRule<SimpleSaveActivity> getActivityRule() {
        return sActivityRule;
    }

    @Override
    protected TestRule getMainTestRule() {
        return RuleChain.outerRule(sActivityRule).around(sWelcomeActivityRule);
    }

    private void restartActivity() {
        final Intent intent = new Intent(mContext.getApplicationContext(),
                SimpleSaveActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        mActivity.startActivity(intent);
    }

    @Test
    public void testAutoFillOneDatasetAndSave() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Select dataset.
        final FillExpectation autofillExpecation = mActivity.expectAutoFill("id", "pass");
        mUiBot.selectDataset("YO");
        autofillExpecation.assertAutoFilled();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("ID");
            mActivity.mPassword.setText("PASS");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertUpdateShowing(SAVE_DATA_TYPE_GENERIC);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "ID");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "PASS");
    }

    @Test
    @AppModeFull(reason = "testAutoFillOneDatasetAndSave() is enough")
    public void testAutoFillOneDatasetAndSave_largeAssistStructure() throws Exception {
        startActivity();

        mActivity.syncRunOnUiThread(
                () -> mActivity.mInput.setAutofillHints(LARGE_STRING, LARGE_STRING, LARGE_STRING));

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        final FillRequest fillRequest = sReplier.getNextFillRequest();
        final ViewNode inputOnFill = findNodeByResourceId(fillRequest.structure, ID_INPUT);
        final String[] hintsOnFill = inputOnFill.getAutofillHints();
        // Cannot compare these large strings directly becauise it could cause ANR
        assertThat(hintsOnFill).hasLength(3);
        Helper.assertEqualsToLargeString(hintsOnFill[0]);
        Helper.assertEqualsToLargeString(hintsOnFill[1]);
        Helper.assertEqualsToLargeString(hintsOnFill[2]);

        // Select dataset.
        final FillExpectation autofillExpecation = mActivity.expectAutoFill("id", "pass");
        mUiBot.selectDataset("YO");
        autofillExpecation.assertAutoFilled();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("ID");
            mActivity.mPassword.setText("PASS");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertUpdateShowing(SAVE_DATA_TYPE_GENERIC);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        final ViewNode inputOnSave = findNodeByResourceId(saveRequest.structure, ID_INPUT);
        assertTextAndValue(inputOnSave, "ID");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "PASS");

        final String[] hintsOnSave = inputOnSave.getAutofillHints();
        // Cannot compare these large strings directly becauise it could cause ANR
        assertThat(hintsOnSave).hasLength(3);
        Helper.assertEqualsToLargeString(hintsOnSave[0]);
        Helper.assertEqualsToLargeString(hintsOnSave[1]);
        Helper.assertEqualsToLargeString(hintsOnSave[2]);
    }

    /**
     * Simple test that only uses UiAutomator to interact with the activity, so it indirectly
     * tests the integration of Autofill with Accessibility.
     */
    @Test
    public void testAutoFillOneDatasetAndSave_usingUiAutomatorOnly() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mUiBot.assertShownByRelativeId(ID_INPUT).click();
        sReplier.getNextFillRequest();

        // Select dataset...
        mUiBot.selectDataset("YO");

        // ...and assert autofilled values.
        final UiObject2 input = mUiBot.assertShownByRelativeId(ID_INPUT);
        final UiObject2 password = mUiBot.assertShownByRelativeId(ID_PASSWORD);

        assertWithMessage("wrong value for 'input'").that(input.getText()).isEqualTo("id");
        // TODO: password field is shown as **** ; ideally we should assert it's a password
        // field, but UiAutomator does not exposes that info.
        final String visiblePassword = password.getText();
        assertWithMessage("'password' should not be visible").that(visiblePassword)
            .isNotEqualTo("pass");
        assertWithMessage("wrong value for 'password'").that(visiblePassword).hasLength(4);

        // Trigger save...
        input.setText("ID");
        password.setText("PASS");
        mUiBot.assertShownByRelativeId(ID_COMMIT).click();
        mUiBot.updateForAutofill(true, SAVE_DATA_TYPE_GENERIC);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "ID");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "PASS");
    }

    @Test
    @AppModeFull(reason = "testAutoFillOneDatasetAndSave() is enough")
    public void testSave() throws Exception {
        saveTest(false);
    }

    @Test
    public void testSave_afterRotation() throws Exception {
        assumeTrue("Rotation is supported", Helper.isRotationSupported(mContext));
        mUiBot.setScreenOrientation(UiBot.PORTRAIT);
        try {
            saveTest(true);
        } finally {
            try {
                mUiBot.setScreenOrientation(UiBot.PORTRAIT);
                cleanUpAfterScreenOrientationIsBackToPortrait();
            } catch (Exception e) {
                mSafeCleanerRule.add(e);
            }
        }
    }

    private void saveTest(boolean rotate) throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        UiObject2 saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        if (rotate) {
            // After the device rotates, the input field get focus and generate a new session.
            sReplier.addResponse(CannedFillResponse.NO_RESPONSE);

            mUiBot.setScreenOrientation(UiBot.LANDSCAPE);
            saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);
        }

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");
    }

    /**
     * Emulates an app dyanmically adding the password field after username is typed.
     */
    @Test
    @AppModeFull(reason = "testAutoFillOneDatasetAndSave() is enough")
    public void testPartitionedSave() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // 1st request

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Set 1st field but don't commit session
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.setText("108"));
        mUiBot.assertSaveNotShowing();

        // 2nd request

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_USERNAME | SAVE_DATA_TYPE_PASSWORD,
                        ID_INPUT, ID_PASSWORD)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mPassword.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mPassword.setText("42");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertSaveShowing(null, SAVE_DATA_TYPE_USERNAME,
                SAVE_DATA_TYPE_PASSWORD);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertThat(saveRequest.contexts.size()).isEqualTo(2);

        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "42");
    }

    /**
     * Emulates an app using fragments to display username and password in 2 steps.
     */
    @Test
    @AppModeFull(reason = "testAutoFillOneDatasetAndSave() is enough")
    public void testDelayedSave() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // 1st fragment.

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setSaveInfoFlags(SaveInfo.FLAG_DELAY_SAVE).build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger delayed save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        mUiBot.assertSaveNotShowing();

        // 2nd fragment.

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                // Must explicitly set visitor, otherwise setRequiredSavableIds() would get the
                // id from the 1st context
                .setVisitor((contexts, builder) -> {
                    final AutofillId passwordId =
                            findAutofillIdByResourceId(contexts.get(1), ID_PASSWORD);
                    final AutofillId inputId =
                            findAutofillIdByResourceId(contexts.get(0), ID_INPUT);
                    builder.setSaveInfo(new SaveInfo.Builder(
                            SAVE_DATA_TYPE_USERNAME | SAVE_DATA_TYPE_PASSWORD,
                            new AutofillId[] {inputId, passwordId})
                            .build());
                })
                .build());

        // Trigger autofill on second "fragment"
        mActivity.syncRunOnUiThread(() -> mActivity.mPassword.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger delayed save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mPassword.setText("42");
            mActivity.mCommit.performClick();
        });

        // Save it...
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_USERNAME, SAVE_DATA_TYPE_PASSWORD);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertThat(saveRequest.contexts.size()).isEqualTo(2);

        // Get username from 1st request.
        final AssistStructure structure1 = saveRequest.contexts.get(0).getStructure();
        assertTextAndValue(findNodeByResourceId(structure1, ID_INPUT), "108");

        // Get password from 2nd request.
        final AssistStructure structure2 = saveRequest.contexts.get(1).getStructure();
        assertTextAndValue(findNodeByResourceId(structure2, ID_INPUT), "108");
        assertTextAndValue(findNodeByResourceId(structure2, ID_PASSWORD), "42");
    }

    @Test
    public void testSave_launchIntent() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.setOnSave(WelcomeActivity.createSender(mContext, "Saved by the bell"))
                .addResponse(new CannedFillResponse.Builder()
                        .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                        .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();

            // Disable autofill so it's not triggered again after WelcomeActivity finishes
            // and mActivity is resumed (with focus on mInput) after the session is closed
            mActivity.mInput.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
        });

        // Save it...
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_GENERIC);
        sReplier.getNextSaveRequest();

        // ... and assert activity was launched
        WelcomeActivity.assertShowing(mUiBot, "Saved by the bell");
    }

    @Test
    public void testSaveThenStartNewSessionRightAwayShouldKeepSaveUi() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });

        // Make sure Save UI for 1st session was shown....
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Start new Activity to have a new autofill session
        startActivityOnNewTask(LoginActivity.class);

        // Make sure LoginActivity started...
        mUiBot.assertShownByRelativeId(ID_USERNAME_CONTAINER);

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_USERNAME)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "id")
                        .setField(ID_PASSWORD, "pwd")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());
        // Trigger fill request on the LoginActivity
        final LoginActivity act = LoginActivity.getCurrentActivity();
        act.syncRunOnUiThread(() -> act.forceAutofillOnUsername());
        sReplier.getNextFillRequest();

        // Make sure Fill UI is not shown. And Save UI for 1st session was still shown.
        mUiBot.assertNoDatasetsEver();
        sReplier.assertNoUnhandledFillRequests();
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        mUiBot.waitForIdle();
        // Trigger dismiss Save UI
        mUiBot.pressBack();

        // Make sure Save UI was not shown....
        mUiBot.assertSaveNotShowing();
        // Make sure Fill UI is shown.
        mUiBot.assertDatasets("YO");
    }

    @Test
    public void testCloseSaveUiThenStartNewSessionRightAway() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });

        // Make sure Save UI for 1st session was shown....
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Trigger dismiss Save UI
        mUiBot.pressBack();

        // Make sure Save UI for 1st session was canceled.
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        // ...then start the new session right away (without finishing the activity).
        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("");
            mActivity.getAutofillManager().requestAutofill(mActivity.mInput);
        });
        sReplier.getNextFillRequest();

        // Make sure Fill UI is shown.
        mUiBot.assertDatasets("YO");
    }

    @Test
    public void testSaveWithParcelableOnClientState() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        final AutofillId id = new AutofillId(42);
        final Bundle clientState = new Bundle();
        clientState.putParcelable("id", id);
        clientState.putParcelable("my_id", new MyAutofillId(id));
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setExtras(clientState)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        UiObject2 saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertMyClientState(saveRequest.data);

        // Also check fillevent history
        final FillEventHistory history = InstrumentedAutoFillService.getFillEventHistory(1);
        @SuppressWarnings("deprecation")
        final Bundle deprecatedState = history.getClientState();
        assertMyClientState(deprecatedState);
        assertMyClientState(history.getEvents().get(0).getClientState());
    }

    private void assertMyClientState(Bundle data) {
        // Must set proper classpath before reading the data, otherwise Bundle will use it's
        // on class classloader, which is the framework's.
        data.setClassLoader(getClass().getClassLoader());

        final AutofillId expectedId = new AutofillId(42);
        final AutofillId actualId = data.getParcelable("id");
        assertThat(actualId).isEqualTo(expectedId);
        final MyAutofillId actualMyId = data.getParcelable("my_id");
        assertThat(actualMyId).isEqualTo(new MyAutofillId(expectedId));
    }

    @Test
    public void testCancelPreventsSaveUiFromShowing() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Cancel session.
        mActivity.getAutofillManager().cancel();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });

        // Assert it's not showing.
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    public void testDismissSave_byTappingBack() throws Exception {
        startActivity();
        dismissSaveTest(DismissType.BACK_BUTTON);
    }

    @Test
    public void testDismissSave_byTappingHome() throws Exception {
        startActivity();
        dismissSaveTest(DismissType.HOME_BUTTON);
    }

    @Test
    public void testDismissSave_byTouchingOutside() throws Exception {
        startActivity();
        dismissSaveTest(DismissType.TOUCH_OUTSIDE);
    }

    @Test
    public void testDismissSave_byFocusingOutside() throws Exception {
        startActivity();
        dismissSaveTest(DismissType.FOCUS_OUTSIDE);
    }

    private void dismissSaveTest(DismissType dismissType) throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Then make sure it goes away when user doesn't want it..
        switch (dismissType) {
            case BACK_BUTTON:
                mUiBot.pressBack();
                break;
            case HOME_BUTTON:
                mUiBot.pressHome();
                break;
            case TOUCH_OUTSIDE:
                mUiBot.assertShownByText(TEXT_LABEL).click();
                break;
            case FOCUS_OUTSIDE:
                mActivity.syncRunOnUiThread(() -> mActivity.mLabel.requestFocus());
                mUiBot.assertShownByText(TEXT_LABEL).click();
                break;
            default:
                throw new IllegalArgumentException("invalid dismiss type: " + dismissType);
        }
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    public void testTapHomeWhileDatasetPickerUiIsShowing() throws Exception {
        startActivity();
        enableService();
        final MyAutofillCallback callback = mActivity.registerCallback();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mUiBot.assertShownByRelativeId(ID_INPUT).click();
        sReplier.getNextFillRequest();
        mUiBot.assertDatasets("YO");
        callback.assertUiShownEvent(mActivity.mInput);

        // Go home, you are drunk!
        mUiBot.pressHome();
        mUiBot.assertNoDatasets();
        callback.assertUiHiddenEvent(mActivity.mInput);

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO2"))
                        .build())
                .build());

        // Switch back to the activity.
        restartActivity();
        mUiBot.assertShownByText(TEXT_LABEL, Timeouts.ACTIVITY_RESURRECTION);
        sReplier.getNextFillRequest();
        final UiObject2 datasetPicker = mUiBot.assertDatasets("YO2");
        callback.assertUiShownEvent(mActivity.mInput);

        // Now autofill it.
        final FillExpectation autofillExpecation = mActivity.expectAutoFill("id", "pass");
        mUiBot.selectDataset(datasetPicker, "YO2");
        autofillExpecation.assertAutoFilled();
    }

    @Test
    public void testTapHomeWhileSaveUiIsShowing() throws Exception {
        startActivity();
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasetsEver();

        // Trigger save, but don't tap it.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Go home, you are drunk!
        mUiBot.pressHome();
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        // Prepare the response for the next session, which will be automatically triggered
        // when the activity is brought back.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Switch back to the activity.
        restartActivity();
        mUiBot.assertShownByText(TEXT_LABEL, Timeouts.ACTIVITY_RESURRECTION);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasetsEver();

        // Trigger and select UI.
        mActivity.syncRunOnUiThread(() -> mActivity.mPassword.requestFocus());
        final FillExpectation autofillExpecation = mActivity.expectAutoFill("id", "pass");
        mUiBot.selectDataset("YO");

        // Assert it.
        autofillExpecation.assertAutoFilled();
    }

    @Override
    protected void saveUiRestoredAfterTappingLinkTest(PostSaveLinkTappedAction type)
            throws Exception {
        startActivity();
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setSaveInfoVisitor((contexts, builder) -> builder
                        .setCustomDescription(newCustomDescription(WelcomeActivity.class)))
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC);

        // Tap the link.
        tapSaveUiLink(saveUi);

        // Make sure new activity is shown...
        WelcomeActivity.assertShowingDefaultMessage(mUiBot);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        // .. then do something to return to previous activity...
        switch (type) {
            case ROTATE_THEN_TAP_BACK_BUTTON:
                // After the device rotates, the input field get focus and generate a new session.
                sReplier.addResponse(CannedFillResponse.NO_RESPONSE);

                mUiBot.setScreenOrientation(UiBot.LANDSCAPE);
                WelcomeActivity.assertShowingDefaultMessage(mUiBot);
                // not breaking on purpose
            case TAP_BACK_BUTTON:
                // ..then go back and save it.
                mUiBot.pressBack();
                break;
            case FINISH_ACTIVITY:
                // ..then finishes it.
                WelcomeActivity.finishIt();
                break;
            default:
                throw new IllegalArgumentException("invalid type: " + type);
        }
        // Make sure previous activity is back...
        mUiBot.assertShownByRelativeId(ID_INPUT);

        // ... and tap save.
        final UiObject2 newSaveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC);
        mUiBot.saveForAutofill(newSaveUi, true);

        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");
    }

    @Override
    protected void cleanUpAfterScreenOrientationIsBackToPortrait() throws Exception {
        sReplier.getNextFillRequest();
    }

    @Override
    protected void tapLinkThenTapBackThenStartOverTest(PostSaveLinkTappedAction action,
            boolean manualRequest) throws Exception {
        startActivity();
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setSaveInfoVisitor((contexts, builder) -> builder
                        .setCustomDescription(newCustomDescription(WelcomeActivity.class)))
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC);

        // Tap the link.
        tapSaveUiLink(saveUi);

        // Make sure new activity is shown.
        WelcomeActivity.assertShowingDefaultMessage(mUiBot);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        // Tap back to restore the Save UI...
        mUiBot.pressBack();
        // Make sure previous activity is back...
        mUiBot.assertShownByRelativeId(ID_LABEL);

        // ...but don't tap it...
        final UiObject2 saveUi2 = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // ...instead, do something to dismiss it:
        switch (action) {
            case TOUCH_OUTSIDE:
                mUiBot.assertShownByRelativeId(ID_LABEL).longClick();
                break;
            case TAP_NO_ON_SAVE_UI:
                mUiBot.saveForAutofill(saveUi2, false);
                break;
            case TAP_YES_ON_SAVE_UI:
                mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_GENERIC);
                final SaveRequest saveRequest = sReplier.getNextSaveRequest();
                assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");
                break;
            default:
                throw new IllegalArgumentException("invalid action: " + action);
        }
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        // Now triggers a new session and do business as usual...
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        if (manualRequest) {
            mActivity.getAutofillManager().requestAutofill(mActivity.mInput);
        } else {
            mActivity.syncRunOnUiThread(() -> mActivity.mPassword.requestFocus());
        }

        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("42");
            mActivity.mCommit.performClick();
        });

        // Save it...
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_GENERIC);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "42");
    }

    @Override
    protected void saveUiCancelledAfterTappingLinkTest(PostSaveLinkTappedAction type)
            throws Exception {
        startActivity(false);
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setSaveInfoVisitor((contexts, builder) -> builder
                        .setCustomDescription(newCustomDescription(WelcomeActivity.class)))
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC);

        // Tap the link.
        tapSaveUiLink(saveUi);
        // Make sure new activity is shown...
        WelcomeActivity.assertShowingDefaultMessage(mUiBot);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        switch (type) {
            case LAUNCH_PREVIOUS_ACTIVITY:
                startActivityOnNewTask(SimpleSaveActivity.class);
                break;
            case LAUNCH_NEW_ACTIVITY:
                // Launch a 3rd activity...
                startActivityOnNewTask(LoginActivity.class);
                mUiBot.assertShownByRelativeId(ID_USERNAME_CONTAINER);
                // ...then go back
                mUiBot.pressBack();
                break;
            default:
                throw new IllegalArgumentException("invalid type: " + type);
        }
        // Make sure right activity is showing
        mUiBot.assertShownByRelativeId(ID_INPUT, Timeouts.ACTIVITY_RESURRECTION);

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    @AppModeFull(reason = "Service-specific test")
    public void testSelectedDatasetsAreSentOnSaveRequest() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                // Added on reversed order on purpose
                .addDataset(new CannedDataset.Builder()
                        .setId("D2")
                        .setField(ID_INPUT, "id again")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("D2"))
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setId("D1")
                        .setField(ID_INPUT, "id")
                        .setPresentation(createPresentation("D1"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Select 1st dataset.
        final FillExpectation autofillExpecation1 = mActivity.expectAutoFill("id");
        final UiObject2 picker1 = mUiBot.assertDatasets("D2", "D1");
        mUiBot.selectDataset(picker1, "D1");
        autofillExpecation1.assertAutoFilled();

        // Select 2nd dataset.
        mActivity.syncRunOnUiThread(() -> mActivity.mPassword.requestFocus());
        final FillExpectation autofillExpecation2 = mActivity.expectAutoFill("id again", "pass");
        final UiObject2 picker2 = mUiBot.assertDatasets("D2");
        mUiBot.selectDataset(picker2, "D2");
        autofillExpecation2.assertAutoFilled();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("ID");
            mActivity.mPassword.setText("PASS");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = mUiBot.assertUpdateShowing(SAVE_DATA_TYPE_GENERIC);

        // Save it...
        mUiBot.saveForAutofill(saveUi, true);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "ID");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "PASS");
        assertThat(saveRequest.datasetIds).containsExactly("D1", "D2").inOrder();
    }

    @Override
    protected void tapLinkLaunchTrampolineActivityThenTapBackAndStartNewSessionTest()
            throws Exception {
        // Prepare activity.
        startActivity();
        mActivity.mInput.getRootView()
                .setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setSaveInfoVisitor((contexts, builder) -> builder
                        .setCustomDescription(
                                newCustomDescription(TrampolineWelcomeActivity.class)))
                .build());

        // Trigger autofill.
        mActivity.getAutofillManager().requestAutofill(mActivity.mInput);
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC);

        // Tap the link.
        tapSaveUiLink(saveUi);

        // Make sure new activity is shown...
        WelcomeActivity.assertShowingDefaultMessage(mUiBot);

        // Save UI should be showing as well, since Trampoline finished.
        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Dismiss Save Dialog
        mUiBot.pressBack();
        // Go back and make sure it's showing the right activity.
        mUiBot.pressBack();
        mUiBot.assertShownByRelativeId(ID_LABEL);

        // Now start a new session.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_PASSWORD)
                .build());
        mActivity.getAutofillManager().requestAutofill(mActivity.mPassword);
        sReplier.getNextFillRequest();
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mPassword.setText("42");
            mActivity.mCommit.performClick();
        });
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_PASSWORD);
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "42");
    }

    @Test
    public void testSanitizeOnSaveWhenAppChangeValues() throws Exception {
        startActivity();

        // Set listeners that will change the saved value
        new AntiTrimmerTextWatcher(mActivity.mInput);
        new AntiTrimmerTextWatcher(mActivity.mPassword);

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId inputId = findAutofillIdByResourceId(context, ID_INPUT);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);
                    builder.addSanitizer(new TextValueSanitizer(TRIMMER_PATTERN, "$1"), inputId,
                            passwordId);
                })
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("id");
            mActivity.mPassword.setText("pass");
            mActivity.mCommit.performClick();
        });

        // Save it...
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_GENERIC);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "id");
        assertTextValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "pass");
    }

    @Test
    @AppModeFull(reason = "testSanitizeOnSaveWhenAppChangeValues() is enough")
    public void testSanitizeOnSaveNoChange() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setOptionalSavableIds(ID_PASSWORD)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId inputId = findAutofillIdByResourceId(context, ID_INPUT);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);
                    builder.addSanitizer(new TextValueSanitizer(TRIMMER_PATTERN, "$1"), inputId,
                            passwordId);
                })
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();
        mUiBot.assertNoDatasetsEver();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("#id#");
            mActivity.mPassword.setText("#pass#");
            mActivity.mCommit.performClick();
        });

        // Save it...
        mUiBot.saveForAutofill(true, SAVE_DATA_TYPE_GENERIC);

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "id");
        assertTextValue(findNodeByResourceId(saveRequest.structure, ID_PASSWORD), "pass");
    }

    @Test
    @AppModeFull(reason = "testSanitizeOnSaveWhenAppChangeValues() is enough")
    public void testDontSaveWhenSanitizedValueForRequiredFieldDidntChange() throws Exception {
        startActivity();

        // Set listeners that will change the saved value
        new AntiTrimmerTextWatcher(mActivity.mInput);
        new AntiTrimmerTextWatcher(mActivity.mPassword);

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId inputId = findAutofillIdByResourceId(context, ID_INPUT);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);
                    builder.addSanitizer(new TextValueSanitizer(TRIMMER_PATTERN, "$1"), inputId,
                            passwordId);
                })
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("id");
            mActivity.mPassword.setText("pass");
            mActivity.mCommit.performClick();
        });

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    @AppModeFull(reason = "testSanitizeOnSaveWhenAppChangeValues() is enough")
    public void testDontSaveWhenSanitizedValueForOptionalFieldDidntChange() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setOptionalSavableIds(ID_PASSWORD)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);
                    builder.addSanitizer(new TextValueSanitizer(Pattern.compile("(pass) "), "$1"),
                            passwordId);
                })
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "pass")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("id");
            mActivity.mPassword.setText("#pass#");
            mActivity.mCommit.performClick();
        });

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    @AppModeFull(reason = "testSanitizeOnSaveWhenAppChangeValues() is enough")
    public void testDontSaveWhenRequiredFieldFailedSanitization() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT, ID_PASSWORD)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId inputId = findAutofillIdByResourceId(context, ID_INPUT);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);
                    builder.addSanitizer(new TextValueSanitizer(Pattern.compile("dude"), "$1"),
                            inputId, passwordId);
                })
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "#id#")
                        .setField(ID_PASSWORD, "#pass#")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("id");
            mActivity.mPassword.setText("pass");
            mActivity.mCommit.performClick();
        });

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    @AppModeFull(reason = "testSanitizeOnSaveWhenAppChangeValues() is enough")
    public void testDontSaveWhenOptionalFieldFailedSanitization() throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setOptionalSavableIds(ID_PASSWORD)
                .setSaveInfoVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    final AutofillId inputId = findAutofillIdByResourceId(context, ID_INPUT);
                    final AutofillId passwordId = findAutofillIdByResourceId(context, ID_PASSWORD);
                    builder.addSanitizer(new TextValueSanitizer(Pattern.compile("dude"), "$1"),
                            inputId, passwordId);

                })
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "id")
                        .setField(ID_PASSWORD, "#pass#")
                        .setPresentation(createPresentation("YO"))
                        .build())
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("id");
            mActivity.mPassword.setText("pass");
            mActivity.mCommit.performClick();
        });

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    @Test
    @AppModeFull(reason = "testSanitizeOnSaveWhenAppChangeValues() is enough")
    public void testDontSaveWhenInitialValueAndNoUserInputAndServiceDatasets() throws Throwable {
        // Prepare activitiy.
        startActivity();
        mActivity.syncRunOnUiThread(() -> {
            // NOTE: input's value must be a subset of the dataset value, otherwise the dataset
            // picker is filtered out
            mActivity.mInput.setText("f");
            mActivity.mPassword.setText("b");
        });

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_INPUT, "foo")
                        .setField(ID_PASSWORD, "bar")
                        .setPresentation(createPresentation("The Dude"))
                        .build())
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD, ID_INPUT, ID_PASSWORD).build());

        // Trigger auto-fill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();
        mUiBot.assertDatasets("The Dude");

        // Trigger save.
        mActivity.getAutofillManager().commit();

        // Assert it's not showing.
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_PASSWORD);
    }

    enum SetTextCondition {
        NORMAL,
        HAS_SESSION,
        EMPTY_TEXT,
        FOCUSED,
        NOT_IMPORTANT_FOR_AUTOFILL,
        INVISIBLE
    }

    /**
     * Tests scenario when a text field's text is set automatically, it should trigger autofill and
     * show Save UI.
     */
    @Test
    public void testShowSaveUiWhenSetTextAutomatically() throws Exception {
        triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition.NORMAL);
    }

    /**
     * Tests scenario when a text field's text is set automatically, it should not trigger autofill
     * when there is an existing session.
     */
    @Test
    public void testNotTriggerAutofillWhenSetTextWhileSessionExists() throws Exception {
        triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition.HAS_SESSION);
    }

    /**
     * Tests scenario when a text field's text is set automatically, it should not trigger autofill
     * when the text is empty.
     */
    @Test
    public void testNotTriggerAutofillWhenSetTextWhileEmptyText() throws Exception {
        triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition.EMPTY_TEXT);
    }

    /**
     * Tests scenario when a text field's text is set automatically, it should not trigger autofill
     * when the field is focused.
     */
    @Test
    public void testNotTriggerAutofillWhenSetTextWhileFocused() throws Exception {
        triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition.FOCUSED);
    }

    /**
     * Tests scenario when a text field's text is set automatically, it should not trigger autofill
     * when the field is not important for autofill.
     */
    @Test
    public void testNotTriggerAutofillWhenSetTextWhileNotImportantForAutofill() throws Exception {
        triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition.NOT_IMPORTANT_FOR_AUTOFILL);
    }

    /**
     * Tests scenario when a text field's text is set automatically, it should not trigger autofill
     * when the field is not visible.
     */
    @Test
    public void testNotTriggerAutofillWhenSetTextWhileInvisible() throws Exception {
        triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition.INVISIBLE);
    }

    private void triggerAutofillWhenSetTextAutomaticallyTest(SetTextCondition condition)
            throws Exception {
        startActivity();

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        CharSequence inputText = "108";

        switch (condition) {
            case NORMAL:
                // Nothing.
                break;
            case HAS_SESSION:
                mActivity.syncRunOnUiThread(() -> {
                    mActivity.mInput.setText("100");
                });
                sReplier.getNextFillRequest();
                break;
            case EMPTY_TEXT:
                inputText = "";
                break;
            case FOCUSED:
                mActivity.syncRunOnUiThread(() -> {
                    mActivity.mInput.requestFocus();
                });
                sReplier.getNextFillRequest();
                break;
            case NOT_IMPORTANT_FOR_AUTOFILL:
                mActivity.syncRunOnUiThread(() -> {
                    mActivity.mInput.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
                });
                break;
            case INVISIBLE:
                mActivity.syncRunOnUiThread(() -> {
                    mActivity.mInput.setVisibility(View.INVISIBLE);
                });
                break;
            default:
                throw new IllegalArgumentException("invalid condition: " + condition);
        }

        // Trigger autofill by setting text.
        final CharSequence text = inputText;
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText(text);
        });

        if (condition == SetTextCondition.NORMAL) {
            sReplier.getNextFillRequest();

            mActivity.syncRunOnUiThread(() -> {
                mActivity.mInput.setText("100");
                mActivity.mCommit.performClick();
            });

            mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);
        } else {
            sReplier.assertOnFillRequestNotCalled();
        }
    }

    @Test
    public void testExplicitlySaveButton() throws Exception {
        explicitlySaveButtonTest(false, 0);
    }

    @Test
    public void testExplicitlySaveButtonWhenAppClearFields() throws Exception {
        explicitlySaveButtonTest(true, 0);
    }

    @Test
    public void testExplicitlySaveButtonOnly() throws Exception {
        explicitlySaveButtonTest(false, SaveInfo.FLAG_DONT_SAVE_ON_FINISH);
    }

    /**
     * Tests scenario where service explicitly indicates which button is used to save.
     */
    private void explicitlySaveButtonTest(boolean clearFieldsOnSubmit, int flags) throws Exception {
        final boolean testBitmap = false;
        startActivity();
        mActivity.setAutoCommit(false);
        mActivity.setClearFieldsOnSubmit(clearFieldsOnSubmit);

        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .setSaveTriggerId(mActivity.mCommit.getAutofillId())
                .setSaveInfoFlags(flags)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.setText("108"));

        // Take a screenshot to make sure button doesn't disappear.
        final String commitBefore = mUiBot.assertShownByRelativeId(ID_COMMIT).getText();
        assertThat(commitBefore.toUpperCase()).isEqualTo("COMMIT");
        // Disable unnecessary screenshot tests as takeScreenshot() fails on some device.

        final Bitmap screenshotBefore = testBitmap ? mActivity.takeScreenshot(mActivity.mCommit)
                : null;

        // Save it...
        mActivity.syncRunOnUiThread(() -> mActivity.mCommit.performClick());
        final UiObject2 saveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);
        mUiBot.saveForAutofill(saveUi, true);

        // Make sure save button is showning (it was removed on earlier versions of the feature)
        final String commitAfter = mUiBot.assertShownByRelativeId(ID_COMMIT).getText();
        assertThat(commitAfter.toUpperCase()).isEqualTo("COMMIT");
        final Bitmap screenshotAfter = testBitmap ? mActivity.takeScreenshot(mActivity.mCommit)
                : null;

        // ... and assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");

        if (testBitmap) {
            Helper.assertBitmapsAreSame("commit-button", screenshotBefore, screenshotAfter);
        }
    }

    @Override
    protected void tapLinkAfterUpdateAppliedTest(boolean updateLinkView) throws Exception {
        startActivity();
        // Set service.
        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setSaveInfoVisitor((contexts, builder) -> {
                    // Set response with custom description
                    final AutofillId id = findAutofillIdByResourceId(contexts.get(0), ID_INPUT);
                    final CustomDescription.Builder customDescription =
                            newCustomDescriptionBuilder(WelcomeActivity.class);
                    final RemoteViews update = newTemplate();
                    if (updateLinkView) {
                        update.setCharSequence(R.id.link, "setText", "TAP ME IF YOU CAN");
                    } else {
                        update.setCharSequence(R.id.static_text, "setText", "ME!");
                    }
                    Validator validCondition = new RegexValidator(id, Pattern.compile(".*"));
                    customDescription.batchUpdate(validCondition,
                            new BatchUpdates.Builder().updateTemplate(update).build());

                    builder.setCustomDescription(customDescription.build());
                })
                .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                .build());

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();
        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        final UiObject2 saveUi;
        if (updateLinkView) {
            saveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC, "TAP ME IF YOU CAN");
        } else {
            saveUi = assertSaveUiWithLinkIsShown(SAVE_DATA_TYPE_GENERIC);
            final UiObject2 changed = saveUi.findObject(By.res(mPackageName, ID_STATIC_TEXT));
            assertThat(changed.getText()).isEqualTo("ME!");
        }

        // Tap the link.
        tapSaveUiLink(saveUi);

        // Make sure new activity is shown...
        WelcomeActivity.assertShowingDefaultMessage(mUiBot);
        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);
    }

    enum DescriptionType {
        SUCCINCT,
        CUSTOM,
    }

    /**
     * Tests scenarios when user taps a span in the custom description, then the new activity
     * finishes:
     * the Save UI should have been restored.
     */
    @Test
    @AppModeFull(reason = "No real use case for instant mode af service")
    public void testTapUrlSpanOnCustomDescription_thenTapBack() throws Exception {
        saveUiRestoredAfterTappingSpanTest(DescriptionType.CUSTOM,
                ViewActionActivity.ActivityCustomAction.NORMAL_ACTIVITY);
    }

    /**
     * Tests scenarios when user taps a span in the succinct description, then the new activity
     * finishes:
     * the Save UI should have been restored.
     */
    @Test
    @AppModeFull(reason = "No real use case for instant mode af service")
    public void testTapUrlSpanOnSuccinctDescription_thenTapBack() throws Exception {
        saveUiRestoredAfterTappingSpanTest(DescriptionType.SUCCINCT,
                ViewActionActivity.ActivityCustomAction.NORMAL_ACTIVITY);
    }

    /**
     * Tests scenarios when user taps a span in the custom description, then the new activity
     * starts an another activity then it finishes:
     * the Save UI should have been restored.
     */
    @Test
    @AppModeFull(reason = "No real use case for instant mode af service")
    public void testTapUrlSpanOnCustomDescription_forwardAnotherActivityThenTapBack()
            throws Exception {
        saveUiRestoredAfterTappingSpanTest(DescriptionType.CUSTOM,
                ViewActionActivity.ActivityCustomAction.FAST_FORWARD_ANOTHER_ACTIVITY);
    }

    /**
     * Tests scenarios when user taps a span in the succinct description, then the new activity
     * starts an another activity then it finishes:
     * the Save UI should have been restored.
     */
    @Test
    @AppModeFull(reason = "No real use case for instant mode af service")
    public void testTapUrlSpanOnSuccinctDescription_forwardAnotherActivityThenTapBack()
            throws Exception {
        saveUiRestoredAfterTappingSpanTest(DescriptionType.SUCCINCT,
                ViewActionActivity.ActivityCustomAction.FAST_FORWARD_ANOTHER_ACTIVITY);
    }

    /**
     * Tests scenarios when user taps a span in the custom description, then the new activity
     * stops but does not finish:
     * the Save UI should have been restored.
     */
    @Test
    @AppModeFull(reason = "No real use case for instant mode af service")
    public void testTapUrlSpanOnCustomDescription_tapBackWithoutFinish() throws Exception {
        saveUiRestoredAfterTappingSpanTest(DescriptionType.CUSTOM,
                ViewActionActivity.ActivityCustomAction.TAP_BACK_WITHOUT_FINISH);
    }

    /**
     * Tests scenarios when user taps a span in the succinct description, then the new activity
     * stops but does not finish:
     * the Save UI should have been restored.
     */
    @Test
    @AppModeFull(reason = "No real use case for instant mode af service")
    public void testTapUrlSpanOnSuccinctDescription_tapBackWithoutFinish() throws Exception {
        saveUiRestoredAfterTappingSpanTest(DescriptionType.SUCCINCT,
                ViewActionActivity.ActivityCustomAction.TAP_BACK_WITHOUT_FINISH);
    }

    private void saveUiRestoredAfterTappingSpanTest(
            DescriptionType type, ViewActionActivity.ActivityCustomAction action) throws Exception {
        startActivity();
        // Set service.
        enableService();

        switch (type) {
            case SUCCINCT:
                // Set expectations with custom description.
                sReplier.addResponse(new CannedFillResponse.Builder()
                        .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                        .setSaveDescription(newDescriptionWithUrlSpan(action.toString()))
                        .build());
                break;
            case CUSTOM:
                // Set expectations with custom description.
                sReplier.addResponse(new CannedFillResponse.Builder()
                        .setRequiredSavableIds(SAVE_DATA_TYPE_GENERIC, ID_INPUT)
                        .setSaveInfoVisitor((contexts, builder) -> builder
                                .setCustomDescription(
                                        newCustomDescriptionWithUrlSpan(action.toString())))
                        .build());
                break;
            default:
                throw new IllegalArgumentException("invalid type: " + type);
        }

        // Trigger autofill.
        mActivity.syncRunOnUiThread(() -> mActivity.mInput.requestFocus());
        sReplier.getNextFillRequest();

        // Trigger save.
        mActivity.syncRunOnUiThread(() -> {
            mActivity.mInput.setText("108");
            mActivity.mCommit.performClick();
        });
        // Waits for the commit be processed
        mUiBot.waitForIdle();

        mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);

        // Tapping URLSpan.
        final URLSpan span = mUiBot.findFirstUrlSpanWithText("Here is URLSpan");
        mActivity.syncRunOnUiThread(() -> span.onClick(/* unused= */ null));
        // Waits for the save UI hided
        mUiBot.waitForIdle();

        mUiBot.assertSaveNotShowing(SAVE_DATA_TYPE_GENERIC);

        // .. check activity show up as expected
        switch (action) {
            case FAST_FORWARD_ANOTHER_ACTIVITY:
                // Show up second activity.
                SecondActivity.assertShowingDefaultMessage(mUiBot);
                break;
            case NORMAL_ACTIVITY:
            case TAP_BACK_WITHOUT_FINISH:
                // Show up view action handle activity.
                ViewActionActivity.assertShowingDefaultMessage(mUiBot);
                break;
            default:
                throw new IllegalArgumentException("invalid action: " + action);
        }

        // ..then go back and save it.
        mUiBot.pressBack();
        // Waits for all UI processes to complete
        mUiBot.waitForIdle();

        // Make sure previous activity is back...
        mUiBot.assertShownByRelativeId(ID_INPUT);

        // ... and tap save.
        final UiObject2 newSaveUi = mUiBot.assertSaveShowing(SAVE_DATA_TYPE_GENERIC);
        mUiBot.saveForAutofill(newSaveUi, /* yesDoIt= */ true);

        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        assertTextAndValue(findNodeByResourceId(saveRequest.structure, ID_INPUT), "108");

        SecondActivity.finishIt();
        ViewActionActivity.finishIt();
    }

    private CustomDescription newCustomDescriptionWithUrlSpan(String action) {
        final RemoteViews presentation = newTemplate();
        presentation.setTextViewText(R.id.custom_text, newDescriptionWithUrlSpan(action));
        return new CustomDescription.Builder(presentation).build();
    }

    private CharSequence newDescriptionWithUrlSpan(String action) {
        final String url = "autofillcts:" + action;
        final SpannableString ss = new SpannableString("Here is URLSpan");
        ss.setSpan(new URLSpan(url),
                /* start= */ 8,  /* end= */ 15, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return ss;
    }
}
