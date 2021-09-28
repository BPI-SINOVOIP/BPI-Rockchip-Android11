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

package com.android.tradefed.util;

import com.android.ddmlib.IShellOutputReceiver;

import java.io.ByteArrayOutputStream;

/** Fake implementation of {@link IShellOutputReceiver} for use in unit tests. */
public class FakeShellOutputReceiver implements IShellOutputReceiver {
    private final ByteArrayOutputStream mStream = new ByteArrayOutputStream();

    public byte[] getReceivedOutput() {
        return mStream.toByteArray();
    }

    @Override
    public void addOutput(byte[] data, int offset, int length) {
        mStream.write(data, offset, length);
    }

    @Override
    public void flush() {}

    @Override
    public boolean isCancelled() {
        return false;
    }
}
