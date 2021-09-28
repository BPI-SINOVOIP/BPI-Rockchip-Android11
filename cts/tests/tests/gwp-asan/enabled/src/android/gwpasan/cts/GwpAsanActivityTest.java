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

package android.gwpasan.cts;

import android.test.ActivityInstrumentationTestCase2;
import static org.junit.Assert.assertTrue;
import android.util.Log;
import android.gwpasan.GwpAsanTestActivity;
import android.gwpasan.Utils;

public class GwpAsanActivityTest extends ActivityInstrumentationTestCase2<GwpAsanTestActivity> {

    private GwpAsanTestActivity mActivity;

    public GwpAsanActivityTest() {
      super(GwpAsanTestActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = getActivity();
    }

    public void testGwpAsanEnabledApplication() throws Exception {
        assertTrue(Utils.isGwpAsanEnabled());
    }

    public void testGwpAsanEnabledActivity() throws Exception {
        mActivity.testGwpAsanEnabledActivity();
    }

    public void testGwpAsanDisabledActivity() throws Exception {
        mActivity.testGwpAsanDisabledActivity();
    }

    public void testGwpAsanDefaultActivity() throws Exception {
        mActivity.testGwpAsanDefaultActivity();
    }
}
