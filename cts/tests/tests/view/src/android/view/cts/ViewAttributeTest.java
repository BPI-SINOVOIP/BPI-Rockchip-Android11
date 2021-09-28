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

package android.view.cts;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.support.test.uiautomator.UiDevice;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ViewAttributeTest {

    @Rule
    public ActivityTestRule<Activity> mActivityRule =
            new ActivityTestRule<>(Activity.class, true, false);

    private static final String DISABLE_SHELL_COMMAND =
            "settings delete global debug_view_attributes_application_package";

    private static final String ENABLE_SHELL_COMMAND =
            "settings put global debug_view_attributes_application_package android.view.cts";

    private UiDevice mUiDevice;

    @Before
    public void setUp() throws Exception {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mUiDevice.executeShellCommand(ENABLE_SHELL_COMMAND);
        mActivityRule.launchActivity(null);
    }

    @After
    public void tearDown() throws Exception {
        mUiDevice.executeShellCommand(DISABLE_SHELL_COMMAND);
    }

    @Test
    public void testGetExplicitStyle() {
        Context context = InstrumentationRegistry.getTargetContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        LinearLayout rootView =
                (LinearLayout) inflater.inflate(R.layout.view_attribute_layout, null);
        View view1 = rootView.findViewById(R.id.view1);
        assertEquals(R.style.ExplicitStyle1, view1.getExplicitStyle());

        View view2 = rootView.findViewById(R.id.view2);
        assertEquals(R.style.ExplicitStyle2, view2.getExplicitStyle());

        View view3 = rootView.findViewById(R.id.view3);
        assertEquals(Resources.ID_NULL, view3.getExplicitStyle());

        View view4 = rootView.findViewById(R.id.view4);
        assertEquals(android.R.style.TextAppearance_Material_Large, view4.getExplicitStyle());
    }

    @Test
    public void testGetAttributeResolutionStack() {
        LayoutInflater inflater = LayoutInflater.from(mActivityRule.getActivity());
        LinearLayout rootView =
                (LinearLayout) inflater.inflate(R.layout.view_attribute_layout, null);
        int[] stackRootView = rootView.getAttributeResolutionStack(android.R.attr.padding);
        assertEquals(0, stackRootView.length);

        // View that has an explicit style ExplicitStyle1 set via style = ...
        View view1 = rootView.findViewById(R.id.view1);
        int[] stackView1 = view1.getAttributeResolutionStack(android.R.attr.padding);
        assertEquals(3, stackView1.length);
        assertEquals(R.layout.view_attribute_layout, stackView1[0]);
        assertEquals(R.style.ExplicitStyle1, stackView1[1]);
        assertEquals(R.style.ParentOfExplicitStyle1, stackView1[2]);
    }

    @Test
    public void testGetAttributeSourceResourceMap() {
        LayoutInflater inflater = LayoutInflater.from(mActivityRule.getActivity());
        LinearLayout rootView =
                (LinearLayout) inflater.inflate(R.layout.view_attribute_layout, null);
        Map<Integer, Integer> attributeMapRootView = rootView.getAttributeSourceResourceMap();
        assertEquals(R.layout.view_attribute_layout,
                (attributeMapRootView.get(android.R.attr.orientation)).intValue());

        // View that has an explicit style ExplicitStyle1 set via style = ...
        View view1 = rootView.findViewById(R.id.view1);
        Map<Integer, Integer> attributeMapView1 = view1.getAttributeSourceResourceMap();
        assertEquals(R.style.ExplicitStyle1,
                (attributeMapView1.get(android.R.attr.padding)).intValue());
        assertEquals(R.style.ParentOfExplicitStyle1,
                (attributeMapView1.get(android.R.attr.paddingLeft)).intValue());
        assertEquals(R.layout.view_attribute_layout,
                (attributeMapView1.get(android.R.attr.paddingTop)).intValue());
    }
}
