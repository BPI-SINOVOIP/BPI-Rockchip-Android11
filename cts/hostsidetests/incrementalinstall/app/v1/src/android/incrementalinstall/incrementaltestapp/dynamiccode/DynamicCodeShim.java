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

package android.incrementalinstall.incrementaltestapp.dynamiccode;

public class DynamicCodeShim implements IDynamicCode {

    private static final String IMPL_CLASS_NAME =
            "android.incrementalinstall.incrementaltestapp.dynamiccode.DynamicCode";
    private IDynamicCode mImpl;

    @Override
    public String getString() throws Exception {
        if (mImpl == null) {
            Class clazz;
            try {
                clazz = Class.forName(IMPL_CLASS_NAME);
            } catch (ClassNotFoundException ex) {
                throw new Exception("Failed to load class: " + IMPL_CLASS_NAME, ex);
            }
            try {
                mImpl = (IDynamicCode) clazz.newInstance();
                return mImpl.getString();
            } catch (ClassCastException | IllegalAccessException | InstantiationException ex) {
                throw new Exception("Failed to create implementation for class: " + IMPL_CLASS_NAME,
                        ex);
            }
        }
        throw new Exception("Implementation not found for class: " + IMPL_CLASS_NAME);
    }
}
