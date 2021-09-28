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

package com.android.car.notification;

import static android.service.notification.NotificationListenerService.Ranking.USER_SENTIMENT_NEGATIVE;
import static android.service.notification.NotificationListenerService.Ranking.USER_SENTIMENT_NEUTRAL;
import static android.service.notification.NotificationListenerService.Ranking.USER_SENTIMENT_POSITIVE;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.PendingIntent;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.os.UserHandle;
import android.service.notification.NotificationListenerService;
import android.service.notification.SnoozeCriterion;
import android.service.notification.StatusBarNotification;
import android.telephony.TelephonyManager;

import com.android.car.notification.testutils.ShadowApplicationPackageManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowApplicationPackageManager.class})
public class PreprocessingManagerTest {

    private Context mContext;

    private static final String PKG = "com.package.PREPROCESSING_MANAGER_TEST";
    private static final String OP_PKG = "OpPackage";
    private static final int ID = 1;
    private static final String TAG = "Tag";
    private static final int UID = 2;
    private static final int INITIAL_PID = 3;
    private static final String CHANNEL_ID = "CHANNEL_ID";
    private static final String CONTENT_TITLE = "CONTENT_TITLE";
    private static final String OVERRIDE_GROUP_KEY = "OVERRIDE_GROUP_KEY";
    private static final long POST_TIME = 12345l;
    private static final UserHandle USER_HANDLE = new UserHandle(12);
    private static final String GROUP_KEY_A = "GROUP_KEY_A";
    private static final String GROUP_KEY_B = "GROUP_KEY_B";
    private static final String GROUP_KEY_C = "GROUP_KEY_C";
    private static final int MAX_STRING_LENGTH = 10;

    private PreprocessingManager mPreprocessingManager;
    @Mock
    private ApplicationInfo mApplicationInfo;
    @Mock
    private StatusBarNotification mStatusBarNotification1;
    @Mock
    private StatusBarNotification mStatusBarNotification2;
    @Mock
    private StatusBarNotification mStatusBarNotification3;
    @Mock
    private StatusBarNotification mStatusBarNotification4;
    @Mock
    private StatusBarNotification mStatusBarNotification5;
    @Mock
    private StatusBarNotification mStatusBarNotification6;
    @Mock
    private StatusBarNotification mAdditionalStatusBarNotification;
    @Mock
    private StatusBarNotification mSummaryStatusBarNotification;
    @Mock
    private CarUxRestrictions mCarUxRestrictions;
    @Mock
    private CarUxRestrictionManagerWrapper mCarUxRestrictionManagerWrapper;
    @Mock
    private PreprocessingManager.CallStateListener mCallStateListener1;
    @Mock
    private PreprocessingManager.CallStateListener mCallStateListener2;
    @Mock
    private Notification mMediaNotification;
    @Mock
    private Notification mSummaryNotification;
    private Notification mForegroundNotification;
    private Notification mBackgroundNotification;
    private Notification mNavigationNotification;

    // Following AlertEntry var names describe the type of notifications they wrap.
    private AlertEntry mLessImportantBackground;
    private AlertEntry mLessImportantForeground;
    private AlertEntry mMedia;
    private AlertEntry mNavigation;
    private AlertEntry mImportantBackground;
    private AlertEntry mImportantForeground;

    private List<AlertEntry> mAlertEntries;
    private Map<String, AlertEntry> mAlertEntriesMap;
    private NotificationListenerService.RankingMap mRankingMap;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mPreprocessingManager = PreprocessingManager.getInstance(mContext);

        mForegroundNotification = generateNotification(
                /* isForeground= */true, /* isNavigation= */ false);
        mBackgroundNotification = generateNotification(
                /* isForeground= */false, /* isNavigation= */ false);
        mNavigationNotification = generateNotification(
                /* isForeground= */true, /* isNavigation= */ true);


        when(mMediaNotification.isMediaNotification()).thenReturn(true);

