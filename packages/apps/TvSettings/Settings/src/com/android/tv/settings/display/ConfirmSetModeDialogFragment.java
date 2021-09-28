package com.android.tv.settings.display;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;

import java.io.Serializable;

import com.android.tv.settings.R;
import android.util.Log;
/**
 * ConfirmSetModeDialogFragment
 */

public class ConfirmSetModeDialogFragment extends DialogFragment {
    protected static final String TAG = "ConfirmSetModeDialogFragment";

    private OnDialogDismissListener mListener;

    public static ConfirmSetModeDialogFragment newInstance(DisplayInfo di, OnDialogDismissListener listener) {
        ConfirmSetModeDialogFragment frag = new ConfirmSetModeDialogFragment();
        Bundle b = new Bundle();
        b.putSerializable("displayinfo", di);
        b.putSerializable("listener", listener);
        frag.setArguments(b);
        return frag;
    }

    private AlertDialog mHdmiResoSetConfirmDialog;
    private boolean mIsOk = false;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {

        mDisplayInfo = (DisplayInfo) getArguments().getSerializable("displayinfo");
        mListener = (OnDialogDismissListener) getArguments().getSerializable("listener");

        mHdmiResoSetConfirmDialog = new AlertDialog.Builder(getActivity())
                .setTitle(getString(R.string.confirm_dialog_title))
                .setMessage(getString(R.string.confirm_dialog_message) + " 10s")
                .setPositiveButton(getString(R.string.title_ok), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mIsOk = true;
                        mHdmiResoSetConfirmDialog.dismiss();
                    }
                }).setNegativeButton(getString(R.string.title_cancel), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mIsOk = false;

                        DrmDisplaySetting.confirmSaveDisplayMode(mDisplayInfo, false);
                        mHdmiResoSetConfirmDialog.dismiss();
                    }
                }).create();
        startTimeCount();
        return mHdmiResoSetConfirmDialog;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        stopTimeCount();
        DrmDisplaySetting.confirmSaveDisplayMode(mDisplayInfo, mIsOk);
        mListener.onDismiss(mIsOk);
        mIsOk = false;
    }

    private DisplayInfo mDisplayInfo;

    private static final int TYPE_NEGATIVE = 0;

    private int mNegativeCount = 10;
    private boolean isTimeCountRunning = false;


    public void startTimeCount() {
        mNegativeCount = 10;
        isTimeCountRunning = true;
        myHander.sendEmptyMessageDelayed(TYPE_NEGATIVE, 1000);
    }

    public void stopTimeCount() {
        isTimeCountRunning = false;
    }

    private Handler myHander = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case TYPE_NEGATIVE:
                    if (!isTimeCountRunning) {
                        return;
                    }
                    if (mNegativeCount > 0) {
                        if (mHdmiResoSetConfirmDialog != null) {
                            String text = getString(R.string.confirm_dialog_message);
                            text = text + " " + mNegativeCount + "s";
                            mHdmiResoSetConfirmDialog.setMessage(text);
                        }
                        mNegativeCount--;
                        myHander.sendEmptyMessageDelayed(TYPE_NEGATIVE, 1000);
                    } else {
                        DrmDisplaySetting.confirmSaveDisplayMode(mDisplayInfo, false);
                        mHdmiResoSetConfirmDialog.dismiss();
                    }
                    break;
            }
        }
    };

    public interface OnDialogDismissListener extends Serializable {
        void onDismiss(boolean isok);
    }
}
