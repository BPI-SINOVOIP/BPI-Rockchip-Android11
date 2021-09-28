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

package com.android.car.ui.imewidescreen;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.android.car.ui.core.SearchResultsProvider.ITEM_ID;
import static com.android.car.ui.core.SearchResultsProvider.SECONDARY_IMAGE_ID;
import static com.android.car.ui.core.SearchResultsProvider.SUBTITLE;
import static com.android.car.ui.core.SearchResultsProvider.TITLE;
import static com.android.car.ui.core.SearchResultsProviderTest.AUTHORITY;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.ADD_DESC_TITLE_TO_CONTENT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.ADD_DESC_TO_CONTENT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.ADD_ERROR_DESC_TO_INPUT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.CONTENT_AREA_SURFACE_PACKAGE;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.REQUEST_RENDER_CONTENT_AREA;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.WIDE_SCREEN_ACTION;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenController.WIDE_SCREEN_SEARCH_RESULTS;
import static com.android.car.ui.imewidescreen.CarUiImeWideScreenTestActivity.sCarUiImeWideScreenController;

import static com.google.common.base.Preconditions.checkNotNull;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.app.Dialog;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.inputmethodservice.ExtractEditText;
import android.inputmethodservice.InputMethodService;
import android.inputmethodservice.InputMethodService.Insets;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.SurfaceControlViewHost;
import android.view.View;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.rule.ActivityTestRule;

import com.android.car.ui.test.R;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link CarUiImeWideScreenController}.
 */
@RunWith(AndroidJUnit4.class)
public class CarUiImeWideScreenControllerTest {

    private final Context mContext = ApplicationProvider.getApplicationContext();

    @Mock
    private EditorInfo mEditorInfoMock;

    @Mock
    private SurfaceControlViewHost.SurfacePackage mSurfacePackageMock;

    @Mock
    private InputMethodService mInputMethodService;

    @Mock
    private Dialog mDialog;

    @Mock
    private Window mWindow;

    private CarUiImeWideScreenTestActivity mActivity;

