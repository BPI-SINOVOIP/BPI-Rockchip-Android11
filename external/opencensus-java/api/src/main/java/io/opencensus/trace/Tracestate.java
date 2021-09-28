/*
 * Copyright 2018, OpenCensus Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.opencensus.trace;

import com.google.auto.value.AutoValue;
import io.opencensus.common.ExperimentalApi;
import io.opencensus.internal.Utils;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import javax.annotation.concurrent.Immutable;

/**
 * Carries tracing-system specific context in a list of key-value pairs. TraceState allows different
 * vendors propagate additional information and inter-operate with their legacy Id formats.
 *
 * <p>Implementation is optimized for a small list of key-value pairs.
 *
 * <p>Key is opaque string up to 256 characters printable. It MUST begin with a lowercase letter,
 * and can only contain lowercase letters a-z, digits 0-9, underscores _, dashes -, asterisks *, and
 * forward slashes /.
 *
 * <p>Value is opaque string up to 256 characters printable ASCII RFC0020 characters (i.e., the
 * range 0x20 to 0x7E) except comma , and =.
 *
 * @since 0.16
 */
@Immutable
@AutoValue
@ExperimentalApi
public abstract class Tracestate {
  private static final int KEY_MAX_SIZE = 256;
  private static final int VALUE_MAX_SIZE = 256;
  private static final int MAX_KEY_VALUE_PAIRS = 32;

  /**
   * Returns the value to which the specified key is mapped, or null if this map contains no mapping
   * for the key.
   *
   * @param key with which the specified value is to be associated
   * @return the value to which the specified key is mapped, or null if this map contains no mapping
   *     for the key.
   * @since 0.16
   */
  @javax.annotation.Nullable
  public String get(String key) {
    for (Entry entry : getEntries()) {
      if (entry.getKey().equals(key)) {
        return entry.getValue();
      }
    }
    return null;
  }

  /**
   * Returns a {@link List} view of the mappings contained in this {@code TraceState}.
   *
   * @return a {@link List} view of the mappings contained in this {@code TraceState}.
   * @since 0.16
   */
  public abstract List<Entry> getEntries();

  /**
   * Returns a {@code Builder} based on an empty {@code Tracestate}.
   *
   * @return a {@code Builder} based on an empty {@code Tracestate}.
   * @since 0.16
   */
  public static Builder builder() {
    return new Builder(Builder.EMPTY);
  }

  /**
   * Returns a {@code Builder} based on this {@code Tracestate}.
   *
   * @return a {@code Builder} based on this {@code Tracestate}.
   * @since 0.16
   */
  public Builder toBuilder() {
    return new Builder(this);
  }

  /**
   * Builder class for {@link MessageEvent}.
   *
   * @since 0.16
   */
  @ExperimentalApi
  public static final class Builder {
    private final Tracestate parent;
    @javax.annotation.Nullable private ArrayList<Entry> entries;

    // Needs to be in this class to avoid initialization deadlock because super class depends on
    // subclass (the auto-value generate class).
    private static final Tracestate EMPTY = create(Collections.<Entry>emptyList());

    private Builder(Tracestate parent) {
      Utils.checkNotNull(parent, "parent");
      this.parent = parent;
      this.entries = null;
    }

    /**
     * Adds or updates the {@code Entry} that has the given {@code key} if it is present. The new
     * {@code Entry} will always be added in the front of the list of entries.
     *
     * @param key the key for the {@code Entry} to be added.
     * @param value the value for the {@code Entry} to be added.
     * @return this.
     * @since 0.16
     */
    @SuppressWarnings("nullness")
    public Builder set(String key, String value) {
      // Initially create the Entry to validate input.
      Entry entry = Entry.create(key, value);
      if (entries == null) {
        // Copy entries from the parent.
        entries = new ArrayList<Entry>(parent.getEntries());
      }
      for (int i = 0; i < entries.size(); i++) {
        if (entries.get(i).getKey().equals(entry.getKey())) {
          entries.remove(i);
          // Exit now because the entries list cannot contain duplicates.
          break;
        }
      }
      // Inserts the element at the front of this list.
      entries.add(0, entry);
      return this;
    }

