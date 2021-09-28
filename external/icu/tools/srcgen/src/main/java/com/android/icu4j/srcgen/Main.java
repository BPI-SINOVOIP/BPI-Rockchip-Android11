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
package com.android.icu4j.srcgen;

import com.android.icu4j.srcgen.checker.GeneratePublicApiReport;

public class Main {
  public static void main(String[] args) throws Exception {
    if (args.length < 1) {
      throw new IllegalArgumentException("At least 1 arguments required.");
    }
    String className = args[0];
    String[] inputArgs = new String[args.length - 1];
    System.arraycopy(args, 1, inputArgs, 0, args.length - 1);
    switch (className) {
      case "Icu4jTransform":
        Icu4jTransform.main(inputArgs);
        break;
      case "Icu4jTestsTransform":
        Icu4jTestsTransform.main(inputArgs);
        break;
      case "Icu4jBasicTransform":
        Icu4jBasicTransform.main(inputArgs);
        break;
      case "GeneratePublicApiReport":
        GeneratePublicApiReport.main(inputArgs);
        break;
      default:
        throw new IllegalArgumentException("Input class name is not valid: " + className);
    }
  }
}
