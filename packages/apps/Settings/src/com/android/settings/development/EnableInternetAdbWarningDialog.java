package com.android.settings.development;

import android.app.AlertDialog;
import android.app.Dialog;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.Fragment;
import android.content.DialogInterface;
import android.os.Bundle;

import com.android.internal.logging.nano.MetricsProto;
import com.android.settings.R;
import com.android.settings.core.instrumentation.InstrumentedDialogFragment;

public class EnableInternetAdbWarningDialog extends InstrumentedDialogFragment implements
        DialogInterface.OnClickListener, DialogInterface.OnDismissListener {

    public static final String TAG = "EnableAdbDialog";

    public static void show(Fragment host) {
        final FragmentManager manager = host.getFragmentManager();
        if (manager.findFragmentByTag(TAG) == null) {
            final EnableInternetAdbWarningDialog dialog = new EnableInternetAdbWarningDialog();
            dialog.setTargetFragment(host, 0 /* requestCode */);
            dialog.show(manager, TAG);
        }
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.DIALOG_ENABLE_ADB;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity())
                .setTitle(R.string.enable_internet_adb_warning_title)
                .setMessage(R.string.enable_internet_adb_warning_message)
                .setPositiveButton(android.R.string.yes, this /* onClickListener */)
                .setNegativeButton(android.R.string.no, this /* onClickListener */)
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        final InternetAdbDialogHost host = (InternetAdbDialogHost) getTargetFragment();
        if (host == null) {
            return;
        }
        if (which == DialogInterface.BUTTON_POSITIVE) {
            host.onEnableInternetAdbDialogConfirmed();
        } else {
            host.onEnableInternetAdbDialogDismissed();
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        final InternetAdbDialogHost host = (InternetAdbDialogHost) getTargetFragment();
        if (host == null) {
            return;
        }
        host.onEnableInternetAdbDialogDismissed();
    }
}