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
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import com.android.car.settings.R;

import java.lang.ref.WeakReference;

/**
 * Class representing a button item for an {@link ActionButtonsPreference}
 */
public class ActionButtonInfo {
    private static final Logger LOG = new Logger(ActionButtonInfo.class);
    private final Context mContext;
    private View mButtonView;
    private ImageView mButtonIconView;
    private TextView mButtonTextView;
    private CharSequence mText;
    private Drawable mIcon;
    private View.OnClickListener mListener;
    private boolean mIsPreferenceRestricted = false;
    private boolean mIsEnabled = true;
    private boolean mIsVisible = true;
    private WeakReference<ButtonInfoChangeListener> mButtonInfoChangeListener;
    private String mMessageToShowWhenUxRestrictedPreferenceClicked;

    ActionButtonInfo(Context context, ButtonInfoChangeListener changeListener) {
        mContext = context;
        mButtonInfoChangeListener = new WeakReference<>(changeListener);
        mMessageToShowWhenUxRestrictedPreferenceClicked = context.getString(
                R.string.car_ui_restricted_while_driving);
    }

    /**
     * Set the visibility state.
     */
    public ActionButtonInfo setVisible(boolean isVisible) {
        if (isVisible != mIsVisible) {
            mIsVisible = isVisible;
            update();
        }
        return this;
    }

    /**
     * Sets the text to be displayed.
     */
    public ActionButtonInfo setText(@StringRes int textResId) {
        final String newText = mContext.getString(textResId);
        if (!TextUtils.equals(newText, mText)) {
            mText = newText;
            update();
        }
        return this;
    }

    /**
     * Sets the drawable to be displayed above of text.
     */
    public ActionButtonInfo setIcon(@DrawableRes int iconResId) {
        if (iconResId == 0) {
            return this;
        }

        final Drawable icon;
        try {
            icon = mContext.getDrawable(iconResId);
            mIcon = icon;
            update();
        } catch (Resources.NotFoundException exception) {
            LOG.e("Resource does not exist: " + iconResId);
        }
        return this;
    }

    /**
     * Set the enabled state.
     */
    public ActionButtonInfo setEnabled(boolean isEnabled) {
        if (isEnabled != mIsEnabled) {
            mIsEnabled = isEnabled;
            update();
        }
        return this;
    }

    /**
     * Register a callback to be invoked when clicked.
     */
    public ActionButtonInfo setOnClickListener(
            View.OnClickListener listener) {
        if (listener != mListener) {
            mListener = listener;
            update();
        }
        return this;
    }

    ActionButtonInfo setButtonView(View view) {
        mButtonView = view;
        return this;
    }

    ActionButtonInfo setButtonTextView(TextView textView) {
        mButtonTextView = textView;
        return this;
    }

    ActionButtonInfo setButtonIconView(ImageView iconView) {
        mButtonIconView = iconView;
        return this;
    }

    ActionButtonInfo setPreferenceRestricted(boolean isRestricted) {
        mIsPreferenceRestricted = isRestricted;
        return this;
    }

    /**
     * Get the current button text.
     */
    @VisibleForTesting
    public CharSequence getText() {
        return mText;
    }

    /**
     * Get the current button icon.
     */
    @VisibleForTesting
    public Drawable getIcon() {
        return mIcon;
    }

    /**
     * Get the current button click listener.
     */
    @VisibleForTesting
    public View.OnClickListener getOnClickListener() {
        return mListener;
    }

    /**
     * Get the current button enabled state.
     */
    @VisibleForTesting
    public boolean isEnabled() {
        return mIsEnabled;
    }

    /**
     * Get the current button visibility.
     */
    @VisibleForTesting
    public boolean isVisible() {
        return shouldBeVisible();
    }

    void setUpButton() {
        mButtonTextView.setText(mText);
        mButtonIconView.setImageDrawable(mIcon);
        mButtonView.setOnClickListener(this::performClick);

        boolean enabled = isEnabled() || mIsPreferenceRestricted;
        mButtonView.setEnabled(enabled);
        mButtonTextView.setEnabled(enabled);
        mButtonIconView.setEnabled(enabled);

        mButtonIconView.setVisibility(mIcon != null ? View.VISIBLE : View.GONE);

        if (shouldBeVisible()) {
            mButtonView.setVisibility(View.VISIBLE);
        } else {
            mButtonView.setVisibility(View.GONE);
        }
    }

    @VisibleForTesting
    void performClick(View v) {
        if (!isEnabled()) {
            return;
        }
        if (mListener == null) {
            return;
        }
        if (mIsPreferenceRestricted) {
            if (!TextUtils.isEmpty(mMessageToShowWhenUxRestrictedPreferenceClicked)) {
                Toast.makeText(mContext, mMessageToShowWhenUxRestrictedPreferenceClicked,
                        Toast.LENGTH_LONG).show();
            }
            return;
        }
        mListener.onClick(v);
    }

    /**
     * By default, four buttons are visible.
     * However, there are two cases which button should be invisible.
     *
     * 1. User set invisible for this button. ex: mIsVisible = false.
     * 2. User didn't set any title or icon.
     */
    private boolean shouldBeVisible() {
        return mIsVisible && (!TextUtils.isEmpty(mText) || mIcon != null);
    }

    private void update() {
        ButtonInfoChangeListener listener = mButtonInfoChangeListener.get();
        if (listener != null) {
            listener.onButtonInfoChange(this);
        }
    }

    /**
     * Listener that is notified when a button has been updated.
     */
    interface ButtonInfoChangeListener {
        void onButtonInfoChange(ActionButtonInfo buttonInfo);
    }
}