        // Key describes the notification that the StatusBarNotification contains.
        when(mStatusBarNotification1.getKey()).thenReturn("KEY_LESS_IMPORTANT_BACKGROUND");
        when(mStatusBarNotification2.getKey()).thenReturn("KEY_LESS_IMPORTANT_FOREGROUND");
        when(mStatusBarNotification3.getKey()).thenReturn("KEY_MEDIA");
        when(mStatusBarNotification4.getKey()).thenReturn("KEY_NAVIGATION");
        when(mStatusBarNotification5.getKey()).thenReturn("KEY_IMPORTANT_BACKGROUND");
        when(mStatusBarNotification6.getKey()).thenReturn("KEY_IMPORTANT_FOREGROUND");
        when(mSummaryStatusBarNotification.getKey()).thenReturn("KEY_SUMMARY");

        when(mStatusBarNotification1.getGroupKey()).thenReturn(GROUP_KEY_A);
        when(mStatusBarNotification2.getGroupKey()).thenReturn(GROUP_KEY_B);
        when(mStatusBarNotification3.getGroupKey()).thenReturn(GROUP_KEY_A);
        when(mStatusBarNotification4.getGroupKey()).thenReturn(GROUP_KEY_B);
        when(mStatusBarNotification5.getGroupKey()).thenReturn(GROUP_KEY_B);
        when(mStatusBarNotification6.getGroupKey()).thenReturn(GROUP_KEY_C);
        when(mSummaryStatusBarNotification.getGroupKey()).thenReturn(GROUP_KEY_C);

        when(mStatusBarNotification1.getNotification()).thenReturn(mBackgroundNotification);
        when(mStatusBarNotification2.getNotification()).thenReturn(mForegroundNotification);
        when(mStatusBarNotification3.getNotification()).thenReturn(mMediaNotification);
        when(mStatusBarNotification4.getNotification()).thenReturn(mNavigationNotification);
        when(mStatusBarNotification5.getNotification()).thenReturn(mBackgroundNotification);
        when(mStatusBarNotification6.getNotification()).thenReturn(mForegroundNotification);
        when(mSummaryStatusBarNotification.getNotification()).thenReturn(mSummaryNotification);

        when(mSummaryNotification.isGroupSummary()).thenReturn(true);

        // prevents less important foreground notifications from not being filtered due to the
        // application and package setup.
        when(mApplicationInfo.isSignedWithPlatformKey()).thenReturn(true);
        when(mApplicationInfo.isSystemApp()).thenReturn(true);
        when(mApplicationInfo.isPrivilegedApp()).thenReturn(true);
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.applicationInfo = mApplicationInfo;
        ShadowApplicationPackageManager.setPackageInfo(packageInfo);

