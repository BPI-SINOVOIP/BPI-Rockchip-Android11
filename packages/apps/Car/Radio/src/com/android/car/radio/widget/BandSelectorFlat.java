/**
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.radio.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.radio.R;
import com.android.car.radio.bands.ProgramType;

import java.util.ArrayList;
import java.util.List;

/**
 * A band selector that shows a flat list of band buttons.
 */
public class BandSelectorFlat extends LinearLayout {
    private final Object mLock = new Object();
    @Nullable private Callback mCallback;

    private final List<Button> mButtons = new ArrayList<>();

    /**
     * Widget's onClick event translated to band callback.
     */
    public interface Callback {
        /**
         * Called when user uses this button to switch the band.
         *
         * @param pt ProgramType to switch to
         */
        void onSwitchTo(@NonNull ProgramType pt);
    }

    public BandSelectorFlat(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public BandSelectorFlat(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    private void switchTo(@NonNull ProgramType ptype) {
        synchronized (mLock) {
            if (mCallback != null) mCallback.onSwitchTo(ptype);
        }
    }

    /**
     * Sets band selection callback.
     */
    public void setCallback(@Nullable Callback callback) {
        synchronized (mLock) {
            mCallback = callback;
        }
    }

    /**
     * Updates the list of supported program types.
     */
    public void setSupportedProgramTypes(@NonNull List<ProgramType> supported) {
        synchronized (mLock) {
            final LayoutInflater inflater = LayoutInflater.from(getContext());

            mButtons.clear();
            removeAllViews();
            for (ProgramType pt : supported) {
                Button btn = (Button) inflater.inflate(R.layout.band_selector_flat_button, null);
                btn.setText(pt.getLocalizedName());
                btn.setTag(pt);
                btn.setOnClickListener(v -> switchTo(pt));
                addView(btn);
                mButtons.add(btn);
            }
        }
    }

    /**
     * Updates the selected button based on the given program type.
     *
     * @param ptype The program type that needs to be selected.
     */
    public void setType(@NonNull ProgramType ptype) {
        synchronized (mLock) {
            Context ctx = getContext();
            for (Button btn : mButtons) {
                boolean active = btn.getTag() == ptype;
                btn.setTextColor(ctx.getColor(active
                        ? R.color.band_selector_flat_text_color_selected
                        : R.color.band_selector_flat_text_color));
                btn.setBackground(ctx.getDrawable(active
                        ? R.drawable.manual_tuner_button_background
                        : R.drawable.radio_control_background));
            }
        }
    }
}
