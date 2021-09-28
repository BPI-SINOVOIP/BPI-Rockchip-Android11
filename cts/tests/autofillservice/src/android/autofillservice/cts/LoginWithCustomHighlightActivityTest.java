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

import static android.autofillservice.cts.Helper.ID_PASSWORD;
import static android.autofillservice.cts.Helper.ID_USERNAME;

import android.autofillservice.cts.CannedFillResponse.CannedDataset;
import android.graphics.Rect;
import android.support.test.uiautomator.UiObject2;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.TimeoutException;

public class LoginWithCustomHighlightActivityTest
        extends AutoFillServiceTestCase.AutoActivityLaunch<LoginWithCustomHighlightActivity> {

    private LoginWithCustomHighlightActivity mActivity;

    @Override
    protected AutofillActivityTestRule<LoginWithCustomHighlightActivity> getActivityRule() {
        return new AutofillActivityTestRule<LoginWithCustomHighlightActivity>(
                LoginWithCustomHighlightActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Before
    public void setup() {
        MyDrawable.initStatus();
    }

    @After
    public void teardown() {
        MyDrawable.clearStatus();
    }

    @Test
    public void testAutofillCustomHighlight_singleField_noHighlight() throws Exception {
        testAutofillCustomHighlight(/* singleField= */true);

        MyDrawable.assertDrawableNotDrawn();
    }

    @Test
    public void testAutofillCustomHighlight_multipleFields_hasHighlight() throws Exception {
        testAutofillCustomHighlight(/* singleField= */false);

        final Rect bounds = MyDrawable.getAutofilledBounds();
        final int width = mActivity.getUsername().getWidth();
        final int height = mActivity.getUsername().getHeight();
        if (bounds.isEmpty() || bounds.right != width || bounds.bottom != height) {
            throw new AssertionError("Field highlight comparison fail. expected: width " + width
                    + ", height " + height + ", but bounds was " + bounds);
        }
    }

    private void testAutofillCustomHighlight(boolean singleField) throws Exception {
        // Set service.
        enableService();

        final CannedDataset.Builder datasetBuilder = new CannedDataset.Builder()
                .setField(ID_USERNAME, "dude")
                .setPresentation(createPresentation("The Dude"));
        if (!singleField) {
            datasetBuilder.setField(ID_PASSWORD, "sweet");
        }

        // Set expectations.
        final CannedFillResponse.Builder builder = new CannedFillResponse.Builder()
                .addDataset(datasetBuilder.build());
        sReplier.addResponse(builder.build());
        if (singleField) {
            mActivity.expectAutoFill("dude");
        } else {
            mActivity.expectAutoFill("dude", "sweet");
        }

        // Dynamically set password to make sure it's sanitized.
        mActivity.onPassword((v) -> v.setText("I AM GROOT"));

        // Trigger auto-fill.
        requestFocusOnUsername();

        // Auto-fill it.
        final UiObject2 picker = mUiBot.assertDatasets("The Dude");
        sReplier.getNextFillRequest();

        mUiBot.selectDataset(picker, "The Dude");

        // Check the results.
        mActivity.assertAutoFilled();
    }

    /**
     * Requests focus on username and expect Window event happens.
     */
    protected void requestFocusOnUsername() throws TimeoutException {
        mUiBot.waitForWindowChange(() -> mActivity.onUsername(View::requestFocus));
    }
}
