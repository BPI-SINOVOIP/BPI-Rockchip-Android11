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

import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;
import static android.autofillservice.cts.Helper.getContext;
import static android.autofillservice.cts.inline.InstrumentedAutoFillServiceInlineEnabled.SERVICE_NAME;

import android.autofillservice.cts.AbstractLoginActivityTestCase;
import android.autofillservice.cts.CannedFillResponse;
import android.autofillservice.cts.Helper;

import org.junit.Test;
import org.junit.rules.TestRule;

// TODO: Move any tests needed from here into DatasetFilteringInlineTest.
/**
 * Tests for inline suggestion filtering. Tests for filtering datasets that need authentication are
 * in {@link InlineAuthenticationTest}.
 */
public class InlineFilteringTest extends AbstractLoginActivityTestCase {

    private static final String TAG = "InlineLoginActivityTest";

    public InlineFilteringTest() {
        super(getInlineUiBot());
    }

    @Override
    protected void enableService() {
        Helper.enableAutofillService(getContext(), SERVICE_NAME);
    }

    @Override
    public TestRule getMainTestRule() {
        return InlineUiBot.annotateRule(super.getMainTestRule());
    }

    @Test
    public void testFiltering_filtersByPrefix() throws Exception {
        enableService();

        // Set expectations.
        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "dude")
                        .setField(ID_PASSWORD, "sweet")
                        .setPresentation(createPresentation("The Dude"))
                        .setInlinePresentation(createInlinePresentation("The Dude"))
                        .build())
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "test")
                        .setField(ID_PASSWORD, "tweet")
                        .setPresentation(createPresentation("Second Dude"))
                        .setInlinePresentation(createInlinePresentation("Second Dude"))
                        .build());
        sReplier.addResponse(builder.build());

        // Trigger autofill, then make sure it's showing initially.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("The Dude", "Second Dude");
        sReplier.getNextFillRequest();

        // Filter out one of the datasets.
        mActivity.onUsername((v) -> v.setText("t"));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("Second Dude");

        // Filter out both datasets.
        mActivity.onUsername((v) -> v.setText("ta"));
        mUiBot.waitForIdleSync();
        mUiBot.assertNoDatasets();

        // Backspace to bring back one dataset.
        mActivity.onUsername((v) -> v.setText("t"));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("Second Dude");

        mActivity.expectAutoFill("test", "tweet");
        mUiBot.selectDataset("Second Dude");
        mUiBot.waitForIdleSync();
        mActivity.assertAutoFilled();
    }

    @Test
    public void testFiltering_privacy() throws Exception {
        enableService();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "sergey")
                        .setPresentation(createPresentation("sergey"))
                        .setInlinePresentation(createInlinePresentation("sergey"))
                        .build())
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "page")
                        .setPresentation(createPresentation("page"))
                        .setInlinePresentation(createInlinePresentation("page"))
                        .build());
        sReplier.addResponse(builder.build());

        // Trigger autofill and enter the correct first char, then make sure it's showing initially.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mActivity.onUsername((v) -> v.setText("s"));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("sergey");
        sReplier.getNextFillRequest();

        // Enter the wrong second char - filters out dataset.
        mActivity.onUsername((v) -> v.setText("sa"));
        mUiBot.waitForIdleSync();
        mUiBot.assertNoDatasets();

        // Backspace to bring back the dataset.
        mActivity.onUsername((v) -> v.setText("s"));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("sergey");

        // Enter the correct second char, then check that suggestions are no longer shown.
        mActivity.onUsername((v) -> v.setText("se"));
        mUiBot.waitForIdleSync();
        mUiBot.assertNoDatasets();

        // Clear the text, then check that all suggestions are shown.
        mActivity.onUsername((v) -> v.setText(""));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("sergey", "page");
    }

    /**
     * Tests that the privacy mechanism still works when the full text is replaced, as opposed to
     * individual characters being added/removed.
     */
    @Test
    public void testFiltering_privacy_textReplacement() throws Exception {
        enableService();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "sergey")
                        .setPresentation(createPresentation("sergey"))
                        .setInlinePresentation(createInlinePresentation("sergey"))
                        .build());
        sReplier.addResponse(builder.build());

        // Trigger autofill and enter a few correct chars, then make sure it's showing initially.
        mUiBot.selectByRelativeId(ID_USERNAME);
        mActivity.onUsername((v) -> v.setText("ser"));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("sergey");
        sReplier.getNextFillRequest();

        // Enter a couple different strings, then check that suggestions are no longer shown.
        mActivity.onUsername((v) -> v.setText("aaa"));
        mActivity.onUsername((v) -> v.setText("bbb"));
        mActivity.onUsername((v) -> v.setText("ser"));
        mUiBot.waitForIdleSync();
        mUiBot.assertNoDatasets();
        mActivity.onUsername((v) -> v.setText(""));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("sergey");
    }

    @Test
    public void testFiltering_pinnedAreNotFiltered() throws Exception {
        enableService();

        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(ID_USERNAME, "pinned")
                        .setPresentation(createPresentation("pinned"))
                        .setInlinePresentation(Helper.createPinnedInlinePresentation("pinned"))
                        .build());
        sReplier.addResponse(builder.build());

        mUiBot.selectByRelativeId(ID_USERNAME);
        mActivity.onUsername((v) -> v.setText("aaa"));
        mUiBot.waitForIdleSync();
        mUiBot.assertDatasets("pinned");
        sReplier.getNextFillRequest();
    }
}
