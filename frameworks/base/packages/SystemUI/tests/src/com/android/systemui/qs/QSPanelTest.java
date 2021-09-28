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

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.UserManager;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;
import android.testing.TestableLooper.RunWithLooper;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.internal.logging.testing.UiEventLoggerFake;
import com.android.settingslib.bluetooth.LocalBluetoothManager;
import com.android.systemui.Dependency;
import com.android.systemui.SysuiTestCase;
import com.android.systemui.broadcast.BroadcastDispatcher;
import com.android.systemui.dump.DumpManager;
import com.android.systemui.media.MediaHost;
import com.android.systemui.plugins.ActivityStarter;
import com.android.systemui.plugins.qs.QSTileView;
import com.android.systemui.qs.customize.QSCustomizer;
import com.android.systemui.qs.logging.QSLogger;
import com.android.systemui.qs.tileimpl.QSTileImpl;
import com.android.systemui.statusbar.notification.NotificationEntryManager;
import com.android.systemui.statusbar.policy.SecurityController;
import com.android.systemui.util.animation.DisappearParameters;
import com.android.systemui.util.animation.UniqueObjectHostView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Collections;

@RunWith(AndroidTestingRunner.class)
@RunWithLooper
@SmallTest
public class QSPanelTest extends SysuiTestCase {

    private MetricsLogger mMetricsLogger;
    private TestableLooper mTestableLooper;
    private QSPanel mQsPanel;
    @Mock
    private QSTileHost mHost;
    @Mock
    private QSCustomizer mCustomizer;
    @Mock
    private QSTileImpl dndTile;
    @Mock
    private BroadcastDispatcher mBroadcastDispatcher;
    @Mock
    private DumpManager mDumpManager;
    @Mock
    private QSLogger mQSLogger;
    private ViewGroup mParentView;
    @Mock
    private QSDetail.Callback mCallback;
    @Mock
    private QSTileView mQSTileView;
    @Mock
    private MediaHost mMediaHost;
    @Mock
    private LocalBluetoothManager mLocalBluetoothManager;
    @Mock
    private ActivityStarter mActivityStarter;
    @Mock
    private NotificationEntryManager mEntryManager;
    private UiEventLoggerFake mUiEventLogger;

    @Before
    public void setup() throws Exception {
        MockitoAnnotations.initMocks(this);
        mTestableLooper = TestableLooper.get(this);

        // Dependencies for QSSecurityFooter
        mDependency.injectTestDependency(ActivityStarter.class, mActivityStarter);
        mDependency.injectMockDependency(SecurityController.class);
        mDependency.injectTestDependency(Dependency.BG_LOOPER, mTestableLooper.getLooper());
        mContext.addMockSystemService(Context.USER_SERVICE, mock(UserManager.class));
        when(mMediaHost.getHostView()).thenReturn(new UniqueObjectHostView(getContext()));
        when(mMediaHost.getDisappearParameters()).thenReturn(new DisappearParameters());

        mUiEventLogger = new UiEventLoggerFake();
        mTestableLooper.runWithLooper(() -> {
            mMetricsLogger = mDependency.injectMockDependency(MetricsLogger.class);
            mQsPanel = new QSPanel(mContext, null, mDumpManager, mBroadcastDispatcher,
                    mQSLogger, mMediaHost, mUiEventLogger);
            mQsPanel.onFinishInflate();
            // Provides a parent with non-zero size for QSPanel
            mParentView = new FrameLayout(mContext);
            mParentView.addView(mQsPanel);

            when(dndTile.getTileSpec()).thenReturn("dnd");
            when(mHost.getTiles()).thenReturn(Collections.emptyList());
            when(mHost.createTileView(any(), anyBoolean())).thenReturn(mQSTileView);

            mQsPanel.setHost(mHost, mCustomizer);
            mQsPanel.addTile(dndTile, true);
            mQsPanel.setCallback(mCallback);
        });
    }

    @Test
    public void testSetExpanded_Metrics() {
        mQsPanel.setExpanded(true);
        verify(mMetricsLogger).visibility(eq(MetricsEvent.QS_PANEL), eq(true));
        verify(mQSLogger).logPanelExpanded(true, mQsPanel.getDumpableTag());
        assertEquals(1, mUiEventLogger.numLogs());
        assertEquals(QSEvent.QS_PANEL_EXPANDED.getId(), mUiEventLogger.eventId(0));
        mUiEventLogger.getLogs().clear();

        mQsPanel.setExpanded(false);
        verify(mMetricsLogger).visibility(eq(MetricsEvent.QS_PANEL), eq(false));
        verify(mQSLogger).logPanelExpanded(false, mQsPanel.getDumpableTag());
        assertEquals(1, mUiEventLogger.numLogs());
        assertEquals(QSEvent.QS_PANEL_COLLAPSED.getId(), mUiEventLogger.eventId(0));
        mUiEventLogger.getLogs().clear();

    }

    @Test
    public void testOpenDetailsWithExistingTile_NoException() {
        mTestableLooper.processAllMessages();
        mQsPanel.openDetails("dnd");
        mTestableLooper.processAllMessages();

        verify(mCallback).onShowingDetail(any(), anyInt(), anyInt());
    }

    @Test
    public void setListening() {
        when(dndTile.getTileSpec()).thenReturn("dnd");

        mQsPanel.setListening(true);
        verify(mQSLogger).logAllTilesChangeListening(true, mQsPanel.getDumpableTag(), "dnd");

        mQsPanel.setListening(false);
        verify(mQSLogger).logAllTilesChangeListening(false, mQsPanel.getDumpableTag(), "dnd");
    }

/*    @Test
    public void testOpenDetailsWithNullParameter_NoException() {
        mTestableLooper.processAllMessages();
        mQsPanel.openDetails(null);
        mTestableLooper.processAllMessages();

        verify(mCallback, never()).onShowingDetail(any(), anyInt(), anyInt());
    }*/

    @Test
    public void testOpenDetailsWithNonExistingTile_NoException() {
        mTestableLooper.processAllMessages();
        mQsPanel.openDetails("invalid-name");
        mTestableLooper.processAllMessages();

        verify(mCallback, never()).onShowingDetail(any(), anyInt(), anyInt());
    }

    @Test
    public void testDump() {
        String mockTileViewString = "Mock Tile View";
        String mockTileString = "Mock Tile";
        doAnswer(invocation -> {
            PrintWriter pw = invocation.getArgument(1);
            pw.println(mockTileString);
            return null;
        }).when(dndTile).dump(any(FileDescriptor.class), any(PrintWriter.class),
                any(String[].class));
        when(mQSTileView.toString()).thenReturn(mockTileViewString);

        StringWriter w = new StringWriter();
        PrintWriter pw = new PrintWriter(w);
        mQsPanel.dump(mock(FileDescriptor.class), pw, new String[]{});
        String expected = "QSPanel:\n"
                + "  Tile records:\n"
                + "    " + mockTileString + "\n"
                + "    " + mockTileViewString + "\n";
        assertEquals(expected, w.getBuffer().toString());
    }

}
