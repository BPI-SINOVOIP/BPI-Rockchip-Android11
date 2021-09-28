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

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import com.android.car.settings.R;
import com.android.car.ui.preference.CarUiPreference;

/**
 * Preference that provides a four button layout.
 * -----------------------------------------
 * | button1 | button2 | button3 | button4 |
 * -----------------------------------------
 *
 * Adapted from {@link com.android.settingslib.widget.ActionButtonsPreference} for CarSettings.
 *
 * This widget also has custom divider elements above and below, which can be customized with the
 * Preference_allowDividerAbove and Preference_allowDividerBelow attributes. The dividers are
 * enabled by default.
 */
public class ActionButtonsPreference extends CarUiPreference implements
        ActionButtonInfo.ButtonInfoChangeListener {

    /**
     * Identifier enum for the four different action buttons.
     */
    public enum ActionButtons {
        BUTTON1,
        BUTTON2,
        BUTTON3,
        BUTTON4
    }

    private ActionButtonInfo mButton1Info;
    private ActionButtonInfo mButton2Info;
    private ActionButtonInfo mButton3Info;
    private ActionButtonInfo mButton4Info;

    private boolean mAllowDividerAbove;
    private boolean mAllowDividerBelow;

    public ActionButtonsPreference(Context context, AttributeSet attrs,
            int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(context, attrs);
    }

    public ActionButtonsPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    public ActionButtonsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public ActionButtonsPreference(Context context) {
        super(context);
        init(context, /* attrs= */ null);
    }

    private void init(Context context, AttributeSet attrs) {
        setLayoutResource(R.layout.action_buttons_preference);
        setSelectable(false);

        TypedArray a = context.obtainStyledAttributes(attrs,
                R.styleable.Preference);
        mAllowDividerAbove = a.getBoolean(R.styleable.Preference_allowDividerAbove,
                /* defValue= */ true);
        mAllowDividerBelow = a.getBoolean(R.styleable.Preference_allowDividerBelow,
                /* defValue= */ true);
        mButton1Info = new ActionButtonInfo(context, /* changeListener= */ this);
        mButton2Info = new ActionButtonInfo(context, /* changeListener= */ this);
        mButton3Info = new ActionButtonInfo(context, /* changeListener= */ this);
        mButton4Info = new ActionButtonInfo(context, /* changeListener= */ this);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        holder.findViewById(R.id.topDivider).setVisibility(
                mAllowDividerAbove ? View.VISIBLE : View.GONE);
        holder.findViewById(R.id.bottomDivider).setVisibility(
                mAllowDividerBelow ? View.VISIBLE : View.GONE);

        mButton1Info
                .setButtonView(holder.findViewById(R.id.button1))
                .setButtonTextView((TextView) holder.findViewById(R.id.button1Text))
                .setButtonIconView((ImageView) holder.findViewById(R.id.button1Icon))
                .setPreferenceRestricted(isUxRestricted());

        mButton2Info
                .setButtonView(holder.findViewById(R.id.button2))
                .setButtonTextView((TextView) holder.findViewById(R.id.button2Text))
                .setButtonIconView((ImageView) holder.findViewById(R.id.button2Icon))
                .setPreferenceRestricted(isUxRestricted());

        mButton3Info
                .setButtonView(holder.findViewById(R.id.button3))
                .setButtonTextView((TextView) holder.findViewById(R.id.button3Text))
                .setButtonIconView((ImageView) holder.findViewById(R.id.button3Icon))
                .setPreferenceRestricted(isUxRestricted());

        mButton4Info
                .setButtonView(holder.findViewById(R.id.button4))
                .setButtonTextView((TextView) holder.findViewById(R.id.button4Text))
                .setButtonIconView((ImageView) holder.findViewById(R.id.button4Icon))
                .setPreferenceRestricted(isUxRestricted());

        mButton1Info.setUpButton();
        mButton2Info.setUpButton();
        mButton3Info.setUpButton();
        mButton4Info.setUpButton();
    }

    @Override
    public void onButtonInfoChange(ActionButtonInfo buttonInfo) {
        notifyChanged();
    }

    /**
     * Retrieve the specified ActionButtonInfo based on the ActionButtons enum.
     */
    public ActionButtonInfo getButton(ActionButtons button) {
        switch(button) {
            case BUTTON1:
                return mButton1Info;
            case BUTTON2:
                return mButton2Info;
            case BUTTON3:
                return mButton3Info;
            case BUTTON4:
                return mButton4Info;
            default:
                throw new IllegalArgumentException("Invalid button requested");
        }
    }
}
