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

import static android.autofillservice.cts.Helper.getContext;
import static android.autofillservice.cts.WebViewActivity.HTML_NAME_PASSWORD;
import static android.autofillservice.cts.WebViewActivity.HTML_NAME_USERNAME;
import static android.autofillservice.cts.inline.InstrumentedAutoFillServiceInlineEnabled.SERVICE_NAME;
import static android.service.autofill.SaveInfo.SAVE_DATA_TYPE_PASSWORD;

import static com.google.common.truth.Truth.assertThat;

import android.app.assist.AssistStructure.ViewNode;
import android.autofillservice.cts.AbstractWebViewTestCase;
import android.autofillservice.cts.AutofillActivityTestRule;
import android.autofillservice.cts.CannedFillResponse;
import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.autofillservice.cts.Helper;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.autofillservice.cts.InstrumentedAutoFillService.SaveRequest;
import android.autofillservice.cts.MyWebView;
import android.autofillservice.cts.WebViewActivity;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.view.KeyEvent;
import android.view.ViewStructure.HtmlInfo;

import org.junit.Test;
import org.junit.rules.TestRule;

public class InlineWebViewActivityTest extends AbstractWebViewTestCase<WebViewActivity> {

    private static final String TAG = "InlineWebViewActivityTest";
    private WebViewActivity mActivity;

    public InlineWebViewActivityTest() {
        super(getInlineUiBot());
    }

