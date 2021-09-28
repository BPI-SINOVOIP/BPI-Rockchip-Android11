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

import java.io.IOException;
import java.io.InputStream;

import javax.obex.ClientSession;
import javax.obex.HeaderSet;

/**
 * This implements a GetImage request, allowing a user to retrieve an image from the remote device
 * with a specified format, encoding, etc.
 */
public class RequestGetImage extends BipRequest {
    // Expected inputs
    private final String mImageHandle;
    private final BipImageDescriptor mImageDescriptor;

    // Expected return type
    private static final String TYPE = "x-bt/img-img";
    private BipImage mImage = null;

    public RequestGetImage(String imageHandle, BipImageDescriptor descriptor) {
        mHeaderSet = new HeaderSet();
        mResponseCode = -1;

        mImageHandle = imageHandle;
        mImageDescriptor = descriptor;

        debug("GetImage - handle: " + mImageHandle + ", descriptor: " + mImageDescriptor);

        mHeaderSet.setHeader(HeaderSet.TYPE, TYPE);
        mHeaderSet.setHeader(HEADER_ID_IMG_HANDLE, mImageHandle);
        if (mImageDescriptor != null) {
            mHeaderSet.setHeader(HEADER_ID_IMG_DESCRIPTOR, mImageDescriptor.serialize());
        } else {
            mHeaderSet.setHeader(HEADER_ID_IMG_DESCRIPTOR, null);
        }
    }

    @Override
    public int getType() {
        return TYPE_GET_IMAGE;
    }

    @Override
    public void execute(ClientSession session) throws IOException {
        executeGet(session);
    }

    @Override
    protected void readResponse(InputStream stream) throws IOException {
        mImage = new BipImage(mImageHandle, stream);
        debug("Response GetImage - handle:" + mImageHandle + ", image: " + mImage);
    }

    /**
     * Get the image handle associated with this request
     *
     * @return image handle used with this request
     */
    public String getImageHandle() {
        return mImageHandle;
    }

    /**
     * Get the downloaded image sent from the remote device
     *
     * @return A BipImage object containing the downloaded image Bitmap
     */
    public BipImage getImage() {
        return mImage;
    }
}
