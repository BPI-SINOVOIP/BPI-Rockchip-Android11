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

package android.app.cts;

import android.os.Parcel;
import android.os.UserHandle;
import android.test.AndroidTestCase;

public class UserHandleTest extends AndroidTestCase {
    private static void assertSameUserHandle(int userId) {
        assertSame(UserHandle.of(userId), UserHandle.of(userId));
    }

    public void testOf() {
        for (int i = -1000; i < 100; i++) {
            assertEquals(i, UserHandle.of(i).getIdentifier());
        }

        // Ensure common objects are cached.
        assertSameUserHandle(UserHandle.USER_SYSTEM);
        assertSameUserHandle(UserHandle.USER_ALL);
        assertSameUserHandle(UserHandle.USER_NULL);
        assertSameUserHandle(UserHandle.MIN_SECONDARY_USER_ID);
        assertSameUserHandle(UserHandle.MIN_SECONDARY_USER_ID + 1);
    }

    private static void assertParcel(int userId) {
        Parcel p = Parcel.obtain();
        p.writeParcelable(UserHandle.of(userId), 0);
        p.setDataPosition(0);

        UserHandle read = p.readParcelable(UserHandleTest.class.getClassLoader());

        assertEquals(userId, read.getIdentifier());

        p.recycle();
    }

    public void testParcel() {
        for (int i = -1000; i < 100; i++) {
            assertParcel(i);
        }
    }
}