    /**
     * Removes the {@code Entry} that has the given {@code key} if it is present.
     *
     * @param key the key for the {@code Entry} to be removed.
     * @return this.
     * @since 0.16
     */
    @SuppressWarnings("nullness")
    public Builder remove(String key) {
      Utils.checkNotNull(key, "key");
      if (entries == null) {
        // Copy entries from the parent.
        entries = new ArrayList<Entry>(parent.getEntries());
      }
      for (int i = 0; i < entries.size(); i++) {
        if (entries.get(i).getKey().equals(key)) {
          entries.remove(i);
          // Exit now because the entries list cannot contain duplicates.
          break;
        }
      }
      return this;
    }

    /**
     * Builds a TraceState by adding the entries to the parent in front of the key-value pairs list
     * and removing duplicate entries.
     *
     * @return a TraceState with the new entries.
     * @since 0.16
     */
    public Tracestate build() {
      if (entries == null) {
        return parent;
      }
      return Tracestate.create(entries);
    }
  }

  /**
   * Immutable key-value pair for {@code Tracestate}.
   *
   * @since 0.16
   */
  @Immutable
  @AutoValue
  @ExperimentalApi
  public abstract static class Entry {
    /**
     * Creates a new {@code Entry} for the {@code Tracestate}.
     *
     * @param key the Entry's key.
     * @param value the Entry's value.
     * @since 0.16
     */
    public static Entry create(String key, String value) {
      Utils.checkNotNull(key, "key");
      Utils.checkNotNull(value, "value");
      Utils.checkArgument(validateKey(key), "Invalid key %s", key);
      Utils.checkArgument(validateValue(value), "Invalid value %s", value);
      return new AutoValue_Tracestate_Entry(key, value);
    }

    /**
     * Returns the key {@code String}.
     *
     * @return the key {@code String}.
     * @since 0.16
     */
    public abstract String getKey();

    /**
     * Returns the value {@code String}.
     *
     * @return the value {@code String}.
     * @since 0.16
     */
    public abstract String getValue();

    Entry() {}
  }

  // Key is opaque string up to 256 characters printable. It MUST begin with a lowercase letter, and
  // can only contain lowercase letters a-z, digits 0-9, underscores _, dashes -, asterisks *, and
  // forward slashes /.
  private static boolean validateKey(String key) {
    if (key.length() > KEY_MAX_SIZE
        || key.isEmpty()
        || key.charAt(0) < 'a'
        || key.charAt(0) > 'z') {
      return false;
    }
    for (int i = 1; i < key.length(); i++) {
      char c = key.charAt(i);
      if (!(c >= 'a' && c <= 'z')
          && !(c >= '0' && c <= '9')
          && c != '_'
          && c != '-'
          && c != '*'
          && c != '/') {
        return false;
      }
    }
    return true;
  }

  // Value is opaque string up to 256 characters printable ASCII RFC0020 characters (i.e., the range
  // 0x20 to 0x7E) except comma , and =.
  private static boolean validateValue(String value) {
    if (value.length() > VALUE_MAX_SIZE || value.charAt(value.length() - 1) == ' ' /* '\u0020' */) {
      return false;
    }
    for (int i = 0; i < value.length(); i++) {
      char c = value.charAt(i);
      if (c == ',' || c == '=' || c < ' ' /* '\u0020' */ || c > '~' /* '\u007E' */) {
        return false;
      }
    }
    return true;
  }

  private static Tracestate create(List<Entry> entries) {
    Utils.checkState(entries.size() <= MAX_KEY_VALUE_PAIRS, "Invalid size");
    return new AutoValue_Tracestate(Collections.unmodifiableList(entries));
  }

  Tracestate() {}
}
