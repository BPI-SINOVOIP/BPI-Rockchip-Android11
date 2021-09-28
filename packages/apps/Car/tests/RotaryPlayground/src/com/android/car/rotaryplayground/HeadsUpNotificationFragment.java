/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.rotaryplayground;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

public class HeadsUpNotificationFragment extends Fragment {
    private static final String NOTIFICATION_CHANNEL_ID = "rotary_notification";
    private static final int NOTIFICATION_ID = 1;

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.rotary_notification, container, false);
        NotificationManager notificationManager =
                getContext().getSystemService(NotificationManager.class);
        notificationManager.createNotificationChannel(
                new NotificationChannel(NOTIFICATION_CHANNEL_ID, "Rotary Playground",
                        NotificationManager.IMPORTANCE_HIGH));
        view.findViewById(R.id.add_notification_button1).setOnClickListener(
                v -> notificationManager.notify(NOTIFICATION_ID, createNotification()));
        view.findViewById(R.id.clear_notification_button1).setOnClickListener(
                v -> notificationManager.cancel(NOTIFICATION_ID));
        view.findViewById(R.id.add_notification_button2).setOnClickListener(
                v -> notificationManager.notify(NOTIFICATION_ID, createNotification()));
        view.findViewById(R.id.clear_notification_button2).setOnClickListener(
                v -> notificationManager.cancel(NOTIFICATION_ID));

        return view;
    }

    /**
     * Creates a notification with CATEGORY_CALL in a channel with IMPORTANCE_HIGH. This will
     * produce a heads-up notification even for non-system apps that aren't privileged and aren't
     * signed with the platform key. The notification includes three actions which appear as buttons
     * in the HUN.
     */
    private Notification createNotification() {
        Intent intent = new Intent(getContext(), RotaryActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(getContext(), 0, intent, 0);
        return new Notification.Builder(getContext(), NOTIFICATION_CHANNEL_ID)
                .setContentTitle("Example heads-up notification")
                .setContentText("Try nudging up to HUN")
                .setSmallIcon(R.drawable.ic_launcher)
                .addAction(new Notification.Action.Builder(null, "Action1", pendingIntent).build())
                .addAction(new Notification.Action.Builder(null, "Action2", pendingIntent).build())
                .addAction(new Notification.Action.Builder(null, "Action3", pendingIntent).build())
                .setColor(getContext().getColor(android.R.color.holo_red_light))
                .setCategory(Notification.CATEGORY_CALL)
                .build();
    }
}
