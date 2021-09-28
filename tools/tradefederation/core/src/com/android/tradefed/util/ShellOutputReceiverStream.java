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

package com.android.tradefed.util;

import com.android.ddmlib.IShellOutputReceiver;

import java.io.OutputStream;

import javax.annotation.Nullable;

/** Utility subclass of OutputStream that writes into an IShellOutputReceiver. */
public final class ShellOutputReceiverStream extends OutputStream {
    private IShellOutputReceiver mReceiver;

    /**
     * Create a new adapter for the given {@link IShellOutputReceiver}.
     *
     * <p>It is valid to provide a null receiver here to simplify code using the adapter, i.e. so
     * that it can use this with try-with-resources without checking for a null receiver itself. If
     * receiver is null, {@link #getOutputStream()} will also return null.
     */
    public ShellOutputReceiverStream(@Nullable IShellOutputReceiver receiver) {
        mReceiver = receiver;
    }

    @Override
    public void write(int b) {
        if (mReceiver == null) {
            return;
        }
        final byte converted = (byte) (b & 0xFF);
        mReceiver.addOutput(new byte[] {converted}, 0, 1);
    }

    @Override
    public void write(byte[] b) {
        write(b, 0, b.length);
    }

    @Override
    public void write(byte[] b, int off, int len) {
        if (mReceiver == null) {
            return;
        }
        mReceiver.addOutput(b, off, len);
    }
}
