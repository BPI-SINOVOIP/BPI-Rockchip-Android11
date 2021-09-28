/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.companiondevicesupport.activity;

import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.ui.core.CarUi.requireToolbar;
import static com.android.car.ui.toolbar.Toolbar.State.SUBPAGE;

import android.annotation.NonNull;
import android.app.AlertDialog;
import android.app.Dialog;
import android.bluetooth.BluetoothAdapter;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.text.Html;
import android.text.Spanned;
import android.widget.Toast;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.AssociatedDevice;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.Arrays;

/** Activity class for association */
public class AssociationActivity extends FragmentActivity {

    /** Intent action used to request a device be associated.*/
    public static final String ACTION_ASSOCIATION_SETTING =
            "com.android.car.companiondevicesupport.ASSOCIATION_ACTIVITY";

    /** Data name for associated device. */
    public static final String ASSOCIATED_DEVICE_DATA_NAME_EXTRA =
            "com.android.car.companiondevicesupport.ASSOCIATED_DEVICE";

    private static final String TAG = "CompanionAssociationActivity";
    private static final String ADD_DEVICE_FRAGMENT_TAG = "AddAssociatedDeviceFragment";
    private static final String DEVICE_DETAIL_FRAGMENT_TAG = "AssociatedDeviceDetailFragment";
    private static final String PAIRING_CODE_FRAGMENT_TAG = "ConfirmPairingCodeFragment";
    private static final String TURN_ON_BLUETOOTH_FRAGMENT_TAG = "TurnOnBluetoothFragment";
    private static final String ASSOCIATION_ERROR_FRAGMENT_TAG = "AssociationErrorFragment";
    private static final String REMOVE_DEVICE_DIALOG_TAG = "RemoveDeviceDialog";
    private static final String TURN_ON_BLUETOOTH_DIALOG_TAG = "TurnOnBluetoothDialog";

    private ToolbarController mToolbar;
    private AssociatedDeviceViewModel mModel;

    @Override
    public void onCreate(Bundle saveInstanceState) {
        super.onCreate(saveInstanceState);
        setContentView(R.layout.base_activity);
        mToolbar = requireToolbar(this);
        mToolbar.setState(SUBPAGE);
        observeViewModel();
        if (saveInstanceState != null) {
            resumePreviousState();
        }
        mToolbar.getProgressBar().setVisible(true);
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        mModel.stopAssociation();
        dismissConfirmButtons();
        mToolbar.getProgressBar().setVisible(false);
    }

    private void observeViewModel() {
        mModel = ViewModelProviders.of(this).get(AssociatedDeviceViewModel.class);

        mModel.getAssociationState().observe(this, state -> {
            switch (state) {
                case PENDING:
                    if (!BluetoothAdapter.getDefaultAdapter().isEnabled()) {
                        runOnUiThread(this::showTurnOnBluetoothFragment);
                    }
                    break;
                case COMPLETED:
                    mModel.resetAssociationState();
                    runOnUiThread(() -> Toast.makeText(getApplicationContext(),
                            getString(R.string.continue_setup_toast_text),
                            Toast.LENGTH_SHORT).show());
                    break;
                case ERROR:
                    mModel.resetAssociationState();
                    runOnUiThread(this::showAssociationErrorFragment);
                    break;
                case NONE:
                case STARTING:
                case STARTED:
                    break;
                default:
                    loge(TAG, "Encountered unexpected association state: " + state);
            }
        });

        mModel.getBluetoothState().observe(this, state -> {
            if (state != BluetoothAdapter.STATE_ON && getSupportFragmentManager()
                    .findFragmentByTag(DEVICE_DETAIL_FRAGMENT_TAG) != null) {
                runOnUiThread(this::showTurnOnBluetoothDialog);
            }
        });

        mModel.getAdvertisedCarName().observe(this, name -> {
            if (name != null) {
                runOnUiThread(() -> showAddAssociatedDeviceFragment(name));
            }
        });
        mModel.getPairingCode().observe(this, code -> {
            if (code != null) {
                runOnUiThread(() -> showConfirmPairingCodeFragment(code));
            }
        });
        mModel.getDeviceDetails().observe(this, deviceDetails -> {
            if (deviceDetails == null) {
                return;
            }
            AssociatedDevice device = deviceDetails.getAssociatedDevice();
            if (isStartedByFeature()) {
                setDeviceToReturn(device);
            } else {
                runOnUiThread(this::showAssociatedDeviceDetailFragment);
            }
        });

        mModel.getDeviceToRemove().observe(this, device -> {
            if (device != null) {
                runOnUiThread(() -> showRemoveDeviceDialog(device));
            }
        });

        mModel.getRemovedDevice().observe(this, device -> {
            if (device != null) {
                runOnUiThread(() -> Toast.makeText(getApplicationContext(),
                        getString(R.string.device_removed_success_toast_text,
                                device.getDeviceName()),
                        Toast.LENGTH_SHORT).show());
                finish();
            }
        });
        mModel.isFinished().observe(this, isFinished -> {
            if (isFinished) {
                finish();
            }
        });
    }

