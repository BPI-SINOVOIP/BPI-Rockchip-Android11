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
package com.google.currysrc.processors;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Information needed to create an annotation.
 */
public class AnnotationInfo {

  private final AnnotationClass annotationClass;
  private final Map<String, Object> properties;

  public AnnotationInfo(AnnotationClass annotationClass,
      Map<String, Object> properties) {
    this.annotationClass = annotationClass;
    this.properties = properties;
  }

  public String getQualifiedName() {
    return annotationClass.name;
  }

  public Map<String, Object> getProperties() {
    return properties;
  }

  @Override
  public String toString() {
    return "AnnotationInfo{" +
        "annotationClass=" + annotationClass +
        ", properties=" + properties +
        '}';
  }

  /**
   * Expected type of an annotation's properties.
   */
  public static class AnnotationClass {

    private final String name;
    private final Map<String, Class<?>> propertyTypes;

    public AnnotationClass(String name) {
      this.name = name;
      propertyTypes = new LinkedHashMap<>();
    }

    public AnnotationClass addProperty(String name, Class<?> type) {
      propertyTypes.put(name, type);
      return this;
    }

    public String getName() {
      return name;
    }

    public Class<?> getPropertyType(String propertyName) {
      Class<?> type = propertyTypes.get(propertyName);
      if (type == null) {
        throw new IllegalStateException(
            String.format("Unknown property: %s in %s", propertyName, name));
      }
      return type;
    }
  }

  /**
   * A placeholder value for an annotation property.
   *
   * <p>Allows an annotation value to be an arbitrary expression.
   */
  public static class Placeholder {
    private final String text;

    public Placeholder(String text) {
      this.text = text;
    }

    public String getText() {
      return text;
    }
  }
}
