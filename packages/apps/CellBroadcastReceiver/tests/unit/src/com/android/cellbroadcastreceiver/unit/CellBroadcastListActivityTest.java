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

package com.android.cellbroadcastreceiver.unit;

import static android.provider.Telephony.CellBroadcasts.CID;
import static android.provider.Telephony.CellBroadcasts.CMAS_CATEGORY;
import static android.provider.Telephony.CellBroadcasts.CMAS_CERTAINTY;
import static android.provider.Telephony.CellBroadcasts.CMAS_MESSAGE_CLASS;
import static android.provider.Telephony.CellBroadcasts.CMAS_RESPONSE_TYPE;
import static android.provider.Telephony.CellBroadcasts.CMAS_SEVERITY;
import static android.provider.Telephony.CellBroadcasts.CMAS_URGENCY;
import static android.provider.Telephony.CellBroadcasts.DATA_CODING_SCHEME;
import static android.provider.Telephony.CellBroadcasts.DELIVERY_TIME;
import static android.provider.Telephony.CellBroadcasts.ETWS_WARNING_TYPE;
import static android.provider.Telephony.CellBroadcasts.LAC;
import static android.provider.Telephony.CellBroadcasts.MAXIMUM_WAIT_TIME;
import static android.provider.Telephony.CellBroadcasts.PLMN;

import static com.android.cellbroadcastreceiver.CellBroadcastListActivity.CursorLoaderListFragment.KEY_LOADER_ID;
import static com.android.cellbroadcastreceiver.CellBroadcastListActivity.CursorLoaderListFragment.LOADER_HISTORY_FROM_CBS;
import static com.android.cellbroadcastreceiver.CellBroadcastListActivity.CursorLoaderListFragment.MENU_DELETE;
import static com.android.cellbroadcastreceiver.CellBroadcastListActivity.CursorLoaderListFragment.MENU_DELETE_ALL;
import static com.android.cellbroadcastreceiver.CellBroadcastListActivity.CursorLoaderListFragment.MENU_VIEW_DETAILS;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.NotificationManager;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.Bundle;
import android.os.Looper;
import android.provider.Telephony;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;

import com.android.cellbroadcastreceiver.CellBroadcastAlertService;
import com.android.cellbroadcastreceiver.CellBroadcastListActivity;
import com.android.cellbroadcastreceiver.CellBroadcastListItem;
import com.android.cellbroadcastreceiver.R;

import org.junit.After;
import org.junit.Before;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.List;

