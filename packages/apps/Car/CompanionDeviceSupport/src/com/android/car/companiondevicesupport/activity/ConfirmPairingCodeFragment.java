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
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import com.android.car.companiondevicesupport.R;

/** Fragment that displays the pairing code. */
public class ConfirmPairingCodeFragment extends Fragment {
    private static final String PAIRING_CODE_KEY = "pairingCodeKey";
    private static final String TAG = "ConfirmPairingCodeFragment";

    static ConfirmPairingCodeFragment newInstance(@NonNull String pairingCode) {
        Bundle bundle = new Bundle();
        bundle.putString(PAIRING_CODE_KEY, pairingCode);
        ConfirmPairingCodeFragment fragment = new ConfirmPairingCodeFragment();
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.confirm_pairing_code_fragment, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Bundle bundle = getArguments();
        String pairingCode = bundle.getString(PAIRING_CODE_KEY);
        TextView pairingCodeTextView = view.findViewById(R.id.pairing_code);
        setPairingCode(pairingCodeTextView, pairingCode);
    }

    private void setPairingCode(TextView textView, String pairingCode) {
        if (textView == null) {
            loge(TAG, "No valid TextView to show pairing code.");
            return;
        }
        textView.setText(pairingCode);
    }
}
