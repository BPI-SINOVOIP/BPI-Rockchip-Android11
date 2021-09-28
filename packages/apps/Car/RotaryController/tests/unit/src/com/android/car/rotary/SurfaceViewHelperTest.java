/*
 * Copyright (C) 2021 The Android Open Source Project
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
package com.android.car.rotary;

import static com.google.common.truth.Truth.assertThat;

import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

@RunWith(AndroidJUnit4.class)
public class SurfaceViewHelperTest {

    private final static String HOST_APP_PACKAGE_NAME = "host.app.package.name";
    private final static String CLIENT_APP_PACKAGE_NAME = "client.app.package.name";

    private SurfaceViewHelper mSurfaceViewHelper;
    private NodeBuilder mNodeBuilder;

    @Before
    public void setUp() {
        mSurfaceViewHelper = new SurfaceViewHelper();
        mNodeBuilder = new NodeBuilder(new ArrayList<>());
    }

    @Test
    public void testIsHostNode() {
        mSurfaceViewHelper.mHostApp = HOST_APP_PACKAGE_NAME;

        AccessibilityNodeInfo node = mNodeBuilder.build();
        assertThat(mSurfaceViewHelper.isHostNode(node)).isFalse();

        AccessibilityNodeInfo hostNode = mNodeBuilder.setPackageName(HOST_APP_PACKAGE_NAME).build();
        assertThat(mSurfaceViewHelper.isHostNode(hostNode)).isTrue();
    }

    @Test
    public void testIsClientNode() {
        mSurfaceViewHelper.addClientApp(CLIENT_APP_PACKAGE_NAME);

        AccessibilityNodeInfo node = mNodeBuilder.build();
        assertThat(mSurfaceViewHelper.isClientNode(node)).isFalse();

        AccessibilityNodeInfo clientNode = mNodeBuilder
                .setPackageName(CLIENT_APP_PACKAGE_NAME)
                .build();
        assertThat(mSurfaceViewHelper.isClientNode(clientNode)).isTrue();
    }

    @Test
    public void testAddClientApp() {
        AccessibilityNodeInfo clientNode = mNodeBuilder
                .setPackageName(CLIENT_APP_PACKAGE_NAME)
                .build();
        assertThat(mSurfaceViewHelper.isClientNode(clientNode)).isFalse();

        mSurfaceViewHelper.addClientApp(CLIENT_APP_PACKAGE_NAME);
        assertThat(mSurfaceViewHelper.isClientNode(clientNode)).isTrue();
    }
}
