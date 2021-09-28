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

package com.android.car.notification.template;

import android.annotation.CallSuper;
import android.annotation.ColorInt;
import android.annotation.Nullable;
import android.app.Notification;
import android.content.Context;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.ImageButton;

import androidx.annotation.VisibleForTesting;
import androidx.cardview.widget.CardView;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.notification.AlertEntry;
import com.android.car.notification.NotificationClickHandlerFactory;
import com.android.car.notification.NotificationUtils;
import com.android.car.notification.R;

/**
 * The base view holder class that all template view holders should extend.
 */
public abstract class CarNotificationBaseViewHolder extends RecyclerView.ViewHolder {
    private final Context mContext;
    private final NotificationClickHandlerFactory mClickHandlerFactory;

    @Nullable
    private final CardView mCardView; // can be null for group child or group summary notification
    @Nullable
    private final View mInnerView; // can be null for GroupNotificationViewHolder
    @Nullable
    private final CarNotificationHeaderView mHeaderView;
    @Nullable
    private final CarNotificationBodyView mBodyView;
    @Nullable
    private final CarNotificationActionsView mActionsView;
    @Nullable
    private final ImageButton mDismissButton;

    /**
     * Focus change listener to make the dismiss button transparent or opaque depending on whether
     * the card view has focus.
     */
    private final ViewTreeObserver.OnGlobalFocusChangeListener mFocusChangeListener;

    /**
     * Whether to hide the dismiss button. If the bound {@link AlertEntry} is dismissible, a dismiss
     * button will normally be shown when card view has focus. If this field is true, no dismiss
     * button will be shown. This is the case for the group summary notification in a collapsed
     * group.
     */
    private boolean mHideDismissButton;

    @ColorInt
    private final int mDefaultBackgroundColor;
    @ColorInt
    private final int mDefaultCarAccentColor;
    @ColorInt
    private final int mDefaultPrimaryForegroundColor;
    @ColorInt
    private final int mDefaultSecondaryForegroundColor;
    @ColorInt
    private int mCalculatedPrimaryForegroundColor;
    @ColorInt
    private int mCalculatedSecondaryForegroundColor;
    @ColorInt
    private int mSmallIconColor;
    @ColorInt
    private int mBackgroundColor;

    private AlertEntry mAlertEntry;
    private boolean mIsAnimating;
    private boolean mHasColor;
    private boolean mIsColorized;
    private boolean mEnableCardBackgroundColorForCategoryNavigation;
    private boolean mEnableCardBackgroundColorForSystemApp;
    private boolean mEnableSmallIconAccentColor;
    private boolean mAlwaysShowDismissButton;

    /**
     * Tracks if the foreground colors have been calculated for the binding of the view holder.
     * The colors should only be calculated once per binding.
     **/
    private boolean mInitializedColors;

    CarNotificationBaseViewHolder(View itemView,
            NotificationClickHandlerFactory clickHandlerFactory) {
        super(itemView);
        mContext = itemView.getContext();
        mClickHandlerFactory = clickHandlerFactory;
        mCardView = itemView.findViewById(R.id.card_view);
        mInnerView = itemView.findViewById(R.id.inner_template_view);
        mHeaderView = itemView.findViewById(R.id.notification_header);
        mBodyView = itemView.findViewById(R.id.notification_body);
        mActionsView = itemView.findViewById(R.id.notification_actions);
        mDismissButton = itemView.findViewById(R.id.dismiss_button);
        mAlwaysShowDismissButton = mContext.getResources().getBoolean(
                R.bool.config_alwaysShowNotificationDismissButton);
        mFocusChangeListener = (oldFocus, newFocus) -> {
            if (mDismissButton != null && !mAlwaysShowDismissButton) {
                // The dismiss button should only be visible when the focus is on this notification
                // or within it. Use alpha rather than visibility so that focus can move up to the
                // previous notification's dismiss button.
                mDismissButton.setImageAlpha(itemView.hasFocus() ? 255 : 0);
            }
        };
        mDefaultBackgroundColor = NotificationUtils.getAttrColor(mContext,
                android.R.attr.colorPrimary);
        mDefaultCarAccentColor = NotificationUtils.getAttrColor(mContext,
                android.R.attr.colorAccent);
        mDefaultPrimaryForegroundColor = mContext.getColor(R.color.primary_text_color);
        mDefaultSecondaryForegroundColor = mContext.getColor(R.color.secondary_text_color);
        mEnableCardBackgroundColorForCategoryNavigation =
                mContext.getResources().getBoolean(
                        R.bool.config_enableCardBackgroundColorForCategoryNavigation);
        mEnableCardBackgroundColorForSystemApp =
                mContext.getResources().getBoolean(
                        R.bool.config_enableCardBackgroundColorForSystemApp);
        mEnableSmallIconAccentColor =
                mContext.getResources().getBoolean(R.bool.config_enableSmallIconAccentColor);
    }

