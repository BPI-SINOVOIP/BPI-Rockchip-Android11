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

import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Timeouts.MOCK_IME_TIMEOUT_MS;

import static com.android.compatibility.common.util.ShellUtils.sendKeyEvent;
import static com.android.cts.mockime.ImeEventStreamTestUtils.editorMatcher;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectBindInput;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectCommand;
import static com.android.cts.mockime.ImeEventStreamTestUtils.expectEvent;

import static org.junit.Assume.assumeTrue;

import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.content.IntentSender;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;
import android.view.KeyEvent;
import android.widget.EditText;

import com.android.cts.mockime.ImeCommand;
import com.android.cts.mockime.ImeEventStream;
import com.android.cts.mockime.MockImeSession;

import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

import java.util.regex.Pattern;

public abstract class DatasetFilteringTest extends AbstractLoginActivityTestCase {

    protected DatasetFilteringTest() {
    }

    protected DatasetFilteringTest(UiBot inlineUiBot) {
        super(inlineUiBot);
    }

    @Override
    protected TestRule getMainTestRule() {
        return RuleChain.outerRule(new MaxVisibleDatasetsRule(4))
                        .around(super.getMainTestRule());
    }


    private void changeUsername(CharSequence username) {
        mActivity.onUsername((v) -> v.setText(username));
    }


    @Test
    public void testFilter() throws Exception {
        final String aa = "Two A's";
        final String ab = "A and B";
        final String b = "Only B";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aa")
                        .setPresentation(aa, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab")
                        .setPresentation(ab, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "b")
                        .setPresentation(b, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(aa, ab, b);

        // Only two datasets start with 'a'
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab);

        // Only one dataset start with 'aa'
        changeUsername("aa");
        mUiBot.assertDatasets(aa);

        // No dataset start with 'aaa'
        final MyAutofillCallback callback = mActivity.registerCallback();
        changeUsername("aaa");
        callback.assertUiHiddenEvent(mActivity.getUsername());
        mUiBot.assertNoDatasets();

        // Delete some text to bring back 2 datasets
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab);

        // With no filter text all datasets should be shown again
        changeUsername("");
        mUiBot.assertDatasets(aa, ab, b);
    }

