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
package com.android.tradefed.lite;

import junit.framework.TestCase;

import org.junit.runners.Suite.SuiteClasses;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.stream.Stream;

/**
 * Implements some useful utility methods for running host tests.
 *
 * <p>This implements a few methods for finding tests on the host and faking execution of JUnit
 * tests so we can "dry run" them.
 */
public final class HostUtils {

    private HostUtils() {}

    /**
     * Gets JUnit4 test cases from provided classnames and jar paths.
     *
     * @param classNames Classes that exist in the current class path to check for JUnit tests
     * @param jarAbsPaths Jars to search for classes with the test annotations.
     * @return a list of class objects that are test classes to execute.
     * @throws IllegalArgumentException
     */
    public static final List<Class<?>> getJUnitClasses(
            Set<String> classNames,
            Set<String> jarAbsPaths,
            List<String> excludePaths,
            ClassLoader pcl)
            throws IllegalArgumentException {
        Set<String> outputNames = new HashSet<String>();
        List<Class<?>> output = new ArrayList<>();

        for (String className : classNames) {
            if (outputNames.contains(className)) {
                continue;
            }
            try {
                Class<?> klass = Class.forName(className, true, pcl);
                outputNames.add(className);
                output.add(klass);
            } catch (ClassNotFoundException e) {
                throw new IllegalArgumentException(
                        String.format("Could not load Test class %s", className), e);
            }
        }

        for (String jarName : jarAbsPaths) {
            JarFile jarFile = null;
            try {
                File file = new File(jarName);
                jarFile = new JarFile(file);
                Stream<JarEntry> s = jarFile.stream();
                URL[] urls = {new URL(String.format("jar:file:%s!/", file.getAbsolutePath()))};
                URLClassLoader cl = URLClassLoader.newInstance(urls);

                // This first stage of filtering makes sure that the jar entries are somewhat valid;
                // this includes not being a directory, not being a java language class,
                // not being a class that we've already seen, etc.
                s =
                        s.filter(
                                je ->
                                        (!je.isDirectory()
                                                && je.getName().endsWith(".class")
                                                && !je.getName().contains("$")
                                                && !outputNames.contains(
                                                        getClassName(je.getName()))));

                if (!excludePaths.isEmpty()) {
                    s =
                            s.filter(
                                    je -> {
                                        return excludePaths
                                                .stream()
                                                .noneMatch(path -> je.getName().startsWith(path));
                                    });
                }

                s.map(je -> HostUtils.getClassName(je.getName()))
                        .filter(className -> HostUtils.testLoadClass(className, cl, jarName))
                        .peek(className -> outputNames.add(className))
                        .map(
                                className -> {
                                    try {
                                        return cl.loadClass(className);
                                    } catch (ClassNotFoundException e) {
                                        // Not possible because we have already loaded this
                                        // class before.
                                        throw new IllegalArgumentException(
                                                String.format(
                                                        "Cannot find test class %s", className));
                                    }
                                })
                        .sorted(
                                Comparator.comparing(
                                        c ->
                                                c.getProtectionDomain()
                                                        .getCodeSource()
                                                        .getLocation()
                                                        .toString()))
                        .forEachOrdered(cls -> output.add(cls));
            } catch (IOException e) {
                throw new IllegalArgumentException(e);
            } finally {
                try {
                    if (jarFile != null) {
                        jarFile.close();
                    }
                } catch (IOException e) {
                    throw new RuntimeException(
                            String.format("Something went wrong closing the jarfile: " + jarName));
                }
            }
        }
        return output;
    }

    public static final List<Class<?>> getJUnitClasses(
            Set<String> classNames, Set<String> jarAbsPaths, ClassLoader pcl)
            throws IllegalArgumentException {
        return HostUtils.getJUnitClasses(classNames, jarAbsPaths, new ArrayList<>(), pcl);
    }

    /**
     * Tests whether the class is a suitable test class or not.
     *
     * <p>In this case, suitable means it is a valid JUnit test class using one of the standard
     * runners or a subclass thereof. The class should also load, obviously.
     *
     * @param className
     * @param cl
     * @param jarName
     * @return true if we should consider this class a test class, false otherwise
     */
    public static boolean testLoadClass(String className, URLClassLoader cl, String jarName)
            throws IllegalArgumentException {
        try {
            Class<?> cls = cl.loadClass(className);
            int modifiers = cls.getModifiers();
            if (HostUtils.hasJUnitAnnotation(cls)
                    && !Modifier.isStatic(modifiers)
                    && !Modifier.isPrivate(modifiers)
                    && !Modifier.isProtected(modifiers)
                    && !Modifier.isInterface(modifiers)
                    && !Modifier.isAbstract(modifiers)) {
                return true;
            }
        } catch (UnsupportedClassVersionError ucve) {
            throw new IllegalArgumentException(
                    String.format(
                            "Could not load class %s from jar %s. Reason:\n%s",
                            className, jarName, ucve.toString()));
        } catch (ClassNotFoundException cnfe) {
            throw new IllegalArgumentException(
                    String.format("Cannot find test class %s", className), cnfe);
        } catch (IllegalAccessError | NoClassDefFoundError err) {
            // IllegalAccessError can happen when the class or one of its super
            // class/interfaces are package-private. We can't load such class from
            // here (= outside of the package). Since our intention is not to load
            // all classes in the jar, but to find our the main test classes, this
            // can be safely skipped.
            // NoClassDefFoundError is also okay because certain CTS test cases
            // might statically link to a jar library (e.g. tools.jar from JDK)
            // where certain internal classes in the library are referencing
            // classes that are not available in the jar. Again, since our goal here
            // is to find test classes, this can be safely skipped.
            return false;
        } catch (SecurityException e) {
            throw new IllegalArgumentException(
                    String.format("Tried to load %s from jar %s , but failed", className, jarName),
                    e);
        }
        return false;
    }

    /**
     * Checks whether a class looks like a JUnit test or not.
     *
     * @param classObj Class to examine for the annotation
     * @return whether the class object has the JUnit4 test annotation
     */
    public static boolean hasJUnitAnnotation(Class<?> classObj) {
        if (classObj.isAnnotationPresent(org.junit.runner.RunWith.class)
                && org.junit.runner.Runner.class.isAssignableFrom(
                        classObj.getAnnotation(org.junit.runner.RunWith.class).value())) {
            return true;
        } else if (!classObj.isAnnotationPresent(org.junit.runner.RunWith.class)) {
            if (classObj.isAnnotationPresent(SuiteClasses.class)) {
                return true;
            }
            for (Method m : classObj.getMethods()) {
                if (m.isAnnotationPresent(org.junit.Test.class)) {
                    return true;
                }
            }
            if (TestCase.class.isAssignableFrom(classObj)) {
                return Arrays.asList(classObj.getDeclaredMethods())
                        .stream()
                        .anyMatch(
                                m -> {
                                    return m.getName().startsWith("test")
                                            && Arrays.asList(m.getAnnotations())
                                                    .stream()
                                                    .map(anno -> anno.toString())
                                                    .noneMatch(s -> s.contains("Suppress"));
                                });
            }
            return false;
        } else {
            return false;
        }
    }

    /**
     * Removes the ".class" file extension from a filename to get the classname.
     *
     * @param name The filename from which to strip the extension
     * @return The name of the class contained in the file
     */
    private static String getClassName(String name) {
        // -6 because of .class
        return name.substring(0, name.length() - 6).replace('/', '.');
    }
}
