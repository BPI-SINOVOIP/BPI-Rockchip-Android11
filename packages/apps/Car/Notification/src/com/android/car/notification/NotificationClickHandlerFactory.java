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
 * limitations under the License
 */

package com.android.car.notification;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;
import android.service.notification.NotificationStats;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Toast;

import com.android.car.assist.CarVoiceInteractionSession;
import com.android.car.assist.client.CarAssistUtils;
import com.android.internal.statusbar.IStatusBarService;
import com.android.internal.statusbar.NotificationVisibility;

import java.util.ArrayList;
import java.util.List;

/**
 * Factory that builds a {@link View.OnClickListener} to handle the logic of what to do when a
 * notification is clicked. It also handles the interaction with the StatusBarService.
 */
public class NotificationClickHandlerFactory {

    /**
     * Callback that will be issued after a notification is clicked.
     */
    public interface OnNotificationClickListener {

        /**
         * A notification was clicked and handleNotificationClicked was invoked.
         *
         * @param launchResult For non-Assistant actions, returned from
         *        {@link PendingIntent#sendAndReturnResult}; for Assistant actions,
         *        returns {@link ActivityManager#START_SUCCESS} on success;
         *        {@link ActivityManager#START_ABORTED} otherwise.
         *
         * @param alertEntry {@link AlertEntry} whose Notification was clicked.
         */
        void onNotificationClicked(int launchResult, AlertEntry alertEntry);
    }

    private static final String TAG = "NotificationClickHandlerFactory";

    private final IStatusBarService mBarService;
    private final List<OnNotificationClickListener> mClickListeners = new ArrayList<>();
    private CarAssistUtils mCarAssistUtils;
    @Nullable
    private NotificationDataManager mNotificationDataManager;
    private Handler mMainHandler;

    public NotificationClickHandlerFactory(IStatusBarService barService) {
        mBarService = barService;
        mCarAssistUtils = null;
        mMainHandler = new Handler(Looper.getMainLooper());
    }

    /**
     * Sets the {@link NotificationDataManager} which contains additional state information of the
     * {@link AlertEntry}s.
     */
    public void setNotificationDataManager(NotificationDataManager manager) {
        mNotificationDataManager = manager;
    }

    /**
     * Returns the {@link NotificationDataManager} which contains additional state information of
     * the {@link AlertEntry}s.
     */
    @Nullable
    public NotificationDataManager getNotificationDataManager() {
        return mNotificationDataManager;
    }

