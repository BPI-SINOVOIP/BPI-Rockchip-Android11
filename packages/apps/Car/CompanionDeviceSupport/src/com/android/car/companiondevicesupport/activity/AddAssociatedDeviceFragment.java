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

package com.android.car.companiondevicesupport.activity;

import static com.android.car.connecteddevice.util.SafeLog.loge;

import android.annotation.NonNull;
import android.graphics.Typeface;
import android.os.Bundle;
import android.text.Html;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import com.android.car.companiondevicesupport.R;

/** Fragment that notifies the user to select the car. */
public class AddAssociatedDeviceFragment extends Fragment {
    private static final String DEVICE_NAME_KEY = "deviceName";
    private static final String TAG = "AddAssociatedDeviceFragment";

    static AddAssociatedDeviceFragment newInstance(@NonNull String deviceName) {
        Bundle bundle = new Bundle();
        bundle.putString(DEVICE_NAME_KEY, deviceName);
        AddAssociatedDeviceFragment fragment = new AddAssociatedDeviceFragment();
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.add_associated_device_fragment, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Bundle bundle = getArguments();
        String deviceName = bundle.getString(DEVICE_NAME_KEY);
        TextView selectTextView = view.findViewById(R.id.associated_device_select_device);
        setDeviceNameForAssociation(selectTextView, deviceName);
    }

    private void setDeviceNameForAssociation(TextView textView, String deviceName) {
        if (textView == null) {
            loge(TAG, "No valid TextView to show device name.");
            return;
        }
        String selectText = getString(R.string.associated_device_select_device, deviceName);
        Spanned styledSelectText = Html.fromHtml(selectText, Html.FROM_HTML_MODE_LEGACY);
        textView.setText(styledSelectText);
    }
}
