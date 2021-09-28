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

package com.android.car.dialer.ui.warning;

import android.app.Dialog;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProvider;

import com.android.car.dialer.R;

/**
 * A fullscreen {@link DialogFragment} that hosts the {@link NoHfpFragment} as an overlay of dialer
 * activities.
 */
public class OverlayFragment extends DialogFragment {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setStyle(DialogFragment.STYLE_NO_FRAME, R.style.Theme_Dialer);

        LiveData<Boolean> hasHfpDeviceConnectedLiveData = new ViewModelProvider(getActivity())
                .get(NoHfpViewModel.class)
                .hasHfpDeviceConnected();
        hasHfpDeviceConnectedLiveData.observe(this, hasHfpDeviceConnected -> {
            if (Boolean.TRUE.equals(hasHfpDeviceConnected)) {
                dismiss();
            }
        });
    }

    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        return new Dialog(this.requireContext(), this.getTheme()) {
            @Override
            public void onBackPressed() {
                if (getChildFragmentManager().popBackStackImmediate()) {
                    return;
                }
                // Exit dialer when press back button
                getActivity().finishAffinity();
            }
        };
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        getChildFragmentManager()
                .beginTransaction()
                .replace(android.R.id.content, new NoHfpFragment())
                .commit();
        return null;
    }
}
