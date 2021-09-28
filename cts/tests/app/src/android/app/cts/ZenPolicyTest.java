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

package android.app.cts;

import android.app.NotificationManager;
import android.os.Parcel;
import android.service.notification.ZenPolicy;

import junit.framework.Assert;

import org.junit.Test;

public class ZenPolicyTest extends Assert {

    @Test
    public void testWriteToParcel() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder()
                .allowAlarms(true)
                .allowCalls(ZenPolicy.PEOPLE_TYPE_NONE)
                .allowMessages(ZenPolicy.PEOPLE_TYPE_STARRED)
                .allowConversations(ZenPolicy.CONVERSATION_SENDERS_ANYONE)
                .showInNotificationList(false);
        ZenPolicy policy = builder.build();

        Parcel parcel = Parcel.obtain();
        policy.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);

        ZenPolicy policyFromParcel = ZenPolicy.CREATOR.createFromParcel(parcel);
        assertEquals(policy, policyFromParcel);
    }

    @Test
    public void testAlarms() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowAlarms(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryAlarms());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowAlarms(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryAlarms());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testMedia() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowMedia(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryMedia());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA);
        assertAllVisualEffectsUnsetExcept(policy, -1);

        policy = builder.allowMedia(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryMedia());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testSystem() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowSystem(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategorySystem());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_SYSTEM);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowSystem(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategorySystem());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_SYSTEM);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testReminders() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowReminders(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryReminders());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_REMINDERS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowReminders(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryReminders());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_REMINDERS);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testEvents() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowEvents(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryEvents());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_EVENTS);
        assertAllVisualEffectsUnset(policy);

        builder.allowEvents(false);
        policy = builder.build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryEvents());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_EVENTS);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testRepeatCallers() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowRepeatCallers(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryRepeatCallers());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_REPEAT_CALLERS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowRepeatCallers(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryRepeatCallers());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_REPEAT_CALLERS);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testMessages() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowMessages(ZenPolicy.PEOPLE_TYPE_NONE).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_NONE, policy.getPriorityMessageSenders());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryMessages());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_MESSAGES);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowMessages(ZenPolicy.PEOPLE_TYPE_ANYONE).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_ANYONE, policy.getPriorityMessageSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryMessages());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_MESSAGES);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowMessages(ZenPolicy.PEOPLE_TYPE_CONTACTS).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_CONTACTS, policy.getPriorityMessageSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryMessages());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_MESSAGES);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowMessages(ZenPolicy.PEOPLE_TYPE_STARRED).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_STARRED, policy.getPriorityMessageSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryMessages());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_MESSAGES);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testCalls() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowCalls(ZenPolicy.PEOPLE_TYPE_NONE).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_NONE, policy.getPriorityCallSenders());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryCalls());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CALLS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowCalls(ZenPolicy.PEOPLE_TYPE_ANYONE).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_ANYONE, policy.getPriorityCallSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryCalls());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CALLS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowCalls(ZenPolicy.PEOPLE_TYPE_CONTACTS).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_CONTACTS, policy.getPriorityCallSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryCalls());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CALLS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowCalls(ZenPolicy.PEOPLE_TYPE_STARRED).build();
        assertEquals(ZenPolicy.PEOPLE_TYPE_STARRED, policy.getPriorityCallSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryCalls());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CALLS);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testConversations() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowConversations(ZenPolicy.CONVERSATION_SENDERS_NONE).build();
        assertEquals(ZenPolicy.CONVERSATION_SENDERS_NONE, policy.getPriorityConversationSenders());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryConversations());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CONVERSATIONS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowConversations(ZenPolicy.CONVERSATION_SENDERS_ANYONE).build();
        assertEquals(ZenPolicy.CONVERSATION_SENDERS_ANYONE,
                policy.getPriorityConversationSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryConversations());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CONVERSATIONS);
        assertAllVisualEffectsUnset(policy);

        policy = builder.allowConversations(ZenPolicy.CONVERSATION_SENDERS_IMPORTANT).build();
        assertEquals(ZenPolicy.CONVERSATION_SENDERS_IMPORTANT,
                policy.getPriorityConversationSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryConversations());
        assertAllPriorityCategoriesUnsetExcept(policy,
                NotificationManager.Policy.PRIORITY_CATEGORY_CONVERSATIONS);
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testFullScreenIntent() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showFullScreenIntent(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectFullScreenIntent());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_FULL_SCREEN_INTENT);

        policy = builder.showFullScreenIntent(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectFullScreenIntent());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_FULL_SCREEN_INTENT);
    }

    @Test
    public void testLights() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showLights(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectLights());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_LIGHTS);

        policy = builder.showLights(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectLights());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_LIGHTS);
    }

    @Test
    public void testPeek() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showPeeking(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectPeek());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_PEEK);

        policy = builder.showPeeking(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectPeek());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_PEEK);
    }

    @Test
    public void testStatusBar() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showStatusBarIcons(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectStatusBar());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_STATUS_BAR);

        policy = builder.showStatusBarIcons(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectStatusBar());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_STATUS_BAR);
    }

    @Test
    public void testBadge() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showBadges(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectBadge());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_BADGE);

        policy = builder.showBadges(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectBadge());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_BADGE);
    }

    @Test
    public void testAmbient() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showInAmbientDisplay(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectAmbient());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_AMBIENT);

        policy = builder.showInAmbientDisplay(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectAmbient());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_AMBIENT);
    }

    @Test
    public void testNotificationList() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showInNotificationList(true).build();
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectNotificationList());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_NOTIFICATION_LIST);

        policy = builder.showInNotificationList(false).build();
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectNotificationList());
        assertAllPriorityCategoriesUnset(policy);
        assertAllVisualEffectsUnsetExcept(policy,
                NotificationManager.Policy.SUPPRESSED_EFFECT_NOTIFICATION_LIST);
    }

    @Test
    public void testDisallowAllSounds() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.disallowAllSounds().build();

        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryAlarms());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryMedia());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategorySystem());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryReminders());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryEvents());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryMessages());
        assertEquals(ZenPolicy.PEOPLE_TYPE_NONE, policy.getPriorityMessageSenders());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryCalls());
        assertEquals(ZenPolicy.PEOPLE_TYPE_NONE, policy.getPriorityCallSenders());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryRepeatCallers());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getPriorityCategoryConversations());
        assertEquals(ZenPolicy.CONVERSATION_SENDERS_NONE,
                policy.getPriorityConversationSenders());
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testAllowAllSounds() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.allowAllSounds().build();

        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryAlarms());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryMedia());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategorySystem());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryReminders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryEvents());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryMessages());
        assertEquals(ZenPolicy.PEOPLE_TYPE_ANYONE, policy.getPriorityMessageSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryCalls());
        assertEquals(ZenPolicy.PEOPLE_TYPE_ANYONE, policy.getPriorityCallSenders());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryRepeatCallers());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getPriorityCategoryConversations());
        assertEquals(ZenPolicy.CONVERSATION_SENDERS_ANYONE,
                policy.getPriorityConversationSenders());
        assertAllVisualEffectsUnset(policy);
    }

    @Test
    public void testAllowAllVisualEffects() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.showAllVisualEffects().build();

        assertAllPriorityCategoriesUnset(policy);
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectFullScreenIntent());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectLights());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectPeek());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectStatusBar());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectBadge());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectAmbient());
        assertEquals(ZenPolicy.STATE_ALLOW, policy.getVisualEffectNotificationList());
    }

    @Test
    public void testDisallowAllVisualEffects() {
        ZenPolicy.Builder builder = new ZenPolicy.Builder();
        ZenPolicy policy = builder.hideAllVisualEffects().build();

        assertAllPriorityCategoriesUnset(policy);
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectFullScreenIntent());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectLights());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectPeek());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectStatusBar());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectBadge());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectAmbient());
        assertEquals(ZenPolicy.STATE_DISALLOW, policy.getVisualEffectNotificationList());
    }

    private void assertAllPriorityCategoriesUnsetExcept(ZenPolicy policy, int except) {
        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_REMINDERS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryReminders());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_EVENTS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryEvents());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_MESSAGES) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryMessages());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_CALLS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryCalls());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_REPEAT_CALLERS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryRepeatCallers());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryAlarms());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryMedia());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_SYSTEM) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategorySystem());
        }

        if (except != NotificationManager.Policy.PRIORITY_CATEGORY_CONVERSATIONS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getPriorityCategoryConversations());
        }
    }

    private void assertAllVisualEffectsUnsetExcept(ZenPolicy policy, int except) {
        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_FULL_SCREEN_INTENT) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectFullScreenIntent());
        }

        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_LIGHTS) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectLights());
        }

        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_PEEK) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectPeek());
        }

        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_STATUS_BAR) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectStatusBar());
        }

        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_BADGE) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectBadge());
        }

        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_AMBIENT) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectAmbient());
        }

        if (except != NotificationManager.Policy.SUPPRESSED_EFFECT_NOTIFICATION_LIST) {
            assertEquals(ZenPolicy.STATE_UNSET, policy.getVisualEffectNotificationList());
        }
    }

    private void assertAllPriorityCategoriesUnset(ZenPolicy policy) {
        assertAllPriorityCategoriesUnsetExcept(policy, -1);
    }

    private void assertAllVisualEffectsUnset(ZenPolicy policy) {
        assertAllVisualEffectsUnsetExcept(policy, -1);
    }
}
