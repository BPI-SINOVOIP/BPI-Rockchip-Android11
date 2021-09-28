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
package android.security.cts;

import org.junit.Test;

import android.content.ComponentName;
import android.content.Intent;
import android.platform.test.annotations.SecurityTest;
import android.test.AndroidTestCase;

@SecurityTest
public class BluetoothIntentsTest extends AndroidTestCase {
  /**
   * b/35258579
   */
  @SecurityTest
  public void testAcceptIntent() {
    genericIntentTest("ACCEPT");
  }

  /**
   * b/35258579
   */
  @SecurityTest
  public void testDeclineIntent() {
      genericIntentTest("DECLINE");
  }

  private static final String prefix = "android.btopp.intent.action.";
  private void genericIntentTest(String action) throws SecurityException {
    try {
      Intent should_be_protected_broadcast = new Intent();
      should_be_protected_broadcast.setComponent(
          new ComponentName("com.android.bluetooth",
            "com.android.bluetooth.opp.BluetoothOppReceiver"));
      should_be_protected_broadcast.setAction(prefix + action);
      mContext.sendBroadcast(should_be_protected_broadcast);
    }
    catch (SecurityException e) {
      return;
    }

    throw new SecurityException("An " + prefix + action +
        " intent should not be broadcastable except by the system (declare " +
        " as protected-broadcast in manifest)");
  }
}
