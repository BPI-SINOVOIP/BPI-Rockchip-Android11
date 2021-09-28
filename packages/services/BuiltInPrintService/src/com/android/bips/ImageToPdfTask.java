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

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.pdf.PdfDocument;
import android.os.AsyncTask;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PrintAttributes;
import android.print.pdf.PrintedPdfDocument;
import android.util.Log;

import java.io.FileOutputStream;

/**
 * A background task that optimizes a {@link Bitmap}, renders it into a PDF, and delivers the PDF
 * to a {@link ParcelFileDescriptor}.
 */
class ImageToPdfTask extends AsyncTask<ParcelFileDescriptor, Void, Throwable> {
    private static final String TAG = ImageToPdfTask.class.getSimpleName();
    private static final boolean DEBUG = false;
    private static final float POINTS_PER_INCH = 72;

    private final PrintedPdfDocument mDocument;
    private final Bitmap mBitmap;
    private final PrintAttributes mAttributes;
    private final int mDpi;
    private final CancellationSignal mCancellationSignal;

    ImageToPdfTask(Context context, Bitmap bitmap, PrintAttributes attributes, int dpi,
                   CancellationSignal cancellationSignal) {
        mBitmap = bitmap;
        mAttributes = attributes;
        mCancellationSignal = cancellationSignal;
        mDpi = dpi;
        mDocument = new PrintedPdfDocument(context, mAttributes);
    }

    @Override
    protected Throwable doInBackground(ParcelFileDescriptor... outputs) {
        try (ParcelFileDescriptor output = outputs[0]) {
            if (DEBUG) Log.d(TAG, "creating document at dpi=" + mDpi);
            writeBitmapToDocument();
            mCancellationSignal.throwIfCanceled();
            if (DEBUG) Log.d(TAG, "writing to output stream");
            mDocument.writeTo(new FileOutputStream(output.getFileDescriptor()));
            mDocument.close();
            if (DEBUG) Log.d(TAG, "finished sending");
            return null;
        } catch (Throwable t) {
            return t;
        }
    }

    /** Create a one-page PDF document containing the bitmap */
    private void writeBitmapToDocument() {
        PdfDocument.Page page = mDocument.startPage(1);
        if (mAttributes.getMediaSize().isPortrait() == mBitmap.getWidth() < mBitmap.getHeight()) {
            writeBitmapToPage(page, true);
        } else {
            // If user selects the opposite orientation, fit instead of fill.
            writeBitmapToPage(page, false);
        }
        mDocument.finishPage(page);
    }

    private void writeBitmapToPage(PdfDocument.Page page, boolean fill) {
        RectF extent = new RectF(page.getInfo().getContentRect());
        float scale;
        boolean rotate;
        if (fill) {
            // Fill the entire page with image data
            scale = Math.max(extent.height() / POINTS_PER_INCH * mDpi / mBitmap.getHeight(),
                extent.width() / POINTS_PER_INCH * mDpi / mBitmap.getWidth());
            rotate = false;
        } else {
            // Scale and rotate the image to fit entirely on the page
            scale = Math.min(extent.height() / POINTS_PER_INCH * mDpi / mBitmap.getWidth(),
                extent.width() / POINTS_PER_INCH * mDpi / mBitmap.getHeight());
            rotate = true;
        }

        if (scale >= 1) {
            // Image will need to be scaled up
            drawDirect(page, extent, fill, rotate);
        } else {
            // Scale image down to the size needed for printing
            drawOptimized(page, extent, scale, rotate);
        }
    }

    /**
     * Render the source bitmap directly into the PDF
     */
    private void drawDirect(PdfDocument.Page page, RectF extent, boolean fill, boolean rotate) {
        float scale;
        if (fill) {
            scale = Math.max(extent.height() / mBitmap.getHeight(),
                extent.width() / mBitmap.getWidth());
        } else {
            scale = Math.min(extent.height() / mBitmap.getWidth(),
                extent.width() / mBitmap.getHeight());
        }

        float offsetX = (extent.width() - mBitmap.getWidth() * scale) / 2;
        float offsetY = (extent.height() - mBitmap.getHeight() * scale) / 2;

        Matrix matrix = new Matrix();
        if (rotate) {
            matrix.postRotate(90, mBitmap.getWidth() / 2, mBitmap.getHeight() / 2);
        }
        matrix.postScale(scale, scale);
        matrix.postTranslate(offsetX, offsetY);
        page.getCanvas().clipRect(extent);
        page.getCanvas().drawBitmap(mBitmap, matrix, new Paint(Paint.FILTER_BITMAP_FLAG));
    }

    /**
     * Scale down the bitmap to specific DPI to reduce delivered PDF size
     */
    private void drawOptimized(PdfDocument.Page page, RectF extent, float scale, boolean rotate) {
        float targetWidth = (extent.width() / POINTS_PER_INCH * mDpi);
        float targetHeight = (extent.height() / POINTS_PER_INCH * mDpi);
        float offsetX = ((targetWidth / scale) - mBitmap.getWidth()) / 2;
        float offsetY = ((targetHeight / scale) - mBitmap.getHeight()) / 2;

        Bitmap targetBitmap = Bitmap.createBitmap((int) targetWidth, (int) targetHeight,
                Bitmap.Config.ARGB_8888);
        Canvas bitmapCanvas = new Canvas(targetBitmap);
        Matrix matrix = new Matrix();
        matrix.postScale(scale, scale);
        if (rotate) {
            matrix.postRotate(90, targetWidth / 2, targetHeight / 2);
        }
        bitmapCanvas.setMatrix(matrix);
        bitmapCanvas.drawBitmap(mBitmap, offsetX, offsetY, new Paint(Paint.FILTER_BITMAP_FLAG));
        page.getCanvas().drawBitmap(targetBitmap, null, extent, null);
        targetBitmap.recycle();
    }
}
