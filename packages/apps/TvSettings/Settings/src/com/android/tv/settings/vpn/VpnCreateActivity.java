/**
 *
 */
package com.android.tv.settings.vpn;

import android.content.DialogInterface;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TextView;
import android.os.ServiceManager;
import android.net.IConnectivityManager;

import com.android.tv.settings.BaseInputActivity;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.ConnectivityManager;
import android.os.Bundle;
import android.os.RemoteException;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.security.Credentials;
import android.security.KeyStore;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.android.internal.net.LegacyVpnInfo;
import com.android.internal.net.VpnConfig;
import com.android.internal.net.VpnProfile;

import java.net.InetAddress;

import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;

/**
 * @author GaoFei
 *         VPN创建Activity
 */
public class VpnCreateActivity extends BaseInputActivity implements TextWatcher,
        View.OnClickListener, AdapterView.OnItemSelectedListener,
        CompoundButton.OnCheckedChangeListener {
    private static final String TAG = "VpnCreateActivity";
    private final IConnectivityManager mService = IConnectivityManager.Stub.asInterface(
            ServiceManager.getService(Context.CONNECTIVITY_SERVICE));
    private final KeyStore mKeyStore = KeyStore.getInstance();
    private View.OnClickListener mListener;
    private VpnProfile mProfile;

    private boolean mEditing = true;
    private boolean mExists;

    private View mView;

    private TextView mName;
    private Spinner mType;
    private TextView mServer;
    private TextView mUsername;
    private TextView mPassword;
    private TextView mSearchDomains;
    private TextView mDnsServers;
    private TextView mRoutes;
    private CheckBox mMppe;
    private TextView mL2tpSecret;
    private TextView mIpsecIdentifier;
    private TextView mIpsecSecret;
    private Spinner mIpsecUserCert;
    private Spinner mIpsecCaCert;
    private Spinner mIpsecServerCert;
    private CheckBox mSaveLogin;
    private CheckBox mShowOptions;
    private CheckBox mAlwaysOnVpn;
    private Button mBtnOK;
    private Button mBtnCancel;
    private Button mBtnForget;
    private Button mBtnEdit;

    @Override
    public void init() {
        mView = getContentView();
        Intent intent = getIntent();
        mProfile = (VpnProfile) intent.getParcelableExtra(ConstData.IntentKey.VPN_PROFILE);
        if (mProfile == null)
            mProfile = new VpnProfile(Long.toHexString(System.currentTimeMillis()));
        mEditing = intent.getBooleanExtra(ConstData.IntentKey.VPN_EDITING, true);
        mExists = intent.getBooleanExtra(ConstData.IntentKey.VPN_EXIST, false);
        Context context = this;

        // First, find out all the fields.
        mName = (TextView) mView.findViewById(R.id.name);
        mType = (Spinner) mView.findViewById(R.id.type);
        mServer = (TextView) mView.findViewById(R.id.server);
        mUsername = (TextView) mView.findViewById(R.id.username);
        mPassword = (TextView) mView.findViewById(R.id.password);
        mSearchDomains = (TextView) mView.findViewById(R.id.search_domains);
        mDnsServers = (TextView) mView.findViewById(R.id.dns_servers);
        mRoutes = (TextView) mView.findViewById(R.id.routes);
        mMppe = (CheckBox) mView.findViewById(R.id.mppe);
        mL2tpSecret = (TextView) mView.findViewById(R.id.l2tp_secret);
        mIpsecIdentifier = (TextView) mView.findViewById(R.id.ipsec_identifier);
        mIpsecSecret = (TextView) mView.findViewById(R.id.ipsec_secret);
        mIpsecUserCert = (Spinner) mView.findViewById(R.id.ipsec_user_cert);
        mIpsecCaCert = (Spinner) mView.findViewById(R.id.ipsec_ca_cert);
        mIpsecServerCert = (Spinner) mView.findViewById(R.id.ipsec_server_cert);
        mSaveLogin = (CheckBox) mView.findViewById(R.id.save_login);
        mShowOptions = (CheckBox) mView.findViewById(R.id.show_options);
        mAlwaysOnVpn = (CheckBox) mView.findViewById(R.id.always_on_vpn);
        mBtnOK = (Button) mView.findViewById(R.id.btn_ok);
        mBtnCancel = (Button) mView.findViewById(R.id.btn_cancel);
        mBtnForget = (Button) mView.findViewById(R.id.btn_forget);
        mBtnEdit = (Button) mView.findViewById(R.id.btn_edit);
        // Second, copy values from the profile.
        mName.setText(mProfile.name);
        mType.setSelection(mProfile.type);
        mServer.setText(mProfile.server);
        if (mProfile.saveLogin) {
            mUsername.setText(mProfile.username);
            mPassword.setText(mProfile.password);
        }
        mSearchDomains.setText(mProfile.searchDomains);
        mDnsServers.setText(mProfile.dnsServers);
        mRoutes.setText(mProfile.routes);
        mMppe.setChecked(mProfile.mppe);
        mL2tpSecret.setText(mProfile.l2tpSecret);
        mIpsecIdentifier.setText(mProfile.ipsecIdentifier);
        mIpsecSecret.setText(mProfile.ipsecSecret);
        loadCertificates(mIpsecUserCert, Credentials.USER_PRIVATE_KEY, 0, mProfile.ipsecUserCert);
        loadCertificates(mIpsecCaCert, Credentials.CA_CERTIFICATE,
                R.string.vpn_no_ca_cert, mProfile.ipsecCaCert);
        loadCertificates(mIpsecServerCert, Credentials.USER_CERTIFICATE,
                R.string.vpn_no_server_cert, mProfile.ipsecServerCert);
        mSaveLogin.setChecked(mProfile.saveLogin);
        mAlwaysOnVpn.setChecked(mProfile.key.equals(VpnUtils.getLockdownVpn()));
        mAlwaysOnVpn.setOnCheckedChangeListener(this);
        // Update SaveLogin checkbox after Always-on checkbox is updated
        updateSaveLoginStatus();

        // Hide lockdown VPN on devices that require IMS authentication
        if (SystemProperties.getBoolean("persist.radio.imsregrequired", false)) {
            mAlwaysOnVpn.setVisibility(View.GONE);
        }

        // Third, add listeners to required fields.
        mName.addTextChangedListener(this);
        mType.setOnItemSelectedListener(this);
        mServer.addTextChangedListener(this);
        mUsername.addTextChangedListener(this);
        mPassword.addTextChangedListener(this);
        mDnsServers.addTextChangedListener(this);
        mRoutes.addTextChangedListener(this);
        mIpsecSecret.addTextChangedListener(this);
        mIpsecUserCert.setOnItemSelectedListener(this);
        mShowOptions.setOnClickListener(this);

        // Fourth, determine whether to do editing or connecting.
        boolean valid = validate(true);
        mEditing = mEditing || !valid;

        if (mEditing) {
            setInputTitle(getString(R.string.vpn_edit));

            // Show common fields.
            mView.findViewById(R.id.editor).setVisibility(View.VISIBLE);

            // Show type-specific fields.
            changeType(mProfile.type);

            // Hide 'save login' when we are editing.
            mSaveLogin.setVisibility(View.GONE);

            // Switch to advanced view immediately if any advanced options are on
            if (!mProfile.searchDomains.isEmpty() || !mProfile.dnsServers.isEmpty() ||
                    !mProfile.routes.isEmpty()) {
                showAdvancedOptions();
            }

            // Create a button to forget the profile if it has already been saved..
            if (mExists) {
                mBtnForget.setVisibility(View.VISIBLE);
                setButton(mBtnForget, this);
               /* setButton(DialogInterface.BUTTON_NEUTRAL,
                        context.getString(R.string.vpn_forget), mListener);*/
            }

            // Create a button to save the profile.
            setButton(mBtnOK, this);
           /* setButton(DialogInterface.BUTTON_POSITIVE,
                    context.getString(R.string.vpn_save), mListener);*/
        } else {
            setInputTitle(context.getString(R.string.vpn_connect_to, mProfile.name));
            mBtnEdit.setVisibility(View.VISIBLE);
            mBtnEdit.setOnClickListener(this);
            // Create a button to connect the network.
            /*setButton(DialogInterface.BUTTON_POSITIVE,
                    context.getString(R.string.vpn_connect), mListener);*/
            setButton(mBtnOK, this);
            mBtnOK.setText(getString(R.string.vpn_connect));
        }

        // Always provide a cancel button.
        mBtnCancel.setText(context.getString(R.string.vpn_cancel));
        setButton(mBtnCancel, this);
        // Let AlertDialog create everything.

        // Disable the action button if necessary.
        /*getButton(DialogInterface.BUTTON_POSITIVE)
                .setEnabled(mEditing ? valid : validate(false));*/
        mBtnOK.setEnabled(mEditing ? valid : validate(false));
        // Workaround to resize the dialog for the input method.
       /* getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE |
                WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);*/
        updateFocus();
    }


    @Override
    public int getContentLayoutRes() {
        return R.layout.content_vpn_create;
    }


    @Override
    public String getInputTitle() {
        return getString(R.string.create_vpn);
    }


    @Override
    public void afterTextChanged(Editable field) {
        mBtnOK.setEnabled(validate(mEditing));
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }

    @Override
    public void onClick(View view) {
        if (view == mShowOptions) {
            showAdvancedOptions();
            updateFocus();
        }
        VpnProfile profile = getProfile();
        if (view == mBtnOK) {
            // Update KeyStore entry
            KeyStore.getInstance().put(Credentials.VPN + profile.key, profile.encode(),
                    KeyStore.UID_SELF, /* flags */ 0);

            // Flush out old version of profile
            disconnect(profile);

            updateLockdownVpn(isVpnAlwaysOn(), profile);

            // If we are not editing, connect!
            if (!isEditing() && !VpnUtils.isVpnLockdown(profile.key)) {
                try {
                    connect(profile);
                } catch (RemoteException e) {
                    Log.e(TAG, "Failed to connect", e);
                }
            }
            finish();
        } else if (view == mBtnForget) {
            // Disable profile if connected
            disconnect(profile);

            // Delete from KeyStore
            KeyStore keyStore = KeyStore.getInstance();
            keyStore.delete(Credentials.VPN + profile.key, KeyStore.UID_SELF);

            updateLockdownVpn(false, profile);
            finish();
        } else if (view == mBtnCancel) {
            finish();
        } else if (view == mBtnEdit) {
            Intent editIntent = new Intent(this, VpnCreateActivity.class);
            editIntent.putExtra(ConstData.IntentKey.VPN_PROFILE, mProfile);
            editIntent.putExtra(ConstData.IntentKey.VPN_EXIST, true);
            editIntent.putExtra(ConstData.IntentKey.VPN_EDITING, true);
            startActivity(editIntent);
            finish();
        }

    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (parent == mType) {
            changeType(position);
        }
        mBtnOK.setEnabled(validate(mEditing));
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    @Override
    public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
        if (compoundButton == mAlwaysOnVpn) {
            updateSaveLoginStatus();
            mBtnOK.setEnabled(validate(mEditing));
        }
    }

    public boolean isVpnAlwaysOn() {
        return mAlwaysOnVpn.isChecked();
    }

    private void updateSaveLoginStatus() {
        if (mAlwaysOnVpn.isChecked()) {
            mSaveLogin.setChecked(true);
            mSaveLogin.setEnabled(false);
        } else {
            mSaveLogin.setChecked(mProfile.saveLogin);
            mSaveLogin.setEnabled(true);
        }
    }

    private void showAdvancedOptions() {
        mView.findViewById(R.id.options).setVisibility(View.VISIBLE);
        mShowOptions.setVisibility(View.GONE);
    }

    private void changeType(int type) {
        // First, hide everything.
        mMppe.setVisibility(View.GONE);
        mView.findViewById(R.id.l2tp).setVisibility(View.GONE);
        mView.findViewById(R.id.ipsec_psk).setVisibility(View.GONE);
        mView.findViewById(R.id.ipsec_user).setVisibility(View.GONE);
        mView.findViewById(R.id.ipsec_peer).setVisibility(View.GONE);

        // Then, unhide type-specific fields.
        switch (type) {
            case VpnProfile.TYPE_PPTP:
                mMppe.setVisibility(View.VISIBLE);
                break;

            case VpnProfile.TYPE_L2TP_IPSEC_PSK:
                mView.findViewById(R.id.l2tp).setVisibility(View.VISIBLE);
                // fall through
            case VpnProfile.TYPE_IPSEC_XAUTH_PSK:
                mView.findViewById(R.id.ipsec_psk).setVisibility(View.VISIBLE);
                break;

            case VpnProfile.TYPE_L2TP_IPSEC_RSA:
                mView.findViewById(R.id.l2tp).setVisibility(View.VISIBLE);
                // fall through
            case VpnProfile.TYPE_IPSEC_XAUTH_RSA:
                mView.findViewById(R.id.ipsec_user).setVisibility(View.VISIBLE);
                // fall through
            case VpnProfile.TYPE_IPSEC_HYBRID_RSA:
                mView.findViewById(R.id.ipsec_peer).setVisibility(View.VISIBLE);
                break;
        }
        updateFocus();
    }

    private boolean validate(boolean editing) {
        if (mAlwaysOnVpn.isChecked() && !getProfile().isValidLockdownProfile()) {
            return false;
        }
        if (!editing) {
            return mUsername.getText().length() != 0 && mPassword.getText().length() != 0;
        }
        if (mName.getText().length() == 0 || mServer.getText().length() == 0 ||
                !validateAddresses(mDnsServers.getText().toString(), false) ||
                !validateAddresses(mRoutes.getText().toString(), true)) {
            return false;
        }
        switch (mType.getSelectedItemPosition()) {
            case VpnProfile.TYPE_PPTP:
            case VpnProfile.TYPE_IPSEC_HYBRID_RSA:
                return true;

            case VpnProfile.TYPE_L2TP_IPSEC_PSK:
            case VpnProfile.TYPE_IPSEC_XAUTH_PSK:
                return mIpsecSecret.getText().length() != 0;

            case VpnProfile.TYPE_L2TP_IPSEC_RSA:
            case VpnProfile.TYPE_IPSEC_XAUTH_RSA:
                return mIpsecUserCert.getSelectedItemPosition() != 0;
        }
        return false;
    }

    private boolean validateAddresses(String addresses, boolean cidr) {
        try {
            for (String address : addresses.split(" ")) {
                if (address.isEmpty()) {
                    continue;
                }
                // Legacy VPN currently only supports IPv4.
                int prefixLength = 32;
                if (cidr) {
                    String[] parts = address.split("/", 2);
                    address = parts[0];
                    prefixLength = Integer.parseInt(parts[1]);
                }
                byte[] bytes = InetAddress.parseNumericAddress(address).getAddress();
                int integer = (bytes[3] & 0xFF) | (bytes[2] & 0xFF) << 8 |
                        (bytes[1] & 0xFF) << 16 | (bytes[0] & 0xFF) << 24;
                if (bytes.length != 4 || prefixLength < 0 || prefixLength > 32 ||
                        (prefixLength < 32 && (integer << prefixLength) != 0)) {
                    return false;
                }
            }
        } catch (Exception e) {
            return false;
        }
        return true;
    }

    private void loadCertificates(Spinner spinner, String prefix, int firstId, String selected) {
        Context context = this;
        String first = (firstId == 0) ? "" : context.getString(firstId);
        String[] certificates = mKeyStore.list(prefix);

        if (certificates == null || certificates.length == 0) {
            certificates = new String[]{first};
        } else {
            String[] array = new String[certificates.length + 1];
            array[0] = first;
            System.arraycopy(certificates, 0, array, 1, certificates.length);
            certificates = array;
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<String>(
                context, android.R.layout.simple_spinner_item, certificates);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);

        for (int i = 1; i < certificates.length; ++i) {
            if (certificates[i].equals(selected)) {
                spinner.setSelection(i);
                break;
            }
        }
    }

    boolean isEditing() {
        return mEditing;
    }

    VpnProfile getProfile() {
        // First, save common fields.
        VpnProfile profile = new VpnProfile(mProfile.key);
        profile.name = mName.getText().toString();
        profile.type = mType.getSelectedItemPosition();
        profile.server = mServer.getText().toString().trim();
        profile.username = mUsername.getText().toString();
        profile.password = mPassword.getText().toString();
        profile.searchDomains = mSearchDomains.getText().toString().trim();
        profile.dnsServers = mDnsServers.getText().toString().trim();
        profile.routes = mRoutes.getText().toString().trim();

        // Then, save type-specific fields.
        switch (profile.type) {
            case VpnProfile.TYPE_PPTP:
                profile.mppe = mMppe.isChecked();
                break;

            case VpnProfile.TYPE_L2TP_IPSEC_PSK:
                profile.l2tpSecret = mL2tpSecret.getText().toString();
                // fall through
            case VpnProfile.TYPE_IPSEC_XAUTH_PSK:
                profile.ipsecIdentifier = mIpsecIdentifier.getText().toString();
                profile.ipsecSecret = mIpsecSecret.getText().toString();
                break;

            case VpnProfile.TYPE_L2TP_IPSEC_RSA:
                profile.l2tpSecret = mL2tpSecret.getText().toString();
                // fall through
            case VpnProfile.TYPE_IPSEC_XAUTH_RSA:
                if (mIpsecUserCert.getSelectedItemPosition() != 0) {
                    profile.ipsecUserCert = (String) mIpsecUserCert.getSelectedItem();
                }
                // fall through
            case VpnProfile.TYPE_IPSEC_HYBRID_RSA:
                if (mIpsecCaCert.getSelectedItemPosition() != 0) {
                    profile.ipsecCaCert = (String) mIpsecCaCert.getSelectedItem();
                }
                if (mIpsecServerCert.getSelectedItemPosition() != 0) {
                    profile.ipsecServerCert = (String) mIpsecServerCert.getSelectedItem();
                }
                break;
        }

        final boolean hasLogin = !profile.username.isEmpty() || !profile.password.isEmpty();
        profile.saveLogin = mSaveLogin.isChecked() || (mEditing && hasLogin);
        return profile;
    }


    private void setButton(Button button, View.OnClickListener onClickListener) {
        //button.setText(text);
        button.setOnClickListener(onClickListener);
    }

    private void updateLockdownVpn(boolean isVpnAlwaysOn, VpnProfile profile) {
        // Save lockdown vpn
        if (isVpnAlwaysOn) {
            // Show toast if vpn profile is not valid
            if (!profile.isValidLockdownProfile()) {
                Toast.makeText(this, R.string.vpn_lockdown_config_error,
                        Toast.LENGTH_LONG).show();
                return;
            }

            final ConnectivityManager conn = ConnectivityManager.from(this);
            conn.setAlwaysOnVpnPackageForUser(UserHandle.myUserId(), null,
                    /* lockdownEnabled */ false, null);
            VpnUtils.setLockdownVpn(this, profile.key);
        } else {
            // update only if lockdown vpn has been changed
            if (VpnUtils.isVpnLockdown(profile.key)) {
                VpnUtils.clearLockdownVpn(this);
            }
        }
    }

    private void disconnect(VpnProfile profile) {
        try {
            LegacyVpnInfo connected = mService.getLegacyVpnInfo(UserHandle.myUserId());
            if (connected != null && profile.key.equals(connected.key)) {
                VpnUtils.clearLockdownVpn(this);
                mService.prepareVpn(VpnConfig.LEGACY_VPN, VpnConfig.LEGACY_VPN,
                        UserHandle.myUserId());
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to disconnect", e);
        }
    }

    private void connect(VpnProfile profile) throws RemoteException {
        try {
            mService.startLegacyVpn(profile);
        } catch (IllegalStateException e) {
            Toast.makeText(this, R.string.vpn_no_network, Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    private void updateFocus() {
        if (!mEditing) {
            mPassword.setNextFocusDownId(R.id.save_login);
            mAlwaysOnVpn.setNextFocusDownId(R.id.btn_ok);
        } else if (!mExists) {
            mName.setNextFocusDownId(R.id.type);
            mType.setNextFocusDownId(mServer.getId());
            mPassword.setNextFocusDownId(R.id.always_on_vpn);
            mAlwaysOnVpn.setNextFocusDownId(R.id.btn_ok);
            switch (mType.getSelectedItemPosition()) {
                case 0:
                    mServer.setNextFocusDownId(mMppe.getId());
                    mAlwaysOnVpn.setNextFocusDownId(mBtnOK.getId());
                    break;
                case 1:
                case 2:
                    mServer.setNextFocusDownId(mL2tpSecret.getId());
                    if (mShowOptions.getVisibility() == View.VISIBLE)
                        mIpsecSecret.setNextFocusDownId(mShowOptions.getId());
                    else
                        mIpsecSecret.setNextFocusDownId(mSearchDomains.getId());
                    break;
                case 3:
                    mServer.setNextFocusDownId(mIpsecIdentifier.getId());
                    if (mShowOptions.getVisibility() == View.VISIBLE)
                        mIpsecSecret.setNextFocusDownId(mShowOptions.getId());
                    else
                        mIpsecSecret.setNextFocusDownId(mSearchDomains.getId());
                    break;
                case 4:
                    mServer.setNextFocusDownId(mIpsecUserCert.getId());
                    if (mShowOptions.getVisibility() == View.VISIBLE)
                        mIpsecServerCert.setNextFocusDownId(mShowOptions.getId());
                    else
                        mIpsecServerCert.setNextFocusDownId(mSearchDomains.getId());
                    break;
                case 5:
                    mServer.setNextFocusDownId(mIpsecCaCert.getId());
                    if (mShowOptions.getVisibility() == View.VISIBLE)
                        mIpsecServerCert.setNextFocusDownId(mShowOptions.getId());
                    else
                        mIpsecServerCert.setNextFocusDownId(mSearchDomains.getId());
                    break;
                default:
                    break;
            }
        }
    }
}
