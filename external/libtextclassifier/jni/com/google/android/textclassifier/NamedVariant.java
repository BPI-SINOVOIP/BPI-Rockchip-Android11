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

package com.google.android.textclassifier;

/**
 * Represents a union of different basic types.
 *
 * @hide
 */
public final class NamedVariant {
  public static final int TYPE_EMPTY = 0;
  public static final int TYPE_INT = 1;
  public static final int TYPE_LONG = 2;
  public static final int TYPE_FLOAT = 3;
  public static final int TYPE_DOUBLE = 4;
  public static final int TYPE_BOOL = 5;
  public static final int TYPE_STRING = 6;
  public static final int TYPE_STRING_ARRAY = 7;
  public static final int TYPE_FLOAT_ARRAY = 8;
  public static final int TYPE_INT_ARRAY = 9;
  public static final int TYPE_NAMED_VARIANT_ARRAY = 10;

  private final String name;
  private final int type;
  private int intValue;
  private long longValue;
  private float floatValue;
  private double doubleValue;
  private boolean boolValue;
  private String stringValue;
  private String[] stringArrValue;
  private float[] floatArrValue;
  private int[] intArrValue;
  private NamedVariant[] namedVariantArray;

  public NamedVariant(String name, int value) {
    this.name = name;
    this.intValue = value;
    this.type = TYPE_INT;
  }

  public NamedVariant(String name, long value) {
    this.name = name;
    this.longValue = value;
    this.type = TYPE_LONG;
  }

  public NamedVariant(String name, float value) {
    this.name = name;
    this.floatValue = value;
    this.type = TYPE_FLOAT;
  }

  public NamedVariant(String name, double value) {
    this.name = name;
    this.doubleValue = value;
    this.type = TYPE_DOUBLE;
  }

  public NamedVariant(String name, boolean value) {
    this.name = name;
    this.boolValue = value;
    this.type = TYPE_BOOL;
  }

  public NamedVariant(String name, String value) {
    this.name = name;
    this.stringValue = value;
    this.type = TYPE_STRING;
  }

  public NamedVariant(String name, String[] value) {
    this.name = name;
    this.stringArrValue = value;
    this.type = TYPE_STRING_ARRAY;
  }

  public NamedVariant(String name, float[] value) {
    this.name = name;
    this.floatArrValue = value;
    this.type = TYPE_FLOAT_ARRAY;
  }

  public NamedVariant(String name, int[] value) {
    this.name = name;
    this.intArrValue = value;
    this.type = TYPE_INT_ARRAY;
  }

  public NamedVariant(String name, NamedVariant[] value) {
    this.name = name;
    this.namedVariantArray = value;
    this.type = TYPE_NAMED_VARIANT_ARRAY;
  }

  public String getName() {
    return name;
  }

  public int getType() {
    return type;
  }

  public int getInt() {
    assert (type == TYPE_INT);
    return intValue;
  }

  public long getLong() {
    assert (type == TYPE_LONG);
    return longValue;
  }

  public float getFloat() {
    assert (type == TYPE_FLOAT);
    return floatValue;
  }

  public double getDouble() {
    assert (type == TYPE_DOUBLE);
    return doubleValue;
  }

  public boolean getBool() {
    assert (type == TYPE_BOOL);
    return boolValue;
  }

  public String getString() {
    assert (type == TYPE_STRING);
    return stringValue;
  }

  public String[] getStringArray() {
    assert (type == TYPE_STRING_ARRAY);
    return stringArrValue;
  }

  public float[] getFloatArray() {
    assert (type == TYPE_FLOAT_ARRAY);
    return floatArrValue;
  }

  public int[] getIntArray() {
    assert (type == TYPE_INT_ARRAY);
    return intArrValue;
  }

  public NamedVariant[] getNamedVariantArray() {
    assert (type == TYPE_NAMED_VARIANT_ARRAY);
    return namedVariantArray;
  }
}