    @Rule
    public ActivityTestRule<CarUiImeWideScreenTestActivity> mActivityRule =
            new ActivityTestRule<>(CarUiImeWideScreenTestActivity.class);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = mActivityRule.getActivity();
    }

    @After
    public void destroy() {
        mActivity.finish();
    }

    @Test
    public void createWideScreenImeView_shouldWrapTheViewInTemplate() {
        // make sure view is wrapped in the template.
        assertNotNull(mActivity.findViewById(R.id.test_ime_input_view_id));

        // check all views in template default visibility.
        onView(withId(R.id.car_ui_wideScreenDescriptionTitle)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_wideScreenDescription)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_inputExtractActionAutomotive)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_wideScreenSearchResultList)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_wideScreenErrorMessage)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_wideScreenError)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_contentAreaAutomotive)).check(matches(not(isDisplayed())));
        onView(withId(R.id.car_ui_ime_surface)).check(matches(not(isDisplayed())));

        onView(withId(R.id.car_ui_wideScreenExtractedTextIcon)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_wideScreenClearData)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_fullscreenArea)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_inputExtractEditTextContainer)).check(matches(isDisplayed()));

        // check if the click listener is installed on the image to clear data.
        View clearDataIcon = mActivity.findViewById(R.id.car_ui_wideScreenClearData);
        assertTrue(clearDataIcon.hasOnClickListeners());
    }

    @Test
    public void onComputeInsets_showContentArea_shouldUpdateEntireAreaAsTouchable() {
        when(mInputMethodService.getWindow()).thenReturn(mDialog);
        when(mDialog.getWindow()).thenReturn(mWindow);
        View view = new FrameLayout(mContext);
        view.setTop(0);
        view.setBottom(200);
        when(mWindow.getDecorView()).thenReturn(view);

        InputMethodService.Insets outInsets = new Insets();
        CarUiImeWideScreenController carUiImeWideScreenController = getController();
        carUiImeWideScreenController.onComputeInsets(outInsets);

        assertThat(outInsets.touchableInsets, is(InputMethodService.Insets.TOUCHABLE_INSETS_FRAME));
        assertThat(outInsets.contentTopInsets, is(200));
        assertThat(outInsets.visibleTopInsets, is(200));
    }

    @Test
    public void onComputeInsets_hideContentArea_shouldUpdateRegionAsTouchable() {
        when(mInputMethodService.getWindow()).thenReturn(mDialog);
        when(mDialog.getWindow()).thenReturn(mWindow);
        View view = new FrameLayout(mContext);
        view.setTop(0);
        view.setBottom(200);
        when(mWindow.getDecorView()).thenReturn(view);

        View imeInputView = LayoutInflater.from(mContext)
                .inflate(R.layout.test_ime_input_view, null, false);
        CarUiImeWideScreenController carUiImeWideScreenController = getController();
        carUiImeWideScreenController.setExtractEditText(new ExtractEditText(mContext));
        carUiImeWideScreenController.createWideScreenImeView(imeInputView);

        Bundle bundle = new Bundle();
        bundle.putBoolean(REQUEST_RENDER_CONTENT_AREA, false);
        carUiImeWideScreenController.onAppPrivateCommand(WIDE_SCREEN_ACTION, bundle);

        InputMethodService.Insets outInsets = new Insets();
        carUiImeWideScreenController.onComputeInsets(outInsets);

        assertThat(outInsets.touchableInsets,
                is(InputMethodService.Insets.TOUCHABLE_INSETS_REGION));
        assertThat(outInsets.contentTopInsets, is(200));
        assertThat(outInsets.visibleTopInsets, is(200));
    }

    @Test
    public void onAppPrivateCommand_shouldShowTitleAndDesc() {
        when(mInputMethodService.getWindow()).thenReturn(mDialog);
        when(mDialog.getWindow()).thenReturn(mWindow);

        sCarUiImeWideScreenController.setExtractEditText(new ExtractEditText(mContext));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            Bundle bundle = new Bundle();
            bundle.putString(ADD_DESC_TITLE_TO_CONTENT_AREA, "Title");
            bundle.putString(ADD_DESC_TO_CONTENT_AREA, "Description");
            sCarUiImeWideScreenController.onAppPrivateCommand(WIDE_SCREEN_ACTION, bundle);
        });

        onView(withId(R.id.car_ui_wideScreenDescriptionTitle)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_wideScreenDescriptionTitle)).check(
                matches(withText(containsString("Title"))));

        onView(withId(R.id.car_ui_wideScreenDescription)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_wideScreenDescription)).check(
                matches(withText(containsString("Description"))));
    }

    @Test
    public void onAppPrivateCommand_shouldShowErrorMessage() {
        when(mInputMethodService.getWindow()).thenReturn(mDialog);
        when(mDialog.getWindow()).thenReturn(mWindow);

        sCarUiImeWideScreenController.setExtractEditText(new ExtractEditText(mContext));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            Bundle bundle = new Bundle();
            bundle.putString(ADD_ERROR_DESC_TO_INPUT_AREA, "Error Message");
            sCarUiImeWideScreenController.onAppPrivateCommand(WIDE_SCREEN_ACTION, bundle);
        });

        onView(withId(R.id.car_ui_wideScreenErrorMessage)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_wideScreenErrorMessage)).check(
                matches(withText(containsString("Error Message"))));
        onView(withId(R.id.car_ui_wideScreenError)).check(matches(isDisplayed()));
    }

    @Test
    public void onAppPrivateCommand_shouldShowSearchResults() {
        ContentResolver cr = ApplicationProvider.getApplicationContext().getContentResolver();
        cr.insert(Uri.parse(AUTHORITY), getRecord());

        when(mInputMethodService.getWindow()).thenReturn(mDialog);
        when(mDialog.getWindow()).thenReturn(mWindow);

        sCarUiImeWideScreenController.setExtractEditText(new ExtractEditText(mContext));
        sCarUiImeWideScreenController.setEditorInfo(mEditorInfoMock);

        CarUiImeWideScreenController spy = Mockito.spy(sCarUiImeWideScreenController);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            doReturn("com.android.car.ui.test").when(spy).getPackageName(ArgumentMatchers.any());
            Bundle bundle = new Bundle();
            spy.onAppPrivateCommand(WIDE_SCREEN_SEARCH_RESULTS, bundle);
        });

        onView(withId(R.id.car_ui_wideScreenSearchResultList)).check(matches(isDisplayed()));
        onView(withId(R.id.car_ui_wideScreenSearchResultList))
                .check(matches(atPosition(0, hasDescendant(withText("Title")))));
        onView(withId(R.id.car_ui_wideScreenSearchResultList))
                .check(matches(atPosition(0, hasDescendant(withText("SubTitle")))));
    }

    @Test
    public void onAppPrivateCommand_shouldShowSurfaceView() {
        when(mInputMethodService.getWindow()).thenReturn(mDialog);
        when(mDialog.getWindow()).thenReturn(mWindow);

        sCarUiImeWideScreenController.setExtractEditText(new ExtractEditText(mContext));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            Bundle bundle = new Bundle();
            bundle.putParcelable(CONTENT_AREA_SURFACE_PACKAGE, mSurfacePackageMock);
            sCarUiImeWideScreenController.onAppPrivateCommand(WIDE_SCREEN_ACTION, bundle);
        });

        onView(withId(R.id.car_ui_ime_surface)).check(matches(isDisplayed()));
    }

    private static ContentValues getRecord() {
        ContentValues values = new ContentValues();
        int id = 1;
        values.put(ITEM_ID, id);
        values.put(SECONDARY_IMAGE_ID, id);
        values.put(TITLE, "Title");
        values.put(SUBTITLE, "SubTitle");
        return values;
    }

    private static Matcher<View> atPosition(final int position,
            @NonNull final Matcher<View> itemMatcher) {
        checkNotNull(itemMatcher);
        return new BoundedMatcher<View, RecyclerView>(RecyclerView.class) {
            @Override
            public void describeTo(Description description) {
                description.appendText("has item at position " + position + ": ");
                itemMatcher.describeTo(description);
            }

            @Override
            protected boolean matchesSafely(final RecyclerView view) {
                RecyclerView.ViewHolder viewHolder = view.findViewHolderForAdapterPosition(
                        position);
                if (viewHolder == null) {
                    // has no item on such position
                    return false;
                }
                return itemMatcher.matches(viewHolder.itemView);
            }
        };
    }

    private CarUiImeWideScreenController getController() {
        return new CarUiImeWideScreenController(mContext, mInputMethodService) {
            @Override
            public boolean isWideScreenMode() {
                return true;
            }
        };
    }
}
