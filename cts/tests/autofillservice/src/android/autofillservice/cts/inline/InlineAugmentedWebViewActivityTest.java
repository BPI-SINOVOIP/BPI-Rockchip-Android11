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
import static android.autofillservice.cts.WebViewActivity.HTML_NAME_PASSWORD;
import static android.autofillservice.cts.WebViewActivity.HTML_NAME_USERNAME;
import static android.autofillservice.cts.augmented.CannedAugmentedFillResponse.NO_AUGMENTED_RESPONSE;

import android.app.assist.AssistStructure.ViewNode;
import android.autofillservice.cts.AutofillActivityTestRule;
import android.autofillservice.cts.CannedFillResponse;
import android.autofillservice.cts.Helper;
import android.autofillservice.cts.InstrumentedAutoFillService.FillRequest;
import android.autofillservice.cts.MyWebView;
import android.autofillservice.cts.WebViewActivity;
import android.autofillservice.cts.augmented.AugmentedAutofillAutoActivityLaunchTestCase;
import android.autofillservice.cts.augmented.CannedAugmentedFillResponse;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.view.KeyEvent;
import android.view.autofill.AutofillId;

import org.junit.Test;

public class InlineAugmentedWebViewActivityTest extends
        AugmentedAutofillAutoActivityLaunchTestCase<WebViewActivity> {

    private static final String TAG = "InlineAugmentedWebViewActivityTest";
    private WebViewActivity mActivity;

    public InlineAugmentedWebViewActivityTest() {
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

    @Test
    public void testAugmentedAutoFillNoDatasets() throws Exception {
        // Set service.
        enableAutofillServices();

        // Load WebView
        mActivity.loadWebView(mUiBot);
        mUiBot.waitForIdleSync();

        // Set expectations.
        sReplier.addResponse(CannedFillResponse.NO_RESPONSE);
        sAugmentedReplier.addResponse(NO_AUGMENTED_RESPONSE);

        // Trigger autofill.
        mActivity.getUsernameInput().click();
        sReplier.getNextFillRequest();
        sAugmentedReplier.getNextFillRequest();

        // Assert not shown.
        mUiBot.assertNoDatasetsEver();
    }

    @Test
    public void testAugmentedAutoFillOneDataset() throws Exception {
        // Set service.
        enableAutofillServices();

        testBasicAugmentedAutofill();
    }

    @Test
    public void testAugmentedAutoFill_startTypingHideInline() throws Exception {
        // Set service.
        enableAutofillServices();

        testBasicAugmentedAutofill();

        // Now pretend user typing something by updating the value in the input field.
        mActivity.getUsernameInput().click();
        mActivity.dispatchKeyPress(KeyEvent.KEYCODE_U);

        // Expect the inline suggestion to disappear.
        mUiBot.assertNoDatasets();
    }

    private void enableAutofillServices() throws Exception {
        enableService();
        enableAugmentedService();
    }

    private void testBasicAugmentedAutofill() throws Exception {
        // Load WebView
        MyWebView myWebView = mActivity.loadWebView(mUiBot);
        mUiBot.waitForIdleSync();

        // Set expectations
        sReplier.addResponse(NO_RESPONSE);

        // Trigger autofill.
        mActivity.getUsernameInput().click();
        mUiBot.waitForIdleSync();

        // We cannot get webview field's AutofillId from AugmentedService FillRequest, we only can
        // get these AutofillIds from AutofillService's AssistStructure by using html tag.
        FillRequest autofillRequest = sReplier.getNextFillRequest();

        // Set expectations for AugmentedService
        AutofillId usernameId = getAutofillIdByWebViewTag(autofillRequest, HTML_NAME_USERNAME);
        AutofillId passwordId = getAutofillIdByWebViewTag(autofillRequest, HTML_NAME_PASSWORD);
        sAugmentedReplier.addResponse(new CannedAugmentedFillResponse.Builder()
                .addInlineSuggestion(new CannedAugmentedFillResponse.Dataset.Builder("Augment Me")
                        .setField(usernameId, "dude", createInlinePresentation("dude"))
                        .setField(passwordId, "sweet", createInlinePresentation("sweet"))
                        .build())
                .setOnlyDataset(new CannedAugmentedFillResponse.Dataset.Builder("req1")
                        .setOnlyField("dude")
                        .build())
                .build());

        sAugmentedReplier.getNextFillRequest();
        final UiObject2 datasetPicker = mUiBot.assertDatasets("dude");

        // Now Autofill it.
        myWebView.expectAutofill("dude", "sweet");
        mUiBot.selectDataset(datasetPicker, "dude");
        myWebView.assertAutofilled();
        mUiBot.assertNoDatasets();
    }

    private AutofillId getAutofillIdByWebViewTag(FillRequest autofillRequest, String tag) {
        ViewNode viewNode = Helper.findNodeByHtmlName(autofillRequest.structure, tag);
        return AutofillId.withoutSession(viewNode.getAutofillId());
    }
}
