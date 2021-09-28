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

package android.telephony.ims.cts;

import static junit.framework.TestCase.assertEquals;

import android.telephony.ims.ImsException;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class ImsExceptionTest {

    @Test
    public void testImsExceptionConstructors() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsException e = new ImsException("Test1");
        assertEquals(ImsException.CODE_ERROR_UNSPECIFIED, e.getCode());
        ImsException e2 = new ImsException("Test2", ImsException.CODE_ERROR_SERVICE_UNAVAILABLE);
        assertEquals(ImsException.CODE_ERROR_SERVICE_UNAVAILABLE, e2.getCode());
        ImsException e3 = new ImsException("Test3", ImsException.CODE_ERROR_UNSUPPORTED_OPERATION);
        assertEquals(ImsException.CODE_ERROR_UNSUPPORTED_OPERATION, e3.getCode());
    }
}
