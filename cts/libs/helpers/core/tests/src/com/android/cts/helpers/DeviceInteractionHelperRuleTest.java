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
package com.android.cts.helpers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/** Unit test for {@link DeviceInteractionHelperRule} */
@RunWith(JUnit4.class)
public final class DeviceInteractionHelperRuleTest {

    @Mock private PackageManager mPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    ApplicationInfo makeApplicationInfo(String packageName, String dir, String prefix) {
        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.packageName = packageName;
        appInfo.publicSourceDir = dir;
        appInfo.metaData = new Bundle();
        if (prefix != null) {
            appInfo.metaData.putString("interaction-helpers-prefix", prefix);
        }
        return appInfo;
    }

    @Test
    public void testMissingProperty() throws NameNotFoundException {
        ApplicationInfo appInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(appInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList(null, mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("Prefix");
        assertThat(rule.getPackageList()).containsExactly("com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/a");
    }

    @Test
    public void testEmptyProperty() throws NameNotFoundException {
        ApplicationInfo appInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(appInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("Prefix");
        assertThat(rule.getPackageList()).containsExactly("com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/a");
    }

    @Test
    public void testExplicitDefaultProperty() throws NameNotFoundException {
        ApplicationInfo appInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(appInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("com.android.cts.helpers.aosp", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("Prefix");
        assertThat(rule.getPackageList()).containsExactly("com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/a");
    }

    @Test
    public void testOnePackageExplicitDefault() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.b:com.android.cts.helpers.aosp", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("BPrefix", "Prefix");
        assertThat(rule.getPackageList()).containsExactly("test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/b", "/tmp/a");
    }

    @Test
    public void testOnePackage() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("BPrefix", "Prefix");
        assertThat(rule.getPackageList()).containsExactly("test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/b", "/tmp/a");
    }

    @Test
    public void testTwoPackages() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "BPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly("test.c", "test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testDefaultAtFront() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("com.android.cts.helpers.aosp:test.c:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "BPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly("test.c", "test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testDefaultInMiddle() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:com.android.cts.helpers.aosp:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "BPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly("test.c", "test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testDefaultAtEnd() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:test.b:com.android.cts.helpers.aosp", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "BPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly("test.c", "test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testDefaultSubstringPrefix() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        ApplicationInfo dInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp.more", "/tmp/d", "DPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp.more", PackageManager.GET_META_DATA))
                .thenReturn(dInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:com.android.cts.helpers.aosp.more:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "DPrefix", "BPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly(
                        "test.c",
                        "com.android.cts.helpers.aosp.more",
                        "test.b",
                        "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/d", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testDefaultSubstringSuffix() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        ApplicationInfo dInfo =
                makeApplicationInfo("notcom.android.cts.helpers.aosp", "/tmp/d", "DPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);
        when(mPackageManager.getApplicationInfo(
                        "notcom.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(dInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:notcom.android.cts.helpers.aosp:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "DPrefix", "BPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly(
                        "test.c",
                        "notcom.android.cts.helpers.aosp",
                        "test.b",
                        "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/d", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testMissingHelperPrefix() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", null);
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly("test.c", "test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testMissingMetadata() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", null);
        bInfo.metaData = null;
        ApplicationInfo cInfo = makeApplicationInfo("test.c", "/tmp/c", "CPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenReturn(cInfo);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("CPrefix", "Prefix");
        assertThat(rule.getPackageList())
                .containsExactly("test.c", "test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/c", "/tmp/b", "/tmp/a");
    }

    @Test
    public void testAppNotInstalled() throws NameNotFoundException {
        ApplicationInfo baseInfo =
                makeApplicationInfo("com.android.cts.helpers.aosp", "/tmp/a", "Prefix");
        ApplicationInfo bInfo = makeApplicationInfo("test.b", "/tmp/b", "BPrefix");
        when(mPackageManager.getApplicationInfo(
                        "com.android.cts.helpers.aosp", PackageManager.GET_META_DATA))
                .thenReturn(baseInfo);
        when(mPackageManager.getApplicationInfo("test.b", PackageManager.GET_META_DATA))
                .thenReturn(bInfo);
        when(mPackageManager.getApplicationInfo("test.c", PackageManager.GET_META_DATA))
                .thenThrow(NameNotFoundException.class);

        DeviceInteractionHelperRule<ICtsDeviceInteractionHelper> rule =
                new DeviceInteractionHelperRule(ICtsDeviceInteractionHelper.class);

        rule.buildPackageList("test.c:test.b", mPackageManager);

        assertThat(rule.getPrefixes()).containsExactly("BPrefix", "Prefix");
        assertThat(rule.getPackageList()).containsExactly("test.b", "com.android.cts.helpers.aosp");
        assertThat(rule.getSourceDirs()).containsExactly("/tmp/b", "/tmp/a");
    }
}
