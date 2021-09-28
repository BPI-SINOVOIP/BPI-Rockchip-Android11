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

package com.android.jarjar;

import com.tonicsystems.jarjar.util.JarTransformer;

import org.objectweb.asm.AnnotationVisitor;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.commons.Remapper;

import java.util.Set;
import java.util.function.Supplier;

/**
 * A transformer that removes annotations from repackaged classes.
 */
public final class RemoveAndroidCompatAnnotationsJarTransformer extends JarTransformer {

    private static final Set<String> REMOVE_ANNOTATIONS = Set.of(
            "Landroid/compat/annotation/UnsupportedAppUsage;");

    private final Remapper remapper;

    public RemoveAndroidCompatAnnotationsJarTransformer(Remapper remapper) {
        this.remapper = remapper;
    }

    protected ClassVisitor transform(ClassVisitor classVisitor) {
        return new AnnotationRemover(classVisitor);
    }

    private class AnnotationRemover extends ClassVisitor {

        private boolean isClassRemapped;

        AnnotationRemover(ClassVisitor cv) {
            super(Opcodes.ASM6, cv);
        }

        @Override
        public void visit(int version, int access, String name, String signature, String superName,
                String[] interfaces) {
            String newName = remapper.map(name);
            // On every new class header visit, remember whether the class is repackaged.
            isClassRemapped = newName != null && !newName.equals(name);
            super.visit(version, access, name, signature, superName, interfaces);
        }

        @Override
        public AnnotationVisitor visitAnnotation(String descriptor, boolean visible) {
            return visitAnnotationCommon(descriptor,
                    () -> super.visitAnnotation(descriptor, visible));
        }

        @Override
        public FieldVisitor visitField(int access, String name, String descriptor, String signature,
                Object value) {
            FieldVisitor superVisitor =
                    super.visitField(access, name, descriptor, signature, value);
            return new FieldVisitor(Opcodes.ASM6, superVisitor) {
                @Override
                public AnnotationVisitor visitAnnotation(String descriptor, boolean visible) {
                    return visitAnnotationCommon(descriptor,
                            () -> super.visitAnnotation(descriptor, visible));

                }
            };
        }

        @Override
        public MethodVisitor visitMethod(int access, String name, String descriptor,
                String signature, String[] exceptions) {
            MethodVisitor superVisitor =
                    super.visitMethod(access, name, descriptor, signature, exceptions);
            return new MethodVisitor(Opcodes.ASM6, superVisitor) {
                @Override
                public AnnotationVisitor visitAnnotation(String descriptor, boolean visible) {
                    return visitAnnotationCommon(descriptor,
                            () -> super.visitAnnotation(descriptor, visible));
                }
            };
        }

        /**
         * Create an {@link AnnotationVisitor} that removes any annotations from {@link
         * #REMOVE_ANNOTATIONS} if the class is being repackaged.
         *
         * <p>For the annotations to be dropped correctly, do not visit the annotation beforehand,
         * provide a supplier instead.
         */
        private AnnotationVisitor visitAnnotationCommon(String annotation,
                Supplier<AnnotationVisitor> defaultVisitorSupplier) {
            if (isClassRemapped && REMOVE_ANNOTATIONS.contains(annotation)) {
                return null;
            }
            // Only get() the default AnnotationVisitor if the annotation is to be included.
            // Invoking super.visitAnnotation(descriptor, visible) causes the annotation to be
            // included in the output even if the resulting AnnotationVisitor is not returned or
            // used.
            return defaultVisitorSupplier.get();
        }
    }
}
