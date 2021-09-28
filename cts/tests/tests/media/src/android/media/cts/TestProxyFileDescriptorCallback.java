/*
 * Copyright 2019 The Android Open Source Project
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

package android.media.cts;

import android.content.res.AssetFileDescriptor;
import android.os.ProxyFileDescriptorCallback;
import android.platform.test.annotations.AppModeFull;
import android.system.ErrnoException;
import android.system.OsConstants;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;

/**
 * A ProxyFileDescriptorCallback that reads from a byte array for use in tests.
 */
@AppModeFull(reason = "TODO: evaluate and port to instant")
public class TestProxyFileDescriptorCallback extends ProxyFileDescriptorCallback {
    private static final String TAG = "TestProxyFileDescriptorCallback";

    private byte[] mData;

    private boolean mThrowFromReadAt;
    private boolean mThrowFromGetSize;
    private Integer mReturnFromReadAt;
    private Long mReturnFromGetSize;
    private boolean mIsClosed;

    // Read an asset fd into a new byte array data source. Closes afd.
    public static TestProxyFileDescriptorCallback fromAssetFd(AssetFileDescriptor afd)
            throws IOException {
        try {
            InputStream in = afd.createInputStream();
            final int size = (int) afd.getDeclaredLength();
            byte[] data = new byte[(int) size];
            int writeIndex = 0;
            int numRead = 0;
            do {
                numRead = in.read(data, writeIndex, size - writeIndex);
                writeIndex += numRead;
            } while (numRead >= 0);
            return new TestProxyFileDescriptorCallback(data);
        } finally {
            afd.close();
        }
    }

    public TestProxyFileDescriptorCallback(byte[] data) {
        mData = data;
    }

    @Override
    public int onRead(long offset, int size, byte[] data) throws ErrnoException {
        if (mThrowFromReadAt) {
            throw new ErrnoException("onRead", OsConstants.EIO);
        }
        if (mReturnFromReadAt != null) {
            return mReturnFromReadAt;
        }

        // Clamp reads past the end of the source.
        if (offset >= mData.length) {
            return 0; // 0 indicates EOF
        }
        if (offset + size > mData.length) {
            size -= (offset + size) - mData.length;
        }
        System.arraycopy(mData, (int)offset, data, 0, size);
        return size;
    }

    @Override
    public long onGetSize() throws ErrnoException {
        if (mThrowFromGetSize) {
            throw new ErrnoException("onGetSize", OsConstants.EIO);
        }
        if (mReturnFromGetSize != null) {
            return mReturnFromGetSize;
        }

        Log.v(TAG, "getSize: " + mData.length);
        return mData.length;
    }

    @Override
    public void onRelease() {
        Log.d(TAG, "onRelease()");
        mIsClosed = true;
    }

    // Whether close() has been called.
    public synchronized boolean isClosed() {
        return mIsClosed;
    }

    public void throwFromReadAt() {
        mThrowFromReadAt = true;
    }

    public void throwFromGetSize() {
        mThrowFromGetSize = true;
    }

    public void returnFromReadAt(int numRead) {
        mReturnFromReadAt = numRead;
    }

    public void returnFromGetSize(long size) {
        mReturnFromGetSize = size;
    }
}

