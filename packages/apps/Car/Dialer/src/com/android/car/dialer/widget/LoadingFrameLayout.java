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

package com.android.car.dialer.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import com.android.car.apps.common.util.ViewUtils;
import com.android.car.dialer.Constants;
import com.android.car.dialer.R;
import com.android.car.dialer.log.L;

/**
 * A widget that supports different {@link State}s: NEW, LOADING, CONTENT, EMPTY OR ERROR.
 */
public class LoadingFrameLayout extends FrameLayout {
    private static final String TAG = "CD.LoadingFrameLayout";

    /**
     * Possible states of a service request display.
     */
    @IntDef({State.NEW, State.LOADING, State.CONTENT, State.ERROR, State.EMPTY})
    public @interface State {
        int NEW = 0;
        int LOADING = 1;
        int CONTENT = 2;
        int ERROR = 3;
        int EMPTY = 4;
    }

    private final Context mContext;

    private ViewContainer mEmptyView;
    private ViewContainer mLoadingView;
    private ViewContainer mErrorView;

    @State
    private int mState = State.NEW;

    public LoadingFrameLayout(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public LoadingFrameLayout(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mContext = context;
        TypedArray values =
                context.obtainStyledAttributes(attrs, R.styleable.LoadingFrameLayout, defStyle, 0);
        setLoadingView(
                values.getResourceId(
                        R.styleable.LoadingFrameLayout_progressViewLayout,
                        R.layout.loading_progress_view));
        setEmptyView(
                values.getResourceId(
                        R.styleable.LoadingFrameLayout_emptyViewLayout,
                        R.layout.loading_info_view));
        setErrorView(
                values.getResourceId(
                        R.styleable.LoadingFrameLayout_errorViewLayout,
                        R.layout.loading_info_view));
        values.recycle();
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        // Start with a loading view when inflated from XML.
        showLoading();
    }

    private void setLoadingView(int loadingLayoutId) {
        mLoadingView = new ViewContainer(State.LOADING, loadingLayoutId, 0, 0, 0, 0);
    }

    private void setEmptyView(int emptyLayoutId) {
        mEmptyView = new ViewContainer(State.EMPTY, emptyLayoutId, R.id.loading_info_icon,
                R.id.loading_info_message, R.id.loading_info_secondary_message,
                R.id.loading_info_action_button);
    }

    private void setErrorView(int errorLayoutId) {
        mErrorView = new ViewContainer(State.ERROR, errorLayoutId, R.id.loading_info_icon,
                R.id.loading_info_message, R.id.loading_info_secondary_message,
                R.id.loading_info_action_button);
    }

    /**
     * Shows the loading view, hides other views.
     */
    @MainThread
    public void showLoading() {
        switchTo(State.LOADING);
    }

    /**
     * Shows the error view where the action button is not available and hides other views.
     *
     * @param iconResId             drawable resource id used for the top icon. When it is invalid,
     *                              hide the icon view.
     * @param messageResId          string resource id used for the error message. When it is
     *                              invalid, hide the message view.
     * @param secondaryMessageResId string resource id for the secondary error message. When it is
     *                              invalid, hide the secondary message view.
     */
    public void showError(@DrawableRes int iconResId, @StringRes int messageResId,
            @StringRes int secondaryMessageResId) {
        showError(iconResId, messageResId, secondaryMessageResId, Constants.INVALID_RES_ID, null,
                false);
    }

    /**
     * Shows the error view, hides other views.
     *
     * @param iconResId                   drawable resource id used for the top icon.When it is
     *                                    invalid, hide the icon view.
     * @param messageResId                string resource id used for the error message. When it is
     *                                    invalid, hide the message view.
     * @param secondaryMessageResId       string resource id for the secondary error message. When
     *                                    it is invalid, hide the secondary message view.
     * @param actionButtonTextResId       string resource id for the action button.
     * @param actionButtonOnClickListener click listener set on the action button.
     * @param showActionButton            boolean flag if the action button will show.
     */
    public void showError(
            @DrawableRes int iconResId,
            @StringRes int messageResId,
            @StringRes int secondaryMessageResId,
            @StringRes int actionButtonTextResId,
            View.OnClickListener actionButtonOnClickListener,
            boolean showActionButton) {
        mErrorView.setIcon(iconResId);
        mErrorView.setMessage(messageResId);
        mErrorView.setSecondaryMessage(secondaryMessageResId);
        mErrorView.setActionButtonText(actionButtonTextResId);
        mErrorView.setActionButtonClickListener(actionButtonOnClickListener);
        mErrorView.setActionButtonVisible(showActionButton);
        switchTo(State.ERROR);
    }

    /**
     * Shows the empty view where the action button is not available and hides other views.
     *
     * @param iconResId             drawable resource id used for the top icon. When it is invalid,
     *                              hide the icon view.
     * @param messageResId          string resource id used for the empty message. When it is
     *                              invalid, hide the message view.
     * @param secondaryMessageResId string resource id for the secondary empty message. When it is
     *                              invalid, hide the secondary message view.
     */
    public void showEmpty(@DrawableRes int iconResId, @StringRes int messageResId,
            @StringRes int secondaryMessageResId) {
        showEmpty(iconResId, messageResId, secondaryMessageResId, Constants.INVALID_RES_ID, null,
                false);
    }

    /**
     * Shows the empty view and hides other views.
     *
     * @param iconResId                   drawable resource id used for the top icon.When it is
     *                                    invalid, hide the icon view.
     * @param messageResId                string resource id used for the empty message. When it is
     *                                    invalid, hide the message view.
     * @param secondaryMessageResId       string resource id for the secondary empty message. When
     *                                    it is invalid, hide the secondary message view.
     * @param actionButtonTextResId       string resource id for the action button.
     * @param actionButtonOnClickListener click listener set on the action button.
     * @param showActionButton            boolean flag if the action button will show.
     */
    public void showEmpty(
            @DrawableRes int iconResId,
            @StringRes int messageResId,
            @StringRes int secondaryMessageResId,
            @StringRes int actionButtonTextResId,
            @Nullable View.OnClickListener actionButtonOnClickListener,
            boolean showActionButton) {
        mEmptyView.setIcon(iconResId);
        mEmptyView.setMessage(messageResId);
        mEmptyView.setSecondaryMessage(secondaryMessageResId);
        mEmptyView.setActionButtonText(actionButtonTextResId);
        mEmptyView.setActionButtonClickListener(actionButtonOnClickListener);
        mEmptyView.setActionButtonVisible(showActionButton);
        switchTo(State.EMPTY);
    }

    /**
     * Shows the content view, hides other views.
     */
    public void showContent() {
        switchTo(State.CONTENT);
    }

    /**
     * Hide all views.
     */
    public void reset() {
        switchTo(State.NEW);
    }

    private void switchTo(@State int state) {
        if (mState != state) {
            L.d(TAG, "Switch to state: %d", state);
            // Hides, or shows, all the children, including the loading and error views.
            ViewUtils.setVisible((View) findViewById(R.id.list_view), state == State.CONTENT);

            // Corrects the visibility setting for error and loading views since they are
            // shown independently of the views content.
            mLoadingView.setVisibilityFromState(state);
            mErrorView.setVisibilityFromState(state);
            mEmptyView.setVisibilityFromState(state);

            mState = state;
        }
    }

    /**
     * Container for views held by this LoadingFrameLayout. Used for deferring view inflation until
     * the view is about to be shown.
     */
    private class ViewContainer {

        @State
        private final int mViewState;
        private final int mLayoutId;
        private final int mIconViewId;
        private final int mMessageViewId;
        private final int mSecondaryMessageViewId;
        private final int mActionButtonId;

        private View mView;

        private ImageView mIconView;
        // Cache image view resource id until imageView is inflated.
        @DrawableRes
        private int mIconResId;

        private TextView mActionButton;
        // Cache action button visibility until action button is inflated.
        private boolean mIsActionButtonVisible;
        // Cache action button onClickListener until action button is inflated.
        private View.OnClickListener mActionButtonOnClickListener;
        // Cache action button text until action button is inflated.
        private int mActionButtonTextResId;

        private TextView mMessageView;
        // Cache message view string until message view is inflated.
        private int mMessageResId;
        private TextView mSecondaryMessageView;
        // Cache the secondary message view string until the secondary message view is inflated.
        private int mSecondaryMessageResId;

        private ViewContainer(@State int state, @LayoutRes int layoutId, @IdRes int iconViewId,
                @IdRes int messageViewId, @IdRes int secondaryMessageViewId,
                @IdRes int actionButtonId) {
            mViewState = state;
            mLayoutId = layoutId;
            mIconViewId = iconViewId;
            mMessageViewId = messageViewId;
            mSecondaryMessageViewId = secondaryMessageViewId;
            mActionButtonId = actionButtonId;
        }

        private View inflateView() {
            View view = LayoutInflater.from(mContext).inflate(mLayoutId, LoadingFrameLayout.this,
                    false);

            if (mMessageViewId > Constants.INVALID_RES_ID) {
                mMessageView = view.findViewById(mMessageViewId);
                setMessage(mMessageResId);
            }

            if (mSecondaryMessageViewId > Constants.INVALID_RES_ID) {
                mSecondaryMessageView = view.findViewById(mSecondaryMessageViewId);
                setSecondaryMessage(mSecondaryMessageResId);
            }

            if (mIconViewId > Constants.INVALID_RES_ID) {
                mIconView = view.findViewById(mIconViewId);
                setIcon(mIconResId);
            }

            if (mActionButtonId > Constants.INVALID_RES_ID) {
                mActionButton = view.findViewById(mActionButtonId);
                setActionButtonClickListener(mActionButtonOnClickListener);
                setActionButtonVisible(mIsActionButtonVisible);
                setActionButtonText(mActionButtonTextResId);
            }

            return view;
        }

        public void setVisibilityFromState(@State int newState) {
            if (mViewState == newState) {
                show();
            } else {
                hide();
            }
        }

        private void show() {
            if (mView == null) {
                mView = inflateView();
                LoadingFrameLayout.this.addView(mView);
            }
            mView.setVisibility(View.VISIBLE);
        }

        private void hide() {
            if (mView != null) {
                mView.setVisibility(View.GONE);
                mView.clearFocus();
            }
        }

        private void setMessage(@StringRes int messageResId) {
            if (messageResId > Constants.INVALID_RES_ID) {
                ViewUtils.setText(mMessageView, messageResId);
            } else {
                ViewUtils.setVisible(mMessageView, false);
            }
            mMessageResId = messageResId;
        }

        private void setSecondaryMessage(@StringRes int secondaryMessageResId) {
            if (secondaryMessageResId > Constants.INVALID_RES_ID) {
                ViewUtils.setText(mSecondaryMessageView, secondaryMessageResId);
            } else {
                ViewUtils.setVisible(mSecondaryMessageView, false);
            }
            mSecondaryMessageResId = secondaryMessageResId;
        }

        private void setActionButtonClickListener(
                View.OnClickListener actionButtonOnClickListener) {
            ViewUtils.setOnClickListener(mActionButton, actionButtonOnClickListener);
            mActionButtonOnClickListener = actionButtonOnClickListener;
        }

        private void setActionButtonText(@StringRes int actionButtonTextResId) {
            if (actionButtonTextResId > Constants.INVALID_RES_ID) {
                ViewUtils.setText(mActionButton, actionButtonTextResId);
            }
            mActionButtonTextResId = actionButtonTextResId;
        }

        private void setActionButtonVisible(boolean visible) {
            ViewUtils.setVisible(mActionButton, visible);
            mIsActionButtonVisible = visible;
        }

        private void setIcon(@DrawableRes int iconResId) {
            if (iconResId > Constants.INVALID_RES_ID) {
                if (mIconView != null) {
                    mIconView.setImageResource(iconResId);
                }
            } else {
                ViewUtils.setVisible(mIconView, false);
            }
            mIconResId = iconResId;
        }
    }
}