    @Override
    protected AutofillActivityTestRule<WebViewActivity> getActivityRule() {
        return new AutofillActivityTestRule<WebViewActivity>(WebViewActivity.class) {
            @Override
            protected void beforeActivityLaunched() {
                super.beforeActivityLaunched();
                Log.i(TAG, "Setting service before launching the activity");
                enableService();
            }

            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Override
    protected void enableService() {
        Helper.enableAutofillService(getContext(), SERVICE_NAME);
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
    public void testAutofillNoDatasets() throws Exception {
        // Set service.
        enableService();

        // Load WebView
        mActivity.loadWebView(mUiBot);

        // Set expectations.
        sReplier.addResponse(CannedFillResponse.NO_RESPONSE);

        // Trigger autofill.
        mActivity.getUsernameInput().click();
        sReplier.getNextFillRequest();

        // Assert not shown.
        mUiBot.assertNoDatasetsEver();
    }

    @Test
    public void testAutofillOneDataset() throws Exception {
        // Set service.
        enableService();

        // Load WebView
        final MyWebView myWebView = mActivity.loadWebView(mUiBot);
        mUiBot.waitForIdleSync();
        // Sanity check to make sure autofill is enabled in the application context
        Helper.assertAutofillEnabled(myWebView.getContext(), true);

        // Set expectations.
        myWebView.expectAutofill("dude", "sweet");
        sReplier.addResponse(new CannedDataset.Builder()
                .setField(HTML_NAME_USERNAME, "dude")
                .setField(HTML_NAME_PASSWORD, "sweet")
                .setPresentation(createPresentation("The Dude"))
                .setInlinePresentation(createInlinePresentation("The Dude"))
                .build());

        // Trigger autofill.
        mActivity.getUsernameInput().click();
        mUiBot.waitForIdleSync();
        final FillRequest fillRequest = sReplier.getNextFillRequest();
        mUiBot.assertDatasets("The Dude");

        // Change focus around
        mActivity.getUsernameLabel().click();
        mUiBot.waitForIdleSync();
        mUiBot.assertNoDatasets();
        mActivity.getPasswordInput().click();
        mUiBot.waitForIdleSync();
        final UiObject2 datasetPicker = mUiBot.assertDatasets("The Dude");

        // Now Autofill it.
        mUiBot.selectDataset(datasetPicker, "The Dude");
        myWebView.assertAutofilled();
        mUiBot.assertNoDatasets();

        try {
            final ViewNode webViewNode =
                    Helper.findWebViewNodeByFormName(fillRequest.structure, "FORM AM I");
            assertThat(webViewNode.getClassName()).isEqualTo("android.webkit.WebView");
            assertThat(webViewNode.getWebDomain()).isEqualTo(WebViewActivity.FAKE_DOMAIN);
            assertThat(webViewNode.getWebScheme()).isEqualTo("https");

            final ViewNode usernameNode =
                    Helper.findNodeByHtmlName(fillRequest.structure, HTML_NAME_USERNAME);
            Helper.assertTextIsSanitized(usernameNode);
            final HtmlInfo usernameHtmlInfo = Helper.assertHasHtmlTag(usernameNode, "input");
            Helper.assertHasAttribute(usernameHtmlInfo, "type", "text");
            Helper.assertHasAttribute(usernameHtmlInfo, "name", "username");
            assertThat(usernameNode.isFocused()).isTrue();
            assertThat(usernameNode.getAutofillHints()).asList().containsExactly("username");
            assertThat(usernameNode.getHint()).isEqualTo("There's no place like a holder");

            final ViewNode passwordNode =
                    Helper.findNodeByHtmlName(fillRequest.structure, HTML_NAME_PASSWORD);
            Helper.assertTextIsSanitized(passwordNode);
            final HtmlInfo passwordHtmlInfo = Helper.assertHasHtmlTag(passwordNode, "input");
            Helper.assertHasAttribute(passwordHtmlInfo, "type", "password");
            Helper.assertHasAttribute(passwordHtmlInfo, "name", "password");
            assertThat(passwordNode.getAutofillHints()).asList()
                    .containsExactly("current-password");
            assertThat(passwordNode.getHint()).isEqualTo("Holder it like it cannnot passer a word");
            assertThat(passwordNode.isFocused()).isFalse();
        } catch (RuntimeException | Error e) {
            Helper.dumpStructure("failed on testAutofillOneDataset()", fillRequest.structure);
            throw e;
        }
    }

    @Test
    public void testAutofillAndSave() throws Exception {
        // Set service.
        enableService();

        // Load WebView
        final MyWebView myWebView = mActivity.loadWebView(mUiBot);

        // Set expectations.
        myWebView.expectAutofill("dude", "sweet");
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setRequiredSavableIds(SAVE_DATA_TYPE_PASSWORD,
                        HTML_NAME_USERNAME, HTML_NAME_PASSWORD)
                .addDataset(new CannedDataset.Builder()
                        .setField(HTML_NAME_USERNAME, "dude")
                        .setField(HTML_NAME_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Dude"))
                        .setInlinePresentation(createInlinePresentation("The Dude"))
                        .build())
                .build());


        // Trigger autofill.
        mActivity.getUsernameInput().click();
        final FillRequest fillRequest = sReplier.getNextFillRequest();
        mUiBot.assertDatasets("The Dude");

        // Assert structure passed to service.
        final ViewNode usernameNode = Helper.findNodeByHtmlName(fillRequest.structure,
                HTML_NAME_USERNAME);
        Helper.assertTextIsSanitized(usernameNode);
        assertThat(usernameNode.isFocused()).isTrue();
        assertThat(usernameNode.getAutofillHints()).asList().containsExactly("username");
        final ViewNode passwordNode = Helper.findNodeByHtmlName(fillRequest.structure,
                HTML_NAME_PASSWORD);
        Helper.assertTextIsSanitized(passwordNode);
        assertThat(passwordNode.getAutofillHints()).asList().containsExactly("current-password");
        assertThat(passwordNode.isFocused()).isFalse();

        // Autofill it.
        mUiBot.selectDataset("The Dude");
        myWebView.assertAutofilled();

        // Now trigger save.
        mActivity.getUsernameInput().click();
        mActivity.dispatchKeyPress(KeyEvent.KEYCODE_U);
        mActivity.getPasswordInput().click();
        mActivity.dispatchKeyPress(KeyEvent.KEYCODE_P);
        mActivity.getLoginButton().click();

        // Assert save UI shown.
        mUiBot.updateForAutofill(true, SAVE_DATA_TYPE_PASSWORD);

        // Assert results
        final SaveRequest saveRequest = sReplier.getNextSaveRequest();
        final ViewNode usernameNode2 = Helper.findNodeByHtmlName(saveRequest.structure,
                HTML_NAME_USERNAME);
        final ViewNode passwordNode2 = Helper.findNodeByHtmlName(saveRequest.structure,
                HTML_NAME_PASSWORD);

        Helper.assertTextAndValue(usernameNode2, "dudeu");
        Helper.assertTextAndValue(passwordNode2, "sweetp");
    }
}
