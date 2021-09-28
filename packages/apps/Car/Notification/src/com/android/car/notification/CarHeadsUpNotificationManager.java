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
package com.android.car.notification;

import static android.view.ViewTreeObserver.InternalInsetsInfo;
import static android.view.ViewTreeObserver.OnComputeInternalInsetsListener;
import static android.view.ViewTreeObserver.OnGlobalFocusChangeListener;
import static android.view.ViewTreeObserver.OnGlobalLayoutListener;

import static com.android.car.assist.client.CarAssistUtils.isCarCompatibleMessagingNotification;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.app.KeyguardManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.content.Context;
import android.service.notification.NotificationListenerService;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.VisibleForTesting;

import com.android.car.notification.headsup.CarHeadsUpNotificationContainer;
import com.android.car.notification.headsup.animationhelper.HeadsUpNotificationAnimationHelper;
import com.android.car.notification.template.MessageNotificationViewHolder;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Notification Manager for heads-up notifications in car.
 */
public class CarHeadsUpNotificationManager
        implements CarUxRestrictionsManager.OnUxRestrictionsChangedListener {

    /**
     * Callback that will be issued after a Heads up notification state is changed.
     */
    public interface OnHeadsUpNotificationStateChange {
        /**
         * Will be called if a new notification added/updated changes the heads up state for that
         * notification.
         */
        void onStateChange(AlertEntry alertEntry, boolean isHeadsUp);
    }

    private static final String TAG = CarHeadsUpNotificationManager.class.getSimpleName();

    private final Beeper mBeeper;
    private final Context mContext;
    private final boolean mEnableNavigationHeadsup;
    private final long mDuration;
    private final long mMinDisplayDuration;
    private HeadsUpNotificationAnimationHelper mAnimationHelper;
    private final int mNotificationHeadsUpCardMarginTop;

    private final KeyguardManager mKeyguardManager;
    private final PreprocessingManager mPreprocessingManager;
    private final LayoutInflater mInflater;
    private final CarHeadsUpNotificationContainer mHunContainer;

    // key for the map is the statusbarnotification key
    private final Map<String, HeadsUpEntry> mActiveHeadsUpNotifications = new HashMap<>();
    private final List<OnHeadsUpNotificationStateChange> mNotificationStateChangeListeners =
            new ArrayList<>();
    private final Map<HeadsUpEntry,
            Pair<OnComputeInternalInsetsListener, OnGlobalFocusChangeListener>>
            mRegisteredViewTreeListeners = new HashMap<>();

    private boolean mShouldRestrictMessagePreview;
    private NotificationClickHandlerFactory mClickHandlerFactory;
    private NotificationDataManager mNotificationDataManager;


    public CarHeadsUpNotificationManager(Context context,
            NotificationClickHandlerFactory clickHandlerFactory,
            NotificationDataManager notificationDataManager,
            CarHeadsUpNotificationContainer hunContainer) {
        mContext = context.getApplicationContext();
        mEnableNavigationHeadsup =
                context.getResources().getBoolean(R.bool.config_showNavigationHeadsup);
        mClickHandlerFactory = clickHandlerFactory;
        mNotificationDataManager = notificationDataManager;
        mBeeper = new Beeper(mContext);
        mDuration = mContext.getResources().getInteger(R.integer.headsup_notification_duration_ms);
        mNotificationHeadsUpCardMarginTop = (int) mContext.getResources().getDimension(
                R.dimen.headsup_notification_top_margin);
        mMinDisplayDuration = mContext.getResources().getInteger(
                R.integer.heads_up_notification_minimum_time);
        mAnimationHelper = getAnimationHelper();

        mKeyguardManager = (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        mPreprocessingManager = PreprocessingManager.getInstance(context);
        mInflater = LayoutInflater.from(mContext);
        mClickHandlerFactory.registerClickListener(
                (launchResult, alertEntry) -> dismissHun(alertEntry));
        mHunContainer = hunContainer;
    }

    private HeadsUpNotificationAnimationHelper getAnimationHelper() {
        String helperName = mContext.getResources().getString(
                R.string.config_headsUpNotificationAnimationHelper);
        try {
            Class<?> clazz = Class.forName(helperName);
            return (HeadsUpNotificationAnimationHelper) clazz.getConstructor().newInstance();
        } catch (Exception e) {
            throw new IllegalArgumentException(
                    String.format("Invalid animation helper: %s", helperName), e);
        }
    }

    /**
     * Show the notification as a heads-up if it meets the criteria.
     *
     * <p>Return's true if the notification will be shown as a heads up, false otherwise.
     */
    public boolean maybeShowHeadsUp(
            AlertEntry alertEntry,
            NotificationListenerService.RankingMap rankingMap,
            Map<String, AlertEntry> activeNotifications) {
        if (!shouldShowHeadsUp(alertEntry, rankingMap)) {
            // check if this is an update to the existing notification and if it should still show
            // as a heads up or not.
            HeadsUpEntry currentActiveHeadsUpNotification = mActiveHeadsUpNotifications.get(
                    alertEntry.getKey());
            if (currentActiveHeadsUpNotification == null) {
                return false;
            }
            if (CarNotificationDiff.sameNotificationKey(currentActiveHeadsUpNotification,
                    alertEntry)
                    && currentActiveHeadsUpNotification.getHandler().hasMessagesOrCallbacks()) {
                dismissHun(alertEntry);
            }
            return false;
        }
        if (!activeNotifications.containsKey(alertEntry.getKey()) || canUpdate(alertEntry)
                || alertAgain(alertEntry.getNotification())) {
            showHeadsUp(mPreprocessingManager.optimizeForDriving(alertEntry),
                    rankingMap);
            return true;
        }
        return false;
    }

    /**
     * This method gets called when an app wants to cancel or withdraw its notification.
     */
    public void maybeRemoveHeadsUp(AlertEntry alertEntry) {
        HeadsUpEntry currentActiveHeadsUpNotification = mActiveHeadsUpNotifications.get(
                alertEntry.getKey());
        // if the heads up notification is already removed do nothing.
        if (currentActiveHeadsUpNotification == null) {
            return;
        }

        long totalDisplayDuration =
                System.currentTimeMillis() - currentActiveHeadsUpNotification.getPostTime();
        // ongoing notification that has passed the minimum threshold display time.
        if (totalDisplayDuration >= mMinDisplayDuration) {
            removeHun(alertEntry);
            return;
        }

        long earliestRemovalTime = mMinDisplayDuration - totalDisplayDuration;

        currentActiveHeadsUpNotification.getHandler().postDelayed(() ->
                removeHun(alertEntry), earliestRemovalTime);
    }

    /**
     * Registers a new {@link OnHeadsUpNotificationStateChange} to the list of listeners.
     */
    public void registerHeadsUpNotificationStateChangeListener(
            OnHeadsUpNotificationStateChange listener) {
        if (!mNotificationStateChangeListeners.contains(listener)) {
            mNotificationStateChangeListeners.add(listener);
        }
    }

    /**
     * Unregisters a {@link OnHeadsUpNotificationStateChange} from the list of listeners.
     */
    public void unregisterHeadsUpNotificationStateChangeListener(
            OnHeadsUpNotificationStateChange listener) {
        mNotificationStateChangeListeners.remove(listener);
    }

    /**
     * Invokes all OnHeadsUpNotificationStateChange handlers registered in {@link
     * OnHeadsUpNotificationStateChange}s array.
     */
    private void handleHeadsUpNotificationStateChanged(AlertEntry alertEntry, boolean isHeadsUp) {
        mNotificationStateChangeListeners.forEach(
                listener -> listener.onStateChange(alertEntry, isHeadsUp));
    }

    /**
     * Returns true if the notification's flag is not set to
     * {@link Notification#FLAG_ONLY_ALERT_ONCE}
     */
    private boolean alertAgain(Notification newNotification) {
        return (newNotification.flags & Notification.FLAG_ONLY_ALERT_ONCE) == 0;
    }

    /**
     * Return true if the currently displaying notification have the same key as the new added
     * notification. In that case it will be considered as an update to the currently displayed
     * notification.
     */
    private boolean isUpdate(AlertEntry alertEntry) {
        HeadsUpEntry currentActiveHeadsUpNotification = mActiveHeadsUpNotifications.get(
                alertEntry.getKey());
        if (currentActiveHeadsUpNotification == null) {
            return false;
        }
        return CarNotificationDiff.sameNotificationKey(currentActiveHeadsUpNotification,
                alertEntry);
    }

    /**
     * Updates only when the notification is being displayed.
     */
    private boolean canUpdate(AlertEntry alertEntry) {
        HeadsUpEntry currentActiveHeadsUpNotification = mActiveHeadsUpNotifications.get(
                alertEntry.getKey());
        return currentActiveHeadsUpNotification != null && System.currentTimeMillis() -
                currentActiveHeadsUpNotification.getPostTime() < mDuration;
    }

    /**
     * Returns the active headsUpEntry or creates a new one while adding it to the list of
     * mActiveHeadsUpNotifications.
     */
    private HeadsUpEntry addNewHeadsUpEntry(AlertEntry alertEntry) {
        HeadsUpEntry currentActiveHeadsUpNotification = mActiveHeadsUpNotifications.get(
                alertEntry.getKey());
        if (currentActiveHeadsUpNotification == null) {
            currentActiveHeadsUpNotification = new HeadsUpEntry(
                    alertEntry.getStatusBarNotification());
            handleHeadsUpNotificationStateChanged(alertEntry, /* isHeadsUp= */ true);
            mActiveHeadsUpNotifications.put(alertEntry.getKey(),
                    currentActiveHeadsUpNotification);
            currentActiveHeadsUpNotification.mIsAlertAgain = alertAgain(
                    alertEntry.getNotification());
            currentActiveHeadsUpNotification.mIsNewHeadsUp = true;
            return currentActiveHeadsUpNotification;
        }
        currentActiveHeadsUpNotification.mIsNewHeadsUp = false;
        currentActiveHeadsUpNotification.mIsAlertAgain = alertAgain(
                alertEntry.getNotification());
        if (currentActiveHeadsUpNotification.mIsAlertAgain) {
            // This is a ongoing notification which needs to be alerted again to the user. This
            // requires for the post time to be updated.
            currentActiveHeadsUpNotification.updatePostTime();
        }
        return currentActiveHeadsUpNotification;
    }

    /**
     * Controls three major conditions while showing heads up notification.
     * <p>
     * <ol>
     * <li> When a new HUN comes in it will be displayed with animations
     * <li> If an update to existing HUN comes in which enforces to alert the HUN again to user,
     * then the post time will be updated to current time. This will only be done if {@link
     * Notification#FLAG_ONLY_ALERT_ONCE} flag is not set.
     * <li> If an update to existing HUN comes in which just updates the data and does not want to
     * alert itself again, then the animations will not be shown and the data will get updated. This
     * will only be done if {@link Notification#FLAG_ONLY_ALERT_ONCE} flag is not set.
     * </ol>
     */
    private void showHeadsUp(AlertEntry alertEntry,
            NotificationListenerService.RankingMap rankingMap) {
        // Show animations only when there is no active HUN and notification is new. This check
        // needs to be done here because after this the new notification will be added to the map
        // holding ongoing notifications.
        boolean shouldShowAnimation = !isUpdate(alertEntry);
        HeadsUpEntry currentNotification = addNewHeadsUpEntry(alertEntry);
        if (currentNotification.mIsNewHeadsUp) {
            playSound(alertEntry, rankingMap);
            setAutoDismissViews(currentNotification, alertEntry);
        } else if (currentNotification.mIsAlertAgain) {
            setAutoDismissViews(currentNotification, alertEntry);
        }
        CarNotificationTypeItem notificationTypeItem = NotificationUtils.getNotificationViewType(
                alertEntry);
        currentNotification.setClickHandlerFactory(mClickHandlerFactory);

        if (currentNotification.getNotificationView() == null) {
            currentNotification.setNotificationView(mInflater.inflate(
                    notificationTypeItem.getHeadsUpTemplate(),
                    null));
            mHunContainer.displayNotification(currentNotification.getNotificationView());
            currentNotification.setViewHolder(
                    notificationTypeItem.getViewHolder(currentNotification.getNotificationView(),
                            mClickHandlerFactory));
        }

        currentNotification.getViewHolder().setHideDismissButton(!shouldDismissOnSwipe(alertEntry));

        if (mShouldRestrictMessagePreview && notificationTypeItem.getNotificationType()
                == NotificationViewType.MESSAGE) {
            ((MessageNotificationViewHolder) currentNotification.getViewHolder())
                    .bindRestricted(alertEntry, /* isInGroup= */ false, /* isHeadsUp= */ true);
        } else {
            currentNotification.getViewHolder().bind(alertEntry, /* isInGroup= */false,
                    /* isHeadsUp= */ true);
        }

        resetViewTreeListenersEntry(currentNotification);

        ViewTreeObserver viewTreeObserver =
                currentNotification.getNotificationView().getViewTreeObserver();

        // measure the size of the card and make that area of the screen touchable
        OnComputeInternalInsetsListener onComputeInternalInsetsListener =
                info -> setInternalInsetsInfo(info, currentNotification,
                        /* panelExpanded= */ false);
        viewTreeObserver.addOnComputeInternalInsetsListener(onComputeInternalInsetsListener);
        // Get the height of the notification view after onLayout() in order to animate the
        // notification into the screen.
        viewTreeObserver.addOnGlobalLayoutListener(
                new OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        View view = currentNotification.getNotificationView();
                        if (shouldShowAnimation) {
                            mAnimationHelper.resetHUNPosition(view);
                            AnimatorSet animatorSet = mAnimationHelper.getAnimateInAnimator(
                                    mContext, view);
                            animatorSet.setTarget(view);
                            animatorSet.start();
                        }
                        view.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                    }
                });
        // Reset the auto dismiss timeout for each rotary event.
        OnGlobalFocusChangeListener onGlobalFocusChangeListener =
                (oldFocus, newFocus) -> setAutoDismissViews(currentNotification, alertEntry);
        viewTreeObserver.addOnGlobalFocusChangeListener(onGlobalFocusChangeListener);

        mRegisteredViewTreeListeners.put(currentNotification,
                new Pair<>(onComputeInternalInsetsListener, onGlobalFocusChangeListener));

        if (currentNotification.mIsNewHeadsUp) {
            // Add swipe gesture
            View cardView = currentNotification.getNotificationView().findViewById(R.id.card_view);
            cardView.setOnTouchListener(new HeadsUpNotificationOnTouchListener(cardView,
                    shouldDismissOnSwipe(alertEntry), () -> resetView(alertEntry)));

            // Add dismiss button listener
            View dismissButton = currentNotification.getNotificationView().findViewById(
                    R.id.dismiss_button);
            if (dismissButton != null) {
                dismissButton.setOnClickListener(v -> dismissHun(alertEntry));
            }
        }
    }

    private void resetViewTreeListenersEntry(HeadsUpEntry headsUpEntry) {
        Pair<OnComputeInternalInsetsListener, OnGlobalFocusChangeListener> listeners =
                mRegisteredViewTreeListeners.get(headsUpEntry);
        if (listeners == null) {
            return;
        }

        ViewTreeObserver observer = headsUpEntry.getNotificationView().getViewTreeObserver();
        observer.removeOnComputeInternalInsetsListener(listeners.first);
        observer.removeOnGlobalFocusChangeListener(listeners.second);
        mRegisteredViewTreeListeners.remove(headsUpEntry);
    }

    protected void setInternalInsetsInfo(InternalInsetsInfo info,
            HeadsUpEntry currentNotification, boolean panelExpanded) {
        // If the panel is not on screen don't modify the touch region
        if (!mHunContainer.isVisible()) return;
        int[] mTmpTwoArray = new int[2];
        View cardView = currentNotification.getNotificationView().findViewById(
                R.id.card_view);

        if (cardView == null) return;

        if (panelExpanded) {
            info.setTouchableInsets(InternalInsetsInfo.TOUCHABLE_INSETS_FRAME);
            return;
        }

        cardView.getLocationInWindow(mTmpTwoArray);
        int minX = mTmpTwoArray[0];
        int maxX = mTmpTwoArray[0] + cardView.getWidth();
        int height = cardView.getHeight();
        info.setTouchableInsets(InternalInsetsInfo.TOUCHABLE_INSETS_REGION);
        info.touchableRegion.set(minX, mNotificationHeadsUpCardMarginTop, maxX,
                height + mNotificationHeadsUpCardMarginTop);
    }

    private void playSound(AlertEntry alertEntry,
            NotificationListenerService.RankingMap rankingMap) {
        NotificationListenerService.Ranking ranking = getRanking();
        if (rankingMap.getRanking(alertEntry.getKey(), ranking)) {
            NotificationChannel notificationChannel = ranking.getChannel();
            // If sound is not set on the notification channel and default is not chosen it
            // can be null.
            if (notificationChannel.getSound() != null) {
                // make the sound
                mBeeper.beep(alertEntry.getStatusBarNotification().getPackageName(),
                        notificationChannel.getSound());
            }
        }
    }

    private boolean shouldDismissOnSwipe(AlertEntry alertEntry) {
        return !(hasFullScreenIntent(alertEntry)
                && Objects.equals(alertEntry.getNotification().category, Notification.CATEGORY_CALL)
                && alertEntry.getStatusBarNotification().isOngoing());
    }

    @VisibleForTesting
    protected Map<String, HeadsUpEntry> getActiveHeadsUpNotifications() {
        return mActiveHeadsUpNotifications;
    }

    private void setAutoDismissViews(HeadsUpEntry currentNotification, AlertEntry alertEntry) {
        // Should not auto dismiss if HUN has a full screen Intent.
        if (hasFullScreenIntent(alertEntry)) {
            return;
        }
        currentNotification.getHandler().removeCallbacksAndMessages(null);
        currentNotification.getHandler().postDelayed(() -> dismissHun(alertEntry), mDuration);
    }

    /**
     * Returns true if AlertEntry has a full screen Intent.
     */
    private boolean hasFullScreenIntent(AlertEntry alertEntry) {
        return alertEntry.getNotification().fullScreenIntent != null;
    }

    /**
     * Animates the heads up notification out of the screen and reset the views.
     */
    private void animateOutHun(AlertEntry alertEntry, boolean isRemoved) {
        Log.d(TAG, "clearViews for Heads Up Notification: ");
        // get the current notification to perform animations and remove it immediately from the
        // active notification maps and cancel all other call backs if any.
        HeadsUpEntry currentHeadsUpNotification = mActiveHeadsUpNotifications.get(
                alertEntry.getKey());
        // view can also be removed when swipped away.
        if (currentHeadsUpNotification == null) {
            return;
        }
        currentHeadsUpNotification.getHandler().removeCallbacksAndMessages(null);
        resetViewTreeListenersEntry(currentHeadsUpNotification);
        View view = currentHeadsUpNotification.getNotificationView();

        AnimatorSet animatorSet = mAnimationHelper.getAnimateOutAnimator(mContext, view);
        animatorSet.setTarget(view);
        animatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mHunContainer.removeNotification(view);

                // Remove HUN after the animation ends to prevent accidental touch on the card
                // triggering another remove call.
                mActiveHeadsUpNotifications.remove(alertEntry.getKey());

                // If the HUN was not specifically removed then add it to the panel.
                if (!isRemoved) {
                    handleHeadsUpNotificationStateChanged(alertEntry, /* isHeadsUp= */ false);
                }
            }
        });
        animatorSet.start();
    }

    private void dismissHun(AlertEntry alertEntry) {
        animateOutHun(alertEntry, /* isRemoved= */ false);
    }

    private void removeHun(AlertEntry alertEntry) {
        animateOutHun(alertEntry, /* isRemoved= */ true);
    }

    /**
     * Removes the view for the active heads up notification and also removes the HUN from the map
     * of active Notifications.
     */
    private void resetView(AlertEntry alertEntry) {
        HeadsUpEntry currentHeadsUpNotification = mActiveHeadsUpNotifications.get(
                alertEntry.getKey());
        if (currentHeadsUpNotification == null) return;

        currentHeadsUpNotification.getHandler().removeCallbacksAndMessages(null);
        mHunContainer.removeNotification(currentHeadsUpNotification.getNotificationView());
        mActiveHeadsUpNotifications.remove(alertEntry.getKey());
        handleHeadsUpNotificationStateChanged(alertEntry, /* isHeadsUp= */ false);
        resetViewTreeListenersEntry(currentHeadsUpNotification);
    }

    /**
     * Helper method that determines whether a notification should show as a heads-up.
     *
     * <p> A notification will never be shown as a heads-up if:
     * <ul>
     * <li> Keyguard (lock screen) is showing
     * <li> OEMs configured CATEGORY_NAVIGATION should not be shown
     * <li> Notification is muted.
     * </ul>
     *
     * <p> A notification will be shown as a heads-up if:
     * <ul>
     * <li> Importance >= HIGH
     * <li> it comes from an app signed with the platform key.
     * <li> it comes from a privileged system app.
     * <li> is a car compatible notification.
     * {@link com.android.car.assist.client.CarAssistUtils#isCarCompatibleMessagingNotification}
     * <li> Notification category is one of CATEGORY_CALL or CATEGORY_NAVIGATION
     * </ul>
     *
     * <p> Group alert behavior still follows API documentation.
     *
     * @return true if a notification should be shown as a heads-up
     */
    private boolean shouldShowHeadsUp(
            AlertEntry alertEntry,
            NotificationListenerService.RankingMap rankingMap) {
        if (mKeyguardManager.isKeyguardLocked()) {
            return false;
        }
        Notification notification = alertEntry.getNotification();

        // Navigation notification configured by OEM
        if (!mEnableNavigationHeadsup && Notification.CATEGORY_NAVIGATION.equals(
                notification.category)) {
            return false;
        }
        // Group alert behavior
        if (notification.suppressAlertingDueToGrouping()) {
            return false;
        }
        // Messaging notification muted by user.
        if (mNotificationDataManager.isMessageNotificationMuted(alertEntry)) {
            return false;
        }

        // Do not show if importance < HIGH
        NotificationListenerService.Ranking ranking = getRanking();
        if (rankingMap.getRanking(alertEntry.getKey(), ranking)) {
            if (ranking.getImportance() < NotificationManager.IMPORTANCE_HIGH) {
                return false;
            }
        }

        if (NotificationUtils.isSystemPrivilegedOrPlatformKey(mContext, alertEntry)) {
            return true;
        }

        // Allow car messaging type.
        if (isCarCompatibleMessagingNotification(alertEntry.getStatusBarNotification())) {
            return true;
        }

        if (notification.category == null) {
            Log.d(TAG, "category not set for: "
                    + alertEntry.getStatusBarNotification().getPackageName());
        }

        // Allow for Call, and nav TBT categories.
        return Notification.CATEGORY_CALL.equals(notification.category)
                || Notification.CATEGORY_NAVIGATION.equals(notification.category);
    }

    @VisibleForTesting
    protected NotificationListenerService.Ranking getRanking() {
        return new NotificationListenerService.Ranking();
    }

    @Override
    public void onUxRestrictionsChanged(CarUxRestrictions restrictions) {
        mShouldRestrictMessagePreview =
                (restrictions.getActiveRestrictions()
                        & CarUxRestrictions.UX_RESTRICTIONS_NO_TEXT_MESSAGE) != 0;
    }

    /**
     * Sets the source of {@link View.OnClickListener}
     *
     * @param clickHandlerFactory used to generate onClickListeners
     */
    @VisibleForTesting
    public void setClickHandlerFactory(NotificationClickHandlerFactory clickHandlerFactory) {
        mClickHandlerFactory = clickHandlerFactory;
    }
}
