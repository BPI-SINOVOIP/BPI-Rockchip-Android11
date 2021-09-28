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

package android.os.powermanagertests;

import static android.os.PowerManager.PARTIAL_WAKE_LOCK;

import android.app.Activity;
import android.os.Bundle;
import android.os.PowerManager.WakeLock;
import android.os.PowerManager;

public class WakeLockTest extends Activity {
  private static final String TAG = WakeLockTest.class.getSimpleName();

  @Override
  public void onCreate(Bundle icicle) {
    super.onCreate(icicle);
    PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
    WakeLock wl = pm.newWakeLock(PARTIAL_WAKE_LOCK, TAG);
    wl.acquire(300000 /* 5 mins */);
  }
}
