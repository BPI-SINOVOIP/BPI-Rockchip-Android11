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

import android.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        NotificationViewType.GROUP_COLLAPSED,
        NotificationViewType.GROUP_EXPANDED,
        NotificationViewType.GROUP_SUMMARY,
        NotificationViewType.BASIC,
        NotificationViewType.BASIC_IN_GROUP,
        NotificationViewType.MESSAGE,
        NotificationViewType.MESSAGE_IN_GROUP,
        NotificationViewType.PROGRESS,
        NotificationViewType.PROGRESS_IN_GROUP,
        NotificationViewType.INBOX,
        NotificationViewType.INBOX_IN_GROUP,
        NotificationViewType.CAR_EMERGENCY,
        NotificationViewType.CAR_WARNING,
        NotificationViewType.CAR_INFORMATION,
        NotificationViewType.CAR_INFORMATION_IN_GROUP,
        NotificationViewType.NAVIGATION,
        NotificationViewType.CALL,
        NotificationViewType.HEADER,
        NotificationViewType.FOOTER,
})
@Retention(RetentionPolicy.SOURCE)
@interface NotificationViewType {

    int GROUP_COLLAPSED = 1;
    int GROUP_EXPANDED = 2;
    int GROUP_SUMMARY = 3;

    int BASIC = 4;
    int BASIC_IN_GROUP = 5;

    int MESSAGE = 6;
    int MESSAGE_IN_GROUP = 7;

    int PROGRESS = 8;
    int PROGRESS_IN_GROUP = 9;

    int INBOX = 10;
    int INBOX_IN_GROUP = 11;

    int CAR_EMERGENCY = 12;

    int CAR_WARNING = 13;

    int CAR_INFORMATION = 14;
    int CAR_INFORMATION_IN_GROUP = 15;

    int NAVIGATION = 16;
    int CALL = 17;

    int HEADER = 18;
    int FOOTER = 19;
}