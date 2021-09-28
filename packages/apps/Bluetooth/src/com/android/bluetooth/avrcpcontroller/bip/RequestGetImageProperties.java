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
 * This implements a GetImageProperties request, allowing a user to retrieve information regarding
 * the image formats, encodings, etc. available for an image.
 */
public class RequestGetImageProperties extends BipRequest {
    // Expected inputs
    private String mImageHandle = null;

    // Expected return type
    private static final String TYPE = "x-bt/img-properties";
    private BipImageProperties mImageProperties = null;

    public RequestGetImageProperties(String imageHandle) {
        mHeaderSet = new HeaderSet();
        mResponseCode = -1;
        mImageHandle = imageHandle;

        debug("GetImageProperties - handle: " + mImageHandle);

        mHeaderSet.setHeader(HeaderSet.TYPE, TYPE);
        mHeaderSet.setHeader(HEADER_ID_IMG_HANDLE, mImageHandle);
    }

    @Override
    public int getType() {
        return TYPE_GET_IMAGE_PROPERTIES;
    }

    @Override
    public void execute(ClientSession session) throws IOException {
        executeGet(session);
    }

    @Override
    protected void readResponse(InputStream stream) throws IOException {
        try {
            mImageProperties = new BipImageProperties(stream);
            debug("Response GetImageProperties - handle: " + mImageHandle + ", properties: "
                    + mImageProperties.toString());
        } catch (ParseException e) {
            error("Failed to parse incoming properties object");
            mImageProperties = null;
        }
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
     * Get the requested set of image properties sent from the remote device
     *
     * @return A BipImageProperties object
     */
    public BipImageProperties getImageProperties() {
        return mImageProperties;
    }
}
