/**
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

package android.ext.services.notification;

import static android.app.NotificationManager.IMPORTANCE_LOW;
import static android.service.notification.Adjustment.KEY_IMPORTANCE;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.app.NotificationChannel;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.UserHandle;
import android.service.notification.Adjustment;
import android.service.notification.NotificationAssistantService;
import android.service.notification.NotificationStats;
import android.service.notification.StatusBarNotification;
import android.util.ArrayMap;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.textclassifier.notification.SmartSuggestions;
import com.android.textclassifier.notification.SmartSuggestionsHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Notification assistant that provides guidance on notification channel blocking
 */
@SuppressLint("OverrideAbstract")
public class Assistant extends NotificationAssistantService {
    private static final String TAG = "ExtAssistant";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    // SBN key : entry
    protected ArrayMap<String, NotificationEntry> mLiveNotifications = new ArrayMap<>();

    private PackageManager mPackageManager;

    private final ExecutorService mSingleThreadExecutor = Executors.newSingleThreadExecutor();
    @VisibleForTesting
    protected AssistantSettings.Factory mSettingsFactory = AssistantSettings.FACTORY;
    @VisibleForTesting
    protected AssistantSettings mSettings;
    private SmsHelper mSmsHelper;
    private SmartSuggestionsHelper mSmartSuggestionsHelper;
    private NotificationCategorizer mNotificationCategorizer;

    public Assistant() {
    }

    @Override
    public void onCreate() {
        super.onCreate();
        // Contexts are correctly hooked up by the creation step, which is required for the observer
        // to be hooked up/initialized.
        mPackageManager = getPackageManager();
        mSettings = mSettingsFactory.createAndRegister();
        mSmartSuggestionsHelper = new SmartSuggestionsHelper(this, mSettings);
        mNotificationCategorizer = new NotificationCategorizer();
        mSmsHelper = new SmsHelper(this);
        mSmsHelper.initialize();
    }

    @Override
    public void onDestroy() {
        // This null check is only for the unit tests as ServiceTestCase.tearDown calls onDestroy
        // without having first called onCreate.
        if (mSmsHelper != null) {
            mSmsHelper.destroy();
        }
        super.onDestroy();
    }

    @Override
    public Adjustment onNotificationEnqueued(StatusBarNotification sbn) {
        // we use the version with channel, so this is never called.
        return null;
    }

    @Override
    public Adjustment onNotificationEnqueued(StatusBarNotification sbn,
            NotificationChannel channel) {
        if (DEBUG) Log.i(TAG, "ENQUEUED " + sbn.getKey() + " on " + channel.getId());
        if (!isForCurrentUser(sbn)) {
            return null;
        }
        mSingleThreadExecutor.submit(() -> {
            NotificationEntry entry =
                    new NotificationEntry(this, mPackageManager, sbn, channel, mSmsHelper);
            SmartSuggestions suggestions = mSmartSuggestionsHelper.onNotificationEnqueued(sbn);
            if (DEBUG) {
                Log.d(TAG, String.format(
                        "Creating Adjustment for %s, with %d actions, and %d replies.",
                        sbn.getKey(),
                        suggestions.getActions().size(),
                        suggestions.getReplies().size()));
            }
            Adjustment adjustment = createEnqueuedNotificationAdjustment(
                    entry,
                    new ArrayList<Notification.Action>(suggestions.getActions()),
                    new ArrayList<>(suggestions.getReplies()));
            adjustNotification(adjustment);
        });
        return null;
    }

