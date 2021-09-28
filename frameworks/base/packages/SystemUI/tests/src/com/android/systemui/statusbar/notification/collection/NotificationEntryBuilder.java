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

package com.android.systemui.statusbar.notification.collection;

import android.annotation.Nullable;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.pm.ShortcutInfo;
import android.os.UserHandle;
import android.service.notification.SnoozeCriterion;
import android.service.notification.StatusBarNotification;

import com.android.internal.logging.InstanceId;
import com.android.systemui.statusbar.RankingBuilder;
import com.android.systemui.statusbar.SbnBuilder;
import com.android.systemui.util.time.FakeSystemClock;

import java.util.ArrayList;

/**
 * Combined builder for constructing a NotificationEntry and its associated StatusBarNotification
 * and Ranking. Is largely a proxy for the SBN and Ranking builders, but does a little extra magic
 * to make sure the keys match between the two, etc.
 *
 * Has the ability to set ListEntry properties as well.
 *
 * Only for use in tests.
 */
public class NotificationEntryBuilder {
    private final SbnBuilder mSbnBuilder = new SbnBuilder();
    private final RankingBuilder mRankingBuilder = new RankingBuilder();
    private final FakeSystemClock mClock = new FakeSystemClock();
    private StatusBarNotification mSbn = null;

    /* ListEntry properties */
    private GroupEntry mParent;
    private int mSection = -1;

    /* If set, use this creation time instead of mClock.uptimeMillis */
    private long mCreationTime = -1;

    public NotificationEntry build() {
        StatusBarNotification sbn = mSbn != null ? mSbn : mSbnBuilder.build();
        mRankingBuilder.setKey(sbn.getKey());
        long creationTime = mCreationTime != -1 ? mCreationTime : mClock.uptimeMillis();
        final NotificationEntry entry = new NotificationEntry(
                sbn, mRankingBuilder.build(), mClock.uptimeMillis());

        /* ListEntry properties */
        entry.setParent(mParent);
        entry.getAttachState().setSectionIndex(mSection);
        return entry;
    }

    /**
     * Sets the parent.
     */
    public NotificationEntryBuilder setParent(@Nullable GroupEntry parent) {
        mParent = parent;
        return this;
    }

    /**
     * Sets the section.
     */
    public NotificationEntryBuilder setSection(int section) {
        mSection = section;
        return this;
    }

    /**
     * Sets the SBN directly. If set, causes all calls to delegated SbnBuilder methods to be
     * ignored.
     */
    public NotificationEntryBuilder setSbn(@Nullable StatusBarNotification sbn) {
        mSbn = sbn;
        return this;
    }

    /**
     * Set the creation time
     */
    public NotificationEntryBuilder setCreationTime(long creationTime) {
        mCreationTime = creationTime;
        return this;
    }

    /* Delegated to SbnBuilder */

    public NotificationEntryBuilder setPkg(String pkg) {
        mSbnBuilder.setPkg(pkg);
        return this;
    }

    public NotificationEntryBuilder setOpPkg(String opPkg) {
        mSbnBuilder.setOpPkg(opPkg);
        return this;
    }

    public NotificationEntryBuilder setId(int id) {
        mSbnBuilder.setId(id);
        return this;
    }

    public NotificationEntryBuilder setTag(String tag) {
        mSbnBuilder.setTag(tag);
        return this;
    }

    public NotificationEntryBuilder setUid(int uid) {
        mSbnBuilder.setUid(uid);
        return this;
    }

    public NotificationEntryBuilder setInitialPid(int initialPid) {
        mSbnBuilder.setInitialPid(initialPid);
        return this;
    }

    public NotificationEntryBuilder setNotification(Notification notification) {
        mSbnBuilder.setNotification(notification);
        return this;
    }

    public Notification.Builder modifyNotification(Context context) {
        return mSbnBuilder.modifyNotification(context);
    }

    public NotificationEntryBuilder setUser(UserHandle user) {
        mSbnBuilder.setUser(user);
        return this;
    }

    public NotificationEntryBuilder setOverrideGroupKey(String overrideGroupKey) {
        mSbnBuilder.setOverrideGroupKey(overrideGroupKey);
        return this;
    }

    public NotificationEntryBuilder setPostTime(long postTime) {
        mSbnBuilder.setPostTime(postTime);
        return this;
    }

    public NotificationEntryBuilder setInstanceId(InstanceId instanceId) {
        mSbnBuilder.setInstanceId(instanceId);
        return this;
    }

