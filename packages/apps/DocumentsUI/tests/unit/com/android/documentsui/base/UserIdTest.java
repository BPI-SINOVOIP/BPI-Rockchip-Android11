/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.documentsui.base;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Process;
import android.os.UserHandle;
import android.test.AndroidTestCase;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.ProtocolException;

@SmallTest
public class UserIdTest extends AndroidTestCase {

    private Context mContext = ApplicationProvider.getApplicationContext();

    @Test
    public void testEquals() {
        assertEquals(UserId.CURRENT_USER, UserId.CURRENT_USER);
        assertEquals(UserId.of(UserHandle.of(10)), UserId.of(UserHandle.of(10)));
    }

    @Test
    public void testEquals_handlesNulls() {
        assertFalse(UserId.CURRENT_USER.equals(null));
    }

    @Test
    public void testNotEquals_differentUserHandle() {
        assertFalse(UserId.of(UserHandle.of(0)).equals(UserId.of(UserHandle.of(10))));
    }

    @Test
    public void testHashCode_sameUserHandle() {
        assertEquals(UserId.of(UserHandle.of(0)).hashCode(),
                UserId.of(UserHandle.of(0)).hashCode());
    }

    @Test
    public void testHashCode_differentUserHandle() {
        assertThat(UserId.of(UserHandle.of(0)).hashCode()).isNotEqualTo(
                UserId.of(UserHandle.of(10)).hashCode());
    }

    @Test
    public void testAsContext_sameUserReturnProvidedContext() {
        UserId userId = UserId.of(Process.myUserHandle());
        assertSame(mContext, userId.asContext(mContext));
    }

    @Test
    public void testAsContext_unspecifiedUserReturnProvidedContext() {
        assertSame(mContext, UserId.UNSPECIFIED_USER.asContext(mContext));
    }

    @Test
    public void testAsContext_differentUserShouldCreatePackageContextAsUser()
            throws Exception {
        Context mockContext = mock(Context.class);
        Context expectedContext = mock(Context.class);
        UserHandle differentUserHandle = UserHandle.of(UserHandle.myUserId() + 1);
        when(mockContext.createPackageContextAsUser("android", 0, differentUserHandle)).thenReturn(
                expectedContext);

        assertThat(UserId.of(differentUserHandle).asContext(mockContext)).isSameAs(
                expectedContext);
    }

    @Test
    public void testOf_UserIdUserHandleIdentifier() {
        UserHandle userHandle = UserHandle.of(10);
        assertEquals(UserId.of(userHandle), UserId.of(userHandle.getIdentifier()));
    }

    @Test
    public void testOf_UserIdRecreatingShouldEqual() {
        UserId userId = UserId.of(3);
        assertEquals(userId, UserId.of(userId.getIdentifier()));
    }

    @Test
    public void testRead_success() throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream o = new DataOutputStream(output);

        o.writeInt(1); // version
        o.writeInt(10); // user id

        UserId userId = UserId.read(
                new DataInputStream(new ByteArrayInputStream(output.toByteArray())));

        assertThat(userId).isEqualTo(UserId.of(UserHandle.of(10)));
    }

    @Test
    public void testRead_failWithInvalidVersion() throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream o = new DataOutputStream(output);

        o.writeInt(2); // version
        o.writeInt(10); // user id

        try {
            UserId.read(new DataInputStream(new ByteArrayInputStream(output.toByteArray())));
            fail();
        } catch (ProtocolException expected) {
        } finally {
            o.close();
            output.close();
        }
    }

    @Test
    public void testWrite_writeDataOutputStream() throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream o = new DataOutputStream(output);

        UserId.write(o, UserId.of(UserHandle.of(12)));

        DataInputStream in = new DataInputStream(new ByteArrayInputStream(output.toByteArray()));

        assertEquals(in.readInt(), 1); // version
        assertEquals(in.readInt(), 12); // user id
        assertEquals(in.read(), -1); // end of stream

        output.close();
        o.close();
        in.close();
    }
}
