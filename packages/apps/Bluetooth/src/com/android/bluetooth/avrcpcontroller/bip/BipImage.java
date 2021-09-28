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

package com.android.bluetooth.avrcpcontroller;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import java.io.InputStream;

/**
 * An image object sent over BIP.
 *
 * The image is sent as bytes in the payload of a GetImage request. The format of those bytes is
 * determined by the BipImageDescriptor used when making the request.
 */
public class BipImage {
    private final String mImageHandle;
    private Bitmap mImage = null;

    public BipImage(String imageHandle, InputStream inputStream) {
        mImageHandle = imageHandle;
        parse(inputStream);
    }

    public BipImage(String imageHandle, Bitmap image) {
        mImageHandle = imageHandle;
        mImage = image;
    }

    private void parse(InputStream inputStream) {
        // BitmapFactory can handle BMP, GIF, JPEG, PNG, WebP, and HEIF formats. Returns null if
        // the stream couldn't be parsed.
        mImage = BitmapFactory.decodeStream(inputStream);
    }

    public String getImageHandle() {
        return mImageHandle;
    }

    public Bitmap getImage() {
        return mImage;
    }
}