    /**
     * Binds a {@link AlertEntry} to a notification template. Base class sets the
     * clicking event for the card view and calls recycling methods.
     *
     * @param alertEntry the notification to be bound.
     * @param isInGroup whether this notification is part of a grouped notification.
     */
    @CallSuper
    public void bind(AlertEntry alertEntry, boolean isInGroup, boolean isHeadsUp) {
        reset();
        mAlertEntry = alertEntry;

        if (isInGroup) {
            mInnerView.setBackgroundColor(mDefaultBackgroundColor);
            mInnerView.setOnClickListener(mClickHandlerFactory.getClickHandler(alertEntry));
        } else if (mCardView != null) {
            mCardView.setOnClickListener(mClickHandlerFactory.getClickHandler(alertEntry));
        }
        updateDismissButton(alertEntry, isHeadsUp);

        bindCardView(mCardView, isInGroup);
        bindHeader(mHeaderView, isInGroup);
        bindBody(mBodyView, isInGroup);
    }

    /**
     * Binds a {@link AlertEntry} to a notification template's card.
     *
     * @param cardView the CardView the notification should be bound to.
     * @param isInGroup whether this notification is part of a grouped notification.
     */
    void bindCardView(CardView cardView, boolean isInGroup) {
        initializeColors(isInGroup);

        if (cardView == null) {
            return;
        }

        if (canChangeCardBackgroundColor() && mHasColor && mIsColorized && !isInGroup) {
            cardView.setCardBackgroundColor(mBackgroundColor);
        }
    }

    /**
     * Binds a {@link AlertEntry} to a notification template's header.
     *
     * @param headerView the CarNotificationHeaderView the notification should be bound to.
     * @param isInGroup whether this notification is part of a grouped notification.
     */
    void bindHeader(CarNotificationHeaderView headerView, boolean isInGroup) {
        if (headerView == null) return;
        initializeColors(isInGroup);

        headerView.setSmallIconColor(mSmallIconColor);
        headerView.setHeaderTextColor(mCalculatedPrimaryForegroundColor);
        headerView.setTimeTextColor(mCalculatedPrimaryForegroundColor);
    }

    /**
     * Binds a {@link AlertEntry} to a notification template's body.
     *
     * @param bodyView the CarNotificationBodyView the notification should be bound to.
     * @param isInGroup whether this notification is part of a grouped notification.
     */
    void bindBody(CarNotificationBodyView bodyView,
            boolean isInGroup) {
        if (bodyView == null) return;
        initializeColors(isInGroup);

        bodyView.setPrimaryTextColor(mCalculatedPrimaryForegroundColor);
        bodyView.setSecondaryTextColor(mCalculatedSecondaryForegroundColor);
    }

    private void initializeColors(boolean isInGroup) {
        if (mInitializedColors) return;
        Notification notification = getAlertEntry().getNotification();

        mHasColor = notification.color != Notification.COLOR_DEFAULT;
        mIsColorized = notification.extras.getBoolean(Notification.EXTRA_COLORIZED, false);

        mCalculatedPrimaryForegroundColor = mDefaultPrimaryForegroundColor;
        mCalculatedSecondaryForegroundColor = mDefaultSecondaryForegroundColor;
        if (canChangeCardBackgroundColor() && mHasColor && mIsColorized && !isInGroup) {
            mBackgroundColor = notification.color;
            mCalculatedPrimaryForegroundColor = NotificationUtils.resolveContrastColor(
                    mDefaultPrimaryForegroundColor, mBackgroundColor);
            mCalculatedSecondaryForegroundColor = NotificationUtils.resolveContrastColor(
                    mDefaultSecondaryForegroundColor, mBackgroundColor);
        }
        mSmallIconColor =
                hasCustomBackgroundColor() ? mCalculatedPrimaryForegroundColor : getAccentColor();

        mInitializedColors = true;
    }


    private boolean canChangeCardBackgroundColor() {
        Notification notification = getAlertEntry().getNotification();

        boolean isSystemApp = mEnableCardBackgroundColorForSystemApp &&
                NotificationUtils.isSystemApp(mContext, getAlertEntry().getStatusBarNotification());
        boolean isSignedWithPlatformKey = NotificationUtils.isSignedWithPlatformKey(mContext,
                getAlertEntry().getStatusBarNotification());
        boolean isNavigationCategory = mEnableCardBackgroundColorForCategoryNavigation &&
                Notification.CATEGORY_NAVIGATION.equals(notification.category);
        return isSystemApp || isNavigationCategory || isSignedWithPlatformKey;
    }

