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
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;
import android.print.PrintJob;
import android.print.PrintManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.webkit.URLUtil;
import android.widget.Toast;

import com.android.bips.jni.MediaSizes;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * Activity to receive share-to-print intents for images.
 */
public class ImagePrintActivity extends Activity {
    private static final String TAG = ImagePrintActivity.class.getSimpleName();
    private static final boolean DEBUG = false;
    private static final int PRINT_DPI = 300;
    private static final PrintAttributes.MediaSize DEFAULT_PHOTO_MEDIA =
            PrintAttributes.MediaSize.NA_INDEX_4X6;

    /** Countries where A5 is a more common photo media size. */
    private static final String[] ISO_A5_COUNTRY_CODES = {
        "IQ", "SY", "YE", "VN", "MA"
    };

    private CancellationSignal mCancellationSignal = new CancellationSignal();
    private String mJobName;
    private Bitmap mBitmap;
    private DisplayMetrics mDisplayMetrics = new DisplayMetrics();
    private Runnable mOnBitmapLoaded = null;
    private AsyncTask<?, ?, ?> mTask = null;
    private PrintJob mPrintJob;
    private Bitmap mGrayscaleBitmap;
    private PrintAttributes.MediaSize mDefaultMediaSize = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String action = getIntent().getAction();
        Uri contentUri = null;
        if (Intent.ACTION_SEND.equals(action)) {
            contentUri = getIntent().getParcelableExtra(Intent.EXTRA_STREAM);
        } else if (Intent.ACTION_VIEW.equals(action)) {
            contentUri = getIntent().getData();
        }
        if (contentUri == null) {
            finish();
        }
        getWindowManager().getDefaultDisplay().getMetrics(mDisplayMetrics);
        mJobName = URLUtil.guessFileName(getIntent().getStringExtra(Intent.EXTRA_TEXT), null,
                getIntent().resolveType(this));

        if (DEBUG) Log.d(TAG, "onCreate() uri=" + contentUri + " jobName=" + mJobName);

