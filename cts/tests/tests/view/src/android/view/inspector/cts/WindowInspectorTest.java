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

package android.view.inspector.cts;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.View;
import android.view.cts.CtsActivity;
import android.view.inspector.WindowInspector;

import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * Tests for {@link WindowInspector}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class WindowInspectorTest {
    private Activity mActivity;

    @Rule
    public ActivityTestRule<CtsActivity> mActivityRule =
            new ActivityTestRule<>(CtsActivity.class);

    @Before
    public void setup() {
        mActivity = mActivityRule.getActivity();
    }

    @Test
    public void testGetGlobalWindowViews() {
        List<View> views = WindowInspector.getGlobalWindowViews();
        assertEquals("Only the activity window view is present", 1, views.size());

        View view = views.get(0);
        assertEquals("The activity window view is the decor view",
                view, mActivity.getWindow().getDecorView());
    }
}
