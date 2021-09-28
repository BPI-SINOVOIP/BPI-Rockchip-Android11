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

import android.app.PendingIntent;
import android.app.RecoverableSecurityException;
import android.app.RemoteAction;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.os.Parcel;
import android.test.AndroidTestCase;

public class RecoverableSecurityExceptionTest extends AndroidTestCase {
    public void testSimple() {
        final RecoverableSecurityException rse = build();

        assertEquals(rse.getMessage(), "foo");
        assertEquals(rse.getUserMessage(), "bar");
        assertEquals(rse.getUserAction().getTitle(), "title");
    }

    public void testParcelable() {
        final RecoverableSecurityException before = build();

        final Parcel parcel = Parcel.obtain();
        parcel.writeParcelable(before, 0);

        parcel.setDataPosition(0);
        final RecoverableSecurityException after = parcel.readParcelable(null);

        assertEquals(before.getMessage(), after.getMessage());
        assertEquals(before.getUserMessage(), after.getUserMessage());
        assertEquals(before.getUserAction().getTitle(), after.getUserAction().getTitle());
    }

    private RecoverableSecurityException build() {
        final PendingIntent pi = PendingIntent.getActivity(getContext(), 42, new Intent(), 0);
        return new RecoverableSecurityException(new SecurityException("foo"), "bar",
                new RemoteAction(Icon.createWithFilePath("/dev/null"), "title", "content", pi));
    }
}
