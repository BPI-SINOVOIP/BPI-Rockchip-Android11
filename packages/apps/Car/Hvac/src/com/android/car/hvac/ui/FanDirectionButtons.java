/*
 * Copyright (c) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.car.hvac.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.IntDef;

import com.android.car.hvac.R;
import java.util.ArrayList;

/**
 * A set of buttons that controls the fan direction of the vehicle. Turning on one state will
 * turn off the other states.
 */
public class FanDirectionButtons extends LinearLayout {
    public static final int FAN_DIRECTION_FACE = 0;
    public static final int FAN_DIRECTION_FACE_FLOOR = 1;
    public static final int FAN_DIRECTION_FLOOR = 2;
    public static final int FAN_DIRECTION_FLOOR_DEFROSTER = 3;
    public static final int FAN_DIRECTION_COUNT = 4;

    @IntDef({FAN_DIRECTION_FACE, FAN_DIRECTION_FACE_FLOOR,
            FAN_DIRECTION_FLOOR, FAN_DIRECTION_FLOOR_DEFROSTER})
    public @interface FanDirection {}

    /**
     * A resource id array for all fan direction buttons.
     */
    private static final int[] FAN_DIRECTION_BUTTON_IDS = { R.id.direction_1, R.id.direction_2,
                                  R.id.direction_3, R.id.direction_4 };

    /**
     * A listener that is notified when a fan direction button has been clicked.
     */
    public interface FanDirectionClickListener {
        void onFanDirectionClicked(@FanDirection int direction);
    }

    private final ArrayList<ImageView> mButtonArray = new ArrayList<ImageView>();

    private int mCurrentDirection = -1;
    private FanDirectionClickListener mListener;

    public FanDirectionButtons(Context context) {
        super(context);
        init();
    }

    public FanDirectionButtons(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public FanDirectionButtons(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public void setFanDirectionClickListener(FanDirectionClickListener listener) {
        mListener = listener;
    }

    public void setFanDirection(@FanDirection int direction) {
        if (direction != mCurrentDirection) {
            updateFanButtonToOn(direction);
        }
    }

    private void init() {
        inflate(getContext(), R.layout.fan_direction, this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        setOrientation(HORIZONTAL);

        for (int i = 0; i < FAN_DIRECTION_COUNT; i++) {
            ImageView button = (ImageView) findViewById(FAN_DIRECTION_BUTTON_IDS[i]);
            button.setTag(i);
            button.setOnClickListener(v -> {
                int direction = (int) v.getTag();
                if (direction != mCurrentDirection) {
                    updateFanButtonToOn(direction);
                    if (mListener != null) {
                        mListener.onFanDirectionClicked(direction);
                    }
                }
            });

            mButtonArray.add(button);
        }
    }

    private void updateFanButtonToOn(int directionToOn) {
        if (mCurrentDirection != -1) {
            mButtonArray.get(mCurrentDirection).setSelected(false);
        }
        mButtonArray.get(directionToOn).setSelected(true);
        mCurrentDirection = directionToOn;
    }
}
