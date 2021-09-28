/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.car.usb.handler;

import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.Manifest;
import android.annotation.Nullable;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.XmlResourceParser;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import com.android.internal.util.XmlUtils;

import org.xmlpull.v1.XmlPullParser;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

/** Resolves supported handlers for USB device. */
public final class UsbDeviceHandlerResolver {

    private static final String TAG = UsbDeviceHandlerResolver.class.getSimpleName();
    private static final boolean LOCAL_LOGD = true;

    private static final String AOAP_HANDLE_PERMISSION =
            "android.car.permission.CAR_HANDLE_USB_AOAP_DEVICE";

    /**
     * Callbacks for device resolver.
     */
    public interface UsbDeviceHandlerResolverCallback {
        /** Handlers are resolved */
        void onHandlersResolveCompleted(
                UsbDevice device, List<UsbDeviceSettings> availableSettings);
        /** Device was dispatched */
        void onDeviceDispatched();
    }

    private final UsbManager mUsbManager;
    private final PackageManager mPackageManager;
    private final UsbDeviceHandlerResolverCallback mDeviceCallback;
    private final Context mContext;
    private final AoapServiceManager mAoapServiceManager;
    private HandlerThread mHandlerThread;
    private UsbDeviceResolverHandler mHandler;

    public UsbDeviceHandlerResolver(UsbManager manager, Context context,
            UsbDeviceHandlerResolverCallback deviceListener) {
        mUsbManager = manager;
        mContext = context;
        mDeviceCallback = deviceListener;
        createHandlerThread();
        mPackageManager = context.getPackageManager();
        mAoapServiceManager = new AoapServiceManager(mContext.getApplicationContext());
    }

    /**
     * Releases current object.
     */
    public void release() {
        if (mHandlerThread != null) {
            mHandlerThread.quitSafely();
        }
    }

    /**
     * Resolves handlers for USB device.
     */
    public void resolve(UsbDevice device) {
        mHandler.requestResolveHandlers(device);
    }

    /**
     * Listener for failed {@code startAosp} command.
     *
     * <p>If {@code startAosp} fails, the device could be left in a inconsistent state, that's why
     * we go back to USB enumeration, instead of just repeating the command.
     */
    public interface StartAoapFailureListener {

        /** Called if startAoap fails. */
        void onFailure();
    }

