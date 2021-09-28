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

import android.util.Log;

import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;

import javax.obex.ClientOperation;
import javax.obex.ClientSession;
import javax.obex.HeaderSet;
import javax.obex.ResponseCodes;

/**
 * This is a base class for implementing AVRCP Controller Basic Image Profile (BIP) requests
 */
abstract class BipRequest {
    private static final String TAG = "avrcpcontroller.BipRequest";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    // User defined OBEX header identifiers
    protected static final byte HEADER_ID_IMG_HANDLE = 0x30;
    protected static final byte HEADER_ID_IMG_DESCRIPTOR = 0x71;

    // Request types
    public static final int TYPE_GET_IMAGE_PROPERTIES = 0;
    public static final int TYPE_GET_IMAGE = 1;

    protected HeaderSet mHeaderSet;
    protected ClientOperation mOperation = null;
    protected int mResponseCode;

    BipRequest() {
        mHeaderSet = new HeaderSet();
        mResponseCode = -1;
    }

    /**
     * A function that returns the type of the request.
     *
     * Used to determine type instead of using 'instanceof'
     */
    public abstract int getType();

    /**
     * A single point of entry for kicking off a AVRCP BIP request.
     *
     * Child classes are expected to implement this interface, filling in the details of the request
     * (headers, operation type, error handling, etc).
     */
    public abstract void execute(ClientSession session) throws IOException;

    /**
     * A generica GET operation, providing overridable hooks to read response headers and content.
     */
    protected void executeGet(ClientSession session) throws IOException {
        debug("Exeucting GET");
        setOperation(null);
        try {
            ClientOperation operation = (ClientOperation) session.get(mHeaderSet);
            setOperation(operation);
            operation.setGetFinalFlag(true);
            operation.continueOperation(true, false);
            readResponseHeaders(operation.getReceivedHeader());
            InputStream inputStream = operation.openInputStream();
            readResponse(inputStream);
            inputStream.close();
            operation.close();
            mResponseCode = operation.getResponseCode();
        } catch (IOException e) {
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
            error("GET threw an exeception: " + e);
            throw e;
        }
        debug("GET final response code is '" + mResponseCode + "'");
    }

    /**
     * A generica PUT operation, providing overridable hooks to read response headers.
     */
    protected void executePut(ClientSession session, byte[] body) throws IOException {
        debug("Exeucting PUT");
        setOperation(null);
        mHeaderSet.setHeader(HeaderSet.LENGTH, Long.valueOf(body.length));
        try {
            ClientOperation operation = (ClientOperation) session.put(mHeaderSet);
            setOperation(operation);
            DataOutputStream outputStream = mOperation.openDataOutputStream();
            outputStream.write(body);
            outputStream.close();
            readResponseHeaders(operation.getReceivedHeader());
            operation.close();
            mResponseCode = operation.getResponseCode();
        } catch (IOException e) {
            mResponseCode = ResponseCodes.OBEX_HTTP_INTERNAL_ERROR;
            error("PUT threw an exeception: " + e);
            throw e;
        }
        debug("PUT final response code is '" + mResponseCode + "'");
    }

    /**
     * Determine if the request was a success
     *
     * @return True if the request was successful, false otherwise
     */
    public final boolean isSuccess() {
        return (mResponseCode == ResponseCodes.OBEX_HTTP_OK);
    }

    /**
     * Get the actual response code associated with the request
     *
     * @return The response code as in integer
     */
    public final int getResponseCode() {
        return mResponseCode;
    }

    /**
     * A callback for subclasses to add logic to make determinations against the content of the
     * returned headers.
     */
    protected void readResponseHeaders(HeaderSet headerset) {
        /* nothing here by default */
    }

    /**
     * A callback for subclasses to add logic to make determinations against the content of the
     * returned response body.
     */
    protected void readResponse(InputStream stream) throws IOException {
        /* nothing here by default */
    }

    private synchronized ClientOperation getOperation() {
        return mOperation;
    }

    private synchronized void setOperation(ClientOperation operation) {
        mOperation = operation;
    }

    @Override
    public String toString() {
        return TAG + " (type: " + getType() + ", mResponseCode: " + mResponseCode + ")";
    }

    /**
     * Print to debug if debug is enabled for this class
     */
    protected void debug(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    /**
     * Print to warn
     */
    protected void warn(String msg) {
        Log.w(TAG, msg);
    }

    /**
     * Print to error
     */
    protected void error(String msg) {
        Log.e(TAG, msg);
    }
}
