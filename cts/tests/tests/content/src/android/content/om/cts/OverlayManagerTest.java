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

package android.content.om.cts;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.om.IOverlayManager;
import android.content.om.OverlayManager;
import android.os.UserHandle;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * This only tests the client API implementation of the OverlayManager
 * and not the service implementation.
 */
@RunWith(AndroidJUnit4.class)
public class OverlayManagerTest {

    private OverlayManager mManager;
    @Mock
    private IOverlayManager mMockService;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        mManager = new OverlayManager(context, mMockService);
    }

    @Test
    public void testSetEnabled() throws Exception {
        String packageName = "overlay source package name";
        int userId = UserHandle.myUserId();
        UserHandle user = UserHandle.of(userId);
        verify(mMockService, times(0)).setEnabled(packageName, true, userId);
        when(mMockService.setEnabled(anyString(), any(Boolean.class), anyInt()))
                .thenReturn(Boolean.TRUE);
        mManager.setEnabled(packageName, true, user);
        verify(mMockService, times(1)).setEnabled(packageName, true, userId);
    }

    @Test
    public void testSetEnabledExclusiveInCategory() throws Exception {
        String packageName = "overlay source package name";
        int userId = UserHandle.myUserId();
        UserHandle user = UserHandle.of(userId);
        verify(mMockService, times(0)).setEnabledExclusiveInCategory(packageName, userId);
        when(mMockService.setEnabledExclusiveInCategory(anyString(), anyInt()))
                .thenReturn(Boolean.TRUE);
        mManager.setEnabledExclusiveInCategory(packageName, user);
        verify(mMockService, times(1)).setEnabledExclusiveInCategory(packageName, userId);
    }

    @Test
    public void testGetOverlayInfosForTarget() throws Exception {
        String targetPackageName = "overlay target package name";
        int userId = UserHandle.myUserId();
        UserHandle user = UserHandle.of(userId);
        verify(mMockService, times(0)).getOverlayInfosForTarget(targetPackageName, userId);
        mManager.getOverlayInfosForTarget(targetPackageName, user);
        verify(mMockService, times(1)).getOverlayInfosForTarget(targetPackageName, userId);
    }
}
