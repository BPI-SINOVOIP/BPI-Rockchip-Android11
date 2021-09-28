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

/**
 * Tests for SAD (sum of absolute differences).
 */
public class SadByte {

  /// CHECK-START: int SadByte.sad1(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int SadByte.sad1(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:i\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad1(byte x, byte y) {
    return x >= y ? x - y : y - x;
  }

  /// CHECK-START: int SadByte.sad2(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int SadByte.sad2(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:i\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad2(byte x, byte y) {
    int diff = x - y;
    if (diff < 0) diff = -diff;
    return diff;
  }

  /// CHECK-START: int SadByte.sad3(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int SadByte.sad3(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:i\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3(byte x, byte y) {
    int diff = x - y;
    return diff >= 0 ? diff : -diff;
  }

  /// CHECK-START: int SadByte.sad3Alt(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int SadByte.sad3Alt(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:i\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3Alt(byte x, byte y) {
    int diff = x - y;
    return 0 <= diff ? diff : -diff;
  }

  /// CHECK-START: long SadByte.sadL1(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long SadByte.sadL1(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:j\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL1(byte x, byte y) {
    long xl = x;
    long yl = y;
    return xl >= yl ? xl - yl : yl - xl;
  }

  /// CHECK-START: long SadByte.sadL2(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long SadByte.sadL2(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:j\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL2(byte x, byte y) {
    long diff = x - y;
    if (diff < 0L) diff = -diff;
    return diff;
  }

  /// CHECK-START: long SadByte.sadL3(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long SadByte.sadL3(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:j\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3(byte x, byte y) {
    long diff = x - y;
    return diff >= 0L ? diff : -diff;
  }

  /// CHECK-START: long SadByte.sadL3Alt(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long SadByte.sadL3Alt(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Intrin:j\d+>> Abs
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3Alt(byte x, byte y) {
    long diff = x - y;
    return 0L <= diff ? diff : -diff;
  }

  public static void main() {
    // Use cross-values to test all cases.
    int n = 256;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        byte x = (byte) i;
        byte y = (byte) j;
        int e = Math.abs(x - y);
        expectEquals(e, sad1(x, y));
        expectEquals(e, sad2(x, y));
        expectEquals(e, sad3(x, y));
        expectEquals(e, sad3Alt(x, y));
        expectEquals(e, sadL2(x, y));
        expectEquals(e, sadL3(x, y));
        expectEquals(e, sadL3Alt(x, y));
      }
    }
    System.out.println("SadByte passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
