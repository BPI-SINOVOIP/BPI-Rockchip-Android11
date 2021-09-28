/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.tradefed.util;

import junit.framework.TestCase;

/** Tests for {@link AaptParser}. */
public class AaptParserTest extends TestCase {

    public void testParseInvalidInput() {
        AaptParser p = new AaptParser();
        assertFalse(p.parse("Bad data"));
    }

    public void testParseEmptyVersionCode() {
        AaptParser p = new AaptParser();
        assertTrue(p.parse("package: name='android.support.graphics.drawable.animated.test'" +
                " versionCode='' versionName=''\n" +
                "sdkVersion:'11'\n" +
                "targetSdkVersion:'23'\n" +
                "uses-permission:'android.permission.WRITE_EXTERNAL_STORAGE'\n" +
                "application: label='' icon=''\n" +
                "application-debuggable\n" +
                "uses-library:'android.test.runner'\n" +
                "uses-permission:'android.permission.READ_EXTERNAL_STORAGE'\n" +
                "uses-implied-permission:'android.permission.READ_EXTERNAL_STORAGE'," +
                "'requested WRITE_EXTERNAL_STORAGE'\n" +
                "uses-feature:'android.hardware.touchscreen'\n" +
                "uses-implied-feature:'android.hardware.touchscreen'," +
                "'assumed you require a touch screen unless explicitly made optional'\n" +
                "other-activities\n" +
                "supports-screens: 'small' 'normal' 'large' 'xlarge'\n" +
                "supports-any-density: 'true'\n" +
                "locales: '--_--'\n" +
                "densities: '160'"));
        assertEquals("", p.getVersionCode());
    }

    public void testParsePackageNameVersionLabel() {
        AaptParser p = new AaptParser();
        p.parse("package: name='com.android.foo' versionCode='13' versionName='2.3'\n" +
                "sdkVersion:'5'\n" +
                "application-label:'Foo'\n" +
                "application-label-fr:'Faa'\n" +
                "uses-permission:'android.permission.INTERNET'");
        assertEquals("com.android.foo", p.getPackageName());
        assertEquals("13", p.getVersionCode());
        assertEquals("2.3", p.getVersionName());
        assertEquals("Foo", p.getLabel());
        assertEquals(5, p.getSdkVersion());
    }

    public void testParseVersionMultipleFieldsNoLabel() {
        AaptParser p = new AaptParser();
        p.parse("package: name='com.android.foo' versionCode='217173' versionName='1.7173' " +
                "platformBuildVersionName=''\n" +
                "install-location:'preferExternal'\n" +
                "sdkVersion:'10'\n" +
                "targetSdkVersion:'21'\n" +
                "uses-permission: name='android.permission.INTERNET'\n" +
                "uses-permission: name='android.permission.ACCESS_NETWORK_STATE'\n");
        assertEquals("com.android.foo", p.getPackageName());
        assertEquals("217173", p.getVersionCode());
        assertEquals("1.7173", p.getVersionName());
        assertEquals("com.android.foo", p.getLabel());
        assertEquals(10, p.getSdkVersion());
    }

    public void testParseInvalidSdkVersion() {
        AaptParser p = new AaptParser();
        p.parse("package: name='com.android.foo' versionCode='217173' versionName='1.7173' " +
                "platformBuildVersionName=''\n" +
                "install-location:'preferExternal'\n" +
                "sdkVersion:'notavalidsdk'\n" +
                "targetSdkVersion:'21'\n" +
                "uses-permission: name='android.permission.INTERNET'\n" +
                "uses-permission: name='android.permission.ACCESS_NETWORK_STATE'\n");
        assertEquals(-1, p.getSdkVersion());
    }

