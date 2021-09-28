/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.cts.mockime;

import android.os.Bundle;
import android.os.PersistableBundle;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An immutable data store to control the behavior of {@link MockIme}.
 */
public class ImeSettings {

    @NonNull
    private final String mClientPackageName;

    @NonNull
    private final String mEventCallbackActionName;

    private static final String EVENT_CALLBACK_INTENT_ACTION_KEY = "eventCallbackActionName";
    private static final String DATA_KEY = "data";

    private static final String BACKGROUND_COLOR_KEY = "BackgroundColor";
    private static final String NAVIGATION_BAR_COLOR_KEY = "NavigationBarColor";
    private static final String INPUT_VIEW_HEIGHT =
            "InputViewHeightWithoutSystemWindowInset";
    private static final String DRAWS_BEHIND_NAV_BAR = "drawsBehindNavBar";
    private static final String WINDOW_FLAGS = "WindowFlags";
    private static final String WINDOW_FLAGS_MASK = "WindowFlagsMask";
    private static final String FULLSCREEN_MODE_ALLOWED = "FullscreenModeAllowed";
    private static final String INPUT_VIEW_SYSTEM_UI_VISIBILITY = "InputViewSystemUiVisibility";
    private static final String WATERMARK_ENABLED = "WatermarkEnabled";
    private static final String HARD_KEYBOARD_CONFIGURATION_BEHAVIOR_ALLOWED =
            "HardKeyboardConfigurationBehaviorAllowed";
    private static final String INLINE_SUGGESTIONS_ENABLED = "InlineSuggestionsEnabled";
    private static final String INLINE_SUGGESTION_VIEW_CONTENT_DESC =
            "InlineSuggestionViewContentDesc";
    private static final String STRICT_MODE_ENABLED = "StrictModeEnabled";

    @NonNull
    private final PersistableBundle mBundle;

    ImeSettings(@NonNull String clientPackageName, @NonNull Bundle bundle) {
        mClientPackageName = clientPackageName;
        mEventCallbackActionName = bundle.getString(EVENT_CALLBACK_INTENT_ACTION_KEY);
        mBundle = bundle.getParcelable(DATA_KEY);
    }

    @Nullable
    String getEventCallbackActionName() {
        return mEventCallbackActionName;
    }

    @NonNull
    String getClientPackageName() {
        return mClientPackageName;
    }

    public boolean fullscreenModeAllowed(boolean defaultValue) {
        return mBundle.getBoolean(FULLSCREEN_MODE_ALLOWED, defaultValue);
    }

    @ColorInt
    public int getBackgroundColor(@ColorInt int defaultColor) {
        return mBundle.getInt(BACKGROUND_COLOR_KEY, defaultColor);
    }

    public boolean hasNavigationBarColor() {
        return mBundle.keySet().contains(NAVIGATION_BAR_COLOR_KEY);
    }

    @ColorInt
    public int getNavigationBarColor() {
        return mBundle.getInt(NAVIGATION_BAR_COLOR_KEY);
    }

    public int getInputViewHeight(int defaultHeight) {
        return mBundle.getInt(INPUT_VIEW_HEIGHT, defaultHeight);
    }

    public boolean getDrawsBehindNavBar() {
        return mBundle.getBoolean(DRAWS_BEHIND_NAV_BAR, false);
    }

    public int getWindowFlags(int defaultFlags) {
        return mBundle.getInt(WINDOW_FLAGS, defaultFlags);
    }

    public int getWindowFlagsMask(int defaultFlags) {
        return mBundle.getInt(WINDOW_FLAGS_MASK, defaultFlags);
    }

    public int getInputViewSystemUiVisibility(int defaultFlags) {
        return mBundle.getInt(INPUT_VIEW_SYSTEM_UI_VISIBILITY, defaultFlags);
    }

    public boolean isWatermarkEnabled(boolean defaultValue) {
        return mBundle.getBoolean(WATERMARK_ENABLED, defaultValue);
    }

    public boolean getHardKeyboardConfigurationBehaviorAllowed(boolean defaultValue) {
        return mBundle.getBoolean(HARD_KEYBOARD_CONFIGURATION_BEHAVIOR_ALLOWED, defaultValue);
    }

    public boolean getInlineSuggestionsEnabled() {
        return mBundle.getBoolean(INLINE_SUGGESTIONS_ENABLED);
    }

    @Nullable
    public String getInlineSuggestionViewContentDesc(@Nullable String defaultValue) {
        return mBundle.getString(INLINE_SUGGESTION_VIEW_CONTENT_DESC, defaultValue);
    }

    public boolean isStrictModeEnabled() {
        return mBundle.getBoolean(STRICT_MODE_ENABLED, false);
    }

    static Bundle serializeToBundle(@NonNull String eventCallbackActionName,
            @Nullable Builder builder) {
        final Bundle result = new Bundle();
        result.putString(EVENT_CALLBACK_INTENT_ACTION_KEY, eventCallbackActionName);
        result.putParcelable(DATA_KEY, builder != null ? builder.mBundle : PersistableBundle.EMPTY);
        return result;
    }

    /**
     * The builder class for {@link ImeSettings}.
     */
    public static final class Builder {
        private final PersistableBundle mBundle = new PersistableBundle();

        /**
         * Controls whether fullscreen mode is allowed or not.
         *
         * <p>By default, fullscreen mode is not allowed in {@link MockIme}.</p>
         *
         * @param allowed {@code true} if fullscreen mode is allowed
         * @see MockIme#onEvaluateFullscreenMode()
         */
        public Builder setFullscreenModeAllowed(boolean allowed) {
            mBundle.putBoolean(FULLSCREEN_MODE_ALLOWED, allowed);
            return this;
        }

