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

package com.android.nn.benchmark.util;

import android.content.res.AssetManager;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Input/Output utilities.
 */
public final class IOUtils {
    private IOUtils() {}

    /** Reads float values from a byte array. */
    public static float[] readFloats(byte[] bytes, int dataSize) {
        ByteBuffer buffer = ByteBuffer.wrap(bytes);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        int size = bytes.length / dataSize;
        float[] result = new float[size];
        for (int i = 0; i < size; ++i) {
            if (dataSize == 4) {
                result[i] = buffer.getFloat();
            } else if (dataSize == 1) {
                result[i] = (float)(buffer.get() & 0xff);
            }
        }
        return result;
    }

    /** Reads data in native byte order */
    public static byte[] readAsset(AssetManager assetManager, String assetFilename,
                                   int dataBytesSize)
            throws IOException {
        try (InputStream in = assetManager.open(assetFilename)) {
            byte[] buffer = new byte[8192];
            int bytesRead;
            ByteArrayOutputStream output = new ByteArrayOutputStream();
            while ((bytesRead = in.read(buffer)) != -1) {
                output.write(buffer, 0, bytesRead);
            }

            byte[] result = output.toByteArray();
            // Do we need to swap data endianess?
            if (dataBytesSize > 1 && ByteOrder.nativeOrder() != ByteOrder.LITTLE_ENDIAN) {
                if (dataBytesSize == 4) {
                    invertOrder4(result);
                } if (dataBytesSize == 2) {
                    invertOrder2(result);
                } else {
                    throw new IllegalArgumentException(
                            "Byte order swapping for " + dataBytesSize
                                    + " bytes is not implmemented (yet)");
                }
            }
            return result;
        }
    }

    /** Reverses endianness on array of 4 byte elements */
    private static void invertOrder4(byte[] data) {
        if (data.length % 4 != 0) {
            throw new IllegalArgumentException("Data is not 4 byte aligned");
        }
        for (int i = 0; i < data.length; i += 4) {
            byte a = data[i];
            byte b = data[i + 1];
            data[i] = data[i + 3];
            data[i + 1] = data[i + 2];
            data[i + 2] = b;
            data[i + 3] = a;
        }
    }

    /** Reverses endianness on array of 2 byte elements */
    private static void invertOrder2(byte[] data) {
        if (data.length % 2 != 0) {
            throw new IllegalArgumentException("Data is not 2 byte aligned");
        }
        for (int i = 0; i < data.length; i += 2) {
            byte a = data[i];
            data[i] = data[i + 1];
            data[i + 1] = a;
        }
    }
}