    /* Delegated to Notification.Builder (via SbnBuilder) */

    public NotificationEntryBuilder setContentTitle(Context context, String contentTitle) {
        mSbnBuilder.setContentTitle(context, contentTitle);
        return this;
    }

    public NotificationEntryBuilder setContentText(Context context, String contentText) {
        mSbnBuilder.setContentText(context, contentText);
        return this;
    }

    public NotificationEntryBuilder setGroup(Context context, String groupKey) {
        mSbnBuilder.setGroup(context, groupKey);
        return this;
    }

    public NotificationEntryBuilder setGroupSummary(Context context, boolean isGroupSummary) {
        mSbnBuilder.setGroupSummary(context, isGroupSummary);
        return this;
    }

    public NotificationEntryBuilder setFlag(Context context, int mask, boolean value) {
        mSbnBuilder.setFlag(context, mask, value);
        return this;
    }

    /* Delegated to RankingBuilder */

    public NotificationEntryBuilder setRank(int rank) {
        mRankingBuilder.setRank(rank);
        return this;
    }

    public NotificationEntryBuilder setMatchesInterruptionFilter(
            boolean matchesInterruptionFilter) {
        mRankingBuilder.setMatchesInterruptionFilter(matchesInterruptionFilter);
        return this;
    }

    public NotificationEntryBuilder setVisibilityOverride(int visibilityOverride) {
        mRankingBuilder.setVisibilityOverride(visibilityOverride);
        return this;
    }

    public NotificationEntryBuilder setSuppressedVisualEffects(int suppressedVisualEffects) {
        mRankingBuilder.setSuppressedVisualEffects(suppressedVisualEffects);
        return this;
    }

    public NotificationEntryBuilder setExplanation(CharSequence explanation) {
        mRankingBuilder.setExplanation(explanation);
        return this;
    }

    public NotificationEntryBuilder setAdditionalPeople(ArrayList<String> additionalPeople) {
        mRankingBuilder.setAdditionalPeople(additionalPeople);
        return this;
    }

    public NotificationEntryBuilder setSnoozeCriteria(
            ArrayList<SnoozeCriterion> snoozeCriteria) {
        mRankingBuilder.setSnoozeCriteria(snoozeCriteria);
        return this;
    }

    public NotificationEntryBuilder setCanShowBadge(boolean canShowBadge) {
        mRankingBuilder.setCanShowBadge(canShowBadge);
        return this;
    }

    public NotificationEntryBuilder setSuspended(boolean suspended) {
        mRankingBuilder.setSuspended(suspended);
        return this;
    }

    public NotificationEntryBuilder setLastAudiblyAlertedMs(long lastAudiblyAlertedMs) {
        mRankingBuilder.setLastAudiblyAlertedMs(lastAudiblyAlertedMs);
        return this;
    }

    public NotificationEntryBuilder setNoisy(boolean noisy) {
        mRankingBuilder.setNoisy(noisy);
        return this;
    }

    public NotificationEntryBuilder setCanBubble(boolean canBubble) {
        mRankingBuilder.setCanBubble(canBubble);
        return this;
    }

    public NotificationEntryBuilder setImportance(@NotificationManager.Importance int importance) {
        mRankingBuilder.setImportance(importance);
        return this;
    }

    public NotificationEntryBuilder setUserSentiment(int userSentiment) {
        mRankingBuilder.setUserSentiment(userSentiment);
        return this;
    }

    public NotificationEntryBuilder setChannel(NotificationChannel channel) {
        mRankingBuilder.setChannel(channel);
        return this;
    }

    public NotificationEntryBuilder setSmartActions(
            ArrayList<Notification.Action> smartActions) {
        mRankingBuilder.setSmartActions(smartActions);
        return this;
    }

    public NotificationEntryBuilder setSmartActions(Notification.Action... smartActions) {
        mRankingBuilder.setSmartActions(smartActions);
        return this;
    }

    public NotificationEntryBuilder setSmartReplies(ArrayList<CharSequence> smartReplies) {
        mRankingBuilder.setSmartReplies(smartReplies);
        return this;
    }

    public NotificationEntryBuilder setSmartReplies(CharSequence... smartReplies) {
        mRankingBuilder.setSmartReplies(smartReplies);
        return this;
    }

    public NotificationEntryBuilder setShortcutInfo(ShortcutInfo shortcutInfo) {
        mRankingBuilder.setShortcutInfo(shortcutInfo);
        return this;
    }
}
