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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.content.Context;
import android.os.UserHandle;
import android.service.notification.StatusBarNotification;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
public class CarNotificationViewTest {

    private static final String PKG_1 = "package_1";
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

    private Context mContext;
    private CarNotificationView mCarNotificationView;
    @Mock
    private NotificationClickHandlerFactory mClickHandlerFactory;
    @Captor
    private ArgumentCaptor<List<NotificationGroup>> mDismissedNotificationsCaptor;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        FrameLayout frameLayout = new FrameLayout(mContext);
        LayoutInflater.from(mContext)
                .inflate(R.layout.test_car_notification_view_layout, frameLayout);
        mCarNotificationView = frameLayout.findViewById(R.id.notification_view);
        mCarNotificationView.setClickHandlerFactory(mClickHandlerFactory);
    }

    @Test
    public void onClickClearAllButton_callsFactoryClearNotificationsWithDismissibleNotifications() {
        Button clearAllButton = mCarNotificationView.findViewById(R.id.clear_all_button);
        NotificationGroup dismissible =
                getNotificationGroup(/* isDismissible= */ true);
        NotificationGroup notDismissible =
                getNotificationGroup(/* isDismissible= */ false);
        List<NotificationGroup> notifications = new ArrayList<>();
        notifications.add(dismissible);
        notifications.add(notDismissible);
        mCarNotificationView.setNotifications(notifications);

        clearAllButton.callOnClick();

        verify(mClickHandlerFactory).clearNotifications(mDismissedNotificationsCaptor.capture());
        assertThat(mDismissedNotificationsCaptor.getValue()).containsExactly(dismissible);
    }

    private NotificationGroup getNotificationGroup(boolean isDismissible) {

        Notification.Builder builder = new Notification.Builder(mContext,
                CHANNEL_ID)
                .setContentTitle(CONTENT_TITLE);
        AlertEntry notification = new AlertEntry(new StatusBarNotification(PKG_1, OP_PKG,
                ID, TAG, UID, INITIAL_PID, builder.build(), USER_HANDLE,
                OVERRIDE_GROUP_KEY, POST_TIME));

        if (!isDismissible) {
            notification.getNotification().flags =
                    notification.getNotification().flags | Notification.FLAG_ONGOING_EVENT;
            notification.getNotification().flags =
                    notification.getNotification().flags | Notification.FLAG_FOREGROUND_SERVICE;
        }

        NotificationGroup notificationGroup = new NotificationGroup();
        notificationGroup.addNotification(notification);

        return notificationGroup;
    }
}