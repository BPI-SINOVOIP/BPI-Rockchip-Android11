/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.cts.deviceowner;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.fail;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Collections;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
public class AffiliationTest {

    private DevicePolicyManager mDevicePolicyManager;
    private ComponentName mAdminComponent;

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getContext();
        mDevicePolicyManager = (DevicePolicyManager)
                context.getSystemService(Context.DEVICE_POLICY_SERVICE);
        mAdminComponent = BasicAdminReceiver.getComponentName(context);
    }

    @Test
    public void testSetAffiliationId_null() {
        try {
            mDevicePolicyManager.setAffiliationIds(mAdminComponent, null);
            fail("Should throw IllegalArgumentException");
        } catch (IllegalArgumentException ex) {
            // Expected
        }
    }

    @Test
    public void testSetAffiliationId_containsEmptyString() {
        try {
            mDevicePolicyManager.setAffiliationIds(mAdminComponent, Collections.singleton(null));
            fail("Should throw IllegalArgumentException");
        } catch (IllegalArgumentException ex) {
            // Expected
        }
    }

    @Test
    public void testSetAffiliationId1() {
        setAffiliationIds(Collections.singleton("id.number.1"));
    }

    @Test
    public void testSetAffiliationId2() {
        setAffiliationIds(Collections.singleton("id.number.2"));
    }

    private void setAffiliationIds(Set<String> ids) {
        mDevicePolicyManager.setAffiliationIds(mAdminComponent, ids);
        assertEquals(ids, mDevicePolicyManager.getAffiliationIds(mAdminComponent));
    }
}
