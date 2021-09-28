/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import static android.view.WindowManager.LayoutParams.SYSTEM_FLAG_HIDE_NON_SYSTEM_OVERLAY_WINDOWS;

import android.annotation.Nullable;
import android.app.ActionBar;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.FragmentManager;
import android.app.ListFragment;
import android.app.LoaderManager;
import android.app.NotificationManager;
import android.content.Context;
import android.content.CursorLoader;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.Loader;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Telephony;
import android.telephony.SmsCbMessage;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;
import android.view.ViewGroup;
import android.widget.CursorAdapter;
import android.widget.ListView;
import android.widget.TextView;

import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;

/**
 * This activity provides a list view of received cell broadcasts. Most of the work is handled
 * in the inner CursorLoaderListFragment class.
 */
public class CellBroadcastListActivity extends Activity {

    @VisibleForTesting
    public CursorLoaderListFragment mListFragment;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            // android.R.id.home will be triggered in onOptionsItemSelected()
            actionBar.setDisplayHomeAsUpEnabled(true);
        }

        setTitle(getString(R.string.cb_list_activity_title));

        // Dismiss the notification that brought us here (if any).
        ((NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE))
                .cancel(CellBroadcastAlertService.NOTIFICATION_ID);

        FragmentManager fm = getFragmentManager();

        // Create the list fragment and add it as our sole content.
        if (fm.findFragmentById(android.R.id.content) == null) {
            mListFragment = new CursorLoaderListFragment();
            fm.beginTransaction().add(android.R.id.content, mListFragment).commit();
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        getWindow().addSystemFlags(SYSTEM_FLAG_HIDE_NON_SYSTEM_OVERLAY_WINDOWS);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            // Respond to the action bar's Up/Home button
            case android.R.id.home:
                finish();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /**
     * List fragment queries SQLite database on worker thread.
     */
    public static class CursorLoaderListFragment extends ListFragment
            implements LoaderManager.LoaderCallbacks<Cursor> {
        private static final String TAG = CellBroadcastListActivity.class.getSimpleName();
        private static final boolean DBG = true;

        // IDs of the main menu items.
        @VisibleForTesting
        public static final int MENU_DELETE_ALL            = 3;
        @VisibleForTesting
        public static final int MENU_SHOW_REGULAR_MESSAGES = 4;
        @VisibleForTesting
        public static final int MENU_SHOW_ALL_MESSAGES     = 5;

        // Load the history from cell broadcast receiver database
        private static final int LOADER_NORMAL_HISTORY      = 1;
        // Load the history from cell broadcast service. This will include all non-shown messages.
        @VisibleForTesting
        public static final int LOADER_HISTORY_FROM_CBS    = 2;

        @VisibleForTesting
        public static final String KEY_LOADER_ID = "loader_id";

        // IDs of the context menu items (package local, accessed from inner DeleteThreadListener).
        @VisibleForTesting
        public static final int MENU_DELETE               = 0;
        @VisibleForTesting
        public static final int MENU_VIEW_DETAILS         = 1;

        // cell broadcast provider from cell broadcast service.
        public static final Uri CONTENT_URI = Uri.parse("content://cellbroadcasts");

        // Query columns for provider from cell broadcast service.
        public static final String[] QUERY_COLUMNS = {
                Telephony.CellBroadcasts._ID,
                Telephony.CellBroadcasts.SLOT_INDEX,
                Telephony.CellBroadcasts.SUBSCRIPTION_ID,
                Telephony.CellBroadcasts.GEOGRAPHICAL_SCOPE,
                Telephony.CellBroadcasts.PLMN,
                Telephony.CellBroadcasts.LAC,
                Telephony.CellBroadcasts.CID,
                Telephony.CellBroadcasts.SERIAL_NUMBER,
                Telephony.CellBroadcasts.SERVICE_CATEGORY,
                Telephony.CellBroadcasts.LANGUAGE_CODE,
                Telephony.CellBroadcasts.DATA_CODING_SCHEME,
                Telephony.CellBroadcasts.MESSAGE_BODY,
                Telephony.CellBroadcasts.MESSAGE_FORMAT,
                Telephony.CellBroadcasts.MESSAGE_PRIORITY,
                Telephony.CellBroadcasts.ETWS_WARNING_TYPE,
                Telephony.CellBroadcasts.CMAS_MESSAGE_CLASS,
                Telephony.CellBroadcasts.CMAS_CATEGORY,
                Telephony.CellBroadcasts.CMAS_RESPONSE_TYPE,
                Telephony.CellBroadcasts.CMAS_SEVERITY,
                Telephony.CellBroadcasts.CMAS_URGENCY,
                Telephony.CellBroadcasts.CMAS_CERTAINTY,
                Telephony.CellBroadcasts.RECEIVED_TIME,
                Telephony.CellBroadcasts.LOCATION_CHECK_TIME,
                Telephony.CellBroadcasts.MESSAGE_BROADCASTED,
                Telephony.CellBroadcasts.MESSAGE_DISPLAYED,
                Telephony.CellBroadcasts.GEOMETRIES,
                Telephony.CellBroadcasts.MAXIMUM_WAIT_TIME
        };

        // This is the Adapter being used to display the list's data.
        @VisibleForTesting
        public CursorAdapter mAdapter;

        private int mCurrentLoaderId = 0;

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // We have a menu item to show in action bar.
            setHasOptionsMenu(true);
        }

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container,
                Bundle savedInstanceState) {
            return inflater.inflate(R.layout.cell_broadcast_list_screen, container, false);
        }

        @Override
        public void onActivityCreated(Bundle savedInstanceState) {
            super.onActivityCreated(savedInstanceState);

            // Set context menu for long-press.
            ListView listView = getListView();
            listView.setOnCreateContextMenuListener(mOnCreateContextMenuListener);

            // Create a cursor adapter to display the loaded data.
            mAdapter = new CellBroadcastCursorAdapter(getActivity());
            setListAdapter(mAdapter);

            mCurrentLoaderId = LOADER_NORMAL_HISTORY;
            if (savedInstanceState != null && savedInstanceState.containsKey(KEY_LOADER_ID)) {
                mCurrentLoaderId = savedInstanceState.getInt(KEY_LOADER_ID);
            }

            if (DBG) Log.d(TAG, "onActivityCreated: id=" + mCurrentLoaderId);

            // Prepare the loader.  Either re-connect with an existing one,
            // or start a new one.
            getLoaderManager().initLoader(mCurrentLoaderId, null, this);
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
            // Save the current id for later restoring activity.
            if (DBG) Log.d(TAG, "onSaveInstanceState: id=" + mCurrentLoaderId);
            outState.putInt(KEY_LOADER_ID, mCurrentLoaderId);
        }

        @Override
        public void onResume() {
            super.onResume();
            if (DBG) Log.d(TAG, "onResume");
            if (mCurrentLoaderId != 0) {
                getLoaderManager().restartLoader(mCurrentLoaderId, null, this);
            }
        }

        @Override
        public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
            menu.add(0, MENU_DELETE_ALL, 0, R.string.menu_delete_all).setIcon(
                    android.R.drawable.ic_menu_delete);
            menu.add(0, MENU_SHOW_ALL_MESSAGES, 0, R.string.show_all_messages);
            menu.add(0, MENU_SHOW_REGULAR_MESSAGES, 0, R.string.show_regular_messages);
        }

        @Override
        public void onPrepareOptionsMenu(Menu menu) {
            boolean isTestingMode = CellBroadcastReceiver.isTestingMode(
                    getContext());
            // Only allowing delete all messages when not in testing mode because when testing mode
            // is enabled, the database source is from cell broadcast service. Deleting them does
            // not affect the database in cell broadcast receiver. Hide the options to reduce
            // confusion.
            menu.findItem(MENU_DELETE_ALL).setVisible(!mAdapter.isEmpty() && !isTestingMode);
            menu.findItem(MENU_SHOW_ALL_MESSAGES).setVisible(isTestingMode
                    && mCurrentLoaderId == LOADER_NORMAL_HISTORY);
            menu.findItem(MENU_SHOW_REGULAR_MESSAGES).setVisible(isTestingMode
                    && mCurrentLoaderId == LOADER_HISTORY_FROM_CBS);
        }

        @Override
        public void onListItemClick(ListView l, View v, int position, long id) {
            CellBroadcastListItem cbli = (CellBroadcastListItem) v;
            showDialogAndMarkRead(cbli.getMessage());
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            mCurrentLoaderId = id;
            if (id == LOADER_NORMAL_HISTORY) {
                Log.d(TAG, "onCreateLoader: normal history.");
                return new CursorLoader(getActivity(), CellBroadcastContentProvider.CONTENT_URI,
                        CellBroadcastDatabaseHelper.QUERY_COLUMNS, null, null,
                        Telephony.CellBroadcasts.DELIVERY_TIME + " DESC");
            } else if (id == LOADER_HISTORY_FROM_CBS) {
                Log.d(TAG, "onCreateLoader: history from cell broadcast service");
                return new CursorLoader(getActivity(), CONTENT_URI,
                        QUERY_COLUMNS, null, null,
                        Telephony.CellBroadcasts.RECEIVED_TIME + " DESC");
            }

            return null;
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor data) {
            if (DBG) Log.d(TAG, "onLoadFinished");
            // Swap the new cursor in.  (The framework will take care of closing the
            // old cursor once we return.)
            mAdapter.swapCursor(data);
            getActivity().invalidateOptionsMenu();
            updateNoAlertTextVisibility();
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (DBG) Log.d(TAG, "onLoaderReset");
            // This is called when the last Cursor provided to onLoadFinished()
            // above is about to be closed.  We need to make sure we are no
            // longer using it.
            mAdapter.swapCursor(null);
        }

        private void showDialogAndMarkRead(SmsCbMessage message) {
            // show emergency alerts with the warning icon, but don't play alert tone
            Intent i = new Intent(getActivity(), CellBroadcastAlertDialog.class);
            ArrayList<SmsCbMessage> messageList = new ArrayList<>();
            messageList.add(message);
            i.putParcelableArrayListExtra(CellBroadcastAlertService.SMS_CB_MESSAGE_EXTRA,
                    messageList);
            startActivity(i);
        }

        private void showBroadcastDetails(SmsCbMessage message, long locationCheckTime,
                                          boolean messageDisplayed, String geometry) {
            // show dialog with delivery date/time and alert details
            CharSequence details = CellBroadcastResources.getMessageDetails(getActivity(),
                    mCurrentLoaderId == LOADER_HISTORY_FROM_CBS, message, locationCheckTime,
                    messageDisplayed, geometry);
            int titleId = (mCurrentLoaderId == LOADER_NORMAL_HISTORY)
                    ? R.string.view_details_title : R.string.view_details_debugging_title;
            new AlertDialog.Builder(getActivity())
                    .setTitle(titleId)
                    .setMessage(details)
                    .setCancelable(true)
                    .show();
        }

        private final OnCreateContextMenuListener mOnCreateContextMenuListener =
                (menu, v, menuInfo) -> {
                    menu.setHeaderTitle(R.string.message_options);
                    menu.add(0, MENU_VIEW_DETAILS, 0, R.string.menu_view_details);
                    if (mCurrentLoaderId == LOADER_NORMAL_HISTORY) {
                        menu.add(0, MENU_DELETE, 0, R.string.menu_delete);
                    }
                };

        private void updateNoAlertTextVisibility() {
            TextView noAlertsTextView = getActivity().findViewById(R.id.empty);
            if (noAlertsTextView != null) {
                noAlertsTextView.setVisibility(!hasAlertsInHistory()
                        ? View.VISIBLE : View.INVISIBLE);
            }
        }

        /**
         * @return {@code true} if the alert history database has any item
         */
        private boolean hasAlertsInHistory() {
            return mAdapter.getCursor().getCount() > 0;
        }

        /**
         * Get the location check time of the message.
         *
         * @param cursor The cursor of the database
         * @return The EPOCH time in milliseconds that the location check was performed on the
         * message. -1 if the information is not available.
         */
        private long getLocationCheckTime(Cursor cursor) {
            if (mCurrentLoaderId != LOADER_HISTORY_FROM_CBS) return -1;
            return cursor.getLong(cursor.getColumnIndex(
                    Telephony.CellBroadcasts.LOCATION_CHECK_TIME));
        }

        /**
         * Check if the message has been displayed to the user or not
         *
         * @param cursor The cursor of the database
         * @return {@code true} if the message was displayed to the user, otherwise {@code false}.
         */
        private boolean wasMessageDisplayed(Cursor cursor) {
            if (mCurrentLoaderId != LOADER_HISTORY_FROM_CBS) return true;
            return cursor.getInt(cursor.getColumnIndex(
                    Telephony.CellBroadcasts.MESSAGE_DISPLAYED)) != 0;
        }

        /**
         * Get the geometry string from the message if available.
         *
         * @param cursor The cursor of the database
         * @return The geometry string
         */
        private @Nullable String getGeometryString(Cursor cursor) {
            if (mCurrentLoaderId != LOADER_HISTORY_FROM_CBS) return null;
            if (cursor.getColumnIndex(Telephony.CellBroadcasts.GEOMETRIES) >= 0) {
                return cursor.getString(cursor.getColumnIndex(Telephony.CellBroadcasts.GEOMETRIES));
            }
            return null;
        }

        @Override
        public boolean onContextItemSelected(MenuItem item) {
            Cursor cursor = mAdapter.getCursor();
            if (cursor != null && cursor.getPosition() >= 0) {
                switch (item.getItemId()) {
                    case MENU_DELETE:
                        confirmDeleteThread(cursor.getLong(cursor.getColumnIndexOrThrow(
                                Telephony.CellBroadcasts._ID)));
                        break;

                    case MENU_VIEW_DETAILS:
                        showBroadcastDetails(CellBroadcastCursorAdapter.createFromCursor(
                                getContext(), cursor), getLocationCheckTime(cursor),
                                wasMessageDisplayed(cursor), getGeometryString(cursor));
                        break;

                    default:
                        break;
                }
            }
            return super.onContextItemSelected(item);
        }

        @Override
        public boolean onOptionsItemSelected(MenuItem item) {
            switch(item.getItemId()) {
                case MENU_DELETE_ALL:
                    confirmDeleteThread(-1);
                    break;

                case MENU_SHOW_ALL_MESSAGES:
                    getLoaderManager().restartLoader(LOADER_HISTORY_FROM_CBS, null, this);
                    break;

                case MENU_SHOW_REGULAR_MESSAGES:
                    getLoaderManager().restartLoader(LOADER_NORMAL_HISTORY, null, this);
                    break;

                default:
                    return true;
            }
            return false;
        }

        /**
         * Start the process of putting up a dialog to confirm deleting a broadcast.
         * @param rowId the row ID of the broadcast to delete, or -1 to delete all broadcasts
         */
        public void confirmDeleteThread(long rowId) {
            DeleteThreadListener listener = new DeleteThreadListener(rowId);
            confirmDeleteThreadDialog(listener, (rowId == -1), getActivity());
        }

        /**
         * Build and show the proper delete broadcast dialog. The UI is slightly different
         * depending on whether there are locked messages in the thread(s) and whether we're
         * deleting a single broadcast or all broadcasts.
         * @param listener gets called when the delete button is pressed
         * @param deleteAll whether to show a single thread or all threads UI
         * @param context used to load the various UI elements
         */
        public static void confirmDeleteThreadDialog(DeleteThreadListener listener,
                boolean deleteAll, Context context) {
            AlertDialog.Builder builder = new AlertDialog.Builder(context);
            builder.setIconAttribute(android.R.attr.alertDialogIcon)
                    .setCancelable(true)
                    .setPositiveButton(R.string.button_delete, listener)
                    .setNegativeButton(R.string.button_cancel, null)
                    .setMessage(deleteAll ? R.string.confirm_delete_all_broadcasts
                            : R.string.confirm_delete_broadcast)
                    .show();
        }

        public class DeleteThreadListener implements OnClickListener {
            private final long mRowId;

            public DeleteThreadListener(long rowId) {
                mRowId = rowId;
            }

            @Override
            public void onClick(DialogInterface dialog, int whichButton) {
                // delete from database on a background thread
                new CellBroadcastContentProvider.AsyncCellBroadcastTask(
                        getActivity().getContentResolver()).execute(
                                (CellBroadcastContentProvider.CellBroadcastOperation) provider -> {
                                    if (mRowId != -1) {
                                        return provider.deleteBroadcast(mRowId);
                                    } else {
                                        return provider.deleteAllBroadcasts();
                                    }
                                });

                dialog.dismiss();
            }
        }
    }
}
