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

import dalvik.system.ZygoteHooks;

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    if (!hasJit()) {
      return;
    }
    ensureJitCompiled(Object.class, "toString");
    ZygoteHooks.preFork();
    ZygoteHooks.postForkChild(
        /*flags=*/0, /*is_system_server=*/false, /*is_zygote=*/false, /*instruction_set=*/null);
    ZygoteHooks.postForkCommon();
    deoptimizeBootImage();
    if (hasJitCompiledEntrypoint(Object.class, "toString")) {
      throw new Error("Expected Object.toString to be deoptimized");
    }
  }

  private static native boolean hasJit();
  private static native void ensureJitCompiled(Class<?> cls, String name);
  private static native boolean hasJitCompiledEntrypoint(Class<?> cls, String name);
  private static native void deoptimizeBootImage();
}
