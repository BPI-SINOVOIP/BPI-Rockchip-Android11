/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * limitations under the License
 */
package com.android.tv.common.compat;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.fail;

import android.content.pm.ResolveInfo;
import android.content.res.XmlResourceParser;
import android.media.tv.TvInputInfo;

import androidx.test.InstrumentationRegistry;

import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.testing.utils.TestUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.StringReader;

/** Tests for {@link TvInputInfoCompat}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class TvInputInfoCompatTest {
    private TvInputInfoCompat mTvInputInfoCompat;
    private String mInputXml;

    @Before
    public void setUp() throws Exception {
        ResolveInfo resolveInfo = TestUtils.createResolveInfo("test", "test");
        TvInputInfo info =
                TestUtils.createTvInputInfo(
                        resolveInfo, "test_input", "test1", TvInputInfo.TYPE_OTHER, false);
        mTvInputInfoCompat =
                new TvInputInfoCompat(InstrumentationRegistry.getTargetContext(), info) {
                    @Override
                    XmlPullParser getXmlResourceParser() {
                        XmlPullParser xpp = null;
                        try {
                            xpp = XmlPullParserFactory.newInstance().newPullParser();
                            xpp.setInput(new StringReader(mInputXml));
                            xpp.setFeature(XmlResourceParser.FEATURE_PROCESS_NAMESPACES, true);
                        } catch (XmlPullParserException e) {
                            fail("failed in setUp() " + e.getMessage());
                        }
                        return xpp;
                    }
                };
    }

    @Test
    public void testGetAttributeValue_notTvInputTag() {
        mInputXml =
                "<not-tv-input xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                        + "    android:setupActivity=\"\"\n"
                        + "    android:settingsActivity=\"\"/>\n";
        assertThat(mTvInputInfoCompat.getExtras()).isEmpty();
    }

    @Test
    public void testGetAttributeValue_noExtra() {
        mInputXml =
                "<not-tv-input xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                        + "    android:setupActivity=\"\"\n"
                        + "    android:settingsActivity=\"\"/>\n";
        assertThat(mTvInputInfoCompat.getExtras()).isEmpty();
    }

    @Test
    public void testGetAttributeValue() {
        mInputXml =
                "<tv-input xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                        + "    android:setupActivity=\"\"\n"
                        + "    android:settingsActivity=\"\">\n"
                        + "      <extra android:name=\"otherAttr1\" android:value=\"false\" />\n"
                        + "      <extra android:name=\"otherAttr2\" android:value=\"false\" />\n"
                        + "      <extra android:name="
                        + "          \"com.android.tv.common.compat.tvinputinfocompat.audioOnly\""
                        + "          android:value=\"true\" />\n"
                        + "</tv-input>";
        assertThat(mTvInputInfoCompat.getExtras())
                .containsExactly(
                        "otherAttr1",
                        "false",
                        "otherAttr2",
                        "false",
                        "com.android.tv.common.compat.tvinputinfocompat.audioOnly",
                        "true");
    }
}
