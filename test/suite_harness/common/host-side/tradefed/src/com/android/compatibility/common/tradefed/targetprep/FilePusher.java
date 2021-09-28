/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.compatibility.common.tradefed.targetprep;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.targetprep.PushFilePreparer;

import java.io.File;

/** Pushes specified testing artifacts from Compatibility repository. */
@OptionClass(alias = "file-pusher")
public final class FilePusher extends PushFilePreparer {

    @Option(name = "append-bitness",
            description = "Append the ABI's bitness to the filename.")
    private boolean mAppendBitness = false;

    /**
     * {@inheritDoc}
     */
    @Override
    public File resolveRelativeFilePath(IBuildInfo buildInfo, String fileName) {
        return super.resolveRelativeFilePath(
                buildInfo,
                String.format(
                        "%s%s", fileName, shouldAppendBitness() ? getAbi().getBitness() : ""));
    }

    /** Whether or not to append bitness to the file name. */
    public boolean shouldAppendBitness() {
        return mAppendBitness;
    }
}
