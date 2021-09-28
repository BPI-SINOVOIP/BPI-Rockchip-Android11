/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.picker;

import static com.android.wallpaper.model.WallpaperRotationInitializer.NETWORK_PREFERENCE_CELLULAR_OK;
import static com.android.wallpaper.model.WallpaperRotationInitializer.NETWORK_PREFERENCE_WIFI_ONLY;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;

import com.android.wallpaper.R;
import com.android.wallpaper.module.InjectorProvider;

/**
 * Dialog which allows user to start a wallpaper rotation or cancel, as well as providing an option
 * whether to rotate wallpapers on wifi-only connections or not.
 */
public class StartRotationDialogFragment extends DialogFragment {
    private static final String KEY_IS_WIFI_ONLY_CHECKED = "key_is_wifi_only_checked";
    private static final boolean DEFAULT_IS_WIFI_ONLY = true;

    private boolean mIsWifiOnlyChecked;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState == null) {
            mIsWifiOnlyChecked = DEFAULT_IS_WIFI_ONLY;
        } else {
            mIsWifiOnlyChecked = savedInstanceState.getBoolean(KEY_IS_WIFI_ONLY_CHECKED);
        }
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View dialogView = inflater.inflate(R.layout.dialog_start_rotation, null);

        TextView bodyText = dialogView.findViewById(R.id.start_rotation_dialog_subhead);
        bodyText.setText(Html.fromHtml(getResources().getString(getBodyTextResourceId())));

        final CheckBox wifiOnlyCheckBox = dialogView.findViewById(
                R.id.start_rotation_wifi_only_checkbox);

        boolean hasTelephony = InjectorProvider.getInjector().getSystemFeatureChecker()
                .hasTelephony(getContext());

        // Only show the "WiFi only" checkbox if the device supports a cellular data connection.
        if (hasTelephony) {
            wifiOnlyCheckBox.setChecked(mIsWifiOnlyChecked);
            wifiOnlyCheckBox.setOnClickListener(
                    v -> mIsWifiOnlyChecked = wifiOnlyCheckBox.isChecked());
        } else {
            wifiOnlyCheckBox.setVisibility(View.GONE);
        }

        return new AlertDialog.Builder(getActivity(), R.style.LightDialogTheme)
                .setView(dialogView)
                .setNegativeButton(android.R.string.cancel, null /* listener */)
                .setPositiveButton(getPositiveButtonTextResourceId(),
                        (dialog, which) -> {
                            RotationStarter callback = (RotationStarter) getTargetFragment();
                            callback.startRotation(
                                    mIsWifiOnlyChecked
                                        ? NETWORK_PREFERENCE_WIFI_ONLY
                                        : NETWORK_PREFERENCE_CELLULAR_OK);
                        })
                .create();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(KEY_IS_WIFI_ONLY_CHECKED, mIsWifiOnlyChecked);
    }

    @Override
    public void onDismiss(@NonNull DialogInterface dialog) {
        super.onDismiss(dialog);
        ((Listener) getTargetFragment()).onStartRotationDialogDismiss(dialog);
    }

    private int getBodyTextResourceId() {
        return R.string.start_rotation_dialog_body;
    }

    private int getPositiveButtonTextResourceId() {
        return android.R.string.ok;
    }

    /** An interface for receiving this DialogFragment dismissed event. */
    public interface Listener {
        /** Gets called when the DialogFragment is dismissed. */
        void onStartRotationDialogDismiss(@NonNull DialogInterface dialog);
    }
}
