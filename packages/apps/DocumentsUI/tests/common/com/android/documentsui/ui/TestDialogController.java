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

import androidx.fragment.app.FragmentManager;

import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.picker.ConfirmFragment;
import com.android.documentsui.services.FileOperation;
import com.android.documentsui.services.FileOperations;

import junit.framework.Assert;

public class TestDialogController implements DialogController {

    private int mFileOpStatus;
    private boolean mActionNotAllowed;
    private boolean mNoApplicationFound;
    private boolean mDocumentsClipped;
    private boolean mViewInArchivesUnsupported;
    private boolean mShowOperationUnsupported;
    private boolean mShowShareOverLimit;
    private DocumentInfo mTarget;
    private int mConfrimType;

    public TestDialogController() {
    }

    @Override
    public void showFileOperationStatus(int status, int opType, int docCount) {
        mFileOpStatus = status;
    }

    @Override
    public void showProgressDialog(String jobId, FileOperation operation) {
        throw new UnsupportedOperationException();
    }

    @Override
    public void showActionNotAllowed() {
        mActionNotAllowed = true;
    }

    @Override
    public void showNoApplicationFound() {
        mNoApplicationFound = true;
    }

    @Override
    public void showOperationUnsupported() {
        mShowOperationUnsupported = true;
    }

    @Override
    public void showViewInArchivesUnsupported() {
        mViewInArchivesUnsupported = true;
    }

    @Override
    public void showDocumentsClipped(int size) {
        mDocumentsClipped = true;
    }

    @Override
    public void showShareOverLimit(int size) {
        mShowShareOverLimit = true;
    }

    @Override
    public void confirmAction(FragmentManager fm, DocumentInfo pickTarget, int type) {
        mTarget = pickTarget;
        mConfrimType = type;
    }

    public void assertNoFileFailures() {
        Assert.assertEquals(FileOperations.Callback.STATUS_ACCEPTED, mFileOpStatus);
    }

    public void assertFileOpFailed() {
        Assert.assertEquals(FileOperations.Callback.STATUS_FAILED, mFileOpStatus);
    }

    public void assertActionNotAllowedShown() {
        Assert.assertTrue(mActionNotAllowed);
    }

    public void assertActionNotAllowedNotShown() {
        Assert.assertFalse(mActionNotAllowed);
    }

    public void assertNoAppFoundShown() {
        Assert.assertFalse(mNoApplicationFound);
    }

    public void assertShowOperationUnsupported() {
        Assert.assertTrue(mShowOperationUnsupported);
    }
    public void assertViewInArchivesShownUnsupported() {
        Assert.assertTrue(mViewInArchivesUnsupported);
    }

    public void assertDocumentsClippedNotShown() {
        Assert.assertFalse(mDocumentsClipped);
    }

    public void assertShareOverLimitShown() {
        Assert.assertTrue(mShowShareOverLimit);
    }

    public void assertOverwriteConfirmed(DocumentInfo expected) {
        Assert.assertEquals(expected, mTarget);
        Assert.assertEquals(ConfirmFragment.TYPE_OVERWRITE, mConfrimType);
    }

    public void assertDocumentTreeConfirmed(DocumentInfo expected) {
        Assert.assertEquals(expected, mTarget);
        Assert.assertEquals(ConfirmFragment.TYPE_OEPN_TREE, mConfrimType);
    }
}
