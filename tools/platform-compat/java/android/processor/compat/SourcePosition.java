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

package android.processor.compat;

import java.util.Objects;

/** POJO to represent source position of a tree in Java source file. */
public final class SourcePosition {

    private final String filename;
    private final long startLineNumber;
    private final long startColumnNumber;
    private final long endLineNumber;
    private final long endColumnNumber;

    public SourcePosition(String filename, long startLineNumber, long startColumnNumber,
            long endLineNumber, long endColumnNumber) {
        this.filename = filename;
        this.startLineNumber = startLineNumber;
        this.startColumnNumber = startColumnNumber;
        this.endLineNumber = endLineNumber;
        this.endColumnNumber = endColumnNumber;
    }

    public String getFilename() {
        return filename;
    }

    public long getStartLineNumber() {
        return startLineNumber;
    }

    public long getStartColumnNumber() {
        return startColumnNumber;
    }

    public long getEndLineNumber() {
        return endLineNumber;
    }

    public long getEndColumnNumber() {
        return endColumnNumber;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        SourcePosition that = (SourcePosition) o;
        return startLineNumber == that.startLineNumber
                && startColumnNumber == that.startColumnNumber
                && endLineNumber == that.endLineNumber
                && endColumnNumber == that.endColumnNumber
                && Objects.equals(filename, that.filename);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                filename,
                startLineNumber,
                startColumnNumber,
                endLineNumber,
                endColumnNumber);
    }
}
