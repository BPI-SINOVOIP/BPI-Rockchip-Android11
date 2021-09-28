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

package com.android.tradefed.testtype.coverage;

import com.android.tradefed.config.Option;

import com.google.common.collect.ImmutableList;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Tradefed object to hold coverage options. */
public final class CoverageOptions {

    @Option(
        name = "coverage",
        description =
                "Collect code coverage for this test run. Note that the build under test must be a "
                        + "coverage build or else this will fail."
    )
    private boolean mCoverage = false;

    @Option(
        name = "coverage-toolchain",
        description =
                "The coverage toolchains that were used to compile the coverage build to "
                        + "collect coverage from."
    )
    private List<Toolchain> mToolchains = new ArrayList<>();

    public enum Toolchain {
        CLANG,
        GCOV,
        JACOCO;
    }

    @Option(
        name = "coverage-flush",
        description = "Forces coverage data to be flushed at the end of the test."
    )
    private boolean mCoverageFlush = false;

    @Option(
        name = "coverage-processes",
        description = "Name of processes to collect coverage data from."
    )
    private List<String> mCoverageProcesses = new ArrayList<>();

    @Option(name = "llvm-profdata-path", description = "Path to llvm-profdata tool.")
    private File mLlvmProfdataPath = null;

    /**
     * Returns whether coverage measurements should be collected from this run.
     *
     * @return whether to collect coverage measurements
     */
    public boolean isCoverageEnabled() {
        return mCoverage;
    }

    /**
     * Returns the coverage toolchains to collect coverage from.
     *
     * @return the toolchains to collect coverage from
     */
    public List<Toolchain> getCoverageToolchains() {
        return ImmutableList.copyOf(mToolchains);
    }

    /**
     * Returns whether coverage measurements should be flushed from running processes after the test
     * has completed.
     *
     * @return whether to flush processes for coverage measurements after the test
     */
    public boolean isCoverageFlushEnabled() {
        return mCoverageFlush;
    }

    /**
     * Returns the name of processes to flush coverage from after the test has completed.
     *
     * @return a {@link List} of process names to flush coverage from after the test
     */
    public List<String> getCoverageProcesses() {
        return ImmutableList.copyOf(mCoverageProcesses);
    }

    /**
     * Returns the directory containing the llvm-profdata tool.
     *
     * @return a {@link File} containing the llvm-profdata tool and its dependencies
     */
    public File getLlvmProfdataPath() {
        return mLlvmProfdataPath;
    }
}
