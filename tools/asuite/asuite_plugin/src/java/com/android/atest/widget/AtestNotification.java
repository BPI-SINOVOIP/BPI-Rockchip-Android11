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
package com.android.atest.widget;

import com.android.atest.Constants;
import com.intellij.notification.Notification;
import com.intellij.notification.NotificationType;
import org.jetbrains.annotations.NotNull;

/** Atest notification. */
public class AtestNotification extends Notification {

    public static final String ATEST_GROUP_ID = Constants.ATEST_NAME;
    public static final String ATEST_TITLE = "Atest plugin";

    /**
     * Atest error notification constructor.
     *
     * @param content notification content.
     */
    public AtestNotification(@NotNull String content) {
        super(ATEST_GROUP_ID, ATEST_TITLE, content, NotificationType.ERROR);
    }
}
