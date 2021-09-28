package com.android.soundrecorder;

import java.util.List;

public class ListViewProperty {
    private List<Integer> mCheckedList;
    private int mCurPos;
    private int mTop;

    public ListViewProperty(List<Integer> list, int curPos, int top) {
        mCheckedList = list;
        mCurPos = curPos;
        mTop = top;
    }

    /**
     * @return the mCheckedList
     */
    public List<Integer> getmCheckedList() {
        return mCheckedList;
    }

    /**
     * @param mCheckedList the mCheckedList to set
     */
    public void setmCheckedList(List<Integer> checkedList) {
        this.mCheckedList = checkedList;
    }

    /**
     * @return the mCurPos
     */
    public int getmCurPos() {
        return mCurPos;
    }

    /**
     * @param mCurPos the mCurPos to set
     */
    public void setmCurPos(int curPos) {
        this.mCurPos = curPos;
    }

    /**
     * @return the mTop
     */
    public int getmTop() {
        return mTop;
    }

    /**
     * @param mTop the mTop to set
     */
    public void setmTop(int top) {
        this.mTop = top;
    }
}