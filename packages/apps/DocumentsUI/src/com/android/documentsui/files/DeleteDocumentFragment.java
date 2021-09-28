/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.documentsui.files;

import static com.android.documentsui.base.SharedMinimal.TAG;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import com.android.documentsui.Injector;
import com.android.documentsui.R;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.Shared;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.ArrayList;
import java.util.List;

/**
 * Dialog to delete file or directory.
 */
public class DeleteDocumentFragment extends DialogFragment {
    private static final String TAG_DELETE_DOCUMENT = "delete_document";

    private List<DocumentInfo> mDocuments;
    private DocumentInfo mSrcParent;

    /**
     * Show the dialog UI.
     *
     * @param fm the fragment manager
     * @param docs the selected documents
     * @param srcParent the parent document of the selection
     */
    public static void show(FragmentManager fm, List<DocumentInfo> docs, DocumentInfo srcParent) {
        if (fm.isStateSaved()) {
            Log.w(TAG, "Skip show delete dialog because state saved");
            return;
        }

        final DeleteDocumentFragment dialog = new DeleteDocumentFragment();
        dialog.mDocuments = docs;
        dialog.mSrcParent = srcParent;
        dialog.show(fm, TAG_DELETE_DOCUMENT);
    }

    /**
     * Creates the dialog UI.
     *
     * @param savedInstanceState
     * @return
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        if (savedInstanceState != null) {
            mSrcParent = savedInstanceState.getParcelable(Shared.EXTRA_DOC);
            mDocuments = savedInstanceState.getParcelableArrayList(Shared.EXTRA_SELECTION);
        }

        Context context = getActivity();
        Injector<?> injector = ((FilesActivity) getActivity()).getInjector();
        LayoutInflater dialogInflater = LayoutInflater.from(context);
        TextView message = (TextView) dialogInflater.inflate(
                R.layout.dialog_delete_confirmation, null, false);
        message.setText(injector.messages.generateDeleteMessage(mDocuments));

        final AlertDialog alertDialog = new MaterialAlertDialogBuilder(context)
                .setView(message)
                .setPositiveButton(
                        android.R.string.ok,
                        (dialog, id) ->
                            injector.actions.deleteSelectedDocuments(mDocuments, mSrcParent))
                .setNegativeButton(android.R.string.cancel, null)
                .create();

        alertDialog.setOnShowListener(
                (dialogInterface) -> {
                    Button positive = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
                    positive.setFocusable(true);
                    positive.requestFocus();
                });
        return alertDialog;
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putParcelable(Shared.EXTRA_DOC, mSrcParent);
        outState.putParcelableArrayList(Shared.EXTRA_SELECTION, (ArrayList) mDocuments);
    }
}