    /**
     * Returns the accent color for this notification.
     */
    @ColorInt
    int getAccentColor() {

        int color = getAlertEntry().getNotification().color;
        if (mEnableSmallIconAccentColor && color != Notification.COLOR_DEFAULT) {
            return color;
        }
        return mDefaultCarAccentColor;
    }

    /**
     * Returns whether this card has a custom background color.
     */
    boolean hasCustomBackgroundColor() {
        return mBackgroundColor != mDefaultBackgroundColor;
    }

    /**
     * Child view holders should override and call super to recycle any custom component
     * that's not handled by {@link CarNotificationHeaderView}, {@link CarNotificationBodyView} and
     * {@link CarNotificationActionsView}.
     * Note that any child class that is not calling {@link #bind} has to call this method directly.
     */
    @CallSuper
    void reset() {
        mAlertEntry = null;
        mBackgroundColor = mDefaultBackgroundColor;
        mInitializedColors = false;

        itemView.setTranslationX(0);
        itemView.setAlpha(1f);

        if (mCardView != null) {
            mCardView.setOnClickListener(null);
            mCardView.setCardBackgroundColor(mDefaultBackgroundColor);
        }

        if (mHeaderView != null) {
            mHeaderView.reset();
        }

        if (mBodyView != null) {
            mBodyView.reset();
        }

        if (mActionsView != null) {
            mActionsView.reset();
        }

        itemView.getViewTreeObserver().removeOnGlobalFocusChangeListener(mFocusChangeListener);
        if (mDismissButton != null) {
            if (!mAlwaysShowDismissButton) {
                mDismissButton.setImageAlpha(0);
            }
            mDismissButton.setVisibility(View.GONE);
        }
    }

    /**
     * Returns the current {@link AlertEntry} that this view holder is holding.
     * Note that any child class that is not calling {@link #bind} has to override this method.
     */
    public AlertEntry getAlertEntry() {
        return mAlertEntry;
    }

    /**
     * Returns true if the panel notification contained in this view holder can be swiped away.
     */
    public boolean isDismissible() {
        if (mAlertEntry == null) {
            return true;
        }

        return (getAlertEntry().getNotification().flags
                & (Notification.FLAG_FOREGROUND_SERVICE | Notification.FLAG_ONGOING_EVENT)) == 0;
    }

    void updateDismissButton(AlertEntry alertEntry, boolean isHeadsUp) {
        if (mDismissButton == null) {
            return;
        }
        // isDismissible only applies to panel notifications, not HUNs
        if ((!isHeadsUp && !isDismissible()) || mHideDismissButton) {
            hideDismissButton();
            return;
        }
        if (!mAlwaysShowDismissButton) {
            mDismissButton.setImageAlpha(0);
        }
        mDismissButton.setVisibility(View.VISIBLE);
        if (!isHeadsUp) {
            // Only set the click listener here for panel notifications - HUNs already have one
            // provided from the CarHeadsUpNotificationManager
            mDismissButton.setOnClickListener(getDismissHandler(alertEntry));
        }
        itemView.getViewTreeObserver().addOnGlobalFocusChangeListener(mFocusChangeListener);
    }

    void hideDismissButton() {
        if (mDismissButton == null) {
            return;
        }
        mDismissButton.setVisibility(View.GONE);
        itemView.getViewTreeObserver().removeOnGlobalFocusChangeListener(mFocusChangeListener);
    }

    /**
     * Returns the TranslationX of the ItemView.
     */
    public float getSwipeTranslationX() {
        return itemView.getTranslationX();
    }

    /**
     * Sets the TranslationX of the ItemView.
     */
    public void setSwipeTranslationX(float translationX) {
        itemView.setTranslationX(translationX);
    }

    /**
     * Sets the alpha of the ItemView.
     */
    public void setSwipeAlpha(float alpha) {
        itemView.setAlpha(alpha);
    }

    /**
     * Sets whether this view holder has ongoing animation.
     */
    public void setIsAnimating(boolean animating) {
        mIsAnimating = animating;
    }

    /**
     * Returns true if this view holder has ongoing animation.
     */
    public boolean isAnimating() {
        return mIsAnimating;
    }

    @VisibleForTesting
    public boolean shouldHideDismissButton() {
        return mHideDismissButton;
    }

    public void setHideDismissButton(boolean hideDismissButton) {
        mHideDismissButton = hideDismissButton;
    }

    View.OnClickListener getDismissHandler(AlertEntry alertEntry) {
        return mClickHandlerFactory.getDismissHandler(alertEntry);
    }
}
