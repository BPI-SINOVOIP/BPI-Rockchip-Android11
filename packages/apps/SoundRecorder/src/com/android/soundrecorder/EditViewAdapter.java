package com.android.soundrecorder;

import android.content.Context;
import android.content.res.Resources;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CheckBox;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

public class EditViewAdapter extends BaseAdapter {
    private static final int DEFAULT_SECONDARY_SIZE_TEXT_COLOR = -4276546;

    private final Resources mResources;
    private final LayoutInflater mInflater;
    private final List<String> mNameList;
    private final List<String> mPathList;
    private final List<String> mTitleList;
    private final List<String> mDurationList;
    private final List<Integer> mIdList;
    private final List<Integer> mCheckedItemId;
    private final SparseBooleanArray mCheckStates;
    private int mCurPos = -1;
    private List<Integer> mPosCheckedList = null;
    private final SparseIntArray mIdPos;

    /**
     * The constructor to construct a navigation view adapter
     *
     * @param context      the context of FileManagerActivity
     * @param nameList     a list of file names
     * @param pathList     a list of file path
     * @param titleList    a list of file title
     * @param durationList a list of file duration
     * @param curPos       current click item index
     */
    public EditViewAdapter(Context context, List<String> nameList,
                           List<String> pathList, List<String> titleList,
                           List<String> durationList, List<Integer> idList, int curPos) {
        mResources = context.getResources();
        mInflater = LayoutInflater.from(context);
        mNameList = nameList;
        mPathList = pathList;
        mTitleList = titleList;
        mDurationList = durationList;
        mIdList = idList;
        mCheckStates = new SparseBooleanArray();
        mCheckedItemId = new ArrayList<Integer>();
        mIdPos = new SparseIntArray();
        mCurPos = curPos;
        if (mCurPos != -1) {
            mCheckStates.put(mCurPos, true);
        }
    }

    /**
     * The constructor to construct a navigation view adapter
     *
     * @param context        the context of FileManagerActivity
     * @param nameList       a list of file names
     * @param pathList       a list of file path
     * @param titleList      a list of file title
     * @param durationList   a list of file duration
     * @param posCheckedList current checked box list
     */
    public EditViewAdapter(Context context, List<String> nameList,
                           List<String> pathList, List<String> titleList,
                           List<String> durationList, List<Integer> idList, List<Integer> posCheckedList) {
        mResources = context.getResources();
        mInflater = LayoutInflater.from(context);
        mNameList = nameList;
        mPathList = pathList;
        mTitleList = titleList;
        mDurationList = durationList;
        mIdList = idList;
        mCheckStates = new SparseBooleanArray();
        mCheckedItemId = new ArrayList<Integer>();
        mPosCheckedList = posCheckedList;
        mIdPos = new SparseIntArray();
        if (mPosCheckedList != null) {
            for (int i = 0; i < mPosCheckedList.size(); i++) {
                mCheckStates.put(mPosCheckedList.get(i), true);
            }
        }
    }

    /**
     * This method sets the item's check boxes
     *
     * @param id      the id of the item
     * @param checked the checked state
     */
    protected void setCheckBox(int id, boolean checked) {
        mCheckStates.put(id, checked);
    }

    /**
     * This method return the list of current checked box
     *
     * @return current list of checked box
     */
    protected List<Integer> getCheckedPosList() {
        for (int i = 0; i < mCheckStates.size(); i++) {
            if (mCheckStates.valueAt(i)) {
                mCheckedItemId.add(mCheckStates.keyAt(i));
            }
        }
        return mCheckedItemId;
    }

    /**
     * This method gets the number of the checked items
     *
     * @return the number of the checked items
     */
    protected int getCheckedItemsCount() {
        int count = 0;

        for (int i = 0; i < mCheckStates.size(); i++) {
            if (mCheckStates.valueAt(i)) {
                ++count;
            }
        }
        return count;
    }

    /**
     * This method gets the list of the checked items
     *
     * @return the list of the checked items
     */
    protected ArrayList<String> getCheckedItemsList() {
        ArrayList<String> checkedItemsList = new ArrayList<String>();
        for (int i = 0; i < mCheckStates.size(); i++) {
            if (mCheckStates.valueAt(i)) {
                checkedItemsList.add(mPathList.get(getItemPos(mCheckStates
                        .keyAt(i))));
            }
        }
        return checkedItemsList;
    }

    /**
     * This method gets the list of the grey out items
     *
     * @return the list of the grey out items
     */
    protected SparseBooleanArray getGrayOutItems() {
        return mCheckStates;
    }

    /**
     * This method gets the count of the items in the name list
     *
     * @return the number of the items
     */
    @Override
    public int getCount() {
        return mNameList.size();
    }

    /**
     * This method gets the name of the item at the specified position
     *
     * @param pos the position of item
     * @return the name of the item
     */
    @Override
    public Object getItem(int pos) {
        return mNameList.get(pos);
    }

    /**
     * This method gets the item id at the specified position
     *
     * @param pos the position of item
     * @return the id of the item
     */
    @Override
    public long getItemId(int pos) {
        long id = mIdList.get(pos);
        mIdPos.put((int) id, pos);
        return id;
    }

    public int getItemPos(int id) {
        return mIdPos.get(id);
    }

    /**
     * This method gets the view for each item to be displayed in the list view
     *
     * @param pos         the position of the item
     * @param convertView the view to be shown
     * @param parent      the parent view
     * @return the view to be shown
     */
    @Override
    public View getView(int pos, View convertView, ViewGroup parent) {
        EditViewTag editViewTag;
        View editListItemView = convertView;

        if (editListItemView == null) {
            editListItemView = mInflater.inflate(R.layout.edit_adapter, null);

            // construct an item tag
            editViewTag = new EditViewTag(
                    (TextView) editListItemView
                            .findViewById(R.id.record_file_name),
                    (CheckBox) editListItemView
                            .findViewById(R.id.record_file_checkbox),
                    (TextView) editListItemView
                            .findViewById(R.id.record_file_title),
                    (TextView) editListItemView
                            .findViewById(R.id.record_file_duration));
            editListItemView.setTag(editViewTag);
        } else {
            editViewTag = (EditViewTag) editListItemView.getTag();
        }

        RelativeLayout.LayoutParams params = null;

        // set name
        params = (RelativeLayout.LayoutParams) editViewTag.mName
                .getLayoutParams();
        editViewTag.mName.setLayoutParams(params);
        String fileName = mNameList.get(pos);
        editViewTag.mName.setText(fileName);

        String title = mTitleList.get(pos);
        editViewTag.mTitle.setText(title);

        String duration = mDurationList.get(pos);
        editViewTag.mDuration.setText(duration);

        editViewTag.mCheckBox.setChecked(mCheckStates.get((int) getItemId(pos)));
        return editListItemView;
    }

    static class EditViewTag {
        protected TextView mName;
        protected CheckBox mCheckBox;
        protected TextView mTitle;
        protected TextView mDuration;

        /**
         * The constructor to construct an edit view tag
         *
         * @param name     the name view of the item
         * @param box      the check box view of the item
         * @param title    the title view of the item
         * @param duration the duration view of the item
         */
        public EditViewTag(TextView name, CheckBox box, TextView title,
                           TextView duration) {
            this.mName = name;
            this.mCheckBox = box;
            this.mTitle = title;
            this.mDuration = duration;
        }
    }
}