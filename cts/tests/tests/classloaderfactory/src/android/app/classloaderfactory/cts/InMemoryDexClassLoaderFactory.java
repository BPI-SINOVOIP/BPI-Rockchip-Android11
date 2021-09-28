/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package android.app.classloaderfactory.cts;

import android.app.AppComponentFactory;
import android.content.pm.ApplicationInfo;
import dalvik.system.InMemoryDexClassLoader;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

// AppComponentFactory which sets up InMemoryDexClassLoader as the main class
// loader of the process.
public class InMemoryDexClassLoaderFactory extends AppComponentFactory {
    @Override
    public ClassLoader instantiateClassLoader(ClassLoader defaultCL, ApplicationInfo aInfo) {
        try {
            // InMemoryDexClassLoader will not load a zip file. Must extract
            // dex files into ByteBuffers.
            ZipFile zipFile = new ZipFile(AppComponentFactoryTest.writeSecondaryApkToDisk(aInfo));

            ArrayList<ByteBuffer> dexFiles = new ArrayList<>();
            for (int dexId = 0;; dexId++) {
                String zipEntryName = "classes" + (dexId == 0 ? "" : dexId) + ".dex";
                ZipEntry zipEntry = zipFile.getEntry(zipEntryName);
                if (zipEntry == null) {
                    break;
                }

                final int zipEntrySize = (int) zipEntry.getSize();
                ByteBuffer buf = ByteBuffer.allocate(zipEntrySize);
                InputStream zipIS = zipFile.getInputStream(zipEntry);

                for (int pos = 0; pos < zipEntrySize;) {
                    int res = zipIS.read(buf.array(), pos, zipEntrySize);
                    if (res < 0) {
                      throw new IllegalStateException("Stream is closed");
                    }
                    pos += res;
                    if (pos > zipEntrySize) {
                      throw new IllegalStateException("Buffer overflow");
                    }
                }

                dexFiles.add(buf);
            }

            ByteBuffer[] dexFilesArray = new ByteBuffer[dexFiles.size()];
            dexFiles.toArray(dexFilesArray);

            // Construct InMemoryDexClassLoader, make it a child of the default
            // class loader.
            return new InMemoryDexClassLoader(dexFilesArray, defaultCL);
        } catch (Exception ex) {
            throw new RuntimeException(ex);
        }
    }
}
