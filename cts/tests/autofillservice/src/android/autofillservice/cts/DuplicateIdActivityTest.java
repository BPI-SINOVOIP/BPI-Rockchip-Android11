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

import static android.autofillservice.cts.CannedFillResponse.NO_RESPONSE;
import static android.autofillservice.cts.DuplicateIdActivity.DUPLICATE_ID;
import static android.autofillservice.cts.Helper.assertEqualsIgnoreSession;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.app.assist.AssistStructure;
import android.app.assist.AssistStructure.ViewNode;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.autofill.AutofillId;
import android.widget.EditText;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class DuplicateIdActivityTest
        extends AutoFillServiceTestCase.AutoActivityLaunch<DuplicateIdActivity> {

    private static final String TAG = "DuplicateIdActivityTest";

    private static final AutofillActivityTestRule<DuplicateIdActivity> sActivityRule =
            new AutofillActivityTestRule<DuplicateIdActivity>(DuplicateIdActivity.class);

    @Override
    protected AutofillActivityTestRule<DuplicateIdActivity> getActivityRule() {
        return sActivityRule;
    }

    @Before
    public void setup() throws Exception {
        Helper.disableAutoRotation(mUiBot);
        mUiBot.setScreenOrientation(0);
    }

    @After
    public void teardown() {
        Helper.allowAutoRotation();
    }

    /**
     * Find the views that are tested from the structure in the request
     *
     * @param request The request
     * @param expectedCount The expected number of children
     *
     * @return An array containing the two tested views
     */
    private AssistStructure.ViewNode[] findViews(InstrumentedAutoFillService.FillRequest request,
            int expectedCount) {
        assertThat(request.structure.getWindowNodeCount()).isEqualTo(1);
        AssistStructure.WindowNode windowNode = request.structure.getWindowNodeAt(0);

        AssistStructure.ViewNode rootNode = windowNode.getRootViewNode();

        assertThat(rootNode.getChildCount()).isEqualTo(expectedCount);
        final ViewNode[] viewNodes = new AssistStructure.ViewNode[expectedCount];
        for (int i = 0; i < expectedCount; i++) {
            viewNodes[i] = rootNode.getChildAt(i);
        }
        return viewNodes;
    }

    @Test
    public void testDoNotRestoreDuplicateAutofillIds() throws Exception {
        assumeTrue("Rotation is supported", Helper.isRotationSupported(mContext));

        enableService();

        sReplier.addResponse(new CannedFillResponse.Builder()
                .addDataset(new CannedFillResponse.CannedDataset.Builder()
                        .setField(DUPLICATE_ID, "value")
                        .setPresentation(createPresentation("dataset"))
                        .build())
                .build());

        // Select field to start autofill
        runShellCommand("input keyevent KEYCODE_TAB");

        final InstrumentedAutoFillService.FillRequest request1 = sReplier.getNextFillRequest();
        Log.v(TAG, "request1: " + request1);

        final AssistStructure.ViewNode[] views1 = findViews(request1, 2);
        final AssistStructure.ViewNode view1 = views1[0];
        final AssistStructure.ViewNode view2 = views1[1];
        final AutofillId id1 = view1.getAutofillId();
        final AutofillId id2 = view2.getAutofillId();

        Log.v(TAG, "view1=" + id1);
        Log.v(TAG, "view2=" + id2);

        // Both checkboxes use the same id
        assertThat(view1.getId()).isEqualTo(view2.getId());

        // They got different autofill ids though
        assertThat(id1).isNotEqualTo(id2);

        // Because service returned a null response, rotation will trigger another request.
        sReplier.addResponse(NO_RESPONSE);
        // Force rotation to force onDestroy->onCreate cycle
        mUiBot.setScreenOrientation(1);
        // Wait context and Views being recreated in rotation
        mUiBot.assertShownByRelativeId(DUPLICATE_ID);
        // Ignore 2nd request.
        final InstrumentedAutoFillService.FillRequest request2 = sReplier.getNextFillRequest();
        Log.v(TAG, "request2: " + request2);

        // Select a new field to trigger new partition (because server return null on 1st response)
        sReplier.addResponse(NO_RESPONSE);
        final DuplicateIdActivity activity = getActivity();
        final EditText child = new EditText(activity);
        activity.syncRunOnUiThread(() -> {
            final View sibling = activity.findViewById(R.id.duplicate_id);
            final ViewGroup parent = (ViewGroup) sibling.getParent();
            parent.addView(child);
            child.requestFocus();
        });

        final InstrumentedAutoFillService.FillRequest request3 = sReplier.getNextFillRequest();
        Log.v(TAG, "request3: " + request3);
        final AssistStructure.ViewNode[] views2 = findViews(request3, 3);
        final AssistStructure.ViewNode recreatedView1 = views2[0];
        final AssistStructure.ViewNode recreatedView2 = views2[1];
        final AssistStructure.ViewNode newView1 = views2[2];
        final AutofillId recreatedId1 = recreatedView1.getAutofillId();
        final AutofillId recreatedId2 = recreatedView2.getAutofillId();
        final AutofillId newId1 = newView1.getAutofillId();

        Log.v(TAG, "restored view1=" + recreatedId1);
        Log.v(TAG, "restored view2=" + recreatedId2);
        Log.v(TAG, "new view1=" + newId1);

        // For the restoring logic the two views are the same. Hence it might happen that the first
        // view is restored with the autofill id of the second view or the other way round.
        // We just need
        // - to restore as many views as we can (i.e. one)
        // - make sure the autofill ids are still unique after
        final boolean view1WasRestored = (recreatedId1.equalsIgnoreSession(id1)
                || recreatedId1.equalsIgnoreSession(id2));
        final boolean view2WasRestored = (recreatedId2.equalsIgnoreSession(id1)
                || recreatedId2.equalsIgnoreSession(id2));

        // One id was restored
        assertThat(view1WasRestored || view2WasRestored).isTrue();

        // The views still have different autofill ids
        assertThat(recreatedId1).isNotEqualTo(recreatedId2);

        // Assert id of new view
        assertEqualsIgnoreSession(newId1, child.getAutofillId());
    }
}
