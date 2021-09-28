/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.net.cts.networkpermission.internetpermission;

import static org.junit.Assert.fail;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.Socket;
/**
* Test that protected android.net.ConnectivityManager methods cannot be called without
* permissions
*/
@RunWith(AndroidJUnit4.class)
public class InternetPermissionTest {

    /**
     * Verify that create inet socket failed because of the permission is missing.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#INTERNET}.
     */
    @SmallTest
    @Test
    public void testCreateSocket() throws Exception {
        try {
            Socket socket = new Socket("example.com", 80);
            fail("Ceate inet socket did not throw SecurityException as expected");
        } catch (SecurityException e) {
            // expected
        }
    }
}
