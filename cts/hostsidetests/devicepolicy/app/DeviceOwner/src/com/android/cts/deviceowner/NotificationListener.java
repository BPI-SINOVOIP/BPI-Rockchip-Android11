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

package com.android.cts.deviceowner;

import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/**
 * A {@link NotificationListenerService} that allows tests to register to respond to notifications.
 */
public class NotificationListener extends NotificationListenerService {

    private static final int TIMEOUT_SECONDS = 120;

    private static NotificationListener instance;
    private static CountDownLatch connectedLatch = new CountDownLatch(1);

    public static NotificationListener getInstance() {
        try {
            connectedLatch.await(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException("NotificationListener not connected.", e);
        }

        return instance;
    }

    private List<Consumer<StatusBarNotification>> mListeners = new ArrayList<>();

    public void addListener(Consumer<StatusBarNotification> listener) {
        mListeners.add(listener);
    }

    public void clearListeners() {
        mListeners.clear();
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        for (Consumer<StatusBarNotification> listener : mListeners) {
            listener.accept(sbn);
        }
    }

    @Override
    public void onListenerConnected() {
        instance = this;
        connectedLatch.countDown();
    }
}
