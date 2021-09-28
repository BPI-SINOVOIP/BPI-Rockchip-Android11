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

package android.server.wm;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.TypeEvaluator;
import android.animation.ValueAnimator;
import android.graphics.Insets;
import android.view.WindowInsetsAnimationControlListener;
import android.view.WindowInsetsAnimationController;
import android.view.WindowInsetsController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class WindowInsetsAnimationUtils {

    private static final int DURATION = 1000;

    /** Interpolates {@link Insets} for use with {@link ValueAnimator}. */
    public static TypeEvaluator<Insets> INSETS_EVALUATOR =
            (fraction, startValue, endValue) -> Insets.of(
                    (int) (startValue.left + fraction * (endValue.left - startValue.left)),
                    (int) (startValue.top + fraction * (endValue.top - startValue.top)),
                    (int) (startValue.right + fraction * (endValue.right - startValue.right)),
                    (int) (startValue.bottom + fraction * (endValue.bottom - startValue.bottom)));


    /**
     * Requests control of the given {@code types} and animates them according to {@code show}.
     *
     * @param show if true, animate the inset in, otherwise animate it out.
     */
    public static void requestControlThenTransitionToVisibility(WindowInsetsController wic,
            int types, boolean show) {
        wic.controlWindowInsetsAnimation(types, -1, null,
                null, new WindowInsetsAnimationControlListener() {
                    Animator mAnimator;

                    @Override
                    public void onReady(@NonNull WindowInsetsAnimationController controller,
                            int types) {
                        mAnimator = runTransition(controller, show);
                    }

                    @Override
                    public void onFinished(
                            @NonNull WindowInsetsAnimationController controller) {
                    }

                    @Override
                    public void onCancelled(
                            @Nullable WindowInsetsAnimationController controller) {
                        if (mAnimator != null) {
                            mAnimator.cancel();
                        }
                    }
                });
    }

    private static ValueAnimator runTransition(WindowInsetsAnimationController controller,
            boolean show) {
        ValueAnimator animator = ValueAnimator.ofObject(
                INSETS_EVALUATOR,
                show ? controller.getHiddenStateInsets()
                        : controller.getShownStateInsets(),
                show ? controller.getShownStateInsets()
                        : controller.getHiddenStateInsets()
        );
        animator.setDuration(DURATION);
        animator.addUpdateListener((animator1) -> {
            if (!controller.isReady()) {
                // Lost control
                animator.cancel();
                return;
            }
            Insets insets = (Insets) animator.getAnimatedValue();
            controller.setInsetsAndAlpha(insets, 1.0f, animator.getAnimatedFraction());
        });
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (!controller.isCancelled()) {
                    controller.finish(show);
                }
            }
        });
        animator.start();
        return animator;
    }
}
