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

package com.android.car.settings.wifi;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import com.android.car.settings.R;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(RobolectricTestRunner.class)
public class ButtonPasswordEditTextPreferenceTest {

    private PreferenceViewHolder mViewHolder;
    private ButtonPasswordEditTextPreference mButtonPreference;

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.application;
        Context themedContext = new ContextThemeWrapper(context, R.style.CarSettingTheme);

        mButtonPreference = new ButtonPasswordEditTextPreference(context);
        View rootView = View.inflate(themedContext, mButtonPreference.getLayoutResource(), null);
        mViewHolder = PreferenceViewHolder.createInstanceForTests(rootView);
    }

    @Test
    public void buttonClicked_callsListener() {
        mButtonPreference.onBindViewHolder(mViewHolder);
        ButtonPasswordEditTextPreference.OnButtonClickListener listener = mock(
                ButtonPasswordEditTextPreference.OnButtonClickListener.class);
        mButtonPreference.setOnButtonClickListener(listener);

        mViewHolder.findViewById(android.R.id.widget_frame).performClick();
        verify(listener).onButtonClick(mButtonPreference);
    }

    @Test
    public void performButtonClick_listenerSetAndButtonVisible_listenerFired() {
        ButtonPasswordEditTextPreference.OnButtonClickListener listener = mock(
                ButtonPasswordEditTextPreference.OnButtonClickListener.class);
        mButtonPreference.setOnButtonClickListener(listener);
        mButtonPreference.showButton(true);

        mButtonPreference.performButtonClick();
        verify(listener).onButtonClick(mButtonPreference);
    }

    @Test
    public void performButtonClick_listenerSetAndButtonInvisible_listenerNotFired() {
        ButtonPasswordEditTextPreference.OnButtonClickListener listener = mock(
                ButtonPasswordEditTextPreference.OnButtonClickListener.class);
        mButtonPreference.setOnButtonClickListener(listener);
        mButtonPreference.showButton(false);

        mButtonPreference.performButtonClick();
        verify(listener, never()).onButtonClick(mButtonPreference);
    }

    @Test
    public void onBindViewHolder_buttonShown() {
        mButtonPreference.showButton(true);
        View containerWithoutWidget = mViewHolder.findViewById(
                com.android.car.ui.R.id.car_ui_preference_container_without_widget);
        View actionContainer = mButtonPreference.getWidgetActionContainer(mViewHolder);
        View widgetFrame = mViewHolder.findViewById(android.R.id.widget_frame);

        mButtonPreference.onBindViewHolder(mViewHolder);

        assertThat(mViewHolder.itemView.isFocusable()).isFalse();
        assertThat(containerWithoutWidget.isClickable()).isTrue();
        assertThat(containerWithoutWidget.isFocusable()).isTrue();
        assertThat(actionContainer.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(widgetFrame.isClickable()).isTrue();
        assertThat(widgetFrame.isFocusable()).isTrue();
    }

    @Test
    public void onBindViewHolder_buttonNotShown() {
        mButtonPreference.showButton(false);
        View containerWithoutWidget = mViewHolder.findViewById(
                com.android.car.ui.R.id.car_ui_preference_container_without_widget);
        View actionContainer = mButtonPreference.getWidgetActionContainer(mViewHolder);
        View widgetFrame = mViewHolder.findViewById(android.R.id.widget_frame);

        mButtonPreference.onBindViewHolder(mViewHolder);

        assertThat(mViewHolder.itemView.isFocusable()).isTrue();
        assertThat(containerWithoutWidget.isClickable()).isFalse();
        assertThat(containerWithoutWidget.isFocusable()).isFalse();
        assertThat(actionContainer.getVisibility()).isEqualTo(View.GONE);
        assertThat(widgetFrame.hasOnClickListeners()).isFalse();
        assertThat(widgetFrame.isFocusable()).isFalse();
    }
}
