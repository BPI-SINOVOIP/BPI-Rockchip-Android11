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

package com.android.internal.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.content.pm.parsing.ParsingPackageRead;
import android.os.Build;
import android.util.ArrayMap;

import com.android.internal.content.om.OverlayConfig.PackageProvider;
import com.android.internal.content.om.OverlayScanner;
import com.android.internal.content.om.OverlayScanner.ParsedOverlayInfo;

import org.junit.Assert;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;

import java.io.File;
import java.io.IOException;
import java.util.Map;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/**
 * A {@link TestRule} that runs a test case twice. First, the test case runs with a non-null
 * {@link OverlayScanner} as if the zygote process is scanning the overlay packages
 * and parsing configuration files. The test case then runs with a non-null
 * {@link PackageProvider} as if the system server is parsing configuration files.
 *
 * This simulates what will happen on device. If an exception would be thrown in the zygote, then
 * the exception should be thrown in the first run of the test case.
 */
public class OverlayConfigIterationRule implements TestRule {

    enum Iteration {
        ZYGOTE,
        SYSTEM_SERVER,
    }

    private final ArrayMap<File, ParsedOverlayInfo> mOverlayStubResults = new ArrayMap<>();
    private Supplier<OverlayScanner> mOverlayScanner;
    private PackageProvider mPkgProvider;
    private Iteration mIteration;

    /**
     * Mocks the parsing of the file to make it appear to the scanner that the file is a valid
     * overlay APK.
     **/
    void addOverlay(File path, String packageName, String targetPackage, int targetSdkVersion,
            boolean isStatic, int priority) {
        try {
            final File canonicalPath = new File(path.getCanonicalPath());
            mOverlayStubResults.put(canonicalPath, new ParsedOverlayInfo(
                    packageName, targetPackage, targetSdkVersion, isStatic, priority,
                    canonicalPath));
        } catch (IOException e) {
            Assert.fail("Failed to add overlay " + e);
        }
    }

    void addOverlay(File path, String packageName) {
        addOverlay(path, packageName, "target");
    }

    void addOverlay(File path, String packageName, String targetPackage) {
        addOverlay(path, packageName, targetPackage, Build.VERSION_CODES.CUR_DEVELOPMENT);
    }

    void addOverlay(File path, String packageName, String targetPackage, int targetSdkVersion) {
        addOverlay(path, packageName, targetPackage, targetSdkVersion, false, 0);
    }

    /** Retrieves the {@link OverlayScanner} for the current run of the test. */
    Supplier<OverlayScanner> getScannerFactory() {
        return mOverlayScanner;
    }

    /** Retrieves the {@link PackageProvider} for the current run of the test. */
    PackageProvider getPackageProvider() {
        return mPkgProvider;
    }

    /** Retrieves the current iteration of the test. */
    Iteration getIteration() {
        return mIteration;
    }


    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Run the test once as if the zygote process is scanning the overlay packages
                // and parsing configuration files.
                mOverlayScanner = () -> {
                    OverlayScanner scanner = Mockito.spy(new OverlayScanner());
                    for (Map.Entry<File, ParsedOverlayInfo> overlay :
                            mOverlayStubResults.entrySet()) {
                        doReturn(overlay.getValue()).when(scanner)
                                .parseOverlayManifest(overlay.getKey());
                    }
                    return scanner;
                };
                mPkgProvider = null;
                mIteration = Iteration.ZYGOTE;
                base.evaluate();

                // Run the test once more (if the first test did not throw an exception) as if
                // the system server is parsing the configuration files and using PackageManager to
                // retrieving information of overlays.
                mOverlayScanner = null;
                mPkgProvider = Mockito.mock(PackageProvider.class);
                mIteration = Iteration.SYSTEM_SERVER;
                doAnswer((InvocationOnMock invocation) -> {
                    final Object[] args = invocation.getArguments();
                    final BiConsumer<ParsingPackageRead, Boolean> f =
                            (BiConsumer<ParsingPackageRead, Boolean>) args[0];
                    for (Map.Entry<File, ParsedOverlayInfo> overlay :
                            mOverlayStubResults.entrySet()) {
                        final ParsingPackageRead a = Mockito.mock(ParsingPackageRead.class);
                        final ParsedOverlayInfo info = overlay.getValue();
                        when(a.getPackageName()).thenReturn(info.packageName);
                        when(a.getOverlayTarget()).thenReturn(info.targetPackageName);
                        when(a.getTargetSdkVersion()).thenReturn(info.targetSdkVersion);
                        when(a.isOverlayIsStatic()).thenReturn(info.isStatic);
                        when(a.getOverlayPriority()).thenReturn(info.priority);
                        when(a.getBaseCodePath()).thenReturn(info.path.getPath());
                        f.accept(a, !info.path.getPath().contains("data/overlay"));
                    }
                    return null;
                }).when(mPkgProvider).forEachPackage(any());

                base.evaluate();
            }
        };
    }
}