    @Test
    public void testFilter_injectingEvents() throws Exception {
        final String aa = "Two A's";
        final String ab = "A and B";
        final String b = "Only B";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aa")
                        .setPresentation(aa, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab")
                        .setPresentation(ab, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "b")
                        .setPresentation(b, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(aa, ab, b);

        // Only two datasets start with 'a'
        sendKeyEvent("KEYCODE_A");
        mUiBot.assertDatasets(aa, ab);

        // Only one dataset start with 'aa'
        sendKeyEvent("KEYCODE_A");
        mUiBot.assertDatasets(aa);

        // Only two datasets start with 'a'
        sendKeyEvent("KEYCODE_DEL");
        mUiBot.assertDatasets(aa, ab);

        // With no filter text all datasets should be shown
        sendKeyEvent("KEYCODE_DEL");
        mUiBot.assertDatasets(aa, ab, b);

        // No dataset start with 'aaa'
        final MyAutofillCallback callback = mActivity.registerCallback();
        sendKeyEvent("KEYCODE_A");
        sendKeyEvent("KEYCODE_A");
        sendKeyEvent("KEYCODE_A");
        callback.assertUiHiddenEvent(mActivity.getUsername());
        mUiBot.assertNoDatasets();
    }

    @Test
    public void testFilter_usingKeyboard() throws Exception {
        final MockImeSession mockImeSession = sMockImeSessionRule.getMockImeSession();
        assumeTrue("MockIME not available", mockImeSession != null);

        final String aa = "Two A's";
        final String ab = "A and B";
        final String b = "Only B";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aa")
                        .setPresentation(aa, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab")
                        .setPresentation(ab, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "b")
                        .setPresentation(b, isInlineMode())
                        .build())
                .build());

        final ImeEventStream stream = mockImeSession.openEventStream();

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();

        // Wait until the MockIme gets bound to the TestActivity.
        expectBindInput(stream, Process.myPid(), MOCK_IME_TIMEOUT_MS);
        expectEvent(stream, editorMatcher("onStartInput", mActivity.getUsername().getId()),
                MOCK_IME_TIMEOUT_MS);

        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(aa, ab, b);

        // Only two datasets start with 'a'
        final ImeCommand cmd1 = mockImeSession.callCommitText("a", 1);
        expectCommand(stream, cmd1, MOCK_IME_TIMEOUT_MS);
        mUiBot.assertDatasets(aa, ab);

        // Only one dataset start with 'aa'
        final ImeCommand cmd2 = mockImeSession.callCommitText("a", 1);
        expectCommand(stream, cmd2, MOCK_IME_TIMEOUT_MS);
        mUiBot.assertDatasets(aa);

        // Only two datasets start with 'a'
        final ImeCommand cmd3 = mockImeSession.callSendDownUpKeyEvents(KeyEvent.KEYCODE_DEL);
        expectCommand(stream, cmd3, MOCK_IME_TIMEOUT_MS);
        mUiBot.assertDatasets(aa, ab);

        // With no filter text all datasets should be shown
        final ImeCommand cmd4 = mockImeSession.callSendDownUpKeyEvents(KeyEvent.KEYCODE_DEL);
        expectCommand(stream, cmd4, MOCK_IME_TIMEOUT_MS);
        mUiBot.assertDatasets(aa, ab, b);

        // No dataset start with 'aaa'
        final MyAutofillCallback callback = mActivity.registerCallback();
        final ImeCommand cmd5 = mockImeSession.callCommitText("aaa", 1);
        expectCommand(stream, cmd5, MOCK_IME_TIMEOUT_MS);
        callback.assertUiHiddenEvent(mActivity.getUsername());
        mUiBot.assertNoDatasets();
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_nullValuesAlwaysMatched() throws Exception {
        final String aa = "Two A's";
        final String ab = "A and B";
        final String b = "Only B";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aa")
                        .setPresentation(aa, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab")
                        .setPresentation(ab, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, (String) null)
                        .setPresentation(b, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(aa, ab, b);

        // Two datasets start with 'a' and one with null value always shown
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab, b);

        // One dataset start with 'aa' and one with null value always shown
        changeUsername("aa");
        mUiBot.assertDatasets(aa, b);

        // Two datasets start with 'a' and one with null value always shown
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab, b);

        // With no filter text all datasets should be shown
        changeUsername("");
        mUiBot.assertDatasets(aa, ab, b);

        // No dataset start with 'aaa' and one with null value always shown
        changeUsername("aaa");
        mUiBot.assertDatasets(b);
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_differentPrefixes() throws Exception {
        final String a = "aaa";
        final String b = "bra";
        final String c = "cadabra";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, a)
                        .setPresentation(a, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, b)
                        .setPresentation(b, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, c)
                        .setPresentation(c, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(a, b, c);

        changeUsername("a");
        mUiBot.assertDatasets(a);

        changeUsername("b");
        mUiBot.assertDatasets(b);

        changeUsername("c");
        if (!isInlineMode()) { // With inline, we don't show the datasets now to protect privacy.
            mUiBot.assertDatasets(c);
        }
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_usingRegex() throws Exception {
        // Dataset presentations.
        final String aa = "Two A's";
        final String ab = "A and B";
        final String b = "Only B";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "whatever", Pattern.compile("a|aa"))
                        .setPresentation(aa, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "whatsoever",
                                Pattern.compile("a|ab"))
                        .setPresentation(ab, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, (String) null, Pattern.compile("b"))
                        .setPresentation(b, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(aa, ab, b);

        // Only two datasets start with 'a'
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab);

        // Only one dataset start with 'aa'
        changeUsername("aa");
        mUiBot.assertDatasets(aa);

        // Only two datasets start with 'a'
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab);

        // With no filter text all datasets should be shown
        changeUsername("");
        mUiBot.assertDatasets(aa, ab, b);

        // No dataset start with 'aaa'
        final MyAutofillCallback callback = mActivity.registerCallback();
        changeUsername("aaa");
        callback.assertUiHiddenEvent(mActivity.getUsername());
        mUiBot.assertNoDatasets();
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_disabledUsingNullRegex() throws Exception {
        // Dataset presentations.
        final String unfilterable = "Unfilterabled";
        final String aOrW = "A or W";
        final String w = "Wazzup";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                // This dataset has a value but filter is disabled
                .addDataset(new CannedDataset.Builder()
                        .setUnfilterableField(ID_USERNAME, "a am I")
                        .setPresentation(unfilterable, isInlineMode())
                        .build())
                // This dataset uses pattern to filter
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "whatsoever",
                                Pattern.compile("a|aw"))
                        .setPresentation(aOrW, isInlineMode())
                        .build())
                // This dataset uses value to filter
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "wazzup")
                        .setPresentation(w, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(unfilterable, aOrW, w);

        // Only one dataset start with 'a'
        changeUsername("a");
        mUiBot.assertDatasets(aOrW);

        // No dataset starts with 'aa'
        changeUsername("aa");
        mUiBot.assertNoDatasets();

        // Only one datasets start with 'a'
        changeUsername("a");
        mUiBot.assertDatasets(aOrW);

        // With no filter text all datasets should be shown
        changeUsername("");
        mUiBot.assertDatasets(unfilterable, aOrW, w);

        // Only one datasets start with 'w'
        changeUsername("w");
        if (!isInlineMode()) { // With inline, we don't show the datasets now to protect privacy.
            mUiBot.assertDatasets(w);
        }

        // No dataset start with 'aaa'
        final MyAutofillCallback callback = mActivity.registerCallback();
        changeUsername("aaa");
        callback.assertUiHiddenEvent(mActivity.getUsername());
        mUiBot.assertNoDatasets();
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_mixPlainAndRegex() throws Exception {
        final String plain = "Plain";
        final String regexPlain = "RegexPlain";
        final String authRegex = "AuthRegex";
        final String kitchnSync = "KitchenSync";
        final Pattern everything = Pattern.compile(".*");

        enableService();

        // Set expectations.
        final IntentSender authentication = AuthenticationActivity.createSender(mContext, 1,
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .build());
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aword")
                        .setPresentation(plain, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "a ignore", everything)
                        .setPresentation(regexPlain, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab ignore", everything)
                        .setAuthentication(authentication)
                        .setPresentation(authRegex, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab ignore",
                                everything)
                        .setPresentation(kitchnSync, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(plain, regexPlain, authRegex, kitchnSync);

        // All datasets start with 'a'
        changeUsername("a");
        mUiBot.assertDatasets(plain, regexPlain, authRegex, kitchnSync);

        // Only the regex datasets should start with 'ab'
        changeUsername("ab");
        mUiBot.assertDatasets(regexPlain, authRegex, kitchnSync);
    }

    @Test
    @AppModeFull(reason = "testFilter_usingKeyboard() is enough")
    public void testFilter_mixPlainAndRegex_usingKeyboard() throws Exception {
        final String plain = "Plain";
        final String regexPlain = "RegexPlain";
        final String authRegex = "AuthRegex";
        final String kitchnSync = "KitchenSync";
        final Pattern everything = Pattern.compile(".*");

        enableService();

        // Set expectations.
        final IntentSender authentication = AuthenticationActivity.createSender(mContext, 1,
                new CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .build());
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aword")
                        .setPresentation(plain, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "a ignore", everything)
                        .setPresentation(regexPlain, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab ignore", everything)
                        .setAuthentication(authentication)
                        .setPresentation(authRegex, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab ignore",
                                everything)
                        .setPresentation(kitchnSync, isInlineMode())
                        .build())
                .build());

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdle();
        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(plain, regexPlain, authRegex, kitchnSync);

        // All datasets start with 'a'
        sendKeyEvent("KEYCODE_A");
        mUiBot.assertDatasets(plain, regexPlain, authRegex, kitchnSync);

        // Only the regex datasets should start with 'ab'
        sendKeyEvent("KEYCODE_B");
        mUiBot.assertDatasets(regexPlain, authRegex, kitchnSync);
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_resetFilter_chooseFirst() throws Exception {
        resetFilterTest(1);
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_resetFilter_chooseSecond() throws Exception {
        resetFilterTest(2);
    }

    @Test
    @AppModeFull(reason = "testFilter() is enough")
    public void testFilter_resetFilter_chooseThird() throws Exception {
        resetFilterTest(3);
    }

    // Tests that datasets are re-shown and filtering still works after clearing a selected value.
    private void resetFilterTest(int number) throws Exception {
        final String aa = "Two A's";
        final String ab = "A and B";
        final String b = "Only B";

        enableService();

        // Set expectations.
        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "aa")
                        .setPresentation(aa, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "ab")
                        .setPresentation(ab, isInlineMode())
                        .build())
                .addDataset(new CannedDataset.Builder()
                        .setField(ID_USERNAME, "b")
                        .setPresentation(b, isInlineMode())
                        .build())
                .build());

        final String chosenOne;
        switch (number) {
            case 1:
                chosenOne = aa;
                mActivity.expectAutoFill("aa");
                break;
            case 2:
                chosenOne = ab;
                mActivity.expectAutoFill("ab");
                break;
            case 3:
                chosenOne = b;
                mActivity.expectAutoFill("b");
                break;
            default:
                throw new IllegalArgumentException("invalid dataset number: " + number);
        }

        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText username = mActivity.getUsername();

        // Trigger auto-fill.
        mUiBot.selectByRelativeId(ID_USERNAME);
        callback.assertUiShownEvent(username);

        sReplier.getNextFillRequest();

        // With no filter text all datasets should be shown
        mUiBot.assertDatasets(aa, ab, b);

        // select the choice
        mUiBot.selectDataset(chosenOne);
        callback.assertUiHiddenEvent(username);
        mUiBot.assertNoDatasets();

        // Check the results.
        mActivity.assertAutoFilled();

        // Change the filled text and check that filtering still works.
        changeUsername("a");
        mUiBot.assertDatasets(aa, ab);

        // Reset back to all choices
        changeUsername("");
        mUiBot.assertDatasets(aa, ab, b);
    }
}