        /**
         * Sets the background color of the {@link MockIme}.
         * @param color background color to be used
         */
        public Builder setBackgroundColor(@ColorInt int color) {
            mBundle.putInt(BACKGROUND_COLOR_KEY, color);
            return this;
        }

        /**
         * Sets the color to be passed to {@link android.view.Window#setNavigationBarColor(int)}.
         *
         * @param color color to be passed to {@link android.view.Window#setNavigationBarColor(int)}
         * @see android.view.View
         */
        public Builder setNavigationBarColor(@ColorInt int color) {
            mBundle.putInt(NAVIGATION_BAR_COLOR_KEY, color);
            return this;
        }

        /**
         * Sets the input view height measured from the bottom of the screen.
         *
         * @param height height of the soft input view. This includes the system window inset such
         *               as navigation bar.
         */
        public Builder setInputViewHeight(int height) {
            mBundle.putInt(INPUT_VIEW_HEIGHT, height);
            return this;
        }

        /**
         * Sets whether IME draws behind navigation bar.
         */
        public Builder setDrawsBehindNavBar(boolean drawsBehindNavBar) {
            mBundle.putBoolean(DRAWS_BEHIND_NAV_BAR, drawsBehindNavBar);
            return this;
        }

        /**
         * Sets window flags to be specified to {@link android.view.Window#setFlags(int, int)} of
         * the main {@link MockIme} window.
         *
         * <p>When {@link android.view.WindowManager.LayoutParams#FLAG_LAYOUT_IN_OVERSCAN} is set,
         * {@link MockIme} tries to render the navigation bar by itself.</p>
         *
         * @param flags flags to be specified
         * @param flagsMask mask bits that specify what bits need to be cleared before setting
         *                  {@code flags}
         * @see android.view.WindowManager
         */
        public Builder setWindowFlags(int flags, int flagsMask) {
            mBundle.putInt(WINDOW_FLAGS, flags);
            mBundle.putInt(WINDOW_FLAGS_MASK, flagsMask);
            return this;
        }

        /**
         * Sets flags to be specified to {@link android.view.View#setSystemUiVisibility(int)} of
         * the main soft input view (the returned view from {@link MockIme#onCreateInputView()}).
         *
         * @param visibilityFlags flags to be specified
         * @see android.view.View
         */
        public Builder setInputViewSystemUiVisibility(int visibilityFlags) {
            mBundle.putInt(INPUT_VIEW_SYSTEM_UI_VISIBILITY, visibilityFlags);
            return this;
        }

        /**
         * Sets whether a unique watermark image needs to be shown on the software keyboard or not.
         *
         * <p>This needs to be enabled to use</p>
         *
         * @param enabled {@code true} when such a watermark image is requested.
         */
        public Builder setWatermarkEnabled(boolean enabled) {
            mBundle.putBoolean(WATERMARK_ENABLED, enabled);
            return this;
        }

        /**
         * Controls whether {@link MockIme} is allowed to change the behavior based on
         * {@link android.content.res.Configuration#keyboard} and
         * {@link android.content.res.Configuration#hardKeyboardHidden}.
         *
         * <p>Methods in {@link android.inputmethodservice.InputMethodService} such as
         * {@link android.inputmethodservice.InputMethodService#onEvaluateInputViewShown()} and
         * {@link android.inputmethodservice.InputMethodService#onShowInputRequested(int, boolean)}
         * change their behaviors when a hardware keyboard is attached.  This is confusing when
         * writing tests so by default {@link MockIme} tries to cancel those behaviors.  This
         * settings re-enables such a behavior.</p>
         *
         * @param allowed {@code true} when {@link MockIme} is allowed to change the behavior when
         *                a hardware keyboard is attached
         *
         * @see android.inputmethodservice.InputMethodService#onEvaluateInputViewShown()
         * @see android.inputmethodservice.InputMethodService#onShowInputRequested(int, boolean)
         */
        public Builder setHardKeyboardConfigurationBehaviorAllowed(boolean allowed) {
            mBundle.putBoolean(HARD_KEYBOARD_CONFIGURATION_BEHAVIOR_ALLOWED, allowed);
            return this;
        }

        /**
         * Controls whether inline suggestions are enabled for {@link MockIme}. If enabled, a
         * suggestion strip will be rendered at the top of the keyboard.
         *
         * @param enabled {@code true} when {@link MockIme} is enabled to show inline suggestions.
         */
        public Builder setInlineSuggestionsEnabled(boolean enabled) {
            mBundle.putBoolean(INLINE_SUGGESTIONS_ENABLED, enabled);
            return this;
        }

        /**
         * Controls whether inline suggestions are enabled for {@link MockIme}. If enabled, a
         * suggestion strip will be rendered at the top of the keyboard.
         *
         * @param contentDesc content description to be set to the inline suggestion View.
         */
        public Builder setInlineSuggestionViewContentDesc(@NonNull String contentDesc) {
            mBundle.putString(INLINE_SUGGESTION_VIEW_CONTENT_DESC, contentDesc);
            return this;
        }

        /** Sets whether to enable {@link android.os.StrictMode} or not. */
        public Builder setStrictModeEnabled(boolean enabled) {
            mBundle.putBoolean(STRICT_MODE_ENABLED, enabled);
            return this;
        }
    }
}
