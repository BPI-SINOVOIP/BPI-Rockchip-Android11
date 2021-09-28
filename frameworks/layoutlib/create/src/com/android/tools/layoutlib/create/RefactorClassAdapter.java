/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.tools.layoutlib.create;

import java.util.Map;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.commons.ClassRemapper;
import org.objectweb.asm.commons.Remapper;

public class RefactorClassAdapter extends ClassRemapper {

    RefactorClassAdapter(ClassVisitor cv, Map<String, String> refactorClasses) {
        super(cv, new RefactorRemapper(refactorClasses));
    }

    private static class RefactorRemapper extends Remapper {
        private final Map<String, String> mRefactorClasses;

        private RefactorRemapper(Map<String, String> refactorClasses) {
            mRefactorClasses = refactorClasses;
        }

        @Override
        public String map(String typeName) {
            String newName = mRefactorClasses.get(typeName);
            if (newName != null) {
                return newName;
            }
            int pos = typeName.indexOf('$');
            if (pos > 0) {
                newName = mRefactorClasses.get(typeName.substring(0, pos));
                if (newName != null) {
                    return newName + typeName.substring(pos);
                }
            }
            return null;
        }
    }
}
