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
import android.animation.ObjectAnimator;
import android.content.Context;
import android.view.View;
import android.view.animation.AnimationUtils;
import android.view.animation.Interpolator;

import com.android.car.notification.R;

/** Sample AnimationHelper that animates the notification fading in from the right of the screen. */
public class CarHeadsUpNotificationRightAnimationHelper implements
        HeadsUpNotificationAnimationHelper {

    @Override
    public AnimatorSet getAnimateInAnimator(Context context, View view) {
        Interpolator interpolator = AnimationUtils.loadInterpolator(
                context,
                R.interpolator.heads_up_entry_direction_interpolator);
        Interpolator alphaInterpolator = AnimationUtils.loadInterpolator(
                context,
                R.interpolator.heads_up_entry_alpha_interpolator);

        ObjectAnimator move = ObjectAnimator.ofFloat(view, "x", 0f);
        move.setDuration(
                context.getResources().getInteger(R.integer.headsup_total_enter_duration_ms));
        move.setInterpolator(interpolator);

        ObjectAnimator alpha = ObjectAnimator.ofFloat(view, "alpha", 1f);
        alpha.setDuration(
                context.getResources().getInteger(R.integer.headsup_alpha_enter_duration_ms));
        alpha.setInterpolator(alphaInterpolator);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(move, alpha);
        return animatorSet;
    }

    @Override
    public AnimatorSet getAnimateOutAnimator(Context context, View view) {
        Interpolator interpolator = AnimationUtils.loadInterpolator(
                context,
                R.interpolator.heads_up_exit_direction_interpolator);
        Interpolator alphaInterpolator = AnimationUtils.loadInterpolator(
                context,
                R.interpolator.heads_up_exit_alpha_interpolator);

        ObjectAnimator move = ObjectAnimator.ofFloat(view, "x", getHUNInitialPosition(view));
        move.setDuration(context.getResources().getInteger(R.integer.headsup_exit_duration_ms));
        move.setInterpolator(interpolator);

        ObjectAnimator alpha = ObjectAnimator.ofFloat(view, "alpha", 0f);
        alpha.setDuration(context.getResources().getInteger(R.integer.headsup_exit_duration_ms));
        alpha.setInterpolator(alphaInterpolator);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(move, alpha);
        return animatorSet;
    }

    @Override
    public void resetHUNPosition(View view) {
        view.setX(getHUNInitialPosition(view));
        view.setAlpha(0);
    }

    private float getHUNInitialPosition(View view) {
        return view.getWidth();
    }
}
