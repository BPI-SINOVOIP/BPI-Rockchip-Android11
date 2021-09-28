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

package com.android.car.settings.common;

import static com.android.car.settings.common.ActionButtonsPreference.ActionButtons;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.preference.PreferenceViewHolder;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.R;
import com.android.dx.mockito.inline.extended.ExtendedMockito;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoSession;

@RunWith(AndroidJUnit4.class)
public class ActionButtonsPreferenceTest {

    private Context mContext = ApplicationProvider.getApplicationContext();
    private View mRootView;
    private ActionButtonsPreference mPref;
    private PreferenceViewHolder mHolder;
    private MockitoSession mSession;

    @Before
    public void setUp() {
        mRootView = View.inflate(mContext, R.layout.action_buttons_preference, /* parent= */ null);
        mHolder = PreferenceViewHolder.createInstanceForTests(mRootView);
        mPref = new ActionButtonsPreference(mContext);

        mSession = ExtendedMockito.mockitoSession().mockStatic(Toast.class).startMocking();
    }

    @After
    public void tearDown() {
        if (mSession != null) {
            mSession.finishMocking();
        }
    }

    @Test
    public void onBindViewHolder_setTitle_shouldShowButtonByDefault() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label);
        mPref.getButton(ActionButtons.BUTTON2).setText(R.string.settings_label);
        mPref.getButton(ActionButtons.BUTTON3).setText(R.string.settings_label);
        mPref.getButton(ActionButtons.BUTTON4).setText(R.string.settings_label);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    public void onBindViewHolder_setIcon_shouldShowButtonByDefault() {
        mPref.getButton(ActionButtons.BUTTON1).setIcon(R.drawable.ic_lock);
        mPref.getButton(ActionButtons.BUTTON2).setIcon(R.drawable.ic_lock);
        mPref.getButton(ActionButtons.BUTTON3).setIcon(R.drawable.ic_lock);
        mPref.getButton(ActionButtons.BUTTON4).setIcon(R.drawable.ic_lock);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    public void onBindViewHolder_notSetTitleOrIcon_shouldNotShowButtonByDefault() {
        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    public void onBindViewHolder_setVisibleIsGoneAndSetTitle_shouldNotShowButton() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON2).setText(R.string.settings_label).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON3).setText(R.string.settings_label).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON4).setText(R.string.settings_label).setVisible(false);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    public void onBindViewHolder_setVisibleIsGoneAndSetIcon_shouldNotShowButton() {
        mPref.getButton(ActionButtons.BUTTON1).setIcon(R.drawable.ic_lock).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON2).setIcon(R.drawable.ic_lock).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON3).setIcon(R.drawable.ic_lock).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON4).setIcon(R.drawable.ic_lock).setVisible(false);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    public void onBindViewHolder_setVisibility_shouldUpdateButtonVisibility() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON2).setText(R.string.settings_label).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON3).setText(R.string.settings_label).setVisible(false);
        mPref.getButton(ActionButtons.BUTTON4).setText(R.string.settings_label).setVisible(false);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.GONE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.GONE);

        mPref.getButton(ActionButtons.BUTTON1).setVisible(true);
        mPref.getButton(ActionButtons.BUTTON2).setVisible(true);
        mPref.getButton(ActionButtons.BUTTON3).setVisible(true);
        mPref.getButton(ActionButtons.BUTTON4).setVisible(true);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button2).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button3).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(mRootView.findViewById(R.id.button4).getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    public void onBindViewHolder_setEnabled_shouldEnableButton() {
        mPref.getButton(ActionButtons.BUTTON1).setEnabled(true);
        mPref.getButton(ActionButtons.BUTTON2).setEnabled(false);
        mPref.getButton(ActionButtons.BUTTON3).setEnabled(true);
        mPref.getButton(ActionButtons.BUTTON4).setEnabled(false);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.button1).isEnabled()).isTrue();
        assertThat(mRootView.findViewById(R.id.button1Icon).isEnabled()).isTrue();
        assertThat(mRootView.findViewById(R.id.button1Text).isEnabled()).isTrue();
        assertThat(mRootView.findViewById(R.id.button2).isEnabled()).isFalse();
        assertThat(mRootView.findViewById(R.id.button2Icon).isEnabled()).isFalse();
        assertThat(mRootView.findViewById(R.id.button2Text).isEnabled()).isFalse();
        assertThat(mRootView.findViewById(R.id.button3).isEnabled()).isTrue();
        assertThat(mRootView.findViewById(R.id.button3Icon).isEnabled()).isTrue();
        assertThat(mRootView.findViewById(R.id.button3Text).isEnabled()).isTrue();
        assertThat(mRootView.findViewById(R.id.button4).isEnabled()).isFalse();
        assertThat(mRootView.findViewById(R.id.button4Icon).isEnabled()).isFalse();
        assertThat(mRootView.findViewById(R.id.button4Text).isEnabled()).isFalse();
    }

    @Test
    public void onBindViewHolder_setText_shouldShowSameText() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label);
        mPref.getButton(ActionButtons.BUTTON2).setText(R.string.settings_label);
        mPref.getButton(ActionButtons.BUTTON3).setText(R.string.settings_label);
        mPref.getButton(ActionButtons.BUTTON4).setText(R.string.settings_label);

        mPref.onBindViewHolder(mHolder);

        assertThat(((TextView) mRootView.findViewById(R.id.button1Text)).getText())
                .isEqualTo(mContext.getText(R.string.settings_label));
        assertThat(((TextView) mRootView.findViewById(R.id.button2Text)).getText())
                .isEqualTo(mContext.getText(R.string.settings_label));
        assertThat(((TextView) mRootView.findViewById(R.id.button3Text)).getText())
                .isEqualTo(mContext.getText(R.string.settings_label));
        assertThat(((TextView) mRootView.findViewById(R.id.button4Text)).getText())
                .isEqualTo(mContext.getText(R.string.settings_label));
    }

    @Test
    public void onBindViewHolder_setButtonIcon_iconMustDisplayAboveText() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label).setIcon(
                R.drawable.ic_lock);

        mPref.onBindViewHolder(mHolder);
        Drawable icon = ((ImageView) mRootView.findViewById(R.id.button1Icon)).getDrawable();

        assertThat(icon).isNotNull();
    }

    @Test
    public void onButtonClicked_shouldOnlyTriggerListenerIfEnabled() {
        mPref.getButton(ActionButtons.BUTTON1).setEnabled(true);
        mPref.getButton(ActionButtons.BUTTON2).setEnabled(false);

        View.OnClickListener enabledListener = mock(View.OnClickListener.class);
        View.OnClickListener disabledListener = mock(View.OnClickListener.class);
        mPref.getButton(ActionButtons.BUTTON1).setOnClickListener(enabledListener);
        mPref.getButton(ActionButtons.BUTTON2).setOnClickListener(disabledListener);

        mPref.onBindViewHolder(mHolder);

        mPref.getButton(ActionButtons.BUTTON1).performClick(null);
        verify(enabledListener).onClick(any());

        mPref.getButton(ActionButtons.BUTTON2).performClick(null);
        verify(disabledListener, never()).onClick(any());
    }

    @Test
    @UiThreadTest
    public void onButtonClicked_makesToastIfPreferenceRestricted() {
        Toast mockToast = mock(Toast.class);
        ExtendedMockito.when(Toast.makeText(any(), anyString(), anyInt())).thenReturn(mockToast);

        mPref.setUxRestricted(true);
        mPref.getButton(ActionButtons.BUTTON1).setEnabled(true);

        View.OnClickListener listener = mock(View.OnClickListener.class);
        mPref.getButton(ActionButtons.BUTTON1).setOnClickListener(listener);

        mPref.onBindViewHolder(mHolder);

        mPref.getButton(ActionButtons.BUTTON1).performClick(null);
        verify(listener, never()).onClick(any());

        verify(mockToast).show();
    }

    @Test
    @UiThreadTest
    public void onButtonClicked_disabled_uxRestricted_shouldDoNothing() {
        mPref.setUxRestricted(true);
        mPref.getButton(ActionButtons.BUTTON1).setEnabled(false);

        View.OnClickListener listener = mock(View.OnClickListener.class);
        mPref.getButton(ActionButtons.BUTTON1).setOnClickListener(listener);

        mPref.onBindViewHolder(mHolder);

        mPref.getButton(ActionButtons.BUTTON1).performClick(null);
        verify(listener, never()).onClick(any());

        ExtendedMockito.verify(
                () -> Toast.makeText(any(), anyString(), anyInt()), never());
    }

    @Test
    public void setButtonIcon_iconResourceIdIsZero_shouldNotDisplayIcon() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label).setIcon(0);

        mPref.onBindViewHolder(mHolder);
        Drawable icon = ((ImageView) mRootView.findViewById(R.id.button1Icon)).getDrawable();

        assertThat(icon).isNull();
    }

    @Test
    public void setButtonIcon_iconResourceIdNotExisting_shouldNotDisplayIconAndCrash() {
        mPref.getButton(ActionButtons.BUTTON1).setText(R.string.settings_label).setIcon(
                999999999 /* not existing id */);
        // Should not crash here
        mPref.onBindViewHolder(mHolder);
        Drawable icon = ((ImageView) mRootView.findViewById(R.id.button1Icon)).getDrawable();

        assertThat(icon).isNull();
    }
}
