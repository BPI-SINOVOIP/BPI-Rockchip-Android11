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

import dalvik.system.VMRuntime;

public class Main {

  public static void main(String[] args) throws Exception {
    VMRuntime runtime = VMRuntime.getRuntime();

    try {
      int N = 1024 * 1024;
      int S = 512;
      for (int n = 0; n < N; ++n) {
        // Allocate unreachable objects.
        $noinline$Alloc(runtime);
        // Allocate an object with a substantial size to increase memory
        // pressure and eventually trigger non-explicit garbage collection
        // (explicit garbage collections triggered by java.lang.Runtime.gc()
        // are always full GCs). Upon garbage collection, the objects
        // allocated in $noinline$Alloc used to trigger a crash.
        Object[] moving_array = new Object[S];
      }
    } catch (OutOfMemoryError e) {
      // Stop here.
    }
    System.out.println("passed");
  }

  // When using the Concurrent Copying (CC) collector (default collector),
  // this method allocates an object in the non-moving space and an object
  // in the region space, make the former reference the later, and returns
  // nothing (so that none of these objects are reachable when upon return).
  static void $noinline$Alloc(VMRuntime runtime) {
    Object[] non_moving_array = (Object[]) runtime.newNonMovableArray(Object.class, 1);
    // Small object, unlikely to trigger garbage collection.
    non_moving_array[0] = new Object();
  }

}