    /**
     * Returns a {@link View.OnClickListener} that should be used for the given
     * {@link AlertEntry}
     *
     * @param alertEntry that will be considered clicked when onClick is called.
     */
    public View.OnClickListener getClickHandler(AlertEntry alertEntry) {
        return v -> {
            Notification notification = alertEntry.getNotification();
            final PendingIntent intent = notification.contentIntent != null
                    ? notification.contentIntent
                    : notification.fullScreenIntent;
            if (intent == null) {
                return;
            }

            int result = ActivityManager.START_ABORTED;
            try {
                result = intent.sendAndReturnResult(/* context= */ null, /* code= */ 0,
                        /* intent= */ null, /* onFinished= */ null,
                        /* handler= */ null, /* requiredPermissions= */ null,
                        /* options= */ null);
            } catch (PendingIntent.CanceledException e) {
                // Do not take down the app over this
                Log.w(TAG, "Sending contentIntent failed: " + e);
            }
            NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                    alertEntry.getKey(),
                    /* rank= */ -1, /* count= */ -1, /* visible= */ true);
            try {
                mBarService.onNotificationClick(alertEntry.getKey(),
                        notificationVisibility);
                if (shouldAutoCancel(alertEntry)) {
                    clearNotification(alertEntry);
                }
            } catch (RemoteException ex) {
                Log.e(TAG, "Remote exception in getClickHandler", ex);
            }
            handleNotificationClicked(result, alertEntry);
        };

    }

    /**
     * Returns a {@link View.OnClickListener} that should be used for the
     * {@link android.app.Notification.Action} contained in the {@link AlertEntry}
     *
     * @param alertEntry that contains the clicked action.
     * @param index the index of the action clicked.
     */
    public View.OnClickListener getActionClickHandler(AlertEntry alertEntry, int index) {
        return v -> {
            Notification notification = alertEntry.getNotification();
            Notification.Action action = notification.actions[index];
            NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                    alertEntry.getKey(),
                    /* rank= */ -1, /* count= */ -1, /* visible= */ true);
            boolean canceledExceptionThrown = false;
            int semanticAction = action.getSemanticAction();
            if (CarAssistUtils.isCarCompatibleMessagingNotification(
                    alertEntry.getStatusBarNotification())) {
                if (semanticAction == Notification.Action.SEMANTIC_ACTION_REPLY) {
                    Context context = v.getContext().getApplicationContext();
                    Intent resultIntent = addCannedReplyMessage(action, context);
                    int result = sendPendingIntent(action.actionIntent, context, resultIntent);
                    if (result == ActivityManager.START_SUCCESS) {
                        showToast(context, R.string.toast_message_sent_success);
                    } else if (result == ActivityManager.START_ABORTED) {
                        canceledExceptionThrown = true;
                    }
                }
            } else {
                int result = sendPendingIntent(action.actionIntent, /* context= */ null,
                        /* resultIntent= */ null);
                if (result == ActivityManager.START_ABORTED) {
                    canceledExceptionThrown = true;
                }
                handleNotificationClicked(result, alertEntry);
            }
            if (!canceledExceptionThrown) {
                try {
                    mBarService.onNotificationActionClick(
                            alertEntry.getKey(),
                            index,
                            action,
                            notificationVisibility,
                            /* generatedByAssistant= */ false);
                } catch (RemoteException e) {
                    Log.e(TAG, "Remote exception in getActionClickHandler", e);
                }
            }
        };
    }

    /**
     * Returns a {@link View.OnClickListener} that should be used for the
     * {@param messageNotification}'s {@param playButton}. Once the message is read aloud, the
     * pending intent should be returned to the messaging app, so it can mark it as read.
     */
    public View.OnClickListener getPlayClickHandler(AlertEntry messageNotification) {
        return view -> {
            if (!CarAssistUtils.isCarCompatibleMessagingNotification(
                    messageNotification.getStatusBarNotification())) {
                return;
            }
            Context context = view.getContext().getApplicationContext();
            if (mCarAssistUtils == null) {
                mCarAssistUtils = new CarAssistUtils(context);
            }
            CarAssistUtils.ActionRequestCallback requestCallback = resultState -> {
                if (CarAssistUtils.ActionRequestCallback.RESULT_FAILED.equals(resultState)) {
                    showToast(context, R.string.assist_action_failed_toast);
                    Log.e(TAG, "Assistant failed to read aloud the message");
                }
                // Don't trigger mCallback so the shade remains open.
            };
            mCarAssistUtils.requestAssistantVoiceAction(
                    messageNotification.getStatusBarNotification(),
                    CarVoiceInteractionSession.VOICE_ACTION_READ_NOTIFICATION,
                    requestCallback);
        };
    }

    /**
     * Returns a {@link View.OnClickListener} that should be used for the
     * {@param messageNotification}'s {@param muteButton}.
     */
    public View.OnClickListener getMuteClickHandler(
            Button muteButton, AlertEntry messageNotification) {
        return v -> {
            if (mNotificationDataManager != null) {
                mNotificationDataManager.toggleMute(messageNotification);
                Context context = v.getContext().getApplicationContext();
                muteButton.setText(
                        (mNotificationDataManager.isMessageNotificationMuted(messageNotification))
                                ? context.getString(R.string.action_unmute_long)
                                : context.getString(R.string.action_mute_long));
                // Don't trigger mCallback so the shade remains open.
            } else {
                Log.d(TAG, "Could not set mute click handler as NotificationDataManager is null");
            }
        };
    }

    /**
     * Returns a {@link View.OnClickListener} that should be used for the {@code alertEntry}'s
     * dismiss button.
     */
    public View.OnClickListener getDismissHandler(AlertEntry alertEntry) {
        return v -> clearNotification(alertEntry);
    }

    /**
     * Registers a new {@link OnNotificationClickListener} to the list of click event listeners.
     */
    public void registerClickListener(OnNotificationClickListener clickListener) {
        if (clickListener != null && !mClickListeners.contains(clickListener)) {
            mClickListeners.add(clickListener);
        }
    }

    /**
     * Unregisters a {@link OnNotificationClickListener} from the list of click event listeners.
     */
    public void unregisterClickListener(OnNotificationClickListener clickListener) {
        mClickListeners.remove(clickListener);
    }

    /**
     * Clears all notifications.
     */
    public void clearAllNotifications() {
        try {
            mBarService.onClearAllNotifications(ActivityManager.getCurrentUser());
        } catch (RemoteException e) {
            Log.e(TAG, "clearAllNotifications: ", e);
        }
    }

    /**
     * Clears the notifications provided.
     */
    public void clearNotifications(List<NotificationGroup> notificationsToClear) {
        notificationsToClear.forEach(notificationGroup -> {
            if (notificationGroup.isGroup()) {
                AlertEntry summaryNotification = notificationGroup.getGroupSummaryNotification();
                clearNotification(summaryNotification);
            }
            notificationGroup.getChildNotifications()
                    .forEach(alertEntry -> clearNotification(alertEntry));
        });
    }

    /**
     * Collapses the notification shade panel.
     */
    public void collapsePanel() {
        try {
            mBarService.collapsePanels();
        } catch (RemoteException e) {
            Log.e(TAG, "collapsePanel: ", e);
        }
    }

    /**
     * Invokes all onNotificationClicked handlers registered in {@link OnNotificationClickListener}s
     * array.
     */
    private void handleNotificationClicked(int launceResult, AlertEntry alertEntry) {
        mClickListeners.forEach(
                listener -> listener.onNotificationClicked(launceResult, alertEntry));
    }

    private void clearNotification(AlertEntry alertEntry) {
        try {
            // rank and count is used for logging and is not need at this time thus -1
            NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                    alertEntry.getKey(),
                    /* rank= */ -1,
                    /* count= */ -1,
                    /* visible= */ true);

            mBarService.onNotificationClear(
                    alertEntry.getStatusBarNotification().getPackageName(),
                    alertEntry.getStatusBarNotification().getTag(),
                    alertEntry.getStatusBarNotification().getId(),
                    alertEntry.getStatusBarNotification().getUser().getIdentifier(),
                    alertEntry.getStatusBarNotification().getKey(),
                    NotificationStats.DISMISSAL_SHADE,
                    NotificationStats.DISMISS_SENTIMENT_NEUTRAL,
                    notificationVisibility);
        } catch (RemoteException e) {
            Log.e(TAG, "clearNotifications: ", e);
        }
    }

    private int sendPendingIntent(PendingIntent pendingIntent, Context context,
            Intent resultIntent) {
        try {
            return pendingIntent.sendAndReturnResult(/* context= */ context, /* code= */ 0,
                    /* intent= */ resultIntent, /* onFinished= */null,
                    /* handler= */ null, /* requiredPermissions= */ null,
                    /* options= */ null);
        } catch (PendingIntent.CanceledException e) {
            // Do not take down the app over this
            Log.w(TAG, "Sending contentIntent failed: " + e);
            return ActivityManager.START_ABORTED;
        }
    }

    /** Adds the canned reply sms message to the {@link Notification.Action}'s RemoteInput. **/
    @Nullable
    private Intent addCannedReplyMessage(Notification.Action action, Context context) {
        RemoteInput remoteInput = action.getRemoteInputs()[0];
        if (remoteInput == null) {
            Log.w("TAG", "Cannot add canned reply message to action with no RemoteInput.");
            return null;
        }
        Bundle messageDataBundle = new Bundle();
        messageDataBundle.putCharSequence(remoteInput.getResultKey(),
                context.getString(R.string.canned_reply_message));
        Intent resultIntent = new Intent();
        RemoteInput.addResultsToIntent(
                new RemoteInput[]{remoteInput}, resultIntent, messageDataBundle);
        return resultIntent;
    }

    private void showToast(Context context, int resourceId) {
        mMainHandler.post(
                Toast.makeText(context, context.getString(resourceId), Toast.LENGTH_LONG)::show);
    }

    private boolean shouldAutoCancel(AlertEntry alertEntry) {
        int flags = alertEntry.getNotification().flags;
        if ((flags & Notification.FLAG_AUTO_CANCEL) != Notification.FLAG_AUTO_CANCEL) {
            return false;
        }
        if ((flags & Notification.FLAG_FOREGROUND_SERVICE) != 0) {
            return false;
        }
        return true;
    }

}