    /**
     * Dispatches device to component.
     */
    public boolean dispatch(UsbDevice device, ComponentName component, boolean inAoap,
            StartAoapFailureListener failureListener) {
        if (LOCAL_LOGD) {
            Log.d(TAG, "dispatch: " + device + " component: " + component + " inAoap: " + inAoap);
        }

        ActivityInfo activityInfo;
        try {
            activityInfo = mPackageManager.getActivityInfo(component, PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            Log.e(TAG, "Activity not found: " + component);
            return false;
        }

        Intent intent = createDeviceAttachedIntent(device);
        if (inAoap) {
            if (AoapInterface.isDeviceInAoapMode(device)) {
                mDeviceCallback.onDeviceDispatched();
            } else {
                UsbDeviceFilter filter =
                        packageMatches(activityInfo, intent.getAction(), device, true);

                if (filter != null) {
                    if (!mHandlerThread.isAlive()) {
                        // Start a new thread. Used only when startAoap fails, and we need to
                        // re-enumerate device in order to try again.
                        createHandlerThread();
                    }
                    mHandlerThread.getThreadHandler().post(() -> {
                        if (mAoapServiceManager.canSwitchDeviceToAoap(device,
                                ComponentName.unflattenFromString(filter.mAoapService))) {
                            try {
                                requestAoapSwitch(device, filter);
                            } catch (IOException e) {
                                Log.w(TAG, "Start AOAP command failed:" + e);
                                failureListener.onFailure();
                            }
                        } else {
                            Log.i(TAG, "Ignore AOAP switch for device " + device
                                    + " handled by " + filter.mAoapService);
                        }
                    });
                    mDeviceCallback.onDeviceDispatched();
                    return true;
                }
            }
        }

        intent.setComponent(component);
        mUsbManager.grantPermission(device, activityInfo.applicationInfo.uid);

        mContext.startActivity(intent);
        mHandler.requestCompleteDeviceDispatch();
        return true;
    }

    private void createHandlerThread() {
        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new UsbDeviceResolverHandler(mHandlerThread.getLooper());
    }

    private static Intent createDeviceAttachedIntent(UsbDevice device) {
        Intent intent = new Intent(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        intent.putExtra(UsbManager.EXTRA_DEVICE, device);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    private void doHandleResolveHandlers(UsbDevice device) {
        if (LOCAL_LOGD) {
            Log.d(TAG, "doHandleResolveHandlers: " + device);
        }

        Intent intent = createDeviceAttachedIntent(device);
        List<UsbHandlerPackage> matches = getDeviceMatches(device, intent, false);
        if (LOCAL_LOGD) {
            Log.d(TAG, "matches size: " + matches.size());
        }
        List<UsbDeviceSettings> settings = new ArrayList<>();
        for (UsbHandlerPackage pkg : matches) {
            settings.add(createSettings(device, pkg));
        }

        UsbDeviceConnection devConnection = UsbUtil.openConnection(mUsbManager, device);
        if (devConnection != null && AoapInterface.isSupported(mContext, device, devConnection)) {
            for (UsbHandlerPackage pkg : getDeviceMatches(device, intent, true)) {
                if (mAoapServiceManager.isDeviceSupported(device, pkg.mAoapService)) {
                    settings.add(createSettings(device, pkg));
                }
            }
        }

        deviceProbingComplete(device, settings);
    }

    private UsbDeviceSettings createSettings(UsbDevice device, UsbHandlerPackage pkg) {
        UsbDeviceSettings settings = UsbDeviceSettings.constructSettings(device);
        settings.setHandler(pkg.mActivity);
        settings.setAoap(pkg.mAoapService != null);
        return settings;
    }

    private void requestAoapSwitch(UsbDevice device, UsbDeviceFilter filter) throws IOException {
        UsbDeviceConnection connection = UsbUtil.openConnection(mUsbManager, device);
        if (connection == null) {
            Log.e(TAG, "Failed to connect to usb device.");
            return;
        }

        try {
            String hashedSerial =  getHashed(Build.getSerial());
            UsbUtil.sendAoapAccessoryStart(
                    connection,
                    filter.mAoapManufacturer,
                    filter.mAoapModel,
                    filter.mAoapDescription,
                    filter.mAoapVersion,
                    filter.mAoapUri,
                    hashedSerial);
        } finally {
            connection.close();
        }
    }

    private String getHashed(String serial) {
        try {
            return MessageDigest.getInstance("MD5").digest(serial.getBytes()).toString();
        } catch (NoSuchAlgorithmException e) {
            Log.w(TAG, "could not create MD5 for serial number: " + serial);
            return Integer.toString(serial.hashCode());
        }
    }

    private void deviceProbingComplete(UsbDevice device, List<UsbDeviceSettings> settings) {
        if (LOCAL_LOGD) {
            Log.d(TAG, "deviceProbingComplete");
        }
        mDeviceCallback.onHandlersResolveCompleted(device, settings);
    }

    private List<UsbHandlerPackage> getDeviceMatches(
            UsbDevice device, Intent intent, boolean forAoap) {
        List<UsbHandlerPackage> matches = new ArrayList<>();
        List<ResolveInfo> resolveInfos =
                mPackageManager.queryIntentActivities(intent, PackageManager.GET_META_DATA);
        for (ResolveInfo resolveInfo : resolveInfos) {
            final String packageName = resolveInfo.activityInfo.packageName;
            if (forAoap && !hasAoapPermission(packageName)) {
                Log.w(TAG, "Package " + packageName + " does not hold "
                        + AOAP_HANDLE_PERMISSION + " permission. Ignore the package.");
                continue;
            }

            UsbDeviceFilter filter = packageMatches(resolveInfo.activityInfo,
                    intent.getAction(), device, forAoap);
            if (filter != null) {
                ActivityInfo ai = resolveInfo.activityInfo;
                ComponentName activity = new ComponentName(ai.packageName, ai.name);
                ComponentName aoapService = filter.mAoapService == null
                        ? null : ComponentName.unflattenFromString(filter.mAoapService);

                if (aoapService != null && !checkServiceRequiresPermission(aoapService)) {
                    continue;
                }

                if (aoapService != null || !forAoap) {
                    matches.add(new UsbHandlerPackage(activity, aoapService));
                }
            }
        }
        return matches;
    }

    private boolean checkServiceRequiresPermission(ComponentName serviceName) {
        Intent intent = new Intent();
        intent.setComponent(serviceName);
        boolean found = false;
        for (ResolveInfo info : mPackageManager.queryIntentServices(intent, 0)) {
            if (info.serviceInfo != null) {
                found = true;
                if ((Manifest.permission.MANAGE_USB.equals(info.serviceInfo.permission))) {
                    return true;
                }
            }
        }
        if (found) {
            Log.w(TAG, "Component " + serviceName + " must be protected with "
                    + Manifest.permission.MANAGE_USB + " permission");
        } else {
            Log.w(TAG, "Component " + serviceName + " not found");
        }
        return false;
    }

    private boolean hasAoapPermission(String packageName) {
        return mPackageManager
                .checkPermission(AOAP_HANDLE_PERMISSION, packageName) == PERMISSION_GRANTED;
    }

    private UsbDeviceFilter packageMatches(ActivityInfo ai, String metaDataName, UsbDevice device,
            boolean forAoap) {
        if (LOCAL_LOGD) {
            Log.d(TAG, "packageMatches ai: " + ai + "metaDataName: " + metaDataName + " forAoap: "
                    + forAoap);
        }
        String filterTagName = forAoap ? "usb-aoap-accessory" : "usb-device";
        try (XmlResourceParser parser = ai.loadXmlMetaData(mPackageManager, metaDataName)) {
            if (parser == null) {
                Log.w(TAG, "no meta-data for " + ai);
                return null;
            }

            XmlUtils.nextElement(parser);
            while (parser.getEventType() != XmlPullParser.END_DOCUMENT) {
                String tagName = parser.getName();
                if (device != null && filterTagName.equals(tagName)) {
                    UsbDeviceFilter filter = UsbDeviceFilter.read(parser, forAoap);
                    if (forAoap || filter.matches(device)) {
                        return filter;
                    }
                }
                XmlUtils.nextElement(parser);
            }
        } catch (Exception e) {
            Log.w(TAG, "Unable to load component info " + ai.toString(), e);
        }
        return null;
    }

    private class UsbDeviceResolverHandler extends Handler {
        private static final int MSG_RESOLVE_HANDLERS = 0;
        private static final int MSG_COMPLETE_DISPATCH = 3;

        private UsbDeviceResolverHandler(Looper looper) {
            super(looper);
        }

        void requestResolveHandlers(UsbDevice device) {
            Message msg = obtainMessage(MSG_RESOLVE_HANDLERS, device);
            sendMessage(msg);
        }

        void requestCompleteDeviceDispatch() {
            sendEmptyMessage(MSG_COMPLETE_DISPATCH);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_RESOLVE_HANDLERS:
                    doHandleResolveHandlers((UsbDevice) msg.obj);
                    break;
                case MSG_COMPLETE_DISPATCH:
                    mDeviceCallback.onDeviceDispatched();
                    break;
                default:
                    Log.w(TAG, "Unsupported message: " + msg);
            }
        }
    }

    private static class UsbHandlerPackage {
        final ComponentName mActivity;
        final @Nullable ComponentName mAoapService;

        UsbHandlerPackage(ComponentName activity, @Nullable ComponentName aoapService) {
            mActivity = activity;
            mAoapService = aoapService;
        }
    }
}
