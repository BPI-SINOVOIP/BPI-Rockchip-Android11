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
package android.controls.cts;

import static org.junit.Assert.assertEquals;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Icon;
import android.service.controls.Control;
import android.service.controls.DeviceTypes;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Covers Control builder methods that are not already covered in CtsControlsServiceTest.
 */
@RunWith(AndroidJUnit4.class)
public class CtsControlBuilderTest {

    private static final String TITLE = "title";
    private static final String SUBTITLE = "subtitle";
    private static final String ZONE = "zone";
    private static final String STRUCTURE = "structure";
    private static final String CONTROL_ID = "testId";
    private static final String CONTROL_ID2 = "testId2";
    private static final int DEVICE_TYPE = DeviceTypes.TYPE_PERGOLA;
    private static final int STATUS_STATEFUL = Control.STATUS_ERROR;
    private static final int STATUS_STATELESS = Control.STATUS_UNKNOWN;
    private static final String STATUS_TEXT_STATELESS = "";
    private static final String STATUS_TEXT_STATEFUL = "statusText";

    private Context mContext;
    private PendingIntent mPendingIntent;
    private PendingIntent mPendingIntent2;
    private ColorStateList mColorStateList;
    private Icon mIcon;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mPendingIntent = PendingIntent.getActivity(mContext, 1, new Intent(),
            PendingIntent.FLAG_UPDATE_CURRENT);
        mPendingIntent2 = PendingIntent.getActivity(mContext, 2, new Intent(),
            PendingIntent.FLAG_UPDATE_CURRENT);
        mIcon = Icon.createWithResource(mContext, R.drawable.ic_device_unknown);
        mColorStateList = mContext.getResources().getColorStateList(R.color.custom_mower, null);
    }

    @Test
    public void testStatelessBuilder() {
        Control control = new Control.StatelessBuilder(CONTROL_ID, mPendingIntent)
                .setTitle(TITLE)
                .setControlId(CONTROL_ID2)
                .setAppIntent(mPendingIntent2)
                .setSubtitle(SUBTITLE)
                .setStructure(STRUCTURE)
                .setDeviceType(DEVICE_TYPE)
                .setZone(ZONE)
                .setCustomIcon(mIcon)
                .setCustomColor(mColorStateList)
                .build();

        assertControl(control, true);
    }

    @Test
    public void testStatefulBuilderAlternateConstructor() {
        Control control = new Control.StatefulBuilder(CONTROL_ID, mPendingIntent)
                .setTitle(TITLE)
                .setControlId(CONTROL_ID2)
                .setAppIntent(mPendingIntent2)
                .setSubtitle(SUBTITLE)
                .setStructure(STRUCTURE)
                .setDeviceType(DEVICE_TYPE)
                .setZone(ZONE)
                .setStatus(STATUS_STATEFUL)
                .setStatusText(STATUS_TEXT_STATEFUL)
                .setCustomIcon(mIcon)
                .setCustomColor(mColorStateList)
                .build();

        Control updatedControl = new Control.StatefulBuilder(control).build();
        assertControl(updatedControl, false);
    }

    private void assertControl(Control control, boolean isStateless) {
        assertEquals(control.getTitle(), TITLE);
        assertEquals(control.getSubtitle(), SUBTITLE);
        assertEquals(control.getStructure(), STRUCTURE);
        assertEquals(control.getZone(), ZONE);
        assertEquals(control.getDeviceType(), DEVICE_TYPE);
        assertEquals(control.getStatus(), isStateless ? STATUS_STATELESS : STATUS_STATEFUL);
        assertEquals(control.getStatusText(),
                isStateless ? STATUS_TEXT_STATELESS : STATUS_TEXT_STATEFUL);
        assertEquals(control.getControlId(), CONTROL_ID2);
        assertEquals(control.getCustomColor(), mColorStateList);
        assertEquals(control.getCustomIcon(), mIcon);
    }
}
