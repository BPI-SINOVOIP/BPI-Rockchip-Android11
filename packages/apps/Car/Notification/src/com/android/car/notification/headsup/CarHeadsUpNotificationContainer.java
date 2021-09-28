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

package com.android.car.notification.headsup;

import android.view.View;

/**
 * Container for displaying Heads Up Notifications.
 */
public interface CarHeadsUpNotificationContainer {
    /**
     * Displays a given notification View to the user.
     */
    void displayNotification(View notificationView);

    /**
     * Removes a given notification View from the container.
     */
    void removeNotification(View notificationView);

    /**
     * @return Whether or not the container is currently visible.
     */
    boolean isVisible();
}
