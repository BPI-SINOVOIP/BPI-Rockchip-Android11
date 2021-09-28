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
package com.android.se.security.gpac;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

/**
 * PERM-AR-DO: The access rule for Device API control consists of a permission bit mask of 8 bytes.
 */
public class PERM_AR_DO extends BerTlv {

    public static final int TAG = 0xDB;

    public static final int PERM_MASK_LEN = 8;

    private byte[] mPermissionMask = new byte[0];

    public PERM_AR_DO(byte[] rawData, int valueIndex, int valueLength) {
        super(rawData, TAG, valueIndex, valueLength);
    }

    public PERM_AR_DO(byte[] permissionMask) {
        super(permissionMask, TAG, 0, (permissionMask == null ? 0 : permissionMask.length));
        if (permissionMask != null) mPermissionMask = permissionMask;
    }

    public byte[] getPermissionMask() {
        return mPermissionMask;
    }

    @Override
    /**
     * Tag: DB Length: 8 Value: Contains a permission mask
     */
    public void interpret() throws ParserException {
        mPermissionMask = new byte[0];

        byte[] data = getRawData();
        int index = getValueIndex();
        int length = getValueLength();

        if (index + getValueLength() > data.length) {
            throw new ParserException("Not enough data for PERM-AR-DO!");
        }

        if (length == PERM_MASK_LEN) {
            mPermissionMask = new byte[length];
            System.arraycopy(data, index, mPermissionMask, 0, length);
        } else {
            throw new ParserException("Invalid length of PERM-AR-DO!");
        }
    }

    @Override
    /**
     * Tag: DB Length: 8 Value: Contains a permission mask
     */
    public void build(ByteArrayOutputStream stream) throws DO_Exception {

        // sanity checks
        if (mPermissionMask.length != PERM_MASK_LEN) {
            throw new DO_Exception("Invalid value length for PERM-AR-DO!");
        }

        // write tag
        stream.write(getTag());
        try {
            stream.write(mPermissionMask.length);
            stream.write(mPermissionMask);
        } catch (IOException ioe) {
            throw new DO_Exception("PERM could not be written!");
        }
    }
}
