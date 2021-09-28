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
import dalvik.system.PathClassLoader;
import java.io.File;

// AppComponentFactory which sets up PathClassLoader as the main class loader
// of the process.
public class PathClassLoaderFactory extends AppComponentFactory {
    @Override
    public ClassLoader instantiateClassLoader(ClassLoader defaultCL, ApplicationInfo aInfo) {
        try {
            File apkFile = AppComponentFactoryTest.writeSecondaryApkToDisk(aInfo);
            return new PathClassLoader(apkFile.getAbsolutePath(), defaultCL);
        } catch (Exception ex) {
            throw new RuntimeException(ex);
        }
    }
}
