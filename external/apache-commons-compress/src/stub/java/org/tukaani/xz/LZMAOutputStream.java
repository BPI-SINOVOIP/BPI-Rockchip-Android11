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

package org.tukaani.xz;

import org.tukaani.xz.LZMA2Options;

import java.io.IOException;
import java.io.OutputStream;

public class LZMAOutputStream extends OutputStream {

    public LZMAOutputStream(final OutputStream outStream, LZMA2Options options, boolean unknown)
            throws IOException {
        throw new UnsupportedOperationException();
    }

    public LZMAOutputStream(final OutputStream outStream, LZMA2Options options, int unknown)
            throws IOException {
        throw new UnsupportedOperationException();
    }

    @Override
    public void write(int b) throws IOException {
        throw new UnsupportedOperationException();
    }

    public void finish() {
        throw new UnsupportedOperationException();
    }
}
