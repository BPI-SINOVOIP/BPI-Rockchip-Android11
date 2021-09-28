/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.systemui.qs;

import static com.android.internal.logging.nano.MetricsProto.MetricsEvent.ACTION_QS_MORE_SETTINGS;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;
import android.testing.TestableLooper.RunWithLooper;
import android.testing.ViewUtils;
import android.view.LayoutInflater;
import android.view.View;

import androidx.test.filters.SmallTest;

import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.testing.UiEventLoggerFake;
import com.android.systemui.R;
import com.android.systemui.SysuiTestCase;
import com.android.systemui.plugins.ActivityStarter;
import com.android.systemui.plugins.qs.DetailAdapter;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidTestingRunner.class)
@RunWithLooper
@SmallTest
public class QSDetailTest extends SysuiTestCase {

    private MetricsLogger mMetricsLogger;
    private QSDetail mQsDetail;
    private QSPanel mQsPanel;
    private QuickStatusBarHeader mQuickHeader;
    private ActivityStarter mActivityStarter;
    private DetailAdapter mMockDetailAdapter;
    private TestableLooper mTestableLooper;
    private UiEventLoggerFake mUiEventLogger;

    @Before
    public void setup() throws Exception {
        mTestableLooper = TestableLooper.get(this);
        mUiEventLogger = QSEvents.INSTANCE.setLoggerForTesting();

        mTestableLooper.runWithLooper(() -> {
            mMetricsLogger = mDependency.injectMockDependency(MetricsLogger.class);
            mActivityStarter = mDependency.injectMockDependency(ActivityStarter.class);
            mQsDetail = (QSDetail) LayoutInflater.from(mContext).inflate(R.layout.qs_detail, null);
            mQsPanel = mock(QSPanel.class);
            mQuickHeader = mock(QuickStatusBarHeader.class);
            mQsDetail.setQsPanel(mQsPanel, mQuickHeader, mock(View.class));

            mMockDetailAdapter = mock(DetailAdapter.class);
            when(mMockDetailAdapter.createDetailView(any(), any(), any()))
                    .thenReturn(mock(View.class));
        });

        // Only detail in use is the user detail
        when(mMockDetailAdapter.openDetailEvent())
                .thenReturn(QSUserSwitcherEvent.QS_USER_DETAIL_OPEN);
        when(mMockDetailAdapter.closeDetailEvent())
                .thenReturn(QSUserSwitcherEvent.QS_USER_DETAIL_CLOSE);
        when(mMockDetailAdapter.moreSettingsEvent())
                .thenReturn(QSUserSwitcherEvent.QS_USER_MORE_SETTINGS);
    }

    @After
    public void tearDown() {
        QSEvents.INSTANCE.resetLogger();
    }

    @Test
    public void testShowDetail_Metrics() {
        ViewUtils.attachView(mQsDetail);
        mTestableLooper.processAllMessages();

        mQsDetail.handleShowingDetail(mMockDetailAdapter, 0, 0, false);
        verify(mMetricsLogger).visible(eq(mMockDetailAdapter.getMetricsCategory()));
        assertEquals(1, mUiEventLogger.numLogs());
        assertEquals(QSUserSwitcherEvent.QS_USER_DETAIL_OPEN.getId(), mUiEventLogger.eventId(0));
        mUiEventLogger.getLogs().clear();

        mQsDetail.handleShowingDetail(null, 0, 0, false);
        verify(mMetricsLogger).hidden(eq(mMockDetailAdapter.getMetricsCategory()));

        assertEquals(1, mUiEventLogger.numLogs());
        assertEquals(QSUserSwitcherEvent.QS_USER_DETAIL_CLOSE.getId(), mUiEventLogger.eventId(0));

        ViewUtils.detachView(mQsDetail);
        mTestableLooper.processAllMessages();
    }

    @Test
    public void testMoreSettingsButton() {
        ViewUtils.attachView(mQsDetail);
        mTestableLooper.processAllMessages();

        mQsDetail.handleShowingDetail(mMockDetailAdapter, 0, 0, false);
        mUiEventLogger.getLogs().clear();
        mQsDetail.requireViewById(android.R.id.button2).performClick();

        int metricsCategory = mMockDetailAdapter.getMetricsCategory();
        verify(mMetricsLogger).action(eq(ACTION_QS_MORE_SETTINGS), eq(metricsCategory));
        assertEquals(1, mUiEventLogger.numLogs());
        assertEquals(QSUserSwitcherEvent.QS_USER_MORE_SETTINGS.getId(), mUiEventLogger.eventId(0));

        verify(mActivityStarter).postStartActivityDismissingKeyguard(any(), anyInt());

        ViewUtils.detachView(mQsDetail);
        mTestableLooper.processAllMessages();
    }

    @Test
    public void testNullAdapterClick() {
        DetailAdapter mock = mock(DetailAdapter.class);
        when(mock.moreSettingsEvent()).thenReturn(DetailAdapter.INVALID);
        mQsDetail.setupDetailFooter(mock);
        mQsDetail.requireViewById(android.R.id.button2).performClick();
    }
}