    private void showTurnOnBluetoothFragment() {
        TurnOnBluetoothFragment fragment = new TurnOnBluetoothFragment();
        mToolbar.getProgressBar().setVisible(true);
        launchFragment(fragment, TURN_ON_BLUETOOTH_FRAGMENT_TAG);
    }

    private void showAddAssociatedDeviceFragment(String deviceName) {
        AddAssociatedDeviceFragment fragment = AddAssociatedDeviceFragment.newInstance(deviceName);
        launchFragment(fragment, ADD_DEVICE_FRAGMENT_TAG);
        mToolbar.getProgressBar().setVisible(true);
    }

    private void showConfirmPairingCodeFragment(String pairingCode) {
        ConfirmPairingCodeFragment fragment = ConfirmPairingCodeFragment.newInstance(pairingCode);
        launchFragment(fragment, PAIRING_CODE_FRAGMENT_TAG);
        showConfirmButtons();
        mToolbar.getProgressBar().setVisible(false);
    }

    private void showAssociationErrorFragment() {
        dismissConfirmButtons();
        mToolbar.getProgressBar().setVisible(true);
        AssociationErrorFragment fragment = new AssociationErrorFragment();
        launchFragment(fragment,  ASSOCIATION_ERROR_FRAGMENT_TAG);
    }

    private void showAssociatedDeviceDetailFragment() {
        AssociatedDeviceDetailFragment fragment = new AssociatedDeviceDetailFragment();
        launchFragment(fragment, DEVICE_DETAIL_FRAGMENT_TAG);
        mToolbar.getProgressBar().setVisible(false);
        showTurnOnBluetoothDialog();
    }

    private void showConfirmButtons() {
        MenuItem cancelButton = MenuItem.builder(this)
                .setTitle(R.string.retry)
                .setOnClickListener(i -> retryAssociation())
                .build();
        MenuItem confirmButton = MenuItem.builder(this)
                .setTitle(R.string.confirm)
                .setOnClickListener(i -> {
                    mModel.acceptVerification();
                    dismissConfirmButtons();
                })
                .build();
        mToolbar.setMenuItems(Arrays.asList(cancelButton, confirmButton));
    }

    private void dismissConfirmButtons() {
        mToolbar.setMenuItems(null);
    }

    private void showRemoveDeviceDialog(AssociatedDevice device) {
        RemoveDeviceDialogFragment removeDeviceDialogFragment =
                RemoveDeviceDialogFragment.newInstance(device.getDeviceName(),
                        (d, which) -> mModel.removeCurrentDevice());
        removeDeviceDialogFragment.show(getSupportFragmentManager(), REMOVE_DEVICE_DIALOG_TAG);
    }

