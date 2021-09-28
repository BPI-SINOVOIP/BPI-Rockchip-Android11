package com.rockchip.graphics;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import java.util.ArrayList;

public class SpinnerItemAdapter extends BaseAdapter {

    private Context mContext;
    private ArrayList<ConnectorInfo> mList;

    public SpinnerItemAdapter(Context mContext, ArrayList<ConnectorInfo> mList) {
        this.mContext = mContext;
        this.mList = mList;
    }

    @Override
    public int getCount() {
        return mList.size();
    }

    @Override
    public Object getItem(int i) {
        return mList.get(i);
    }

    @Override
    public long getItemId(int i) {
        return i;
    }

    @Override
    public View getView(int i, View view, ViewGroup viewGroup) {
        View v = LayoutInflater.from(mContext).inflate(R.layout.spinner_item, null);
        TextView textView = (TextView) v.findViewById(R.id.item_text);
        textView.setText("Type:" + ConnectorInfo.CONNECTOR_TYPE[mList.get(i).getType()] + "  ID:" + mList.get(i).getId());
        return v;
    }
}
