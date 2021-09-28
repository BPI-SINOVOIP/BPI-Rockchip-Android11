/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.documentsui.ui;

import android.app.Activity;

import androidx.fragment.app.FragmentManager;

import com.android.documentsui.R;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.Features;
import com.android.documentsui.picker.ConfirmFragment;
import com.android.documentsui.services.FileOperation;
import com.android.documentsui.services.FileOperationService;
import com.android.documentsui.services.FileOperationService.OpType;
import com.android.documentsui.services.FileOperations;
import com.android.documentsui.services.FileOperations.Callback.Status;

import com.google.android.material.snackbar.Snackbar;

public interface DialogController {

    // Dialogs used in FilesActivity
    void showFileOperationStatus(int status, int opType, int docCount);

    /**
     * There can be only one progress dialog active at the time. Each call to this
     * method will discard any previously created progress dialogs.
     */
    void showProgressDialog(String jobId, FileOperation operation);

    void showNoApplicationFound();
    void showOperationUnsupported();
    void showViewInArchivesUnsupported();
    void showDocumentsClipped(int size);
    void showActionNotAllowed();

    /**
     * Dialogs used when share file count over limit
     */
    void showShareOverLimit(int size);

    // Dialogs used in PickActivity
    void confirmAction(FragmentManager fm, DocumentInfo pickTarget, int type);

    // Should be private, but Java doesn't like me treating an interface like a mini-package.
    public static final class RuntimeDialogController implements DialogController {

        private final Activity mActivity;
        private final Features mFeatures;
        private OperationProgressDialog mCurrentProgressDialog = null;

        public RuntimeDialogController(Features features, Activity activity) {
            mFeatures = features;
            mActivity = activity;
        }

        @Override
        public void showFileOperationStatus(@Status int status, @OpType int opType, int docCount) {
            if (status == FileOperations.Callback.STATUS_REJECTED) {
                showOperationUnsupported();
                return;
            }
            if (status == FileOperations.Callback.STATUS_FAILED) {
                Snackbars.showOperationFailed(mActivity);
                return;
            }

            if (docCount == 0) {
                // Nothing has been pasted, so there is no need to show a snackbar.
                return;
            }

            if (shouldShowProgressDialogForOperation(opType)) {
                // The operation has a progress dialog created, so do not show a snackbar
                // for operation start, as it would duplicate the UI.
                return;
            }

            switch (opType) {
                case FileOperationService.OPERATION_MOVE:
                    Snackbars.showMove(mActivity, docCount);
                    break;
                case FileOperationService.OPERATION_COPY:
                    Snackbars.showCopy(mActivity, docCount);
                    break;
                case FileOperationService.OPERATION_COMPRESS:
                    Snackbars.showCompress(mActivity, docCount);
                    break;
                case FileOperationService.OPERATION_EXTRACT:
                    Snackbars.showExtract(mActivity, docCount);
                    break;
                case FileOperationService.OPERATION_DELETE:
                    Snackbars.showDelete(mActivity, docCount);
                    break;
                default:
                    throw new UnsupportedOperationException("Unsupported Operation: " + opType);
            }
        }

        private boolean shouldShowProgressDialogForOperation(@OpType int opType) {
            // TODO: Hook up progress dialog to the delete operation.
            if (opType == FileOperationService.OPERATION_DELETE) {
                return false;
            }

            return mFeatures.isJobProgressDialogEnabled();
        }

        @Override
        public void showProgressDialog(String jobId, FileOperation operation) {
            assert(operation.getOpType() != FileOperationService.OPERATION_UNKNOWN);

            if (!shouldShowProgressDialogForOperation(operation.getOpType())) {
                return;
            }

            if (mCurrentProgressDialog != null) {
                mCurrentProgressDialog.dismiss();
            }

            mCurrentProgressDialog = OperationProgressDialog.create(mActivity, jobId, operation);
            mCurrentProgressDialog.show();
        }

        @Override
        public void showActionNotAllowed() {
            // Shows as a last resort when a document is not allowed to share across users
            Snackbars.makeSnackbar(
                    mActivity, R.string.toast_action_not_allowed, Snackbar.LENGTH_LONG).show();
        }

        @Override
        public void showNoApplicationFound() {
            Snackbars.makeSnackbar(
                    mActivity, R.string.toast_no_application, Snackbar.LENGTH_LONG).show();
        }

        @Override
        public void showOperationUnsupported() {
            Snackbars.showOperationRejected(mActivity);
        }

        @Override
        public void showViewInArchivesUnsupported() {
            Snackbars.makeSnackbar(mActivity, R.string.toast_view_in_archives_unsupported,
                    Snackbar.LENGTH_LONG).show();
        }

        @Override
        public void showDocumentsClipped(int size) {
            Snackbars.showDocumentsClipped(mActivity, size);
        }

        @Override
        public void showShareOverLimit(int size) {
            String message = mActivity.getString(R.string.toast_share_over_limit, size);
            Snackbars.makeSnackbar(mActivity, message, Snackbar.LENGTH_LONG).show();
        }

        @Override
        public void confirmAction(FragmentManager fm, DocumentInfo pickTarget, int type) {
            ConfirmFragment.show(fm, pickTarget, type);
        }
    }

    /**
     * Create DialogController Impl.
     */
    static DialogController create(Features features, Activity activity) {
        return new RuntimeDialogController(features, activity);
    }
}
