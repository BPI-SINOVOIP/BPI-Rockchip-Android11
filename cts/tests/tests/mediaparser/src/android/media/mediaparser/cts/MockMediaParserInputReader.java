/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.media.mediaparser.cts;

import android.media.MediaParser;

import com.google.android.exoplayer2.testutil.FakeExtractorInput;

import java.io.IOException;

public class MockMediaParserInputReader implements MediaParser.SeekableInputReader {

    private final FakeExtractorInput mFakeExtractorInput;

    public MockMediaParserInputReader(FakeExtractorInput fakeExtractorInput) throws IOException {
        mFakeExtractorInput = fakeExtractorInput;
    }

    public void reset() {
        mFakeExtractorInput.reset();
    }

    public void setPosition(int position) {
        mFakeExtractorInput.setPosition(position);
    }

    // SeekableInputReader implementation.

    @Override
    public int read(byte[] buffer, int offset, int readLength) throws IOException {
        return mFakeExtractorInput.read(buffer, offset, readLength);
    }

    @Override
    public long getPosition() {
        return mFakeExtractorInput.getPosition();
    }

    @Override
    public long getLength() {
        return mFakeExtractorInput.getLength();
    }

    @Override
    public void seekToPosition(long position) {
        mFakeExtractorInput.setPosition((int) position);
    }
}
