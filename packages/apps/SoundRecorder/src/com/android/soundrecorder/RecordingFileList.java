package com.android.soundrecorder;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.provider.MediaStore;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.SimpleAdapter;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class RecordingFileList extends Activity implements
        ImageButton.OnClickListener {
    private static final String TAG = "SoundRecorder/RecordingFileList";
    ListView mRecordingFileListView;
    ImageButton mRecordButton;
    ImageButton mDeleteButton;
    View mEmptyView;
    private ArrayList<HashMap<String, Object>> mArrlist;
    private ArrayList<String> mNameList;
    private ArrayList<String> mPathList;
    private ArrayList<String> mTitleList;
    private ArrayList<String> mDurationList;
    private List<Integer> mIdList;

    private static final int NORMAL = 1;
    private static final int EDIT = 2;
    private int mCurrentDapterMode = NORMAL;
    private int mSelection = 0;
    private int mTop = 0;

    private static final String PLAY = "play";
    private static final String RECORD = "record";
    private static final String INIT = "init";
    private static final String DOWHAT = "dowhat";
    private static final String PATH = "path";
    private static final String DURATION = "duration";
    private static final String FILE_NAME = "filename";
    private static final String CREAT_DATE = "creatdate";
    private static final String FORMAT_DURATION = "formatduration";
    private static final String RECORD_ID = "recordid";
    private static final String SINGLE = "Single";
    private boolean mActivityForeground = true;
    private List<Integer> mCheckedList;

    private BroadcastReceiver mSDCardMountEventReceiver = null;
    private static final int ALERT_DELETE_DIALOG = 1;

    @Override
    public void onCreate(Bundle icycle) {
        super.onCreate(icycle);
        setContentView(R.layout.recording_file_list);
        mRecordingFileListView = (ListView) findViewById(R.id.recording_file_list_view);
        mRecordButton = (ImageButton) findViewById(R.id.recordButton);
        mDeleteButton = (ImageButton) findViewById(R.id.deleteButton);
        mEmptyView = findViewById(R.id.empty_view);
        mRecordButton.setOnClickListener(this);
        mDeleteButton.setOnClickListener(this);
        mRecordingFileListView.setOnCreateContextMenuListener(this);

        mRecordingFileListView
                .setOnItemClickListener(new AdapterView.OnItemClickListener() {
                    @Override
                    public void onItemClick(AdapterView<?> arg0, View view,
                                            int arg2, long arg3) {
                        if (mCurrentDapterMode == EDIT) {
                            int id = (int) ((EditViewAdapter) mRecordingFileListView
                                    .getAdapter()).getItemId(arg2);
                            CheckBox checkBox = (CheckBox) view
                                    .findViewById(R.id.record_file_checkbox);
                            if (checkBox.isChecked()) {
                                checkBox.setChecked(false);
                                ((EditViewAdapter) mRecordingFileListView
                                        .getAdapter()).setCheckBox(id, false);

                                int count = ((EditViewAdapter) mRecordingFileListView
                                        .getAdapter()).getCheckedItemsCount();
                                if (count == 0) {
                                    saveLastSelection();
                                    mCurrentDapterMode = NORMAL;
                                    swicthAdapterView(-1);
                                }
                            } else {
                                checkBox.setChecked(true);
                                ((EditViewAdapter) mRecordingFileListView
                                        .getAdapter()).setCheckBox(id, true);
                            }
                        } else {
                            Intent intent = new Intent();
                            HashMap<String, Object> map =
                                    (HashMap<String, Object>) mRecordingFileListView
                                            .getItemAtPosition(arg2);
                            intent.putExtra(DOWHAT, PLAY);
                            if (map != null && map.get(PATH) != null) {
                                intent.putExtra(PATH, map.get(PATH).toString());
                            }
                            if (map != null && map.get(DURATION) != null) {
                                intent.putExtra(DURATION, Integer.parseInt(map
                                        .get(DURATION).toString()));
                            }
                            intent.setClass(RecordingFileList.this,
                                    SoundRecorder.class);
                            setResult(RESULT_OK, intent);
                            finish();
                        }
                    }
                });

        mRecordingFileListView
                .setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {

                    @Override
                    public boolean onItemLongClick(AdapterView<?> arg0,
                                                   View arg1, int arg2, long arg3) {
                        int id = 0;
                        if (mCurrentDapterMode == EDIT) {
                            id = (int) ((EditViewAdapter) mRecordingFileListView
                                    .getAdapter()).getItemId(arg2);
                        } else {
                            HashMap<String, Object> map =
                                    (HashMap<String, Object>) mRecordingFileListView
                                            .getItemAtPosition(arg2);
                            id = (Integer) map.get(RECORD_ID);
                        }
                        if (mCurrentDapterMode == NORMAL) {
                            saveLastSelection();
                            mCurrentDapterMode = EDIT;
                            swicthAdapterView(id);
                        }
                        return true;
                    }

                });
        registerExternalStorageListener();
    }

    /**
     * save the old data
     */
    @Override
    public Object onRetainNonConfigurationInstance() {
        List<Integer> checkedList = null;
        saveLastSelection();
        if (mCurrentDapterMode == EDIT) {
            if (((EditViewAdapter) mRecordingFileListView.getAdapter()) != null) {
                checkedList = ((EditViewAdapter) mRecordingFileListView
                        .getAdapter()).getCheckedPosList();
            }
        }
        ListViewProperty mListViewProperty = new ListViewProperty(checkedList, mSelection, mTop);
        return mListViewProperty;
    }

    @Override
    protected void onResume() {
        super.onResume();
        setListData(mCheckedList);
        mActivityForeground = true;
    }

    /**
     * This method save the selection of list view on present screen
     */
    protected void saveLastSelection() {
        if (mRecordingFileListView != null) {
            mSelection = mRecordingFileListView.getFirstVisiblePosition();
            View cv = mRecordingFileListView.getChildAt(0);
            if (cv != null) {
                mTop = cv.getTop();
            }
        }
    }

    /**
     * This method restore the selection saved before
     */
    protected void restoreLastSelection() {
        if (mSelection >= 0) {
            mRecordingFileListView.setSelectionFromTop(mSelection, mTop);
            mSelection = -1;
        }
    }


    @Override
    protected void onStart() {
        super.onStart();
        ListViewProperty mListViewProperty = (ListViewProperty) getLastNonConfigurationInstance();
        if (mListViewProperty != null) {
            if (mListViewProperty.getmCheckedList() != null) {
                mCheckedList = mListViewProperty.getmCheckedList();
            }
            mSelection = mListViewProperty.getmCurPos();
            mTop = mListViewProperty.getmTop();
        }
    }

    /**
     * bind data to list view
     */
    private void setListData(List<Integer> list) {
        mRecordingFileListView.setAdapter(null);
        QueryDataTask queryTask = new QueryDataTask(list);
        queryTask.execute();
    }

    /**
     * query sound recorder recording file data
     */
    public ArrayList<HashMap<String, Object>> queryData() {
        Cursor mRecordingFileCursor = this.getContentResolver().query(
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                new String[]{MediaStore.Audio.Media.ARTIST,
                        MediaStore.Audio.Media.ALBUM,
                        MediaStore.Audio.Media.DATA,
                        MediaStore.Audio.Media.DURATION,
                        MediaStore.Audio.Media.DISPLAY_NAME,
                        MediaStore.Audio.Media.DATE_ADDED,
                        MediaStore.Audio.Media.TITLE,
                        MediaStore.Audio.Media._ID},
                MediaStore.Audio.Media.IS_MUSIC + " =0 and "
                        + MediaStore.Audio.Media.DATA + " LIKE '" + "%" + "/"
                        + Recorder.RECORD_FOLDER + "%" + "'", null, null);
        try {
            if (mRecordingFileCursor == null
                    || mRecordingFileCursor.getCount() == 0) {
                return null;
            }
            mRecordingFileCursor.moveToFirst();
            mArrlist = new ArrayList<HashMap<String, Object>>();
            mNameList = new ArrayList<String>();
            mPathList = new ArrayList<String>();
            mTitleList = new ArrayList<String>();
            mDurationList = new ArrayList<String>();
            mIdList = new ArrayList<Integer>();
            int num = mRecordingFileCursor.getCount();
            for (int j = 0; j < num; j++) {
                HashMap<String, Object> map = new HashMap<String, Object>();
                map.put(FILE_NAME, mRecordingFileCursor.getString(4));
                map.put(PATH, mRecordingFileCursor.getString(2));
                map.put(DURATION, mRecordingFileCursor.getInt(3));
                map.put(CREAT_DATE, mRecordingFileCursor.getString(6));
                map.put(FORMAT_DURATION,
                        formatDuration(mRecordingFileCursor.getInt(3)));
                map.put(RECORD_ID,
                        mRecordingFileCursor.getInt(7));

                mNameList.add(mRecordingFileCursor.getString(4));
                mPathList.add(mRecordingFileCursor.getString(2));
                mTitleList.add(mRecordingFileCursor.getString(6));
                mDurationList
                        .add(formatDuration(mRecordingFileCursor.getInt(3)));
                mIdList.add(mRecordingFileCursor.getInt(7));
                mRecordingFileCursor.moveToNext();
                mArrlist.add(map);
            }
        } catch (IllegalStateException e) {
            e.printStackTrace();
        } finally {
            if (mRecordingFileCursor != null) {
                mRecordingFileCursor.close();
            }
        }
        return mArrlist;
    }

    /**
     * update UI after query data
     */
    public void afterQuery(List<Integer> list) {
        if (list == null) {
            mCurrentDapterMode = NORMAL;
            swicthAdapterView(-1);
        } else {
            list.retainAll(mIdList);
            if (list.size() == 0) {
                removeDialog(ALERT_DELETE_DIALOG);
                mCurrentDapterMode = NORMAL;
                swicthAdapterView(-1);
            } else {
                mCurrentDapterMode = EDIT;
                EditViewAdapter adapter = new EditViewAdapter(
                        getApplicationContext(), mNameList, mPathList, mTitleList,
                        mDurationList, mIdList, list);
                mRecordingFileListView.setAdapter(adapter);
                mDeleteButton.setVisibility(View.VISIBLE);
                mRecordButton.setVisibility(View.GONE);
                restoreLastSelection();
            }
        }
    }

    /**
     * format duartion to display as 00:00
     */
    public String formatDuration(int duration) {
        String mTimerFormat = getResources().getString(R.string.timer_format);
        int time = duration / 1000;
        return String.format(mTimerFormat, time / 60, time % 60);
    }

    @Override
    public void onClick(View button) {
        switch (button.getId()) {
            case R.id.recordButton:

                mRecordButton.setEnabled(false);
                Intent mIntent = new Intent();
                mIntent.setClass(this, SoundRecorder.class);
                mIntent.putExtra(DOWHAT, RECORD);
                setResult(RESULT_OK, mIntent);
                this.finish();
                break;
            case R.id.deleteButton:

                Bundle bundle = new Bundle();
                int count = ((EditViewAdapter) mRecordingFileListView.getAdapter())
                        .getCheckedItemsCount();
                if (count == 1) {
                    bundle.putBoolean(SINGLE, true);
                } else {
                    bundle.putBoolean(SINGLE, false);
                }
                showDialog(ALERT_DELETE_DIALOG, bundle);
                break;
            default:
                break;
        }
    }

    /**
     * switch adapter mode which weather contains check box
     */
    public void swicthAdapterView(int pos) {
        if (mCurrentDapterMode == NORMAL) {
            SimpleAdapter adapter = new SimpleAdapter(this, mArrlist,
                    R.layout.navigation_adapter, new String[]{FILE_NAME,
                    CREAT_DATE, FORMAT_DURATION}, new int[]{
                    R.id.record_file_name, R.id.record_file_title,
                    R.id.record_file_duration});
            mRecordingFileListView.setAdapter(adapter);
            mDeleteButton.setVisibility(View.GONE);
            mRecordButton.setVisibility(View.VISIBLE);
        } else {
            EditViewAdapter adapter = new EditViewAdapter(this, mNameList,
                    mPathList, mTitleList, mDurationList, mIdList, pos);
            mRecordingFileListView.setAdapter(adapter);
            mDeleteButton.setVisibility(View.VISIBLE);
            mRecordButton.setVisibility(View.GONE);
        }
        restoreLastSelection();
    }

    /**
     * The method gets the selected items and create a list of File objects
     *
     * @return a list of File objects
     */
    protected List<File> getSelectedFiles() {
        List<File> list = new ArrayList<File>();
        if (EDIT != mCurrentDapterMode) {
            return list;
        }
        if (((EditViewAdapter) mRecordingFileListView.getAdapter()) != null) {
            List<String> checkedList = ((EditViewAdapter) mRecordingFileListView
                    .getAdapter()).getCheckedItemsList();

            for (int i = 0; i < checkedList.size(); i++) {
                File file = new File(checkedList.get(i));
                if (file.exists()) {
                    list.add(new File(checkedList.get(i)));
                } else {
                    deleteFromMediaDB(file);
                }
            }
        }
        return list;
    }

    /**
     * delete file frome Media DB, if the file is add by media scanner .
     */
    private void deleteFromMediaDB(File file) {
        ContentResolver resolver = getContentResolver();
        Uri base = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        final String[] ids = new String[]{MediaStore.Audio.Media._ID};
        final String where = MediaStore.Audio.Media.DATA + " LIKE '%"
                + file.getAbsolutePath().replaceFirst("file:///", "") + "'";

        Cursor cursor = query(base, ids, where, null, null);
        try {
            if (null != cursor && cursor.getCount() > 0) {
                resolver.delete(base, where, null);
            }
        } catch (IllegalStateException e) {
            e.printStackTrace();
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }
    }

    /**
     * A simple utility to do a query into the databases.
     */
    private Cursor query(Uri uri, String[] projection, String selection,
                         String[] selectionArgs, String sortOrder) {

        try {
            ContentResolver resolver = getContentResolver();
            if (resolver == null) {
                return null;
            }
            return resolver.query(uri, projection, selection, selectionArgs,
                    sortOrder);

        } catch (UnsupportedOperationException ex) {
            return null;
        }
    }

    @Override
    protected Dialog onCreateDialog(int id, Bundle args) {
        if (ALERT_DELETE_DIALOG == id) {
            return alertMultiDeleteDialog(args);
        } else {
            return null;
        }
    }

    @Override
    protected void onPrepareDialog(int id, Dialog dialog, Bundle args) {
        if (ALERT_DELETE_DIALOG == id) {
            AlertDialog alertDialog = (AlertDialog) dialog;
            if (args.getBoolean(SINGLE)) {
                alertDialog.setMessage(getResources().getString(R.string.alert_delete_single));
            } else {
                alertDialog.setMessage(getResources().getString(R.string.alert_delete_multiple));
            }
        }
        super.onPrepareDialog(id, dialog, args);
    }

    /**
     * The method creates an alert delete dialog
     *
     * @param args argument, the boolean value who will indicates whether the
     *             selected files just only one. The prompt message will be
     *             different.
     * @return a dialog
     */
    protected AlertDialog alertMultiDeleteDialog(Bundle args) {
        Resources mResources = this.getResources();
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        String alertMsg = null;
        if (args.getBoolean(SINGLE)) {
            alertMsg = mResources.getString(R.string.alert_delete_single);
        } else {
            alertMsg = mResources.getString(R.string.alert_delete_multiple);
        }

        builder.setTitle(R.string.delete)
                .setIcon(android.R.drawable.ic_dialog_alert)
                .setMessage(alertMsg)
                .setPositiveButton(mResources.getString(R.string.ok),
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                // delete selected items
                                FileTask fileTask = new FileTask();
                                fileTask.execute();
                            }
                        })
                .setNegativeButton(mResources.getString(R.string.cancel),
                        new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                            }
                        });
        return builder.create();
    }

    @Override
    protected void onPause() {
        List<Integer> checkedList = null;
        if (mCurrentDapterMode == EDIT) {
            if (((EditViewAdapter) mRecordingFileListView.getAdapter()) != null) {
                checkedList = ((EditViewAdapter) mRecordingFileListView
                        .getAdapter()).getCheckedPosList();
                if (checkedList.size() >= 1) {
                    mCheckedList = checkedList;
                }
            }
        } else {
            mCheckedList = null;
        }
        mActivityForeground = false;
        saveLastSelection();
        super.onPause();
    }

    @Override
    public void onBackPressed() {
        mCurrentDapterMode = NORMAL;
        onInit();
        super.onBackPressed();
    }

    /**
     * FileTask for delete some recording file
     */
    public class FileTask extends AsyncTask<Void, Object, Void> {
        private static final String TAG = "FileTask";
        private final ProgressDialog mDialog = new ProgressDialog(
                RecordingFileList.this);
        Resources mResources = RecordingFileList.this.getResources();

        /**
         * A callback method to be invoked before the background thread starts
         * running
         */
        @Override
        protected void onPreExecute() {
            mDialog.setTitle(mResources.getString(R.string.delete));
            mDialog.setMessage(mResources.getString(R.string.deleting));
            mDialog.setCancelable(false);
            mDialog.show();
        }

        /**
         * A callback method to be invoked when the background thread starts
         * running
         *
         * @param params the method need not parameters here
         * @return null, the background thread need not return anything
         */
        @Override
        protected Void doInBackground(Void... params) {
            // delete files
            List<File> list = getSelectedFiles();
            for (int i = 0; i < list.size(); i++) {
                if (!list.get(i).delete()) {

                }
                deleteFromMediaDB(list.get(i));
            }
            return null;
        }

        /**
         * A callback method to be invoked after the background thread performs
         * the task
         *
         * @param result the value returned by doInBackground(), but it is not
         *               needed here
         */
        @Override
        protected void onPostExecute(Void result) {
            if (mDialog != null) {
                if (mDialog.isShowing()) {
                    mDialog.dismiss();
                    if (mActivityForeground) {
                        mCurrentDapterMode = NORMAL;
                        setListData(null);
                    }
                }
            }
        }

        /**
         * A callback method to be invoked when the background thread's task is
         * cancelled
         */
        @Override
        protected void onCancelled() {
            if (mDialog != null) {
                mDialog.dismiss();
            }
        }
    }

    /**
     * through AsyncTask to query recording file data from database
     */
    public class QueryDataTask extends AsyncTask<Void, Object, ArrayList<HashMap<String, Object>>> {
        List<Integer> mList;

        QueryDataTask(List<Integer> list) {
            mList = list;
        }

        /**
         * query data from database
         */
        protected ArrayList<HashMap<String, Object>> doInBackground(Void... params) {
            return queryData();
        }

        /**
         * update ui
         */
        protected void onPostExecute(ArrayList<HashMap<String, Object>> result) {
            if (mActivityForeground) {
                if (result == null) {
                    removeDialog(ALERT_DELETE_DIALOG);
                    mRecordingFileListView.setEmptyView(mEmptyView);
                    mDeleteButton.setVisibility(View.GONE);
                    mRecordButton.setVisibility(View.VISIBLE);
                } else {
                    afterQuery(mList);
                }
            }
        }
    }

    /**
     * back to init state
     */
    public void onInit() {
        mCurrentDapterMode = NORMAL;
        Intent mIntent = new Intent();
        mIntent.setClass(this, SoundRecorder.class);
        mIntent.putExtra(DOWHAT, INIT);
        setResult(RESULT_OK, mIntent);
        finish();
    }

    @Override
    public void onDestroy() {
        if (mSDCardMountEventReceiver != null) {
            unregisterReceiver(mSDCardMountEventReceiver);
            mSDCardMountEventReceiver = null;
        }
        super.onDestroy();
    }

    /**
     * deal with SDCard mount and eject event
     */
    private void registerExternalStorageListener() {
        if (mSDCardMountEventReceiver == null) {
            mSDCardMountEventReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (action.equals(Intent.ACTION_MEDIA_EJECT)) {
                        onInit();
                    } else if (action.equals(Intent.ACTION_MEDIA_MOUNTED)) {
                        onInit();
                    }
                }
            };
            IntentFilter iFilter = new IntentFilter();
            iFilter.addAction(Intent.ACTION_MEDIA_EJECT);
            iFilter.addAction(Intent.ACTION_MEDIA_MOUNTED);
            iFilter.addDataScheme("file");
            registerReceiver(mSDCardMountEventReceiver, iFilter);
        }
    }

}