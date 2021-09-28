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

package com.android.car.settings.common.rotary;

import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.core.util.Preconditions;

/**
 * A {@link View.OnKeyListener} and {@link View.OnGenericMotionListener} that adds a
 * "Direct Manipulation" mode to any {@link View} that uses it.
 * <p>
 * Direct Manipulation mode in the Rotary context is a mode in which the user can use the
 * Rotary controls to manipulate and change the UI elements they are interacting with rather
 * than navigate through the entire UI.
 * <p>
 * Treats {@link KeyEvent#KEYCODE_DPAD_CENTER} as the signal to enter Direct Manipulation
 * mode, and {@link KeyEvent#KEYCODE_BACK} as the signal to exit, and keeps track of which
 * mode the {@link View} using it is currently in.
 * <p>
 * When in Direct Manipulation mode, it delegates to {@code mNudgeDelegate}
 * for handling nudge behavior, {@code mBackDelegate} for back behavior, and
 * {@code mRotationDelegate} for rotation behavior. Generally it is expected that in Direct
 * Manipulation mode, nudges are used for navigation and rotation is used for "manipulating" the
 * value of the selected {@link View}.
 * <p>
 * To reduce boilerplate, this class provides "no op" nudge and rotation behavior if
 * no {@link View.OnKeyListener} or {@link View.OnGenericMotionListener} are provided as
 * delegates for tackling the relevant events.
 * <p>
 * Allows {@link View}s that are within a {@link ViewGroup} to provide a link to their
 * ancestor {@link ViewGroup} from which Direct Manipulation mode was first enabled. That way
 * when the user finally exits Direct Manipulation mode, both objects are restored to their
 * original state.
 */
public class DirectManipulationHandler implements View.OnKeyListener,
        View.OnGenericMotionListener {
    private static final String TAG = DirectManipulationHandler.class.getSimpleName();

    /**
     * Sets the provided {@link DirectManipulationHandler} to the key listener and motion
     * listener of the provided view.
     */
    public static void setDirectManipulationHandler(@Nullable View view,
            @Nullable DirectManipulationHandler handler) {
        if (view == null) {
            return;
        }
        view.setOnKeyListener(handler);
        view.setOnGenericMotionListener(handler);
    }

    /** A builder for {@link DirectManipulationHandler}. */
    public static class Builder {
        private final DirectManipulationState mDmState;
        private View.OnKeyListener mNudgeDelegate;
        private EventListener mCenterButtonDelegate;
        private EventListener mBackDelegate;
        private View.OnGenericMotionListener mRotationDelegate;

        public Builder(DirectManipulationState dmState) {
            Preconditions.checkNotNull(dmState);
            mDmState = dmState;
        }

        /** Sets a nudge handler. */
        public Builder setNudgeHandler(View.OnKeyListener nudgeDelegate) {
            Preconditions.checkNotNull(nudgeDelegate);
            mNudgeDelegate = nudgeDelegate;
            return this;
        }

        /** Sets an enter handler. */
        public Builder setCenterButtonHandler(EventListener centerButtonDelegate) {
            Preconditions.checkNotNull(centerButtonDelegate);
            mCenterButtonDelegate = centerButtonDelegate;
            return this;
        }

        /** Sets a back handler. */
        public Builder setBackHandler(EventListener backDelegate) {
            Preconditions.checkNotNull(backDelegate);
            mBackDelegate = backDelegate;
            return this;
        }

        /** Sets a rotation handler. */
        public Builder setRotationHandler(View.OnGenericMotionListener rotationDelegate) {
            Preconditions.checkNotNull(rotationDelegate);
            mRotationDelegate = rotationDelegate;
            return this;
        }

        /** Constructs a {@link DirectManipulationHandler}. */
        public DirectManipulationHandler build() {
            if (mNudgeDelegate == null && mRotationDelegate == null) {
                throw new IllegalStateException("Nudge and/or rotation delegate must be provided.");
            }
            return new DirectManipulationHandler(mDmState, mNudgeDelegate, mCenterButtonDelegate,
                    mBackDelegate, mRotationDelegate);
        }
    }

    private final DirectManipulationState mDirectManipulationMode;
    private final View.OnKeyListener mNudgeDelegate;
    private final EventListener mCenterButtonDelegate;
    private final EventListener mBackDelegate;
    private final View.OnGenericMotionListener mRotationDelegate;

    private DirectManipulationHandler(DirectManipulationState dmState,
            @Nullable View.OnKeyListener nudgeDelegate,
            @Nullable EventListener centerButtonDelegate,
            @Nullable EventListener backDelegate,
            @Nullable View.OnGenericMotionListener rotationDelegate) {
        mDirectManipulationMode = dmState;
        mNudgeDelegate = nudgeDelegate;
        mCenterButtonDelegate = centerButtonDelegate;
        mBackDelegate = backDelegate;
        mRotationDelegate = rotationDelegate;
    }

    @Override
    public boolean onKey(View view, int keyCode, KeyEvent keyEvent) {
        boolean isActionUp = keyEvent.getAction() == KeyEvent.ACTION_UP;
        Log.d(TAG, "View: " + view + " is handling " + KeyEvent.keyCodeToString(keyCode)
                + " and action " + KeyEvent.actionToString(keyEvent.getAction())
                + " direct manipulation mode is "
                + (mDirectManipulationMode.isActive() ? "active" : "inactive"));

        boolean inDirectManipulationMode = mDirectManipulationMode.isActive();
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_CENTER:
                // If not yet in Direct Manipulation mode, switch to that mode. If in Direct
                // Manipulation mode, exit, and clean up state.
                if (isActionUp) {
                    if (inDirectManipulationMode) {
                        mDirectManipulationMode.disable();
                    } else {
                        mDirectManipulationMode.enable(view);
                    }
                }

                if (mCenterButtonDelegate == null) {
                    return true;
                }

                return mCenterButtonDelegate.onEvent(inDirectManipulationMode);
            case KeyEvent.KEYCODE_BACK:
                // If in Direct Manipulation mode, exit, and clean up state.
                if (inDirectManipulationMode && isActionUp) {
                    mDirectManipulationMode.disable();
                }
                // If no delegate is present, silently consume the events.
                if (mBackDelegate == null) {
                    return true;
                }

                return mBackDelegate.onEvent(inDirectManipulationMode);
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                // This handler is only responsible for nudging behavior during Direct Manipulation
                // mode. When the mode is disabled, ignore events.
                if (!inDirectManipulationMode) {
                    return false;
                }
                // If no delegate is present, silently consume the events.
                if (mNudgeDelegate == null) {
                    return true;
                }
                return mNudgeDelegate.onKey(view, keyCode, keyEvent);
            default:
                // Ignore all other key events.
                return false;
        }
    }

    @Override
    public boolean onGenericMotion(View v, MotionEvent event) {
        // This handler is only responsible for behavior during Direct Manipulation
        // mode. When the mode is disabled, ignore events.
        if (!mDirectManipulationMode.isActive()) {
            return false;
        }

        // If no delegate is present, silently consume the events.
        if (mRotationDelegate == null) {
            return true;
        }

        return mRotationDelegate.onGenericMotion(v, event);
    }

    /** A custom event listener. */
    public interface EventListener {
        /**
         * Handles an event.
         *
         * @param inDirectManipulationMode specifies whether we were in direct manipulation mode
         *                                 before the event is handled
         */
        boolean onEvent(boolean inDirectManipulationMode);
    }
}