        // Load the bitmap while we start the print
        mTask = new LoadBitmapTask().execute(contentUri);
    }

    /**
     * A background task to load the bitmap and start the print job.
     */
    private class LoadBitmapTask extends AsyncTask<Uri, Boolean, Bitmap> {
        @Override
        protected Bitmap doInBackground(Uri... uris) {
            if (DEBUG) Log.d(TAG, "Loading bitmap from stream");
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inJustDecodeBounds = true;
            loadBitmap(uris[0], options);
            if (options.outWidth <= 0 || options.outHeight <= 0) {
                Log.w(TAG, "Failed to load bitmap");
                return null;
            }
            if (mCancellationSignal.isCanceled()) {
                return null;
            } else {
                // Publish progress and load for real
                publishProgress(options.outHeight > options.outWidth);
                options.inJustDecodeBounds = false;
                return loadBitmap(uris[0], options);
            }
        }

        /**
         * Return a bitmap as loaded from {@param contentUri} using {@param options}.
         */
        private Bitmap loadBitmap(Uri contentUri, BitmapFactory.Options options) {
            try (InputStream inputStream = getContentResolver().openInputStream(contentUri)) {
                return BitmapFactory.decodeStream(inputStream, null, options);
            } catch (IOException | SecurityException e) {
                Log.w(TAG, "Failed to load bitmap", e);
                return null;
            }
        }

        @Override
        protected void onProgressUpdate(Boolean... values) {
            // Once we have a portrait/landscape determination, launch the print job
            boolean isPortrait = values[0];
            if (DEBUG) Log.d(TAG, "startPrint(portrait=" + isPortrait + ")");
            PrintManager printManager = (PrintManager) getSystemService(Context.PRINT_SERVICE);
            if (printManager == null) {
                finish();
                return;
            }

            PrintAttributes printAttributes = new PrintAttributes.Builder()
                    .setMediaSize(isPortrait ? getLocaleDefaultMediaSize() :
                            getLocaleDefaultMediaSize().asLandscape())
                    .setColorMode(PrintAttributes.COLOR_MODE_COLOR)
                    .build();
            mPrintJob = printManager.print(mJobName, new ImageAdapter(), printAttributes);
        }

        @Override
        protected void onPostExecute(Bitmap bitmap) {
            if (mCancellationSignal.isCanceled()) {
                if (DEBUG) Log.d(TAG, "LoadBitmapTask cancelled");
            } else if (bitmap == null) {
                if (mPrintJob != null) {
                    mPrintJob.cancel();
                }
                Toast.makeText(ImagePrintActivity.this, R.string.unreadable_input,
                    Toast.LENGTH_LONG).show();
                finish();
            } else {
                if (DEBUG) Log.d(TAG, "LoadBitmapTask complete");
                mBitmap = bitmap;
                if (mOnBitmapLoaded != null) {
                    mOnBitmapLoaded.run();
                }
            }
        }
    }

    private PrintAttributes.MediaSize getLocaleDefaultMediaSize() {
        if (mDefaultMediaSize == null) {
            String country = getResources().getConfiguration().getLocales().get(0).getCountry();
            Set<String> a5Countries = new HashSet<>(Arrays.asList(ISO_A5_COUNTRY_CODES));
            if (Locale.JAPAN.getCountry().equals(country)) {
                // Photo L is a more common media size in Japan
                mDefaultMediaSize = new PrintAttributes.MediaSize(MediaSizes.OE_PHOTO_L,
                        getString(R.string.media_size_l), 3500, 5000);
            } else if (a5Countries.contains(country)) {
                mDefaultMediaSize = PrintAttributes.MediaSize.ISO_A5;
            } else {
                mDefaultMediaSize = DEFAULT_PHOTO_MEDIA;
            }
        }
        return mDefaultMediaSize;
    }

    @Override
    protected void onDestroy() {
        if (DEBUG) Log.d(TAG, "onDestroy()");
        mCancellationSignal.cancel();
        if (mTask != null) {
            mTask.cancel(true);
            mTask = null;
        }
        if (mBitmap != null) {
            mBitmap.recycle();
            mBitmap = null;
        }
        if (mGrayscaleBitmap != null) {
            mGrayscaleBitmap.recycle();
            mGrayscaleBitmap = null;
        }
        super.onDestroy();
    }

    /**
     * An adapter that converts the image to PDF format as requested by the print system
     */
    private class ImageAdapter extends PrintDocumentAdapter {
        private PrintAttributes mAttributes;
        private int mDpi;

        @Override
        public void onLayout(PrintAttributes oldAttributes, PrintAttributes newAttributes,
                CancellationSignal cancellationSignal, LayoutResultCallback callback,
                Bundle bundle) {
            if (DEBUG) Log.d(TAG, "onLayout() attrs=" + newAttributes);

            if (mBitmap == null) {
                if (DEBUG) Log.d(TAG, "waiting for bitmap...");
                // Try again when bitmap has arrived
                mOnBitmapLoaded = () -> onLayout(oldAttributes, newAttributes, cancellationSignal,
                    callback, bundle);
                return;
            }

            int oldDpi = mDpi;
            mAttributes = newAttributes;

            // Calculate required DPI (print or display)
            if (bundle.getBoolean(EXTRA_PRINT_PREVIEW, false)) {
                PrintAttributes.MediaSize mediaSize = mAttributes.getMediaSize();
                mDpi = Math.min(
                    mDisplayMetrics.widthPixels * 1000 / mediaSize.getWidthMils(),
                    mDisplayMetrics.heightPixels * 1000 / mediaSize.getHeightMils());
            } else {
                mDpi = PRINT_DPI;
            }

            PrintDocumentInfo info = new PrintDocumentInfo.Builder(mJobName)
                    .setContentType(PrintDocumentInfo.CONTENT_TYPE_PHOTO)
                    .setPageCount(1)
                    .build();
            callback.onLayoutFinished(info, !newAttributes.equals(oldAttributes) || oldDpi != mDpi);
        }

        @Override
        public void onWrite(PageRange[] pageRanges, ParcelFileDescriptor fileDescriptor,
                CancellationSignal cancellationSignal, WriteResultCallback callback) {
            if (DEBUG) Log.d(TAG, "onWrite()");
            mCancellationSignal = cancellationSignal;

            mTask = new ImageToPdfTask(ImagePrintActivity.this, getBitmap(mAttributes), mAttributes,
                mDpi, cancellationSignal) {
                @Override
                protected void onPostExecute(Throwable throwable) {
                    if (cancellationSignal.isCanceled()) {
                        if (DEBUG) Log.d(TAG, "writeBitmap() cancelled");
                        callback.onWriteCancelled();
                    } else if (throwable != null) {
                        Log.w(TAG, "Failed to write bitmap", throwable);
                        callback.onWriteFailed(null);
                    } else {
                        if (DEBUG) Log.d(TAG, "Calling onWriteFinished");
                        callback.onWriteFinished(new PageRange[] { PageRange.ALL_PAGES });
                    }
                    mTask = null;
                }
            }.execute(fileDescriptor);
        }

        @Override
        public void onFinish() {
            if (DEBUG) Log.d(TAG, "onFinish()");
            finish();
        }
    }

    /**
     * Return an appropriate bitmap to use when rendering {@param attributes}.
     */
    private Bitmap getBitmap(PrintAttributes attributes) {
        if (attributes.getColorMode() == PrintAttributes.COLOR_MODE_MONOCHROME) {
            if (mGrayscaleBitmap == null) {
                mGrayscaleBitmap = Bitmap.createBitmap(mBitmap.getWidth(), mBitmap.getHeight(),
                    Bitmap.Config.ARGB_8888);
                Canvas canvas = new Canvas(mGrayscaleBitmap);
                Paint paint = new Paint();
                ColorMatrix colorMatrix = new ColorMatrix();
                colorMatrix.setSaturation(0);
                paint.setColorFilter(new ColorMatrixColorFilter(colorMatrix));
                canvas.drawBitmap(mBitmap, 0, 0, paint);
            }
            return mGrayscaleBitmap;
        } else {
            return mBitmap;
        }
    }
}
