/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.car.settings.bluetooth;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import com.android.car.settings.R;
import com.android.car.ui.AlertDialogBuilder;
import com.android.car.ui.preference.CarUiDialogFragment;

import java.util.Objects;

/** Dialog fragment for renaming a Bluetooth device. */
public abstract class BluetoothRenameDialogFragment extends CarUiDialogFragment implements
        TextWatcher, TextView.OnEditorActionListener {

    // Keys to save the edited name and edit status for restoring after configuration change.
    private static final String KEY_NAME = "device_name";

    private static final int BLUETOOTH_NAME_MAX_LENGTH_BYTES = 248;

    private AlertDialog mAlertDialog;
    private String mDeviceName;

    /** Returns the title to use for the dialog. */
    @StringRes
    protected abstract int getDialogTitle();

    /** Returns the current name used for this device or {@code null} if a name is not available. */
    @Nullable
    protected abstract String getDeviceName();

    /**
     * Set the device to the given name.
     *
     * @param deviceName the name to use.
     */
    protected abstract void setDeviceName(String deviceName);

    @Override
    @NonNull
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        String deviceName = getDeviceName();
        if (savedInstanceState != null) {
            deviceName = savedInstanceState.getString(KEY_NAME, deviceName);
        }

        createAlertDialog(deviceName);

        return mAlertDialog;
    }

    private void createAlertDialog(String deviceName) {
        InputFilter[] inputFilters = new InputFilter[]{new Utf8ByteLengthFilter(
                BLUETOOTH_NAME_MAX_LENGTH_BYTES)};
        AlertDialogBuilder builder = new AlertDialogBuilder(requireActivity())
                .setTitle(getDialogTitle())
                .setPositiveButton(R.string.bluetooth_rename_button,
                        (dialog, which) -> setDeviceName(getEditedName()))
                .setNegativeButton(android.R.string.cancel, /* listener= */ null)
                .setEditBox(deviceName, /* textChangedListener= */ this, inputFilters,
                        InputType.TYPE_CLASS_TEXT);

        mAlertDialog = builder.create();

        mAlertDialog.setOnShowListener(d -> {

            EditText deviceNameView = getDeviceNameView();

            if (mDeviceName != null) {
                deviceNameView.setSelection(mDeviceName.length());
            }

            deviceNameView.setOnEditorActionListener(this);
            deviceNameView.setImeOptions(EditorInfo.IME_ACTION_DONE);

            if (deviceNameView.requestFocus()) {
                InputMethodManager imm = (InputMethodManager) requireContext().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(deviceNameView, InputMethodManager.SHOW_IMPLICIT);
                }
            }

            refreshRenameButton();
        });

    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        outState.putString(KEY_NAME, getEditedName());
    }


    @Override
    public void onStart() {
        super.onStart();
        refreshRenameButton();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mAlertDialog = null;
    }

    /** Refreshes the displayed device name with the latest value from {@link #getDeviceName()}. */
    protected void updateDeviceName() {
        String deviceName = getDeviceName();
        if (deviceName != null) {
            mDeviceName = deviceName;
            EditText deviceNameView = getDeviceNameView();
            if (deviceNameView != null) {
                deviceNameView.setText(deviceName);
            }
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        mDeviceName = s.toString();
    }

    @Override
    public void afterTextChanged(Editable s) {
        refreshRenameButton();
    }

    @Override
    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
        if (actionId == EditorInfo.IME_ACTION_DONE) {
            String editedName = getEditedName();
            if (TextUtils.isEmpty(editedName)) {
                return false;
            }
            setDeviceName(editedName);
            if (mAlertDialog != null && mAlertDialog.isShowing()) {
                mAlertDialog.dismiss();
            }
            return true;
        }
        return false;
    }

    private void refreshRenameButton() {
        if (mAlertDialog != null) {
            Button renameButton = mAlertDialog.getButton(DialogInterface.BUTTON_POSITIVE);
            if (renameButton != null) {
                String editedName = getEditedName();
                renameButton.setEnabled(
                        !TextUtils.isEmpty(editedName) && !Objects.equals(editedName,
                                getDeviceName()));
            }
        }
    }

    private String getEditedName() {
        return mDeviceName.trim();
    }

    private EditText getDeviceNameView() {
        return mAlertDialog.getWindow().findViewById(R.id.textbox);
    }
}
