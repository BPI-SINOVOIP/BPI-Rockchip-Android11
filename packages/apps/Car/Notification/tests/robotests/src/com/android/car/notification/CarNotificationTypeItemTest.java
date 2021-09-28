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
 * limitations under the License
 */

package com.android.car.notification;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.LayoutRes;
import androidx.test.core.app.ApplicationProvider;

import com.android.car.notification.template.BasicNotificationViewHolder;
import com.android.car.notification.template.CallNotificationViewHolder;
import com.android.car.notification.template.CarNotificationBaseViewHolder;
import com.android.car.notification.template.GroupNotificationViewHolder;
import com.android.car.notification.template.GroupSummaryNotificationViewHolder;
import com.android.car.notification.template.InboxNotificationViewHolder;
import com.android.car.notification.template.MessageNotificationViewHolder;
import com.android.car.notification.template.NavigationNotificationViewHolder;
import com.android.car.notification.template.ProgressNotificationViewHolder;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class CarNotificationTypeItemTest {
    // No template is represented by -1 in CarNotificationTypeItem.
    private static final int NO_TEMPLATE = -1;

    private final Context mApplicationContext = ApplicationProvider.getApplicationContext();

    @Mock
    private NotificationClickHandlerFactory mClickHandlerFactory;

    @Test
    public void navigationNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem navigation = CarNotificationTypeItem.of(
                NotificationViewType.NAVIGATION);
        assertThat(navigation.getNotificationType()).isEqualTo(NotificationViewType.NAVIGATION);

        assertProperties(navigation, R.layout.navigation_headsup_notification_template,
                NO_TEMPLATE, NavigationNotificationViewHolder.class);
    }

    @Test
    public void callNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem call = CarNotificationTypeItem.of(
                NotificationViewType.CALL);
        assertThat(call.getNotificationType()).isEqualTo(NotificationViewType.CALL);

        assertProperties(call, R.layout.call_headsup_notification_template,
                R.layout.call_notification_template, CallNotificationViewHolder.class);
    }

    @Test
    public void messageNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem message = CarNotificationTypeItem.of(
                NotificationViewType.MESSAGE);
        assertThat(message.getNotificationType()).isEqualTo(NotificationViewType.MESSAGE);

        assertProperties(message, R.layout.message_headsup_notification_template,
                R.layout.message_notification_template, MessageNotificationViewHolder.class);
    }

    @Test
    public void messageInGroupNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem messageInGroup = CarNotificationTypeItem.of(
                NotificationViewType.MESSAGE_IN_GROUP);
        assertThat(messageInGroup.getNotificationType()).isEqualTo(
                NotificationViewType.MESSAGE_IN_GROUP);

        assertProperties(messageInGroup, NO_TEMPLATE, R.layout.message_notification_template_inner,
                MessageNotificationViewHolder.class);
    }

    @Test
    public void inboxNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem inbox = CarNotificationTypeItem.of(
                NotificationViewType.INBOX);
        assertThat(inbox.getNotificationType()).isEqualTo(NotificationViewType.INBOX);

        assertProperties(inbox, R.layout.inbox_headsup_notification_template,
                R.layout.inbox_notification_template, InboxNotificationViewHolder.class);
    }

    @Test
    public void inboxInGroupNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem inboxInGroup = CarNotificationTypeItem.of(
                NotificationViewType.INBOX_IN_GROUP);
        assertThat(inboxInGroup.getNotificationType()).isEqualTo(
                NotificationViewType.INBOX_IN_GROUP);

        assertProperties(inboxInGroup, NO_TEMPLATE, R.layout.inbox_notification_template_inner,
                InboxNotificationViewHolder.class);
    }

    @Test
    public void groupExpandedNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem groupExpanded = CarNotificationTypeItem.of(
                NotificationViewType.GROUP_EXPANDED);
        assertThat(groupExpanded.getNotificationType()).isEqualTo(
                NotificationViewType.GROUP_EXPANDED);

        assertProperties(groupExpanded, NO_TEMPLATE, R.layout.group_notification_template,
                GroupNotificationViewHolder.class);
    }

    @Test
    public void groupCollapsedNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem groupCollapsed = CarNotificationTypeItem.of(
                NotificationViewType.GROUP_COLLAPSED);
        assertThat(groupCollapsed.getNotificationType()).isEqualTo(
                NotificationViewType.GROUP_COLLAPSED);

        assertProperties(groupCollapsed, NO_TEMPLATE, R.layout.group_notification_template,
                GroupNotificationViewHolder.class);
    }

    @Test
    public void groupSummaryNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem groupSummary = CarNotificationTypeItem.of(
                NotificationViewType.GROUP_SUMMARY);
        assertThat(groupSummary.getNotificationType()).isEqualTo(
                NotificationViewType.GROUP_SUMMARY);

        assertProperties(groupSummary, NO_TEMPLATE, R.layout.group_summary_notification_template,
                GroupSummaryNotificationViewHolder.class);
    }

    @Test
    public void progressNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem progress = CarNotificationTypeItem.of(
                NotificationViewType.PROGRESS);
        assertThat(progress.getNotificationType()).isEqualTo(NotificationViewType.PROGRESS);

        assertProperties(progress, NO_TEMPLATE, R.layout.progress_notification_template,
                ProgressNotificationViewHolder.class);
    }

    @Test
    public void progressInGroupNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem progressInGroup = CarNotificationTypeItem.of(
                NotificationViewType.PROGRESS_IN_GROUP);
        assertThat(progressInGroup.getNotificationType()).isEqualTo(
                NotificationViewType.PROGRESS_IN_GROUP);

        assertProperties(progressInGroup, NO_TEMPLATE,
                R.layout.progress_notification_template_inner,
                ProgressNotificationViewHolder.class);
    }

    @Test
    public void basicNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem basic = CarNotificationTypeItem.of(
                NotificationViewType.BASIC);
        assertThat(basic.getNotificationType()).isEqualTo(NotificationViewType.BASIC);

        assertProperties(basic, R.layout.basic_headsup_notification_template,
                R.layout.basic_notification_template, BasicNotificationViewHolder.class);
    }

    @Test
    public void basicInGroupNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem basicInGroup = CarNotificationTypeItem.of(
                NotificationViewType.BASIC_IN_GROUP);
        assertThat(basicInGroup.getNotificationType()).isEqualTo(
                NotificationViewType.BASIC_IN_GROUP);

        assertProperties(basicInGroup, NO_TEMPLATE, R.layout.basic_notification_template_inner,
                BasicNotificationViewHolder.class);
    }

    @Test
    public void carWarningNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem carWarning = CarNotificationTypeItem.of(
                NotificationViewType.CAR_WARNING);
        assertThat(carWarning.getNotificationType()).isEqualTo(
                NotificationViewType.CAR_WARNING);

        assertProperties(carWarning, R.layout.car_warning_headsup_notification_template,
                R.layout.car_warning_notification_template, BasicNotificationViewHolder.class);
    }

    @Test
    public void carInformationNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem carInformation = CarNotificationTypeItem.of(
                NotificationViewType.CAR_INFORMATION);
        assertThat(carInformation.getNotificationType()).isEqualTo(
                NotificationViewType.CAR_INFORMATION);

        assertProperties(carInformation, R.layout.car_information_headsup_notification_template,
                R.layout.car_information_notification_template, BasicNotificationViewHolder.class);
    }

    @Test
    public void carInformationInGroupNotificationType_shouldHaveCorrectValues() {
        CarNotificationTypeItem carInfoInGroup = CarNotificationTypeItem.of(
                NotificationViewType.CAR_INFORMATION_IN_GROUP);
        assertThat(carInfoInGroup.getNotificationType()).isEqualTo(
                NotificationViewType.CAR_INFORMATION_IN_GROUP);

        assertProperties(carInfoInGroup, NO_TEMPLATE,
                R.layout.car_information_notification_template_inner,
                BasicNotificationViewHolder.class);
    }

    @Test
    public void getCarNotificationTypeItem_invalidNotificationType_shouldThrowException() {
        assertThrows(IllegalArgumentException.class, () -> CarNotificationTypeItem.of(123));
    }

    private void assertProperties(CarNotificationTypeItem item, @LayoutRes int itemHeadsUpTemplate,
            @LayoutRes int itemNotificationCenterTemplate, Class expectedViewHolderClazz) {
        int headsUpTemplate = item.getHeadsUpTemplate();
        assertThat(headsUpTemplate).isEqualTo(itemHeadsUpTemplate);

        int notificationCenterTemplate = item.getNotificationCenterTemplate();
        assertThat(notificationCenterTemplate).isEqualTo(itemNotificationCenterTemplate);

        View viewTemplate = LayoutInflater.from(mApplicationContext).inflate(
                headsUpTemplate != NO_TEMPLATE ? headsUpTemplate : notificationCenterTemplate,
                null);

        CarNotificationBaseViewHolder viewHolder = item.getViewHolder(viewTemplate,
                mClickHandlerFactory);
        assertThat(viewHolder).isInstanceOf(expectedViewHolderClazz);
    }
}