    private void showTurnOnBluetoothDialog() {
        if (!BluetoothAdapter.getDefaultAdapter().isEnabled()) {
            TurnOnBluetoothDialogFragment fragment = new TurnOnBluetoothDialogFragment();
            fragment.show(getSupportFragmentManager(), TURN_ON_BLUETOOTH_DIALOG_TAG);
        }
    }

    private void resumePreviousState() {
        if (getSupportFragmentManager().findFragmentByTag(PAIRING_CODE_FRAGMENT_TAG) != null) {
            showConfirmButtons();
        }

        RemoveDeviceDialogFragment removeDeviceDialogFragment =
                (RemoveDeviceDialogFragment) getSupportFragmentManager()
                .findFragmentByTag(REMOVE_DEVICE_DIALOG_TAG);
        if (removeDeviceDialogFragment != null) {
            removeDeviceDialogFragment.setOnConfirmListener((d, which) ->
                    mModel.removeCurrentDevice());
        }
    }

    private void launchFragment(Fragment fragment, String tag) {
        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.fragment_container, fragment, tag)
                .commit();
    }

    private void retryAssociation() {
        dismissConfirmButtons();
        mToolbar.getProgressBar().setVisible(true);
        Fragment fragment = getSupportFragmentManager()
                .findFragmentByTag(PAIRING_CODE_FRAGMENT_TAG);
        if (fragment != null) {
            getSupportFragmentManager().beginTransaction().remove(fragment).commit();
        }
        mModel.retryAssociation();
    }

    private void setDeviceToReturn(AssociatedDevice device) {
        if (!isStartedByFeature()) {
            return;
        }
        Intent intent = new Intent();
        intent.putExtra(ASSOCIATED_DEVICE_DATA_NAME_EXTRA, device);
        setResult(RESULT_OK, intent);
        finish();
    }

    private boolean isStartedByFeature() {
        String action = getIntent().getAction();
        return ACTION_ASSOCIATION_SETTING.equals(action);
    }

    /** Dialog fragment to confirm removing an associated device. */
    public static class RemoveDeviceDialogFragment extends DialogFragment {
        private static final String DEVICE_NAME_KEY = "device_name";

        private DialogInterface.OnClickListener mOnConfirmListener;

        static RemoveDeviceDialogFragment newInstance(@NonNull String deviceName,
                DialogInterface.OnClickListener listener) {
            Bundle bundle = new Bundle();
            bundle.putString(DEVICE_NAME_KEY, deviceName);
            RemoveDeviceDialogFragment fragment = new RemoveDeviceDialogFragment();
            fragment.setArguments(bundle);
            fragment.setOnConfirmListener(listener);
            return fragment;
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            Bundle bundle = getArguments();
            String deviceName = bundle.getString(DEVICE_NAME_KEY);
            String title = getString(R.string.remove_associated_device_title, deviceName);
            Spanned styledTitle = Html.fromHtml(title, Html.FROM_HTML_MODE_LEGACY);
            return new AlertDialog.Builder(getActivity())
                    .setTitle(styledTitle)
                    .setMessage(getString(R.string.remove_associated_device_message))
                    .setNegativeButton(getString(R.string.cancel), null)
                    .setPositiveButton(getString(R.string.forget), mOnConfirmListener)
                    .setCancelable(true)
                    .create();
        }

        void setOnConfirmListener(DialogInterface.OnClickListener onConfirmListener) {
            mOnConfirmListener = onConfirmListener;
        }
    }

    public static class TurnOnBluetoothDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle(getString(R.string.turn_on_bluetooth_dialog_title))
                    .setMessage(getString(R.string.turn_on_bluetooth_dialog_message))
                    .setPositiveButton(getString(R.string.turn_on), (d, w) ->
                            BluetoothAdapter.getDefaultAdapter().enable())
                    .setNegativeButton(getString(R.string.not_now), null)
                    .setCancelable(true)
                    .create();
        }
    }
}
