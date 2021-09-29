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
 * limitations under the License
 */

package com.google.android.jacoco.reporter;

import org.jacoco.report.InputStreamSourceFileLocator;

import java.io.IOException;
import java.io.InputStream;
import java.util.Optional;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

/**
 * Jacoco source file locator that picks files from a jar containing .java files.
 */
public class JarSourceFileLocator extends InputStreamSourceFileLocator {

    private final JarFile mJarFile;

    protected JarSourceFileLocator(JarFile jarFile, String encoding, int tabWidth) {
        super(encoding, tabWidth);

        mJarFile = jarFile;
    }

    @Override
    protected InputStream getSourceStream(String s) throws IOException {
        Optional<JarEntry> e = mJarFile.stream().filter(it -> it.getName().endsWith(s)).findFirst();
        if (e.isPresent()) {
            return mJarFile.getInputStream(e.get());
        } else {
            return null;
        }
    }
}
