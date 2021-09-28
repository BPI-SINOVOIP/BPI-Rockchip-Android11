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

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.Context;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.VisibilitySetterAction;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnitRunner;

@RunWith(MockitoJUnitRunner.class)
@AppModeFull(reason = "Unit test")
public class VisibilitySetterActionTest {

    private static final Context sContext = getInstrumentation().getTargetContext();
    private final ViewGroup mRootView = new ViewGroup(sContext) {

        @Override
        protected void onLayout(boolean changed, int l, int t, int r, int b) {}
    };

    @Test
    public void testValidVisibilities() {
        assertThat(new VisibilitySetterAction.Builder(42, View.VISIBLE).build()).isNotNull();
        assertThat(new VisibilitySetterAction.Builder(42, View.GONE).build()).isNotNull();
        assertThat(new VisibilitySetterAction.Builder(42, View.INVISIBLE).build()).isNotNull();
    }

    @Test
    public void testInvalidVisibilities() {
        assertThrows(IllegalArgumentException.class,
                () -> new VisibilitySetterAction.Builder(42, 666).build());
        final VisibilitySetterAction.Builder validBuilder =
                new VisibilitySetterAction.Builder(42, View.VISIBLE);
        assertThrows(IllegalArgumentException.class,
                () -> validBuilder.setVisibility(108, 666).build());
    }

    @Test
    public void testOneChild() {
        final VisibilitySetterAction action = new VisibilitySetterAction.Builder(42, View.VISIBLE)
                .build();
        final View view = new View(sContext);
        view.setId(42);
        view.setVisibility(View.GONE);
        mRootView.addView(view);

        action.onClick(mRootView);

        assertThat(view.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void testOneChildAddedTwice() {
        final VisibilitySetterAction action = new VisibilitySetterAction.Builder(42, View.VISIBLE)
                .setVisibility(42, View.INVISIBLE)
                .build();
        final View view = new View(sContext);
        view.setId(42);
        view.setVisibility(View.GONE);
        mRootView.addView(view);

        action.onClick(mRootView);

        assertThat(view.getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void testMultipleChildren() {
        final VisibilitySetterAction action = new VisibilitySetterAction.Builder(42, View.VISIBLE)
                .setVisibility(108, View.INVISIBLE)
                .build();
        final View view1 = new View(sContext);
        view1.setId(42);
        view1.setVisibility(View.GONE);
        mRootView.addView(view1);

        final View view2 = new View(sContext);
        view2.setId(108);
        view2.setVisibility(View.GONE);
        mRootView.addView(view2);

        action.onClick(mRootView);

        assertThat(view1.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(view2.getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void testNoMoreInteractionsAfterBuild() {
        final VisibilitySetterAction.Builder builder =
                new VisibilitySetterAction.Builder(42, View.VISIBLE);

        assertThat(builder.build()).isNotNull();
        assertThrows(IllegalStateException.class, () -> builder.build());
        assertThrows(IllegalStateException.class, () -> builder.setVisibility(108, View.GONE));
    }
}
