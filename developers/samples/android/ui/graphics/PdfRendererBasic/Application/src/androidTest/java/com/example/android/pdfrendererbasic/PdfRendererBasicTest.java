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

package com.example.android.pdfrendererbasic;

import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.fragment.app.Fragment;
import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.LargeTest;
import androidx.test.runner.AndroidJUnit4;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static com.google.common.truth.Truth.assertThat;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.core.AllOf.allOf;

@RunWith(AndroidJUnit4.class)
@LargeTest
public class PdfRendererBasicTest {

    @Test
    public void integration() {
        final ActivityScenario<MainActivity> scenario = ActivityScenario.launch(MainActivity.class);
        scenario.onActivity((activity) -> {
            final Fragment fragment = activity.getSupportFragmentManager()
                    .findFragmentById(R.id.container);
            assertThat(fragment).isInstanceOf(PdfRendererBasicFragment.class);
        });
        onView(withId(R.id.image)).check(matches(isDisplayed()));
        onView(withId(R.id.previous)).check(matches(allOf(isDisplayed(), not(isEnabled()))));
        onView(withId(R.id.next)).check(matches(allOf(isDisplayed(), isEnabled())));
        onView(withText("PdfRendererBasic (1/10)")).check(matches(isDisplayed()));
        onView(withId(R.id.next)).perform(click());
        onView(withText("PdfRendererBasic (2/10)")).check(matches(isDisplayed()));
        onView(withId(R.id.previous)).check(matches(allOf(isDisplayed(), isEnabled())));
        onView(withId(R.id.next)).check(matches(allOf(isDisplayed(), isEnabled())));
        for (int i = 0; i < 8; i++) {
            onView(withId(R.id.next)).perform(click());
        }
        onView(withText("PdfRendererBasic (10/10)")).check(matches(isDisplayed()));
        onView(withId(R.id.previous)).check(matches(allOf(isDisplayed(), isEnabled())));
        onView(withId(R.id.next)).check(matches(allOf(isDisplayed(), not(isEnabled()))));
    }

}
