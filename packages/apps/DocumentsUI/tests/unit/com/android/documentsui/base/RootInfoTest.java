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

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class RootInfoTest {

    @Test
    public void testEquals_sameUser() {
        RootInfo rootInfo = new RootInfo();
        rootInfo.userId = UserId.of(100);
        rootInfo.authority = "authority";
        rootInfo.rootId = "root";

        RootInfo rootInfo2 = new RootInfo();
        rootInfo2.userId = UserId.of(100);
        rootInfo2.authority = "authority";
        rootInfo2.rootId = "root";

        assertThat(rootInfo).isEqualTo(rootInfo2);
    }

    @Test
    public void testNotEquals_differentUser() {
        RootInfo rootInfo = new RootInfo();
        rootInfo.userId = UserId.of(100);
        rootInfo.authority = "authority";
        rootInfo.rootId = "root";

        RootInfo rootInfo2 = new RootInfo();
        rootInfo2.userId = UserId.of(101);
        rootInfo2.authority = "authority";
        rootInfo2.rootId = "root";

        assertThat(rootInfo).isNotEqualTo(rootInfo2);
    }

    @Test
    public void testCopyInfo_equal() {
        RootInfo rootInfo = new RootInfo();
        rootInfo.userId = UserId.of(100);
        rootInfo.authority = "authority";
        rootInfo.rootId = "root";

        RootInfo copied = RootInfo.copyRootInfo(rootInfo);
        assertThat(copied).isEqualTo(rootInfo);
        assertThat(copied).isNotSameAs(rootInfo);
    }
}
