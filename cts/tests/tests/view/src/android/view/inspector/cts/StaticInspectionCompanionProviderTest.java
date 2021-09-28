/*
 * Copyright 2019 The Android Open Source Project
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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.view.inspector.InspectionCompanion;
import android.view.inspector.PropertyMapper;
import android.view.inspector.PropertyReader;
import android.view.inspector.StaticInspectionCompanionProvider;
import android.widget.RelativeLayout;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for {@link StaticInspectionCompanionProvider}
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class StaticInspectionCompanionProviderTest {
    private StaticInspectionCompanionProvider mProvider;

    @Before
    public void setup() {
        mProvider = new StaticInspectionCompanionProvider();
    }

    @Test
    public void testViewWorks() {
        InspectionCompanion<View> companion = mProvider.provide(View.class);
        assertNotNull(companion);
        assertEquals("android.view.View$InspectionCompanion", companion.getClass().getName());
    }

    @Test
    public void testRelativeLayoutWorks() {
        InspectionCompanion<RelativeLayout.LayoutParams> companion =
                mProvider.provide(RelativeLayout.LayoutParams.class);
        assertNotNull(companion);
        assertEquals(
                "android.widget.RelativeLayout$LayoutParams$InspectionCompanion",
                companion.getClass().getName());
    }

    static class NestedClassTest {
        public static final class InspectionCompanion
                implements android.view.inspector.InspectionCompanion<NestedClassTest> {
            @Override
            public void mapProperties(PropertyMapper propertyMapper) {

            }

            @Override
            public void readProperties(NestedClassTest inspectable, PropertyReader propertyReader) {

            }
        }
    }

    @Test
    public void testNestedClassLoading() {
        InspectionCompanion<NestedClassTest> companion = mProvider.provide(NestedClassTest.class);
        assertNotNull(companion);
        assertTrue(companion instanceof NestedClassTest.InspectionCompanion);
    }

    static class NonImplementingNestedClassTest {
        public static final class InspectionCompanion {
        }
    }

    @Test
    public void testNonImplementingNestedClass() {
        InspectionCompanion<NonImplementingNestedClassTest> companion =
                mProvider.provide(NonImplementingNestedClassTest.class);
        assertNull(companion);
    }

    class NoCompanionTest {
    }

    @Test
    public void testNoCompanion() {
        InspectionCompanion<NoCompanionTest> companion = mProvider.provide(NoCompanionTest.class);
        assertNull(companion);
    }
}
