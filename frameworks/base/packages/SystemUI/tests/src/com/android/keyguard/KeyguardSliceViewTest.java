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
package com.android.keyguard;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.net.Uri;
import android.test.suitebuilder.annotation.SmallTest;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper.RunWithLooper;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;

import androidx.slice.SliceProvider;
import androidx.slice.SliceSpecs;
import androidx.slice.builders.ListBuilder;

import com.android.systemui.R;
import com.android.systemui.SysuiTestCase;
import com.android.systemui.keyguard.KeyguardSliceProvider;
import com.android.systemui.plugins.ActivityStarter;
import com.android.systemui.statusbar.policy.ConfigurationController;
import com.android.systemui.tuner.TunerService;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Collections;
import java.util.HashSet;
import java.util.concurrent.atomic.AtomicBoolean;

@SmallTest
@RunWithLooper
@RunWith(AndroidTestingRunner.class)
public class KeyguardSliceViewTest extends SysuiTestCase {
    private KeyguardSliceView mKeyguardSliceView;
    private Uri mSliceUri;

    @Mock
    private TunerService mTunerService;
    @Mock
    private ConfigurationController mConfigurationController;
    @Mock
    private ActivityStarter mActivityStarter;
    @Mock
    private Resources mResources;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        allowTestableLooperAsMainThread();
        LayoutInflater layoutInflater = LayoutInflater.from(getContext());
        layoutInflater.setPrivateFactory(new LayoutInflater.Factory2() {

            @Override
            public View onCreateView(View parent, String name, Context context,
                    AttributeSet attrs) {
                return onCreateView(name, context, attrs);
            }

            @Override
            public View onCreateView(String name, Context context, AttributeSet attrs) {
                if ("com.android.keyguard.KeyguardSliceView".equals(name)) {
                    return new KeyguardSliceView(getContext(), attrs, mActivityStarter,
                            mConfigurationController, mTunerService, mResources);
                }
                return null;
            }
        });
        mKeyguardSliceView = (KeyguardSliceView) layoutInflater
                .inflate(R.layout.keyguard_status_area, null);
        mKeyguardSliceView.setupUri(KeyguardSliceProvider.KEYGUARD_SLICE_URI);
        mSliceUri = Uri.parse(KeyguardSliceProvider.KEYGUARD_SLICE_URI);
        SliceProvider.setSpecs(new HashSet<>(Collections.singletonList(SliceSpecs.LIST)));
    }

    @Test
    public void showSlice_notifiesListener() {
        ListBuilder builder = new ListBuilder(getContext(), mSliceUri, ListBuilder.INFINITY);
        AtomicBoolean notified = new AtomicBoolean();
        mKeyguardSliceView.setContentChangeListener(()-> notified.set(true));
        mKeyguardSliceView.onChanged(builder.build());
        Assert.assertTrue("Listener should be notified about slice changes.",
                notified.get());
    }

    @Test
    public void showSlice_emptySliceNotifiesListener() {
        AtomicBoolean notified = new AtomicBoolean();
        mKeyguardSliceView.setContentChangeListener(()-> notified.set(true));
        mKeyguardSliceView.onChanged(null);
        Assert.assertTrue("Listener should be notified about slice changes.",
                notified.get());
    }

    @Test
    public void hasHeader_readsSliceData() {
        ListBuilder builder = new ListBuilder(getContext(), mSliceUri, ListBuilder.INFINITY);
        mKeyguardSliceView.onChanged(builder.build());
        Assert.assertFalse("View should not have a header", mKeyguardSliceView.hasHeader());

        builder.setHeader(new ListBuilder.HeaderBuilder().setTitle("header title!"));
        mKeyguardSliceView.onChanged(builder.build());
        Assert.assertTrue("View should have a header", mKeyguardSliceView.hasHeader());
    }

    @Test
    public void refresh_replacesSliceContentAndNotifiesListener() {
        AtomicBoolean notified = new AtomicBoolean();
        mKeyguardSliceView.setContentChangeListener(()-> notified.set(true));
        mKeyguardSliceView.refresh();
        Assert.assertTrue("Listener should be notified about slice changes.",
                notified.get());
    }

    @Test
    public void getTextColor_whiteTextWhenAOD() {
        // Set text color to red since the default is white and test would always pass
        mKeyguardSliceView.setTextColor(Color.RED);
        mKeyguardSliceView.setDarkAmount(0);
        Assert.assertEquals("Should be using regular text color", Color.RED,
                mKeyguardSliceView.getTextColor());
        mKeyguardSliceView.setDarkAmount(1);
        Assert.assertEquals("Should be using AOD text color", Color.WHITE,
                mKeyguardSliceView.getTextColor());
    }

    @Test
    public void onAttachedToWindow_registersListeners() {
        mKeyguardSliceView.onAttachedToWindow();
        verify(mTunerService).addTunable(eq(mKeyguardSliceView), anyString());
        verify(mConfigurationController).addCallback(eq(mKeyguardSliceView));
    }

    @Test
    public void onDetachedFromWindow_unregistersListeners() {
        mKeyguardSliceView.onDetachedFromWindow();
        verify(mTunerService).removeTunable(eq(mKeyguardSliceView));
        verify(mConfigurationController).removeCallback(eq(mKeyguardSliceView));
    }
}
