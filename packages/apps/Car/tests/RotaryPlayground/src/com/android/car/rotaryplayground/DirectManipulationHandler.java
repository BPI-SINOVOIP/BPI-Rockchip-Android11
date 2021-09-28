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
 * When in Direct Manipulation mode, it delegates to {@code mDirectionalDelegate}
 * for handling nudge behavior and {@code mMotionDelegate} for rotation behavior. Generally
 * it is expected that in Direct Manipulation mode, nudges are used for navigation and
 * rotation is used for "manipulating" the value of the selected {@link View}.
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

    /**
     * Sets the provided {@link DirectManipulationHandler} to the key listener and motion
     * listener of the provided view.
     */
    public static void setDirectManipulationHandler(@Nullable View view,
            DirectManipulationHandler handler) {
        if (view == null) {
            return;
        }
        view.setOnKeyListener(handler);
        view.setOnGenericMotionListener(handler);
    }

    /**
     * A builder for {@link DirectManipulationHandler}.
     */
    public static class Builder {
        private final DirectManipulationState mDmState;
        private View.OnKeyListener mNudgeDelegate;
        private View.OnGenericMotionListener mRotationDelegate;
        private View.OnKeyListener mBackDelegate;

        public Builder(DirectManipulationState dmState) {
            Preconditions.checkNotNull(dmState);
            this.mDmState = dmState;
        }

        public Builder setNudgeHandler(View.OnKeyListener nudgeDelegate) {
            Preconditions.checkNotNull(nudgeDelegate);
            this.mNudgeDelegate = nudgeDelegate;
            return this;
        }

        public Builder setBackHandler(View.OnKeyListener backDelegate) {
            Preconditions.checkNotNull(backDelegate);
            this.mBackDelegate = backDelegate;
            return this;
        }

        public Builder setRotationHandler(View.OnGenericMotionListener rotationDelegate) {
            Preconditions.checkNotNull(rotationDelegate);
            this.mRotationDelegate = rotationDelegate;
            return this;
        }

        public DirectManipulationHandler build() {
            if (mNudgeDelegate == null && mRotationDelegate == null) {
                throw new IllegalStateException("Nudge and/or rotation delegate must be provided.");
            }
            return new DirectManipulationHandler(mDmState, mNudgeDelegate, mBackDelegate,
                    mRotationDelegate);
        }
    }

    private final DirectManipulationState mDirectManipulationMode;
    private final View.OnKeyListener mNudgeDelegate;
    private final View.OnKeyListener mBackDelegate;
    private final View.OnGenericMotionListener mRotationDelegate;

    private DirectManipulationHandler(DirectManipulationState dmState,
            @Nullable View.OnKeyListener nudgeDelegate,
            @Nullable View.OnKeyListener backDelegate,
            @Nullable View.OnGenericMotionListener rotationDelegate) {
        mDirectManipulationMode = dmState;
        mNudgeDelegate = nudgeDelegate;
        mBackDelegate = backDelegate;
        mRotationDelegate = rotationDelegate;
    }

    @Override
    public boolean onKey(View view, int keyCode, KeyEvent keyEvent) {
        boolean isActionUp = keyEvent.getAction() == KeyEvent.ACTION_UP;
        Log.d(L.TAG, "View: " + view + " is handling " + KeyEvent.keyCodeToString(keyCode)
                + " and action " + keyEvent.getAction()
                + " direct manipulation mode is "
                + (mDirectManipulationMode.isActive() ? "active" : "inactive"));

        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_CENTER:
                // If not yet in Direct Manipulation mode, switch to that mode.

                if (!mDirectManipulationMode.isActive() && isActionUp) {
                    mDirectManipulationMode.enable(view);
                }
                return true;
            case KeyEvent.KEYCODE_BACK:
                // If in Direct Manipulation mode, exit, and clean up state.
                if (mDirectManipulationMode.isActive() && isActionUp) {
                    mDirectManipulationMode.disable();
                }
                // If no delegate is present, silently consume the events.
                if (mBackDelegate == null) {
                    return true;
                }

                return mBackDelegate.onKey(view, keyCode, keyEvent);
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                // This handler is only responsible for nudging behavior during Direct Manipulation
                // mode. When the mode is disabled, ignore events.
                if (!mDirectManipulationMode.isActive()) {
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
}
