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

package com.android.tv.settings.connectivity;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;

import com.android.settingslib.core.lifecycle.ObservableDialogFragment;
import com.android.tv.settings.R;

/**
 * The dialog shows an error message when requesting network {@link NetworkRequestDialogFragment}.
 * Contains multi-error types in {@code ERROR_DIALOG_TYPE}.
 */
public class NetworkRequestErrorDialogFragment extends ObservableDialogFragment {

    public static final String DIALOG_TYPE = "DIALOG_ERROR_TYPE";

    public enum ERROR_DIALOG_TYPE {TIME_OUT, ABORT}

    /** Creates Network Error dialog. */
    public static NetworkRequestErrorDialogFragment newInstance() {
        return new NetworkRequestErrorDialogFragment();
    }

    private NetworkRequestErrorDialogFragment() {
        super();
    }

    @Override
    public void onCancel(@NonNull DialogInterface dialog) {
        super.onCancel(dialog);
        // Wants to finish the activity when user clicks back key or outside of the dialog.
        getActivity().finish();
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        // Gets error type to construct dialog. Default is TIME_OUT dialog.
        ERROR_DIALOG_TYPE msgType = ERROR_DIALOG_TYPE.TIME_OUT;
        if (getArguments() != null) {
            msgType = (ERROR_DIALOG_TYPE) getArguments().getSerializable(DIALOG_TYPE);
        }

        final AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        if (msgType == ERROR_DIALOG_TYPE.TIME_OUT) {
            builder.setMessage(R.string.network_connection_timeout_dialog_message)
                    .setPositiveButton(R.string.network_connection_timeout_dialog_ok,
                            (dialog, which) -> startScanningDialog())
                    .setNegativeButton(R.string.cancel, (dialog, which) -> getActivity().finish());
        } else {
            builder.setMessage(R.string.network_connection_errorstate_dialog_message)
                    .setPositiveButton(R.string.okay, (dialog, which) -> getActivity().finish());
        }
        return builder.create();
    }

    protected void startScanningDialog() {
        final NetworkRequestDialogFragment fragment = NetworkRequestDialogFragment.newInstance();
        fragment.show(getActivity().getSupportFragmentManager(),
                NetworkRequestErrorDialogFragment.class.getSimpleName());
    }
}
