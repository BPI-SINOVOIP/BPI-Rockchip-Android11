/*
 * Copyright (C) 2017 The Android Open Source Project
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
package android.signature.cts;

import java.lang.annotation.Annotation;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Checks that the runtime representation of a class matches the API representation of a class.
 */
public class AnnotationChecker extends AbstractApiChecker {

    private final String annotationSpec;

    private final Map<String, Class<?>> annotatedClassesMap = new HashMap<>();
    private final Map<String, Set<Constructor<?>>> annotatedConstructorsMap = new HashMap<>();
    private final Map<String, Set<Method>> annotatedMethodsMap = new HashMap<>();
    private final Map<String, Set<Field>> annotatedFieldsMap = new HashMap<>();

    /**
     * @param annotationName name of the annotation class for the API type (e.g.
     *      android.annotation.SystemApi)
     */
    public AnnotationChecker(
            ResultObserver resultObserver, ClassProvider classProvider, String annotationSpec) {
        super(classProvider, resultObserver);

        this.annotationSpec = annotationSpec;
        classProvider.getAllClasses().forEach(clazz -> {
            if (ReflectionHelper.hasMatchingAnnotation(clazz, annotationSpec)) {
                annotatedClassesMap.put(clazz.getName(), clazz);
            }
            Set<Constructor<?>> constructors = ReflectionHelper.getAnnotatedConstructors(clazz,
                    annotationSpec);
            if (!constructors.isEmpty()) {
                annotatedConstructorsMap.put(clazz.getName(), constructors);
            }
            Set<Method> methods = ReflectionHelper.getAnnotatedMethods(clazz, annotationSpec);
            if (!methods.isEmpty()) {
                annotatedMethodsMap.put(clazz.getName(), methods);
            }
            Set<Field> fields = ReflectionHelper.getAnnotatedFields(clazz, annotationSpec);
            if (!fields.isEmpty()) {
                annotatedFieldsMap.put(clazz.getName(), fields);
            }
        });
    }

    @Override
    public void checkDeferred() {
        for (Class<?> clazz : annotatedClassesMap.values()) {
            resultObserver.notifyFailure(FailureType.EXTRA_CLASS, clazz.getName(),
                    "Class annotated with " + annotationSpec
                            + " does not exist in the documented API");
        }
        for (Set<Constructor<?>> set : annotatedConstructorsMap.values()) {
            for (Constructor<?> c : set) {
                resultObserver.notifyFailure(FailureType.EXTRA_METHOD, c.toString(),
                        "Constructor annotated with " + annotationSpec
                                + " does not exist in the API");
            }
        }
        for (Set<Method> set : annotatedMethodsMap.values()) {
            for (Method m : set) {
                resultObserver.notifyFailure(FailureType.EXTRA_METHOD, m.toString(),
                        "Method annotated with " + annotationSpec
                                + " does not exist in the API");
            }
        }
        for (Set<Field> set : annotatedFieldsMap.values()) {
            for (Field f : set) {
                resultObserver.notifyFailure(FailureType.EXTRA_FIELD, f.toString(),
                        "Field annotated with " + annotationSpec
                                + " does not exist in the API");
            }
        }
    }

    @Override
    protected boolean allowMissingClass(JDiffClassDescription classDescription) {
        // A class that exist in the API document is not found in the runtime.
        // This can happen for classes that are optional (e.g. classes for
        // Android Auto). This, however, should not be considered as a test
        // failure, because the purpose of this test is to ensure that every
        // runtime classes found in the device have more annotations than
        // the documented.
        return true;
    }

    @Override
    protected boolean checkClass(JDiffClassDescription classDescription, Class<?> runtimeClass) {
        // remove the class from the set if found
        annotatedClassesMap.remove(runtimeClass.getName());
        return true;
    }

    @Override
    protected void checkField(JDiffClassDescription classDescription, Class<?> runtimeClass,
            JDiffClassDescription.JDiffField fieldDescription, Field field) {
        // make sure that the field (or its declaring class) is annotated
        if (!ReflectionHelper.isAnnotatedOrInAnnotatedClass(field, annotationSpec)) {
            resultObserver.notifyFailure(FailureType.MISSING_ANNOTATION,
                    field.toString(),
                    "Annotation " + annotationSpec + " is missing");
        }

        // remove it from the set if found in the API doc
        Set<Field> annotatedFields = annotatedFieldsMap.get(runtimeClass.getName());
        if (annotatedFields != null) {
            annotatedFields.remove(field);
        }
    }

    @Override
    protected void checkConstructor(JDiffClassDescription classDescription, Class<?> runtimeClass,
            JDiffClassDescription.JDiffConstructor ctorDescription, Constructor<?> ctor) {
        Set<Constructor<?>> annotatedConstructors = annotatedConstructorsMap
                .get(runtimeClass.getName());

        // make sure that the constructor (or its declaring class) is annotated
        if (annotationSpec != null) {
            if (!ReflectionHelper.isAnnotatedOrInAnnotatedClass(ctor, annotationSpec)) {
                resultObserver.notifyFailure(FailureType.MISSING_ANNOTATION,
                        ctor.toString(),
                        "Annotation " + annotationSpec + " is missing");
            }
        }

        // remove it from the set if found in the API doc
        if (annotatedConstructors != null) {
            annotatedConstructors.remove(ctor);
        }
    }

    @Override
    protected void checkMethod(JDiffClassDescription classDescription, Class<?> runtimeClass,
            JDiffClassDescription.JDiffMethod methodDescription, Method method) {
        // make sure that the method (or its declaring class) is annotated or overriding
        // annotated method.
        if (!ReflectionHelper.isAnnotatedOrInAnnotatedClass(method, annotationSpec)
                && !ReflectionHelper.isOverridingAnnotatedMethod(method,
                        annotationSpec)) {
            resultObserver.notifyFailure(FailureType.MISSING_ANNOTATION,
                    method.toString(),
                    "Annotation " + annotationSpec + " is missing");
        }

        // remove it from the set if found in the API doc
        Set<Method> annotatedMethods = annotatedMethodsMap.get(runtimeClass.getName());
        if (annotatedMethods != null) {
            annotatedMethods.remove(method);
        }
    }
}