public class CellBroadcastListActivityTest extends
        CellBroadcastActivityTestCase<CellBroadcastListActivity> {

    @Mock
    private NotificationManager mMockNotificationManager;


    @Captor
    private ArgumentCaptor<String> mColumnCaptor;

    @Before
    public void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        injectSystemService(NotificationManager.class, mMockNotificationManager);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    public CellBroadcastListActivityTest() {
        super(CellBroadcastListActivity.class);
    }

    public void testOnCreate() throws Throwable {
        startActivity();
        verify(mMockNotificationManager).cancel(eq(CellBroadcastAlertService.NOTIFICATION_ID));
        stopActivity();
    }

    public void testOnListItemClick() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);
        // onListItemClick only checks the view
        View view = new CellBroadcastListItem(mContext, null);
        boolean startActivityWasCalled = false;
        try {
            activity.mListFragment.onListItemClick(null, view, 0, 0);
        } catch (NullPointerException e) {
            // NullPointerException is thrown in startActivity because we have no application thread
            startActivityWasCalled = true;
        }
        assertTrue("onListItemClick should call startActivity", startActivityWasCalled);
        stopActivity();
    }

    public void testOnActivityCreatedLoaderHistoryFromCbs() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        // Override the loader ID to use history from CellBroadcastService
        int testLoaderId = LOADER_HISTORY_FROM_CBS;
        Bundle savedInstanceState = new Bundle();
        savedInstanceState.putInt(KEY_LOADER_ID, testLoaderId);

        // Looper.prepare must be called for the CursorLoader to query an outside ContentProvider
        Looper.prepare();
        activity.mListFragment.onActivityCreated(savedInstanceState);
        assertNotNull(activity.mListFragment.getLoaderManager());
        stopActivity();
    }

    public void testOnLoadFinishedWithData() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        // create data with one entry so that the "no alert" text view is invisible
        MatrixCursor data =
                new MatrixCursor(CellBroadcastListActivity.CursorLoaderListFragment.QUERY_COLUMNS);
        data.addRow(new Object[] {
                0, //Telephony.CellBroadcasts._ID,
                0, //Telephony.CellBroadcasts.SLOT_INDEX,
                1, //Telephony.CellBroadcasts.SUBSCRIPTION_ID,
                -1, //Telephony.CellBroadcasts.GEOGRAPHICAL_SCOPE,
                "", //Telephony.CellBroadcasts.PLMN,
                0, //Telephony.CellBroadcasts.LAC,
                0, //Telephony.CellBroadcasts.CID,
                "", //Telephony.CellBroadcasts.SERIAL_NUMBER,
                0, //Telephony.CellBroadcasts.SERVICE_CATEGORY,
                "", //Telephony.CellBroadcasts.LANGUAGE_CODE,
                0, //Telephony.CellBroadcasts.DATA_CODING_SCHEME,
                "", //Telephony.CellBroadcasts.MESSAGE_BODY,
                0, //Telephony.CellBroadcasts.MESSAGE_FORMAT,
                0, //Telephony.CellBroadcasts.MESSAGE_PRIORITY,
                0, //Telephony.CellBroadcasts.ETWS_WARNING_TYPE,
                0, //Telephony.CellBroadcasts.CMAS_MESSAGE_CLASS,
                0, //Telephony.CellBroadcasts.CMAS_CATEGORY,
                0, //Telephony.CellBroadcasts.CMAS_RESPONSE_TYPE,
                0, //Telephony.CellBroadcasts.CMAS_SEVERITY,
                0, //Telephony.CellBroadcasts.CMAS_URGENCY,
                0, //Telephony.CellBroadcasts.CMAS_CERTAINTY,
                0, //Telephony.CellBroadcasts.RECEIVED_TIME,
                0, //Telephony.CellBroadcasts.LOCATION_CHECK_TIME,
                false, //Telephony.CellBroadcasts.MESSAGE_BROADCASTED,
                true, //Telephony.CellBroadcasts.MESSAGE_DISPLAYED,
                "", //Telephony.CellBroadcasts.GEOMETRIES,
                0 //Telephony.CellBroadcasts.MAXIMUM_WAIT_TIME
        });
        activity.mListFragment.onLoadFinished(null, data);
        assertEquals(View.INVISIBLE, activity.findViewById(R.id.empty).getVisibility());
        stopActivity();
    }

    public void testOnLoadFinishedEmptyData() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        // create empty data so that the "no alert" text view becomes visible
        Cursor data =
                new MatrixCursor(CellBroadcastListActivity.CursorLoaderListFragment.QUERY_COLUMNS);
        activity.mListFragment.onLoadFinished(null, data);
        assertEquals(View.VISIBLE, activity.findViewById(R.id.empty).getVisibility());
        stopActivity();
    }

    public void testOnLoaderReset() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        activity.mListFragment.onLoaderReset(null);
        assertNull("mAdapter.getCursor() should be null after reset",
                activity.mListFragment.mAdapter.getCursor());
        stopActivity();
    }

    public void testOnContextItemSelectedDelete() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        // Mock out the adapter cursor
        Cursor mockCursor = mock(Cursor.class);
        doReturn(0).when(mockCursor).getPosition();
        doReturn(0L).when(mockCursor).getLong(anyInt());
        activity.mListFragment.mAdapter.swapCursor(mockCursor);

        // create mock delete menu item
        MenuItem mockMenuItem = mock(MenuItem.class);
        doReturn(MENU_DELETE).when(mockMenuItem).getItemId();

        // must call looper.prepare to create alertdialog
        Looper.prepare();
        boolean alertDialogCreated = false;
        try {
            activity.mListFragment.onContextItemSelected(mockMenuItem);
        } catch (WindowManager.BadTokenException e) {
            // We can't mock WindowManager because WindowManagerImpl is final, so instead we just
            // verify that this exception is thrown when we try to create the AlertDialog
            alertDialogCreated = true;
        }

        assertTrue("onContextItemSelected - MENU_DELETE should create alert dialog",
                alertDialogCreated);
        verify(mockCursor, atLeastOnce()).getColumnIndexOrThrow(eq(Telephony.CellBroadcasts._ID));
        stopActivity();
    }

    public void testOnContextItemSelectedViewDetails() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        // Mock out the adapter cursor
        Cursor mockCursor = mock(Cursor.class);
        doReturn(1).when(mockCursor).getPosition();
        doReturn(0L).when(mockCursor).getLong(anyInt());
        activity.mListFragment.mAdapter.swapCursor(mockCursor);

        // create mock delete menu item
        MenuItem mockMenuItem = mock(MenuItem.class);
        doReturn(MENU_VIEW_DETAILS).when(mockMenuItem).getItemId();

        // must call looper.prepare to create alertdialog
        Looper.prepare();
        boolean alertDialogCreated = false;
        try {
            activity.mListFragment.onContextItemSelected(mockMenuItem);
        } catch (WindowManager.BadTokenException e) {
            // We can't mock WindowManager because WindowManagerImpl is final, so instead we just
            // verify that this exception is thrown when we try to create the AlertDialog
            alertDialogCreated = true;
        }

        assertTrue("onContextItemSelected - MENU_VIEW_DETAILS should create alert dialog",
                alertDialogCreated);

        // getColumnIndex is called 13 times within CellBroadcastCursorAdapter.createFromCursor
        verify(mockCursor, times(13)).getColumnIndex(mColumnCaptor.capture());
        List<String> columns = mColumnCaptor.getAllValues();
        assertTrue(contains(columns, PLMN));
        assertTrue(contains(columns, LAC));
        assertTrue(contains(columns, CID));
        assertTrue(contains(columns, ETWS_WARNING_TYPE));
        assertTrue(contains(columns, CMAS_MESSAGE_CLASS));
        assertTrue(contains(columns, CMAS_CATEGORY));
        assertTrue(contains(columns, CMAS_RESPONSE_TYPE));
        assertTrue(contains(columns, CMAS_SEVERITY));
        assertTrue(contains(columns, CMAS_URGENCY));
        assertTrue(contains(columns, CMAS_CERTAINTY));
        assertTrue(contains(columns, DELIVERY_TIME));
        assertTrue(contains(columns, DATA_CODING_SCHEME));
        assertTrue(contains(columns, MAXIMUM_WAIT_TIME));
        stopActivity();
    }

    private boolean contains(List<String> columns, String column) {
        for (String c : columns) {
            if (c.equals(column)) {
                return true;
            }
        }
        return false;
    }

    public void testOnOptionsItemSelected() throws Throwable {
        CellBroadcastListActivity activity = startActivity();
        assertNotNull(activity.mListFragment);

        // create mock delete all menu item
        MenuItem mockMenuItem = mock(MenuItem.class);
        doReturn(MENU_DELETE_ALL).when(mockMenuItem).getItemId();

        // must call looper.prepare to create alertdialog
        Looper.prepare();
        boolean alertDialogCreated = false;
        try {
            activity.mListFragment.onOptionsItemSelected(mockMenuItem);
        } catch (WindowManager.BadTokenException e) {
            // We can't mock WindowManager because WindowManagerImpl is final, so instead we just
            // verify that this exception is thrown when we try to create the AlertDialog
            alertDialogCreated = true;
        }

        assertTrue("onContextItemSelected - MENU_DELETE_ALL should create alert dialog",
                alertDialogCreated);
        stopActivity();
    }
}
