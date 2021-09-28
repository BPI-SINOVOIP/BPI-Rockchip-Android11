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

import android.content.Context;
import android.view.View;

import androidx.preference.PreferenceViewHolder;

import com.android.car.settings.common.PasswordEditTextPreference;
import com.android.car.ui.R;
import com.android.car.ui.utils.CarUiUtils;

/**
 * A {@link PasswordEditTextPreference} which has a second button which can perform another
 * action defined by {@link OnButtonClickListener}.
 */
public class ButtonPasswordEditTextPreference extends PasswordEditTextPreference {

    private OnButtonClickListener mOnButtonClickListener;

    private boolean mIsButtonShown = true;

    public ButtonPasswordEditTextPreference(Context context) {
        super(context);
        init();
    }

    private void init() {
        setTwoActionLayout();
    }

    /**
     * Sets whether the secondary button is visible in the preference.
     *
     * @param isShown {@code true} if the secondary button should be shown.
     */
    public void showButton(boolean isShown) {
        mIsButtonShown = isShown;
        notifyChanged();
    }

    /** Returns {@code true} if action is shown. */
    public boolean isButtonShown() {
        return mIsButtonShown;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        View containerWithoutWidget = CarUiUtils.findViewByRefId(holder.itemView,
                R.id.car_ui_preference_container_without_widget);
        View actionContainer = getWidgetActionContainer(holder);
        View widgetFrame = holder.findViewById(android.R.id.widget_frame);
        holder.itemView.setFocusable(!mIsButtonShown);
        containerWithoutWidget.setOnClickListener(mIsButtonShown ? this::performClick : null);
        containerWithoutWidget.setClickable(mIsButtonShown);
        containerWithoutWidget.setFocusable(mIsButtonShown);
        actionContainer.setVisibility(mIsButtonShown ? View.VISIBLE : View.GONE);
        widgetFrame.setOnClickListener(mIsButtonShown ? v -> performButtonClick() : null);
        widgetFrame.setFocusable(mIsButtonShown);
    }

    /**
     * Sets an {@link OnButtonClickListener} to be invoked when the button is clicked.
     */
    public void setOnButtonClickListener(OnButtonClickListener listener) {
        mOnButtonClickListener = listener;
    }

    /** Virtually clicks the button contained inside this preference. */
    public void performButtonClick() {
        if (isButtonShown()) {
            if (mOnButtonClickListener != null) {
                mOnButtonClickListener.onButtonClick(this);
            }
        }
    }

    /** Callback to be invoked when the button is clicked. */
    public interface OnButtonClickListener {
        /**
         * Called when a button has been clicked.
         *
         * @param preference the preference whose button was clicked.
         */
        void onButtonClick(ButtonPasswordEditTextPreference preference);
    }
}
