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
package com.google.currysrc.api.process.ast;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;
import org.eclipse.jdt.core.dom.BodyDeclaration;

/**
 * Associates a {@link BodyDeclaration} (identified by a {@link BodyDeclarationLocator}) with some
 * data, represented as a {@link Mapping}.
 *
 * @param <T> the type of data that can be associated with a {@link BodyDeclarationLocator}.
 */
public class BodyDeclarationLocatorStore<T> {

  private final Map<BodyDeclarationLocator, T> locator2Data;

  public BodyDeclarationLocatorStore() {
    locator2Data = new LinkedHashMap<>();
  }

  /**
   * Adds a mapping between a {@link BodyDeclarationLocator} and some data.
   *
   * <p>If two of more {@link BodyDeclarationLocator} instances are added that will match the
   * same {@link BodyDeclaration} then the first one added will take precedence.
   *
   * <p>It is an error to add the same {@link BodyDeclarationLocator} more than once.
   */
  public void add(BodyDeclarationLocator locator, T data) {
    T existing = locator2Data.putIfAbsent(locator, data);
    if (existing != null) {
      throw new IllegalStateException(
          "Locator " + locator + " already has existing mapping: " + existing);
    }
  }

  /**
   * Finds the data associated with the supplied {@link BodyDeclaration}.
   *
   * <p>This iterates through the list of mappings (in insertion order). For each mapping the
   * {@link BodyDeclarationLocator} is checked to see if it matches the {@code declaration}. If it
   * does match then the associated data is returned, otherwise the next mapping is checked. If no
   * mappings matched then null is returned.
   *
   * @param declaration the {@link BodyDeclaration} whose associated data is required.
   * @return null if there is no data associated with the supplied {@link BodyDeclaration},
   * otherwise the data provided with the matching {@link BodyDeclarationLocator}.
   */
  public T find(BodyDeclaration declaration) {
    for (Entry<BodyDeclarationLocator, T> entry : locator2Data.entrySet()) {
      if (entry.getKey().matches(declaration)) {
        return entry.getValue();
      }
    }

    return null;
  }

  public static class Mapping<T> {
    private final BodyDeclarationLocator locator;
    private final T value;

    private Mapping(BodyDeclarationLocator locator, T value) {
      this.locator = locator;
      this.value = value;
    }

    public BodyDeclarationLocator getLocator() {
      return locator;
    }

    public T getValue() {
      return value;
    }
  }

  /**
   * Finds the mapping associated with the supplied {@link BodyDeclaration}.
   *
   * <p>This iterates through the list of mappings (in insertion order). For each mapping the
   * {@link BodyDeclarationLocator} is checked to see if it matches the {@code declaration}. If it
   * does match then the mapping is returned, otherwise the next mapping is checked. If no mappings
   * matched then null is returned.
   *
   * @param declaration the {@link BodyDeclaration} whose associated data is required.
   * @return null if there is no mapping associated with the supplied {@link BodyDeclaration},
   * otherwise the mapping provided with the matching {@link BodyDeclarationLocator}.
   */
  public Mapping<T> findMapping(BodyDeclaration declaration) {
    for (Entry<BodyDeclarationLocator, T> entry : locator2Data.entrySet()) {
      if (entry.getKey().matches(declaration)) {
        return new Mapping<>(entry.getKey(), entry.getValue());
      }
    }

    return null;
  }
}