    /** A convenience helper for creating an adjustment for an SBN. */
    @VisibleForTesting
    @Nullable
    Adjustment createEnqueuedNotificationAdjustment(
            @NonNull NotificationEntry entry,
            @NonNull ArrayList<Notification.Action> smartActions,
            @NonNull ArrayList<CharSequence> smartReplies) {
        Bundle signals = new Bundle();

        if (!smartActions.isEmpty()) {
            signals.putParcelableArrayList(Adjustment.KEY_CONTEXTUAL_ACTIONS, smartActions);
        }
        if (!smartReplies.isEmpty()) {
            signals.putCharSequenceArrayList(Adjustment.KEY_TEXT_REPLIES, smartReplies);
        }
        if (mNotificationCategorizer.shouldSilence(entry)) {
            final int importance = entry.getImportance() < IMPORTANCE_LOW
                    ? entry.getImportance() : IMPORTANCE_LOW;
            signals.putInt(KEY_IMPORTANCE, importance);
        } else {
            // Even if no change is made, send an identity adjustment for metric logging.
            signals.putInt(KEY_IMPORTANCE, entry.getImportance());
        }

        return new Adjustment(
                entry.getSbn().getPackageName(),
                entry.getSbn().getKey(),
                signals,
                "",
                entry.getSbn().getUserId());
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn, RankingMap rankingMap) {
        if (DEBUG) Log.i(TAG, "POSTED " + sbn.getKey());
        try {
            if (!isForCurrentUser(sbn)) {
                return;
            }
            Ranking ranking = new Ranking();
            rankingMap.getRanking(sbn.getKey(), ranking);
            if (ranking != null && ranking.getChannel() != null) {
                NotificationEntry entry = new NotificationEntry(this, mPackageManager,
                        sbn, ranking.getChannel(), mSmsHelper);
                mLiveNotifications.put(sbn.getKey(), entry);
            }
        } catch (Throwable e) {
            Log.e(TAG, "Error occurred processing post", e);
        }
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn, RankingMap rankingMap,
            NotificationStats stats, int reason) {
        try {
            if (!isForCurrentUser(sbn)) {
                return;
            }

            mLiveNotifications.remove(sbn.getKey());

        } catch (Throwable e) {
            Log.e(TAG, "Error occurred processing removal of " + sbn.getKey(), e);
        }
    }

    @Override
    public void onNotificationSnoozedUntilContext(StatusBarNotification sbn,
            String snoozeCriterionId) {
    }

    @Override
    public void onNotificationsSeen(List<String> keys) {
    }

    @Override
    public void onNotificationExpansionChanged(@NonNull String key, boolean isUserAction,
            boolean isExpanded) {
        if (DEBUG) {
            Log.d(TAG, "onNotificationExpansionChanged() called with: key = [" + key
                    + "], isUserAction = [" + isUserAction + "], isExpanded = [" + isExpanded
                    + "]");
        }
        NotificationEntry entry = mLiveNotifications.get(key);

        if (entry != null) {
            mSingleThreadExecutor.submit(
                    () -> mSmartSuggestionsHelper.onNotificationExpansionChanged(
                            entry.getSbn(), isExpanded));
        }
    }

    @Override
    public void onNotificationDirectReplied(@NonNull String key) {
        if (DEBUG) Log.i(TAG, "onNotificationDirectReplied " + key);
        mSingleThreadExecutor.submit(() -> mSmartSuggestionsHelper.onNotificationDirectReplied(key));
    }

    @Override
    public void onSuggestedReplySent(@NonNull String key, @NonNull CharSequence reply,
            int source) {
        if (DEBUG) {
            Log.d(TAG, "onSuggestedReplySent() called with: key = [" + key + "], reply = [" + reply
                    + "], source = [" + source + "]");
        }
        mSingleThreadExecutor.submit(
                () -> mSmartSuggestionsHelper.onSuggestedReplySent(key, reply, source));
    }

    @Override
    public void onActionInvoked(@NonNull String key, @NonNull Notification.Action action,
            int source) {
        if (DEBUG) {
            Log.d(TAG,
                    "onActionInvoked() called with: key = [" + key + "], action = [" + action.title
                            + "], source = [" + source + "]");
        }
        mSingleThreadExecutor.submit(
                () -> mSmartSuggestionsHelper.onActionClicked(key, action, source));
    }

    @Override
    public void onListenerConnected() {
        if (DEBUG) Log.i(TAG, "Connected");
    }

    @Override
    public void onListenerDisconnected() {
    }

    private boolean isForCurrentUser(StatusBarNotification sbn) {
        return sbn != null && sbn.getUserId() == UserHandle.myUserId();
    }
}
