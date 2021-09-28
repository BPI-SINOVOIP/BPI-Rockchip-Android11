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

package com.android.textclassifier.notification;

import static com.google.common.truth.Truth.assertThat;

import android.app.Notification;
import android.app.Notification.MessagingStyle;
import android.app.PendingIntent;
import android.app.Person;
import android.app.RemoteInput;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Process;
import android.service.notification.StatusBarNotification;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import org.junit.Test;
import org.junit.runner.RunWith;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class NotificationUtilsTest {

  @Test
  public void isMessaging_categoryMessage() {
    Notification notification =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setCategory(Notification.CATEGORY_MESSAGE)
            .build();
    StatusBarNotification statusBarNotification = createStatusBarNotification(notification);

    assertThat(NotificationUtils.isMessaging(statusBarNotification)).isTrue();
  }

  @Test
  public void isMessaging_messagingStyle() {
    Notification notification =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setStyle(new MessagingStyle(new Person.Builder().setName("name").build()))
            .build();
    StatusBarNotification statusBarNotification = createStatusBarNotification(notification);

    assertThat(NotificationUtils.isMessaging(statusBarNotification)).isTrue();
  }

  @Test
  public void isMessaging_publicVersionCategoryMessage() {
    Notification publicVersion =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setCategory(Notification.CATEGORY_MESSAGE)
            .build();
    Notification notification =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setPublicVersion(publicVersion)
            .build();
    StatusBarNotification statusBarNotification = createStatusBarNotification(notification);

    assertThat(NotificationUtils.isMessaging(statusBarNotification)).isTrue();
  }

  @Test
  public void isMessaging_negative() {
    Notification notification =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setContentText("Hello")
            .build();
    StatusBarNotification statusBarNotification = createStatusBarNotification(notification);

    assertThat(NotificationUtils.isMessaging(statusBarNotification)).isFalse();
  }

  @Test
  public void hasInlineReply_positive() {
    Notification.Action archiveAction =
        new Notification.Action.Builder(
                Icon.createWithData(new byte[0], 0, 0),
                "archive",
                PendingIntent.getActivity(
                    ApplicationProvider.getApplicationContext(), 0, new Intent(), 0))
            .build();
    Notification.Action replyAction =
        new Notification.Action.Builder(
                Icon.createWithData(new byte[0], 0, 0),
                "reply",
                PendingIntent.getActivity(
                    ApplicationProvider.getApplicationContext(), 0, new Intent(), 0))
            .addRemoteInput(
                new RemoteInput.Builder("resultKey").setAllowFreeFormInput(true).build())
            .build();

    Notification notification =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setActions(archiveAction, replyAction)
            .build();
    StatusBarNotification statusBarNotification = createStatusBarNotification(notification);

    assertThat(NotificationUtils.hasInlineReply(statusBarNotification)).isTrue();
  }

  @Test
  public void hasInlineReply_negative() {
    Notification.Action archiveAction =
        new Notification.Action.Builder(
                Icon.createWithData(new byte[0], 0, 0),
                "archive",
                PendingIntent.getActivity(
                    ApplicationProvider.getApplicationContext(), 0, new Intent(), 0))
            .build();

    Notification notification =
        new Notification.Builder(ApplicationProvider.getApplicationContext(), "channel")
            .setActions(archiveAction)
            .build();
    StatusBarNotification statusBarNotification = createStatusBarNotification(notification);

    assertThat(NotificationUtils.hasInlineReply(statusBarNotification)).isFalse();
  }

  private static StatusBarNotification createStatusBarNotification(Notification notification) {
    return new StatusBarNotification(
        "pkg.name",
        "pkg.name",
        /* id= */ 2,
        "tag",
        /* uid= */ 1,
        /* initialPid= */ 1,
        /* score= */ 1,
        notification,
        Process.myUserHandle(),
        System.currentTimeMillis());
  }
}
