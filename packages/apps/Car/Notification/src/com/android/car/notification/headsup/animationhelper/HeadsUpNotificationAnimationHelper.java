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

package com.android.car.notification.headsup.animationhelper;

import android.animation.AnimatorSet;
import android.content.Context;
import android.view.View;

/** Interface defining how the notification view is animated in and out of the screen. */
public interface HeadsUpNotificationAnimationHelper {

    /** Returns the animator that animates the notification appearing on screen. */
    AnimatorSet getAnimateInAnimator(Context context, View view);

    /** Returns the animator that animates the notification leaving the screen. */
    AnimatorSet getAnimateOutAnimator(Context context, View view);

    /** Resets the notification to the starting position before being animated onto the screen. */
    void resetHUNPosition(View view);
}
