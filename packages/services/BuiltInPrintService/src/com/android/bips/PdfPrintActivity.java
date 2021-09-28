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

package com.android.bips;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;
import android.print.PrintManager;
import android.util.Log;
import android.webkit.URLUtil;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Activity to receive share-to-print intents for PDF documents.
 */
public class PdfPrintActivity extends Activity {
    private static final String TAG = PdfPrintActivity.class.getSimpleName();
    private static final boolean DEBUG = false;

    private CancellationSignal mCancellationSignal;
    private String mJobName;
    Uri mContentUri = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String action = getIntent().getAction();
        if (Intent.ACTION_SEND.equals(action)) {
            mContentUri = getIntent().getParcelableExtra(Intent.EXTRA_STREAM);
        } else if (Intent.ACTION_VIEW.equals(action)) {
            mContentUri = getIntent().getData();
        }
        if (mContentUri == null) {
            finish();
        }
        mJobName = URLUtil.guessFileName(getIntent().getStringExtra(Intent.EXTRA_TEXT), null,
            getIntent().resolveType(this));
        if (DEBUG) Log.d(TAG, "onCreate() uri=" + mContentUri + " jobName=" + mJobName);

        PrintManager printManager = (PrintManager) getSystemService(Context.PRINT_SERVICE);
        if (printManager == null) {
            finish();
            return;
        }

        PrintAttributes printAttributes = new PrintAttributes.Builder()
                .setColorMode(PrintAttributes.COLOR_MODE_COLOR)
                .build();
        printManager.print(mJobName, new PdfAdapter(), printAttributes);
    }

    @Override
    protected void onDestroy() {
        if (DEBUG) Log.d(TAG, "onDestroy()");
        if (mCancellationSignal != null) {
            mCancellationSignal.cancel();
        }
        super.onDestroy();
    }

    private class PdfAdapter extends PrintDocumentAdapter {
        @Override
        public void onLayout(PrintAttributes oldAttributes, PrintAttributes newAttributes,
                CancellationSignal cancellationSignal, LayoutResultCallback callback,
                Bundle bundle) {
            if (DEBUG) Log.d(TAG, "onLayout() attrs=" + newAttributes);

            PrintDocumentInfo info = new PrintDocumentInfo.Builder(mJobName)
                    .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
                    .setPageCount(PrintDocumentInfo.PAGE_COUNT_UNKNOWN)
                    .build();
            callback.onLayoutFinished(info, false);
        }

        @Override
        public void onWrite(PageRange[] pageRanges, ParcelFileDescriptor fileDescriptor,
                CancellationSignal cancellationSignal, WriteResultCallback callback) {
            if (DEBUG) Log.d(TAG, "onWrite()");
            mCancellationSignal = cancellationSignal;
            new PdfDeliverTask(fileDescriptor, callback).execute();
        }

        @Override
        public void onFinish() {
            if (DEBUG) Log.d(TAG, "onFinish()");
            finish();
        }
    }

    private class PdfDeliverTask extends AsyncTask<Void, Void, Void> {
        ParcelFileDescriptor mDescriptor;
        PrintDocumentAdapter.WriteResultCallback mCallback;

        PdfDeliverTask(ParcelFileDescriptor descriptor,
                       PrintDocumentAdapter.WriteResultCallback callback) {
            mDescriptor = descriptor;
            mCallback = callback;
        }

        @Override
        protected Void doInBackground(Void... voids) {
            try (InputStream in = getContentResolver().openInputStream(mContentUri)) {
                if (in == null) {
                    throw new IOException("Failed to open input stream");
                }
                try (OutputStream out = new FileOutputStream(mDescriptor.getFileDescriptor())) {
                    byte[] buffer = new byte[10 * 1024];
                    int length;
                    while ((length = in.read(buffer)) >= 0 && !mCancellationSignal.isCanceled()) {
                        out.write(buffer, 0, length);
                    }
                }
                if (mCancellationSignal.isCanceled()) {
                    mCallback.onWriteCancelled();
                } else {
                    mCallback.onWriteFinished(new PageRange[] { PageRange.ALL_PAGES });
                }
            } catch (IOException e) {
                Log.w(TAG, "Failed to deliver content", e);
                mCallback.onWriteFailed(e.getMessage());
            }
            return null;
        }
    }
}
