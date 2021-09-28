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

import static java.lang.Math.min;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * A {@link View} used to demonstrate direct manipulation mode.
 * <p>
 * This view draws nothing but a circle. It provides APIs to change the center and the radius of the
 * circle.
 */
public class DirectManipulationView extends View {

    /**
     * How many pixels do we want to move the center of the circle horizontally from its initial
     * position.
     */
    private float mDeltaX;
    /**
     * How many pixels do we want to move the center of the circle vertically from its initial
     * position.
     */
    private float mDeltaY;
    /** How many pixels do we want change the radius of the circle from its initial radius. */
    private float mDeltaRadius;

    private Paint mPaint;

    public DirectManipulationView(Context context) {
        super(context);
        init();
    }

    public DirectManipulationView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public DirectManipulationView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public DirectManipulationView(Context context, @Nullable AttributeSet attrs, int defStyleAttr,
            int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        // Draw the circle. Initially the circle is in the center of the canvas, and its radius is
        // min(getWidth(), getHeight()) / 4. We need to translate it and scale it.
        canvas.drawCircle(
                /* cx= */getWidth() / 2 + mDeltaX,
                /* cy= */getHeight() / 2 + mDeltaY,
                /* radius= */min(getWidth(), getHeight()) / 4 + mDeltaRadius,
                mPaint);

    }

    /**
     * Moves the center of the circle by {@code dx} horizontally and by {@code dy} vertically, then
     * redraws it.
     */
    void move(float dx, float dy) {
        mDeltaX += dx;
        mDeltaY += dy;
        invalidate();
    }

    /** Changes the radius of the circle by {@code dr} then redraws it. */
    void resizeCircle(float dr) {
        mDeltaRadius += dr;
        invalidate();
    }

    private void init() {
        // The view must be focusable to enter direct manipulation mode.
        setFocusable(View.FOCUSABLE);

        // Set up paint with color and stroke styles.
        mPaint = new Paint();
        mPaint.setColor(Color.GREEN);
        mPaint.setAntiAlias(true);
        mPaint.setStrokeWidth(5);
        mPaint.setStyle(Paint.Style.FILL_AND_STROKE);
        mPaint.setStrokeJoin(Paint.Join.ROUND);
        mPaint.setStrokeCap(Paint.Cap.ROUND);
    }

    /**
     * A {@link View.OnKeyListener} for handling Direct Manipulation rotary nudge behavior
     * for a {@link DirectManipulationView}.
     * <p>
     * This handler expects that it is being used in Direct Manipulation mode, i.e. as a directional
     * delegate through a {@link DirectManipulationHandler} which can invoke it at the
     * appropriate times.
     * <p>
     * Moves the circle drawn in the {@link DirectManipulationView} in the relevant direction for
     * following {@link KeyEvent}s:
     * <ul>
     *     <li>{@link KeyEvent#KEYCODE_DPAD_UP}
     *     <li>{@link KeyEvent#KEYCODE_DPAD_DOWN}
     *     <li>{@link KeyEvent#KEYCODE_DPAD_LEFT}
     *     <li>{@link KeyEvent#KEYCODE_DPAD_RIGHT}
     * </ul>
     */
    static class NudgeHandler implements View.OnKeyListener {

        /** How many pixels do we want to move the {@link DirectManipulationView} per nudge. */
        private static final float DIRECT_MANIPULATION_VIEW_PX_PER_NUDGE = 10f;

        @Override
        public boolean onKey(View v, int keyCode, KeyEvent keyEvent) {
            if (keyEvent.getAction() != KeyEvent.ACTION_UP) {
                return true;
            }

            if (v instanceof DirectManipulationView) {
                DirectManipulationView dmv = (DirectManipulationView) v;
                handleNudgeEvent(dmv, keyCode);
                return true;
            }

            throw new UnsupportedOperationException("NudgeHandler shouldn't be registered "
                    + "as a listener on a view other than a DirectManipulationView.");
        }

        /** Moves the circle of the DirectManipulationView when the controller nudges. */
        private void handleNudgeEvent(@NonNull DirectManipulationView dmv, int keyCode) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_DPAD_UP:
                    dmv.move(0f, -DIRECT_MANIPULATION_VIEW_PX_PER_NUDGE);
                    return;
                case KeyEvent.KEYCODE_DPAD_DOWN:
                    dmv.move(0f, DIRECT_MANIPULATION_VIEW_PX_PER_NUDGE);
                    return;
                case KeyEvent.KEYCODE_DPAD_LEFT:
                    dmv.move(-DIRECT_MANIPULATION_VIEW_PX_PER_NUDGE, 0f);
                    return;
                case KeyEvent.KEYCODE_DPAD_RIGHT:
                    dmv.move(DIRECT_MANIPULATION_VIEW_PX_PER_NUDGE, 0f);
                    return;
                default:
                    throw new IllegalArgumentException("Invalid keycode: " + keyCode);
            }
        }
    }

    /**
     * A {@link View.OnGenericMotionListener} for handling Direct Manipulation rotation events for
     * a {@link DirectManipulationView}. It does so by increasing or decreasing the radius of
     * the circle drawn depending on the direction of rotation.
     */
    static class RotationHandler implements View.OnGenericMotionListener {

        /**
         * How many pixels do we want to change the radius of the circle in the
         * {@link DirectManipulationView} for a rotation.
         */
        private static final float DIRECT_MANIPULATION_VIEW_PX_PER_ROTATION = 10f;

        @Override
        public boolean onGenericMotion(View v, MotionEvent event) {
            if (v instanceof DirectManipulationView) {
                handleRotateEvent(
                        (DirectManipulationView) v,
                        event.getAxisValue(MotionEvent.AXIS_SCROLL));
                return true;
            }

            throw new UnsupportedOperationException("RotationHandler shouldn't be registered "
                    + "as a listener on a view other than a DirectManipulationView.");
        }

        /** Resizes the circle of the DirectManipulationView when the controller rotates. */
        private void handleRotateEvent(@NonNull DirectManipulationView dmv, float scroll) {
            dmv.resizeCircle(DIRECT_MANIPULATION_VIEW_PX_PER_ROTATION * scroll);
        }
    }
}
