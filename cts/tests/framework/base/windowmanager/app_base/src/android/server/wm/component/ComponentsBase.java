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

package android.server.wm.component;

import android.content.ComponentName;

/**
 * Base class for Components constants holding class.
 *
 * Every testing APK should have Components class in Java package that equals to package of APK.
 * Those Components class should extends {@link ComponentsBase}.
 */
public class ComponentsBase {

    /**
     * Build {@link ComponentName} constant which belongs to {@code componentsClass}'s package.
     * @param componentsClass Components class object which has the same package of APK.
     * @param className simple class name (has no '.') or fully qualified class name.
     * @return {@link ComponentName} object.
     */
    protected static ComponentName component(
            Class<? extends ComponentsBase> componentsClass, String className) {
        if (className.startsWith(".")) {
            throw new AssertionError("Class name should not start with '.'");
        }
        final String packageName = getPackageName(componentsClass);
        final boolean isSimpleClassName = className.indexOf('.') < 0;
        final String fullClassName = isSimpleClassName ? packageName + "." + className : className;
        return new ComponentName(packageName, fullClassName);
    }

    /**
     * Get package name of {@code componentsClass}.
     * @param componentsClass Components class object which has the same package of APK.
     * @return package name of APK.
     */
    protected static String getPackageName(Class<? extends ComponentsBase> componentsClass) {
        if (!"Components".equals(componentsClass.getSimpleName())) {
            throw new AssertionError("The class name must be 'Components'");
        }
        return componentsClass.getPackage().getName();
    }
}
