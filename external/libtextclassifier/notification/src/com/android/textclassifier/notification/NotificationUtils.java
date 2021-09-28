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

import android.app.Notification;
import android.app.Notification.Style;
import android.app.RemoteInput;
import android.service.notification.StatusBarNotification;
import java.util.Objects;

final class NotificationUtils {

  /**
   * Returns whether the given status bar notification is showing some incoming messages.
   *
   * @see Notification#CATEGORY_MESSAGE
   * @see Notification.MessagingStyle
   */
  static boolean isMessaging(StatusBarNotification statusBarNotification) {
    return isCategory(statusBarNotification, Notification.CATEGORY_MESSAGE)
        || isPublicVersionCategory(statusBarNotification, Notification.CATEGORY_MESSAGE)
        || hasStyle(statusBarNotification, Notification.MessagingStyle.class);
  }

  /**
   * Returns whether the given status bar notification has an reply button that allows user to do
   * inline reply.
   */
  static boolean hasInlineReply(StatusBarNotification statusBarNotification) {
    Notification.Action[] actions = statusBarNotification.getNotification().actions;
    if (actions == null) {
      return false;
    }
    for (Notification.Action action : actions) {
      RemoteInput[] remoteInputs = action.getRemoteInputs();
      if (remoteInputs == null) {
        continue;
      }
      for (RemoteInput remoteInput : remoteInputs) {
        if (remoteInput.getAllowFreeFormInput()) {
          return true;
        }
      }
    }
    return false;
  }

  private static boolean hasStyle(
      StatusBarNotification statusBarNotification, Class<? extends Style> targetStyle) {
    String templateClass =
        statusBarNotification.getNotification().extras.getString(Notification.EXTRA_TEMPLATE);
    return targetStyle.getName().equals(templateClass);
  }

  private static boolean isCategory(StatusBarNotification statusBarNotification, String category) {
    return Objects.equals(statusBarNotification.getNotification().category, category);
  }

  private static boolean isPublicVersionCategory(
      StatusBarNotification statusBarNotification, String category) {
    Notification publicVersion = statusBarNotification.getNotification().publicVersion;
    return publicVersion != null && isCategoryInternal(publicVersion, category);
  }

  private static boolean isCategoryInternal(Notification notification, String category) {
    return Objects.equals(notification.category, category);
  }

  private NotificationUtils() {}
}
