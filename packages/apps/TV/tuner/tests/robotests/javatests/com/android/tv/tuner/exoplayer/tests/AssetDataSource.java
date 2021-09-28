/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *            http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.tv.tuner.exoplayer.tests;

import android.content.Context;
import android.content.res.AssetManager;
import android.net.Uri;
import com.google.android.exoplayer.C;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.android.exoplayer2.upstream.DataSpec;
import com.google.android.exoplayer2.upstream.TransferListener;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

/** A local asset. */
// Copied from com.google.android.exoplayer.upstream.AssetDataSource for test.
final class AssetDataSource implements DataSource {
    /** Thrown when an {@link IOException} is encountered reading a local asset. */
    private static final class AssetDataSourceException extends IOException {
        private AssetDataSourceException(IOException cause) {
            super(cause);
        }
    }

    private final AssetManager mAssetManager;

    private InputStream mInputStream;
    private long mBytesRemaining;
    private Uri mUri;

    /** Constructs a new {@link DataSource} that retrieves data from a local asset. */
    AssetDataSource(Context context) {
        mAssetManager = context.getAssets();
    }

    @Override
    public long open(DataSpec dataSpec) throws AssetDataSourceException {
        try {
            String path = dataSpec.uri.getPath();
            if (path.startsWith("/android_asset/")) {
                path = path.substring(15);
            } else if (path.startsWith("/")) {
                path = path.substring(1);
            }
            mInputStream = mAssetManager.open(path, AssetManager.ACCESS_RANDOM);
            long skipped = mInputStream.skip(dataSpec.position);
            if (skipped < dataSpec.position) {
                // mAssetManager.open() returns an AssetInputStream, whose skip() implementation
                // only skips fewer bytes than requested if the skip is beyond the end of the
                // asset's data.
                throw new EOFException();
            }
            if (dataSpec.length != C.LENGTH_UNBOUNDED) {
                mBytesRemaining = dataSpec.length;
            } else {
                mBytesRemaining = mInputStream.available();
                if (mBytesRemaining == Integer.MAX_VALUE) {
                    // mAssetManager.open() returns an AssetInputStream, whose available()
                    // implementation returns Integer.MAX_VALUE if the remaining length is greater
                    // than (or equal to) Integer.MAX_VALUE. We don't know the true length in this
                    // case, so treat as unbounded.
                    mBytesRemaining = C.LENGTH_UNBOUNDED;
                }
            }
        } catch (IOException e) {
            throw new AssetDataSourceException(e);
        }

        mUri = dataSpec.uri;
        return mBytesRemaining;
    }

    @Override
    public int read(byte[] buffer, int offset, int readLength) throws AssetDataSourceException {
        if (mBytesRemaining == 0) {
            return -1;
        } else {
            int bytesRead = 0;
            try {
                int bytesToRead =
                        mBytesRemaining == C.LENGTH_UNBOUNDED
                                ? readLength
                                : (int) Math.min(mBytesRemaining, readLength);
                bytesRead = mInputStream.read(buffer, offset, bytesToRead);
            } catch (IOException e) {
                throw new AssetDataSourceException(e);
            }

            if (bytesRead > 0 && mBytesRemaining != C.LENGTH_UNBOUNDED) {
                mBytesRemaining -= bytesRead;
            }

            return bytesRead;
        }
    }

    @Override
    public void close() throws AssetDataSourceException {
        mUri = null;
        if (mInputStream != null) {
            try {
                mInputStream.close();
            } catch (IOException e) {
                throw new AssetDataSourceException(e);
            } finally {
                mInputStream = null;
            }
        }
    }

    @Override
    public void addTransferListener(TransferListener transferListener) {
        // TODO: Implement to support metrics collection.
    }

    @Override
    public Uri getUri() {
        return mUri;
    }
}
