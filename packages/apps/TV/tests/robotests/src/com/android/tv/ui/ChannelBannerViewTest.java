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
 * limitations under the License.
 */
package com.android.tv.ui;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import com.android.tv.R;
import com.android.tv.common.flags.impl.DefaultLegacyFlags;
import com.android.tv.common.singletons.HasSingletons;
import com.android.tv.data.api.Channel;
import com.android.tv.data.api.Program;
import com.android.tv.dvr.DvrManager;
import com.android.tv.testing.TvRobolectricTestRunner;
import com.android.tv.testing.constants.ConfigConstants;
import com.android.tv.ui.ChannelBannerView.MySingletons;
import com.android.tv.util.TvInputManagerHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import javax.inject.Provider;

/** Tests for {@link ChannelBannerView}. */
@RunWith(TvRobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class ChannelBannerViewTest {

    private TestActivity testActivity;
    private ChannelBannerView mChannelBannerView;
    private ImageView mChannelSignalStrengthView;

    @Before
    public void setUp() {
        testActivity = Robolectric.buildActivity(TestActivity.class).create().get();
        mChannelBannerView =
                (ChannelBannerView)
                        LayoutInflater.from(testActivity).inflate(R.layout.channel_banner, null);
        mChannelSignalStrengthView = mChannelBannerView.findViewById(R.id.channel_signal_strength);
    }

    @Test
    public void updateChannelSignalStrengthView_valueIsNotValid() {
        mChannelBannerView.updateChannelSignalStrengthView(-1);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.GONE);
        mChannelBannerView.updateChannelSignalStrengthView(101);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void updateChannelSignalStrengthView_20() {
        mChannelBannerView.updateChannelSignalStrengthView(20);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(shadowOf(mChannelSignalStrengthView.getDrawable()).getCreatedFromResId())
                .isEqualTo(R.drawable.quantum_ic_signal_cellular_0_bar_white_24);
    }

    @Test
    public void updateChannelSignalStrengthView_40() {
        mChannelBannerView.updateChannelSignalStrengthView(40);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(shadowOf(mChannelSignalStrengthView.getDrawable()).getCreatedFromResId())
                .isEqualTo(R.drawable.quantum_ic_signal_cellular_1_bar_white_24);
    }

    @Test
    public void updateChannelSignalStrengthView_60() {
        mChannelBannerView.updateChannelSignalStrengthView(60);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(shadowOf(mChannelSignalStrengthView.getDrawable()).getCreatedFromResId())
                .isEqualTo(R.drawable.quantum_ic_signal_cellular_2_bar_white_24);
    }

    @Test
    public void updateChannelSignalStrengthView_80() {
        mChannelBannerView.updateChannelSignalStrengthView(80);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(shadowOf(mChannelSignalStrengthView.getDrawable()).getCreatedFromResId())
                .isEqualTo(R.drawable.quantum_ic_signal_cellular_3_bar_white_24);
    }

    @Test
    public void updateChannelSignalStrengthView_100() {
        mChannelBannerView.updateChannelSignalStrengthView(100);
        assertThat(mChannelSignalStrengthView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(shadowOf(mChannelSignalStrengthView.getDrawable()).getCreatedFromResId())
                .isEqualTo(R.drawable.quantum_ic_signal_cellular_4_bar_white_24);
    }

    /** An activity for {@link ChannelBannerViewTest}. */
    public static class TestActivity extends Activity implements HasSingletons<MySingletons> {

        MySingletonsImpl mSingletons = new MySingletonsImpl();
        Context mContext = this;

        @Override
        public ChannelBannerView.MySingletons singletons() {
            return mSingletons;
        }

        /** MySingletons implementation needed for this class. */
        public class MySingletonsImpl implements ChannelBannerView.MySingletons {

            @Override
            public Provider<Channel> getCurrentChannelProvider() {
                return null;
            }

            @Override
            public Provider<Program> getCurrentProgramProvider() {
                return null;
            }

            @Override
            public Provider<TvOverlayManager> getOverlayManagerProvider() {
                return null;
            }

            @Override
            public TvInputManagerHelper getTvInputManagerHelperSingleton() {
                return new TvInputManagerHelper(mContext, DefaultLegacyFlags.DEFAULT);
            }

            @Override
            public Provider<Long> getCurrentPlayingPositionProvider() {
                return null;
            }

            @Override
            public DvrManager getDvrManagerSingleton() {
                return null;
            }
        }
    }
}
