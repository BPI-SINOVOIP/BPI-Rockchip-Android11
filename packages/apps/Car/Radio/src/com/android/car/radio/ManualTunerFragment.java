/*
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

package com.android.car.radio;

import static com.android.car.ui.core.CarUi.requireInsets;

import android.hardware.radio.RadioManager.ProgramInfo;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import com.android.car.radio.bands.ProgramType;
import com.android.car.radio.util.Log;
import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;

/**
 * Fragment that allows tuning to a specific frequency using a keypad
 */
public class ManualTunerFragment extends Fragment implements InsetsChangedListener {
    private static final String TAG = "BcRadioApp.TunFrg";

    private ManualTunerController mController;
    private RadioController mRadioController;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.tuner_fragment, container, false);
        mController = new ManualTunerController(getContext(), view,
                mRadioController.getRegionConfig(), mRadioController::tune);

        return view;
    }

    @Override
    public void setUserVisibleHint(boolean isVisibleToUser) {
        super.setUserVisibleHint(isVisibleToUser);

        if (!isVisibleToUser) return;

        ProgramInfo current = mRadioController.getCurrentProgram().getValue();
        if (current != null) {
            mController.switchProgramType(ProgramType.fromSelector(current.getSelector()));
        }

        try {
            mRadioController.setSkipMode(SkipMode.TUNE);
        } catch (IllegalStateException e) {
            Log.e(TAG, "Can't set skip mode", e);
        }
    }

    @Override
    public void onCarUiInsetsChanged(Insets insets) {
        View view = requireView();
        view.setPadding(insets.getLeft(), insets.getTop(),
                insets.getRight(), insets.getBottom());
    }

    @Override
    public void onStart() {
        super.onStart();

        // This is needed to apply the inset changes that happened before this fragment was visible
        onCarUiInsetsChanged(requireInsets(getActivity()));
    }

    static ManualTunerFragment newInstance(RadioController radioController) {
        ManualTunerFragment fragment = new ManualTunerFragment();
        fragment.mRadioController = radioController;
        return fragment;
    }
}