        // Always start system with no phone calls in progress.
        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_IDLE);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        initTestData();
    }

    @After
    public void tearDown() {
        ShadowApplicationPackageManager.reset();
    }

    @Test
    public void onFilter_showLessImportantNotifications_doesNotFilterNotifications() {
        List<AlertEntry> unfiltered = mAlertEntries.stream().collect(Collectors.toList());
        mPreprocessingManager
                .filter(/* showLessImportantNotifications= */true, mAlertEntries, mRankingMap);

        assertThat(mAlertEntries.equals(unfiltered)).isTrue();
    }

    @Test
    public void onFilter_dontShowLessImportantNotifications_filtersLessImportantForeground() {
        mPreprocessingManager
                .filter( /* showLessImportantNotifications= */ false, mAlertEntries, mRankingMap);

        assertThat(mAlertEntries.contains(mLessImportantBackground)).isTrue();
        assertThat(mAlertEntries.contains(mLessImportantForeground)).isFalse();
    }

    @Test
    public void onFilter_dontShowLessImportantNotifications_doesNotFilterMoreImportant() {
        mPreprocessingManager
                .filter(/* showLessImportantNotifications= */false, mAlertEntries, mRankingMap);

        assertThat(mAlertEntries.contains(mImportantBackground)).isTrue();
        assertThat(mAlertEntries.contains(mImportantForeground)).isTrue();
    }

    @Test
    public void onFilter_dontShowLessImportantNotifications_filtersMediaAndNavigation() {
        mPreprocessingManager
                .filter(/* showLessImportantNotifications= */false, mAlertEntries, mRankingMap);

        assertThat(mAlertEntries.contains(mMedia)).isFalse();
        assertThat(mAlertEntries.contains(mNavigation)).isFalse();
    }

    @Test
    public void onFilter_doShowLessImportantNotifications_doesNotFilterMediaOrNavigation() {
        mPreprocessingManager
                .filter(/* showLessImportantNotifications= */true, mAlertEntries, mRankingMap);

        assertThat(mAlertEntries.contains(mMedia)).isTrue();
        assertThat(mAlertEntries.contains(mNavigation)).isTrue();
    }

    @Test
    public void onFilter_doShowLessImportantNotifications_filtersCalls() {
        StatusBarNotification callSBN = mock(StatusBarNotification.class);
        Notification callNotification = new Notification();
        callNotification.category = Notification.CATEGORY_CALL;
        when(callSBN.getNotification()).thenReturn(callNotification);
        List<AlertEntry> entries = new ArrayList<>();
        entries.add(new AlertEntry(callSBN));

        mPreprocessingManager.filter(true, entries, mRankingMap);
        assertThat(entries).isEmpty();
    }

    @Test
    public void onFilter_dontShowLessImportantNotifications_filtersCalls() {
        StatusBarNotification callSBN = mock(StatusBarNotification.class);
        Notification callNotification = new Notification();
        callNotification.category = Notification.CATEGORY_CALL;
        when(callSBN.getNotification()).thenReturn(callNotification);
        List<AlertEntry> entries = new ArrayList<>();
        entries.add(new AlertEntry(callSBN));

        mPreprocessingManager.filter(false, entries, mRankingMap);
        assertThat(entries).isEmpty();
    }

    @Test
    public void onOptimizeForDriving_alertEntryHasNonMessageNotification_trimsNotificationTexts() {
        when(mCarUxRestrictions.getMaxRestrictedStringLength()).thenReturn(MAX_STRING_LENGTH);
        when(mCarUxRestrictionManagerWrapper.getCurrentCarUxRestrictions())
                .thenReturn(mCarUxRestrictions);
        mPreprocessingManager.setCarUxRestrictionManagerWrapper(mCarUxRestrictionManagerWrapper);

        Notification nonMessageNotification
                = generateNotification(/* isForeground= */ true, /* isNavigation= */ true);
        nonMessageNotification.extras
                .putString(Notification.EXTRA_TITLE, generateStringOfLength(100));
        nonMessageNotification.extras
                .putString(Notification.EXTRA_TEXT, generateStringOfLength(100));
        nonMessageNotification.extras
                .putString(Notification.EXTRA_TITLE_BIG, generateStringOfLength(100));
        nonMessageNotification.extras
                .putString(Notification.EXTRA_SUMMARY_TEXT, generateStringOfLength(100));

        when(mNavigation.getNotification()).thenReturn(nonMessageNotification);

        AlertEntry optimized = mPreprocessingManager.optimizeForDriving(mNavigation);
        Bundle trimmed = optimized.getNotification().extras;

        for (String key : trimmed.keySet()) {
            switch (key) {
                case Notification.EXTRA_TITLE:
                case Notification.EXTRA_TEXT:
                case Notification.EXTRA_TITLE_BIG:
                case Notification.EXTRA_SUMMARY_TEXT:
                    CharSequence text = trimmed.getCharSequence(key);
                    assertThat(text.length() <= MAX_STRING_LENGTH).isTrue();
                default:
                    continue;
            }
        }
    }

    @Test
    public void onOptimizeForDriving_alertEntryHasMessageNotification_doesNotTrimMessageTexts() {
        when(mCarUxRestrictions.getMaxRestrictedStringLength()).thenReturn(MAX_STRING_LENGTH);
        when(mCarUxRestrictionManagerWrapper.getCurrentCarUxRestrictions())
                .thenReturn(mCarUxRestrictions);
        mPreprocessingManager.setCarUxRestrictionManagerWrapper(mCarUxRestrictionManagerWrapper);

        Notification messageNotification
                = generateNotification(/* isForeground= */ true, /* isNavigation= */ true);
        messageNotification.extras
                .putString(Notification.EXTRA_TITLE, generateStringOfLength(100));
        messageNotification.extras
                .putString(Notification.EXTRA_TEXT, generateStringOfLength(100));
        messageNotification.extras
                .putString(Notification.EXTRA_TITLE_BIG, generateStringOfLength(100));
        messageNotification.extras
                .putString(Notification.EXTRA_SUMMARY_TEXT, generateStringOfLength(100));
        messageNotification.category = Notification.CATEGORY_MESSAGE;

        when(mImportantForeground.getNotification()).thenReturn(messageNotification);

        AlertEntry optimized = mPreprocessingManager.optimizeForDriving(mImportantForeground);
        Bundle trimmed = optimized.getNotification().extras;

        for (String key : trimmed.keySet()) {
            switch (key) {
                case Notification.EXTRA_TITLE:
                case Notification.EXTRA_TEXT:
                case Notification.EXTRA_TITLE_BIG:
                case Notification.EXTRA_SUMMARY_TEXT:
                    CharSequence text = trimmed.getCharSequence(key);
                    assertThat(text.length() <= MAX_STRING_LENGTH).isFalse();
                default:
                    continue;
            }
        }
    }

    @Test
    public void onGroup_groupsNotificationsByGroupKey() {
        List<NotificationGroup> groupResult = mPreprocessingManager.group(mAlertEntries);
        String[] actualGroupKeys = new String[groupResult.size()];
        String[] expectedGroupKeys = {GROUP_KEY_A, GROUP_KEY_B, GROUP_KEY_C};

        for (int i = 0; i < groupResult.size(); i++) {
            actualGroupKeys[i] = groupResult.get(i).getGroupKey();
        }

        Arrays.sort(actualGroupKeys);
        Arrays.sort(expectedGroupKeys);

        assertThat(actualGroupKeys).isEqualTo(expectedGroupKeys);
    }

    @Test
    public void onGroup_autoGeneratedGroupWithNoGroupChildren_doesNotShowGroupSummary() {
        List<AlertEntry> list = new ArrayList<>();
        list.add(getEmptyAutoGeneratedGroupSummary());
        List<NotificationGroup> groupResult = mPreprocessingManager.group(list);

        assertThat(groupResult.size() == 0).isTrue();
    }

    @Test
    public void addCallStateListener_preCall_triggerChanges() {
        InOrder listenerInOrder = Mockito.inOrder(mCallStateListener1);
        mPreprocessingManager.addCallStateListener(mCallStateListener1);
        listenerInOrder.verify(mCallStateListener1).onCallStateChanged(false);

        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_OFFHOOK);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        listenerInOrder.verify(mCallStateListener1).onCallStateChanged(true);
    }

    @Test
    public void addCallStateListener_midCall_triggerChanges() {
        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_OFFHOOK);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        mPreprocessingManager.addCallStateListener(mCallStateListener1);

        verify(mCallStateListener1).onCallStateChanged(true);
    }

    @Test
    public void addCallStateListener_postCall_triggerChanges() {
        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_OFFHOOK);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_IDLE);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        mPreprocessingManager.addCallStateListener(mCallStateListener1);

        verify(mCallStateListener1).onCallStateChanged(false);
    }

    @Test
    public void addSameCallListenerTwice_dedupedCorrectly() {
        mPreprocessingManager.addCallStateListener(mCallStateListener1);

        verify(mCallStateListener1).onCallStateChanged(false);
        mPreprocessingManager.addCallStateListener(mCallStateListener1);

        verify(mCallStateListener1, times(1)).onCallStateChanged(false);
    }

    @Test
    public void removeCallStateListener_midCall_triggerChanges() {
        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_OFFHOOK);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        mPreprocessingManager.addCallStateListener(mCallStateListener1);
        // Should get triggered with true before calling removeCallStateListener
        mPreprocessingManager.removeCallStateListener(mCallStateListener1);

        intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_IDLE);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        verify(mCallStateListener1, never()).onCallStateChanged(false);
    }

    @Test
    public void multipleCallStateListeners_triggeredAppropriately() {
        InOrder listenerInOrder1 = Mockito.inOrder(mCallStateListener1);
        InOrder listenerInOrder2 = Mockito.inOrder(mCallStateListener2);
        mPreprocessingManager.addCallStateListener(mCallStateListener1);
        listenerInOrder1.verify(mCallStateListener1).onCallStateChanged(false);

        Intent intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_OFFHOOK);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        mPreprocessingManager.addCallStateListener(mCallStateListener2);
        mPreprocessingManager.removeCallStateListener(mCallStateListener1);

        listenerInOrder1.verify(mCallStateListener1).onCallStateChanged(true);
        listenerInOrder2.verify(mCallStateListener2).onCallStateChanged(true);

        intent = new Intent(TelephonyManager.ACTION_PHONE_STATE_CHANGED);
        intent.putExtra(TelephonyManager.EXTRA_STATE, TelephonyManager.EXTRA_STATE_IDLE);
        mPreprocessingManager.mIntentReceiver.onReceive(mContext, intent);

        // only listener 2 should be triggered w/ false
        listenerInOrder1.verifyNoMoreInteractions();
        listenerInOrder2.verify(mCallStateListener2).onCallStateChanged(false);
    }

    @Test
    public void onGroup_removesNotificationGroupWithOnlySummaryNotification() {
        List<AlertEntry> list = new ArrayList<>();
        list.add(new AlertEntry(mSummaryStatusBarNotification));
        List<NotificationGroup> groupResult = mPreprocessingManager.group(list);

        assertThat(groupResult.isEmpty()).isTrue();
    }

    @Test
    public void onGroup_childNotificationHasTimeStamp_groupHasMostRecentTimeStamp() {
        mBackgroundNotification.when = 0;
        mForegroundNotification.when = 1;
        mNavigationNotification.when = 2;

        mBackgroundNotification.extras.putBoolean(Notification.EXTRA_SHOW_WHEN, true);
        mForegroundNotification.extras.putBoolean(Notification.EXTRA_SHOW_WHEN, true);
        mNavigationNotification.extras.putBoolean(Notification.EXTRA_SHOW_WHEN, true);

        List<NotificationGroup> groupResult = mPreprocessingManager.group(mAlertEntries);

        groupResult.forEach(group -> {
            AlertEntry groupSummaryNotification = group.getGroupSummaryNotification();
            if (groupSummaryNotification != null
                    && groupSummaryNotification.getNotification() != null) {
                assertThat(groupSummaryNotification.getNotification()
                        .extras.getBoolean(Notification.EXTRA_SHOW_WHEN)).isTrue();
            }
        });
    }

    @Test
    public void onRank_ranksNotificationGroups() {
        List<NotificationGroup> groupResult = mPreprocessingManager.group(mAlertEntries);
        List<NotificationGroup> rankResult = mPreprocessingManager.rank(groupResult, mRankingMap);

        // generateRankingMap ranked the notifications in the reverse order.
        String[] expectedOrder = {
                GROUP_KEY_C,
                GROUP_KEY_B,
                GROUP_KEY_A
        };

        for (int i = 0; i < rankResult.size(); i++) {
            String actualGroupKey = rankResult.get(i).getGroupKey();
            String expectedGroupKey = expectedOrder[i];

            assertThat(actualGroupKey).isEqualTo(expectedGroupKey);
        }
    }

    @Test
    public void onRank_ranksNotificationsInEachGroup() {
        List<NotificationGroup> groupResult = mPreprocessingManager.group(mAlertEntries);
        List<NotificationGroup> rankResult = mPreprocessingManager.rank(groupResult, mRankingMap);
        NotificationGroup groupB = rankResult.get(1);

        // first make sure that we have Group B
        assertThat(groupB.getGroupKey()).isEqualTo(GROUP_KEY_B);

        // generateRankingMap ranked the non-background notifications in the reverse order
        String[] expectedOrder = {
                "KEY_NAVIGATION",
                "KEY_LESS_IMPORTANT_FOREGROUND"
        };

        for (int i = 0; i < groupB.getChildNotifications().size(); i++) {
            String actualKey = groupB.getChildNotifications().get(i).getKey();
            String expectedGroupKey = expectedOrder[i];

            assertThat(actualKey).isEqualTo(expectedGroupKey);
        }
    }

    @Test
    public void onAdditionalGroup_returnsTheSameGroupsAsStandardGroup() {
        Notification additionalNotification =
                generateNotification( /* isForegrond= */ true, /* isNavigation= */ false);
        additionalNotification.category = Notification.CATEGORY_MESSAGE;
        when(mAdditionalStatusBarNotification.getKey()).thenReturn("ADDITIONAL");
        when(mAdditionalStatusBarNotification.getGroupKey()).thenReturn(GROUP_KEY_C);
        when(mAdditionalStatusBarNotification.getNotification()).thenReturn(additionalNotification);
        AlertEntry additionalAlertEntry = new AlertEntry(mAdditionalStatusBarNotification);

        mPreprocessingManager.init(mAlertEntriesMap, mRankingMap);
        List<AlertEntry> copy = mPreprocessingManager.filter(/* showLessImportantNotifications= */
                false, new ArrayList<>(mAlertEntries), mRankingMap);
        copy.add(additionalAlertEntry);
        List<NotificationGroup> expected = mPreprocessingManager.group(copy);
        String[] expectedKeys = new String[expected.size()];
        for (int i = 0; i < expectedKeys.length; i++) {
            expectedKeys[i] = expected.get(i).getGroupKey();
        }

        List<NotificationGroup> actual =
                mPreprocessingManager.additionalGroup(additionalAlertEntry);
        String[] actualKeys = new String[actual.size()];
        for (int i = 0; i < actualKeys.length; i++) {
            actualKeys[i] = actual.get(i).getGroupKey();
        }
        // We do not care about the order since they are not ranked yet.
        Arrays.sort(actualKeys);
        Arrays.sort(expectedKeys);
        assertThat(actualKeys).isEqualTo(expectedKeys);
    }

    @Test
    public void onAdditionalRank_returnsTheSameOrderAsStandardRank() {
        List<AlertEntry> testCopy = new ArrayList<>(mAlertEntries);

        List<NotificationGroup> additionalRanked = mPreprocessingManager.additionalRank(
                mPreprocessingManager.group(mAlertEntries), mRankingMap);
        List<NotificationGroup> standardRanked = mPreprocessingManager.rank(
                mPreprocessingManager.group(testCopy), mRankingMap);

        assertThat(additionalRanked.size()).isEqualTo(standardRanked.size());

        for (int i = 0; i < additionalRanked.size(); i++) {
            assertThat(additionalRanked.get(i).getGroupKey()).isEqualTo(
                    standardRanked.get(i).getGroupKey());
        }
    }

    @Test
    public void onUpdateNotifications_notificationRemoved_removesNotification() {
        mPreprocessingManager.init(mAlertEntriesMap, mRankingMap);

        List<NotificationGroup> newList =
                mPreprocessingManager.updateNotifications(
                        /* showLessImportantNotifications= */ false,
                        mImportantForeground,
                        CarNotificationListener.NOTIFY_NOTIFICATION_REMOVED,
                        mRankingMap);

        assertThat(mPreprocessingManager.getOldNotifications().containsKey(
                mImportantForeground.getKey())).isFalse();
    }

    @Test
    public void onUpdateNotification_notificationPosted_isUpdate_putsNotification() {
        mPreprocessingManager.init(mAlertEntriesMap, mRankingMap);
        int beforeSize = mPreprocessingManager.getOldNotifications().size();
        Notification newNotification = new Notification.Builder(mContext, CHANNEL_ID)
                .setContentTitle("NEW_TITLE")
                .setGroup(OVERRIDE_GROUP_KEY)
                .setGroupSummary(false)
                .build();
        newNotification.category = Notification.CATEGORY_NAVIGATION;
        when(mImportantForeground.getStatusBarNotification().getNotification())
                .thenReturn(newNotification);
        List<NotificationGroup> newList =
                mPreprocessingManager.updateNotifications(
                        /* showLessImportantNotifications= */ false,
                        mImportantForeground,
                        CarNotificationListener.NOTIFY_NOTIFICATION_POSTED,
                        mRankingMap);

        int afterSize = mPreprocessingManager.getOldNotifications().size();
        AlertEntry updated = (AlertEntry) mPreprocessingManager.getOldNotifications().get(
                mImportantForeground.getKey());
        assertThat(updated).isNotNull();
        assertThat(updated.getNotification().category).isEqualTo(Notification.CATEGORY_NAVIGATION);
        assertThat(afterSize).isEqualTo(beforeSize);
    }

    @Test
    public void onUpdateNotification_notificationPosted_isNotUpdate_addsNotification() {
        mPreprocessingManager.init(mAlertEntriesMap, mRankingMap);
        int beforeSize = mPreprocessingManager.getOldNotifications().size();
        Notification additionalNotification =
                generateNotification( /* isForegrond= */ true, /* isNavigation= */ false);
        additionalNotification.category = Notification.CATEGORY_MESSAGE;
        when(mAdditionalStatusBarNotification.getKey()).thenReturn("ADDITIONAL");
        when(mAdditionalStatusBarNotification.getGroupKey()).thenReturn(GROUP_KEY_C);
        when(mAdditionalStatusBarNotification.getNotification()).thenReturn(additionalNotification);
        AlertEntry additionalAlertEntry = new AlertEntry(mAdditionalStatusBarNotification);

        List<NotificationGroup> newList =
                mPreprocessingManager.updateNotifications(
                        /* showLessImportantNotifications= */ false,
                        additionalAlertEntry,
                        CarNotificationListener.NOTIFY_NOTIFICATION_POSTED,
                        mRankingMap);

        int afterSize = mPreprocessingManager.getOldNotifications().size();
        AlertEntry posted = (AlertEntry) mPreprocessingManager.getOldNotifications().get(
                additionalAlertEntry.getKey());
        assertThat(posted).isNotNull();
        assertThat(posted.getKey()).isEqualTo("ADDITIONAL");
        assertThat(afterSize).isEqualTo(beforeSize + 1);
    }

    /**
     * Wraps StatusBarNotifications with AlertEntries and generates AlertEntriesMap and
     * RankingsMap.
     */
    private void initTestData() {
        mAlertEntries = new ArrayList<>();
        mLessImportantBackground = new AlertEntry(mStatusBarNotification1);
        mLessImportantForeground = new AlertEntry(mStatusBarNotification2);
        mMedia = new AlertEntry(mStatusBarNotification3);
        mNavigation = new AlertEntry(mStatusBarNotification4);
        mImportantBackground = new AlertEntry(mStatusBarNotification5);
        mImportantForeground = new AlertEntry(mStatusBarNotification6);
        mAlertEntries.add(mLessImportantBackground);
        mAlertEntries.add(mLessImportantForeground);
        mAlertEntries.add(mMedia);
        mAlertEntries.add(mNavigation);
        mAlertEntries.add(mImportantBackground);
        mAlertEntries.add(mImportantForeground);
        mAlertEntriesMap = new HashMap<>();
        mAlertEntriesMap.put(mLessImportantBackground.getKey(), mLessImportantBackground);
        mAlertEntriesMap.put(mLessImportantForeground.getKey(), mLessImportantForeground);
        mAlertEntriesMap.put(mMedia.getKey(), mMedia);
        mAlertEntriesMap.put(mNavigation.getKey(), mNavigation);
        mAlertEntriesMap.put(mImportantBackground.getKey(), mImportantBackground);
        mAlertEntriesMap.put(mImportantForeground.getKey(), mImportantForeground);
        mRankingMap = generateRankingMap(mAlertEntries);
    }

    private AlertEntry getEmptyAutoGeneratedGroupSummary() {
        Notification notification = new Notification.Builder(mContext, CHANNEL_ID)
                .setContentTitle(CONTENT_TITLE)
                .setSmallIcon(android.R.drawable.sym_def_app_icon)
                .setGroup(OVERRIDE_GROUP_KEY)
                .setGroupSummary(true)
                .build();
        StatusBarNotification statusBarNotification = new StatusBarNotification(
                PKG, OP_PKG, ID, TAG, UID, INITIAL_PID, notification, USER_HANDLE,
                OVERRIDE_GROUP_KEY, POST_TIME);
        statusBarNotification.setOverrideGroupKey(OVERRIDE_GROUP_KEY);

        return new AlertEntry(statusBarNotification);
    }

    private Notification generateNotification(boolean isForeground, boolean isNavigation) {
        Notification notification = new Notification.Builder(mContext, CHANNEL_ID)
                .setContentTitle(CONTENT_TITLE)
                .setSmallIcon(android.R.drawable.sym_def_app_icon)
                .setGroup(OVERRIDE_GROUP_KEY)
                .setGroupSummary(true)
                .build();

        if (isForeground) {
            notification.flags = Notification.FLAG_FOREGROUND_SERVICE;
        }

        if (isNavigation) {
            notification.category = Notification.CATEGORY_NAVIGATION;
        }
        return notification;
    }

    private String generateStringOfLength(int length) {
        String string = "";
        for (int i = 0; i < length; i++) {
            string += "*";
        }

        return string;
    }

    /**
     * Ranks the provided alertEntries in reverse order.
     *
     * All methods that follow afterwards help assigning diverse attributes to the {@link
     * android.service.notification.NotificationListenerService.Ranking} instances.
     */
    private NotificationListenerService.RankingMap generateRankingMap(
            List<AlertEntry> alertEntries) {
        NotificationListenerService.Ranking[] rankings =
                new NotificationListenerService.Ranking[alertEntries.size()];
        for (int i = 0; i < alertEntries.size(); i++) {
            String key = alertEntries.get(i).getKey();
            int rank = alertEntries.size() - i; // ranking in reverse order;
            NotificationListenerService.Ranking ranking = new NotificationListenerService.Ranking();
            ranking.populate(
                    key,
                    rank,
                    !isIntercepted(i),
                    getVisibilityOverride(i),
                    getSuppressedVisualEffects(i),
                    getImportance(i),
                    getExplanation(key),
                    getOverrideGroupKey(key),
                    getChannel(key, i),
                    getPeople(key, i),
                    getSnoozeCriteria(key, i),
                    getShowBadge(i),
                    getUserSentiment(i),
                    getHidden(i),
                    lastAudiblyAlerted(i),
                    getNoisy(i),
                    getSmartActions(key, i),
                    getSmartReplies(key, i),
                    canBubble(i),
                    isVisuallyInterruptive(i),
                    isConversation(i),
                    null,
                    isBubble(i)

            );
            rankings[i] = ranking;
        }

        NotificationListenerService.RankingMap rankingMap
                = new NotificationListenerService.RankingMap(rankings);

        return rankingMap;
    }

    private int getVisibilityOverride(int index) {
        return index * 9;
    }

    private String getOverrideGroupKey(String key) {
        return key + key;
    }

    private boolean isIntercepted(int index) {
        return index % 2 == 0;
    }

    private int getSuppressedVisualEffects(int index) {
        return index * 2;
    }

    private int getImportance(int index) {
        return index;
    }

    private String getExplanation(String key) {
        return key + "explain";
    }

    private NotificationChannel getChannel(String key, int index) {
        return new NotificationChannel(key, key, getImportance(index));
    }

    private boolean getShowBadge(int index) {
        return index % 3 == 0;
    }

    private int getUserSentiment(int index) {
        switch (index % 3) {
            case 0:
                return USER_SENTIMENT_NEGATIVE;
            case 1:
                return USER_SENTIMENT_NEUTRAL;
            case 2:
                return USER_SENTIMENT_POSITIVE;
        }
        return USER_SENTIMENT_NEUTRAL;
    }

    private boolean getHidden(int index) {
        return index % 2 == 0;
    }

    private long lastAudiblyAlerted(int index) {
        return index * 2000;
    }

    private boolean getNoisy(int index) {
        return index < 1;
    }

    private ArrayList<String> getPeople(String key, int index) {
        ArrayList<String> people = new ArrayList<>();
        for (int i = 0; i < index; i++) {
            people.add(i + key);
        }
        return people;
    }

    private ArrayList<SnoozeCriterion> getSnoozeCriteria(String key, int index) {
        ArrayList<SnoozeCriterion> snooze = new ArrayList<>();
        for (int i = 0; i < index; i++) {
            snooze.add(new SnoozeCriterion(key + i, getExplanation(key), key));
        }
        return snooze;
    }

    private ArrayList<Notification.Action> getSmartActions(String key, int index) {
        ArrayList<Notification.Action> actions = new ArrayList<>();
        for (int i = 0; i < index; i++) {
            PendingIntent intent = PendingIntent.getBroadcast(
                    mContext,
                    index /*requestCode*/,
                    new Intent("ACTION_" + key),
                    0 /*flags*/);
            actions.add(new Notification.Action.Builder(null /*icon*/, key, intent).build());
        }
        return actions;
    }

    private ArrayList<CharSequence> getSmartReplies(String key, int index) {
        ArrayList<CharSequence> choices = new ArrayList<>();
        for (int i = 0; i < index; i++) {
            choices.add("choice_" + key + "_" + i);
        }
        return choices;
    }

    private boolean canBubble(int index) {
        return index % 4 == 0;
    }

    private boolean isVisuallyInterruptive(int index) {
        return index % 4 == 0;
    }

    private boolean isConversation(int index) {
        return index % 4 == 0;
    }

    private boolean isBubble(int index) {
        return index % 4 == 0;
    }
}