    public void testParseNativeCode() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='217173' versionName='1.7173' "
                        + "platformBuildVersionName=''\n"
                        + "install-location:'preferExternal'\n"
                        + "sdkVersion:'notavalidsdk'\n"
                        + "targetSdkVersion:'21'\n"
                        + "uses-permission: name='android.permission.INTERNET'\n"
                        + "uses-permission: name='android.permission.ACCESS_NETWORK_STATE'\n"
                        + "densities: '160'"
                        + "native-code: 'arm64-v8a'");
        assertEquals("arm64-v8a", p.getNativeCode().get(0));
    }

    public void testParseNativeCode_multi() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='217173' versionName='1.7173' "
                        + "platformBuildVersionName=''\n"
                        + "install-location:'preferExternal'\n"
                        + "sdkVersion:'notavalidsdk'\n"
                        + "targetSdkVersion:'21'\n"
                        + "uses-permission: name='android.permission.INTERNET'\n"
                        + "uses-permission: name='android.permission.ACCESS_NETWORK_STATE'\n"
                        + "densities: '160'"
                        + "native-code: 'arm64-v8a' 'armeabi-v7a'");
        assertEquals("arm64-v8a", p.getNativeCode().get(0));
        assertEquals("armeabi-v7a", p.getNativeCode().get(1));
    }

    public void testParseNativeCode_alt() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='217173' versionName='1.7173' "
                        + "platformBuildVersionName=''\n"
                        + "install-location:'preferExternal'\n"
                        + "sdkVersion:'notavalidsdk'\n"
                        + "targetSdkVersion:'21'\n"
                        + "uses-permission: name='android.permission.INTERNET'\n"
                        + "uses-permission: name='android.permission.ACCESS_NETWORK_STATE'\n"
                        + "densities: '160'\n"
                        + "native-code: 'arm64-v8a'\n"
                        + "alt-native-code: 'armeabi-v7a'");
        assertEquals("arm64-v8a", p.getNativeCode().get(0));
        assertEquals("armeabi-v7a", p.getNativeCode().get(1));
    }

    public void testParseXmlTree_withRequestLegacyFlagTrue() {
        AaptParser p = new AaptParser();
        p.parseXmlTree(
                "N: android=http://schemas.android.com/apk/res/android\n"
                        + "  E: manifest (line=2)\n"
                        + "    A: android:versionCode(0x0101021b)=(type 0x10)0x1d\n"
                        + "    A: android:versionName(0x0101021c)=\"R\" (Raw: \"R\")\n"
                        + "    A: android:compileSdkVersion(0x01010572)=(type 0x10)0x1d\n"
                        + "    A: android:compileSdkVersionCodename(0x01010573)=\"R\" (Raw: "
                        + "\"R\")\n"
                        + "    A: package=\"com.android.foo\" (Raw: \"com.android.foo\")\n"
                        + "    A: platformBuildVersionCode=(type 0x10)0x1d\n"
                        + "    A: platformBuildVersionName=\"R\" (Raw: \"R\")\n"
                        + "    E: uses-sdk (line=5)\n"
                        + "      A: android:minSdkVersion(0x0101020c)=(type 0x10)0x1c\n"
                        + "      A: android:targetSdkVersion(0x01010270)=\"R\" (Raw: \"R\")\n"
                        + "    E: application (line=12)\n"
                        + "      A: android:targetSdkVersion(0x01010270)=(type 0x10)0x1e\n"
                        + "      A: android:supportsRtl(0x010103af)=(type 0x12)0xffffffff\n"
                        + "      A: android:extractNativeLibs(0x010104ea)=(type 0x12)0xffffffff\n"
                        + "      A: android:appComponentFactory(0x0101057a)=\"androidx.core.app"
                        + ".CoreComponentFactory\" (Raw: \"androidx.core.app"
                        + ".CoreComponentFactory\")\n"
                        + "      A: android:requestLegacyExternalStorage(0x01010603)=(type 0x12)"
                        + "0xffffffff\n");
        assertTrue(p.isRequestingLegacyStorage());
    }

    public void testParseXmlTree_withRequestLegacyFlagFalse() {
        AaptParser p = new AaptParser();
        p.parseXmlTree(
                "N: android=http://schemas.android.com/apk/res/android\n"
                        + "  E: manifest (line=2)\n"
                        + "    A: android:versionCode(0x0101021b)=(type 0x10)0x1d\n"
                        + "    A: android:versionName(0x0101021c)=\"R\" (Raw: \"R\")\n"
                        + "    A: android:compileSdkVersion(0x01010572)=(type 0x10)0x1d\n"
                        + "    A: android:compileSdkVersionCodename(0x01010573)=\"R\" (Raw: "
                        + "\"R\")\n"
                        + "    A: package=\"com.android.foo\" (Raw: \"com.android.foo\")\n"
                        + "    A: platformBuildVersionCode=(type 0x10)0x1d\n"
                        + "    A: platformBuildVersionName=\"R\" (Raw: \"R\")\n"
                        + "    E: uses-sdk (line=5)\n"
                        + "      A: android:minSdkVersion(0x0101020c)=(type 0x10)0x1c\n"
                        + "      A: android:targetSdkVersion(0x01010270)=\"R\" (Raw: \"R\")\n"
                        + "    E: application (line=12)\n"
                        + "      A: android:targetSdkVersion(0x01010270)=(type 0x10)0x1e\n"
                        + "      A: android:supportsRtl(0x010103af)=(type 0x12)0xffffffff\n"
                        + "      A: android:extractNativeLibs(0x010104ea)=(type 0x12)0xffffffff\n"
                        + "      A: android:appComponentFactory(0x0101057a)=\"androidx.core.app"
                        + ".CoreComponentFactory\" (Raw: \"androidx.core.app"
                        + ".CoreComponentFactory\")\n"
                        + "      A: android:requestLegacyExternalStorage(0x01010603)=(type 0x12)"
                        + "0x0\n");
        assertFalse(p.isRequestingLegacyStorage());
    }

    public void testParseXmlTree_withoutRequestLegacyFlag() {
        AaptParser p = new AaptParser();
        p.parseXmlTree(
                "N: android=http://schemas.android.com/apk/res/android\n"
                        + "  E: manifest (line=2)\n"
                        + "    A: android:versionCode(0x0101021b)=(type 0x10)0x1d\n"
                        + "    A: android:versionName(0x0101021c)=\"R\" (Raw: \"R\")\n"
                        + "    A: android:compileSdkVersion(0x01010572)=(type 0x10)0x1d\n"
                        + "    A: android:compileSdkVersionCodename(0x01010573)=\"R\" (Raw: "
                        + "\"R\")\n"
                        + "    A: package=\"com.android.foo\" (Raw: \"com.android.foo\")\n"
                        + "    A: platformBuildVersionCode=(type 0x10)0x1d\n"
                        + "    A: platformBuildVersionName=\"R\" (Raw: \"R\")\n"
                        + "    E: uses-sdk (line=5)\n"
                        + "      A: android:minSdkVersion(0x0101020c)=(type 0x10)0x1c\n"
                        + "      A: android:targetSdkVersion(0x01010270)=\"R\" (Raw: \"R\")\n"
                        + "    E: application (line=12)\n"
                        + "      A: android:targetSdkVersion(0x01010270)=(type 0x10)0x1e\n"
                        + "      A: android:supportsRtl(0x010103af)=(type 0x12)0xffffffff\n"
                        + "      A: android:extractNativeLibs(0x010104ea)=(type 0x12)0xffffffff\n"
                        + "      A: android:appComponentFactory(0x0101057a)=\"androidx.core.app"
                        + ".CoreComponentFactory\" (Raw: \"androidx.core.app");
        assertFalse(p.isRequestingLegacyStorage());
    }

    public void testParseXmlTree_withUsesPermissionManageExternalStorage() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='217173' versionName='1.7173' "
                        + "platformBuildVersionName=''\n"
                        + "install-location:'preferExternal'\n"
                        + "sdkVersion:'10'\n"
                        + "targetSdkVersion:'21'\n"
                        + "uses-permission: name='android.permission.INTERNET'\n"
                        + "uses-permission: name='android.permission.MANAGE_EXTERNAL_STORAGE'\n");
        assertTrue(p.isUsingPermissionManageExternalStorage());
    }

    public void testParseXmlTree_withoutUsesPermissionManageExternalStorage() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='217173' versionName='1.7173' "
                        + "platformBuildVersionName=''\n"
                        + "install-location:'preferExternal'\n"
                        + "sdkVersion:'10'\n"
                        + "targetSdkVersion:'21'\n"
                        + "uses-permission: name='android.permission.INTERNET'\n");
        assertFalse(p.isUsingPermissionManageExternalStorage());
    }

    public void testParseTargetSdkVersion() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='13' versionName='2.3'\n"
                        + "sdkVersion:'5'\n"
                        + "targetSdkVersion:'29'\n"
                        + "application-label-fr:'Faa'\n"
                        + "uses-permission:'android.permission.INTERNET'");
        assertEquals(29, p.getTargetSdkVersion());
    }

    public void testParseInvalidTargetSdkVersion() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='13' versionName='2.3'\n"
                        + "sdkVersion:'5'\n"
                        + "targetSdkVersion:'R'\n"
                        + "application-label-fr:'Faa'\n"
                        + "uses-permission:'android.permission.INTERNET'");
        assertEquals(10000, p.getTargetSdkVersion());
    }

    public void testParseNoTargetSdkVersion() {
        AaptParser p = new AaptParser();
        p.parse(
                "package: name='com.android.foo' versionCode='13' versionName='2.3'\n"
                        + "sdkVersion:'5'\n"
                        + "application-label-fr:'Faa'\n"
                        + "uses-permission:'android.permission.INTERNET'");
        assertEquals(10000, p.getTargetSdkVersion());
    }
}
