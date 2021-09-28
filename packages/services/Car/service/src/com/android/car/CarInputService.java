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
package com.android.car;

import static android.hardware.input.InputManager.INJECT_INPUT_EVENT_MODE_ASYNC;
import static android.service.voice.VoiceInteractionSession.SHOW_SOURCE_PUSH_TO_TALK;

import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadsetClient;
import android.bluetooth.BluetoothProfile;
import android.car.CarProjectionManager;
import android.car.input.CarInputHandlingService;
import android.car.input.CarInputHandlingService.InputFilter;
import android.car.input.CarInputManager;
import android.car.input.ICarInput;
import android.car.input.ICarInputCallback;
import android.car.input.ICarInputListener;
import android.car.input.RotaryEvent;
import android.car.user.CarUserManager;
import android.car.userlib.UserHelper;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.Resources;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.UserHandle;
import android.provider.CallLog.Calls;
import android.provider.Settings;
import android.telecom.TelecomManager;
import android.text.TextUtils;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.ViewConfiguration;

import com.android.car.hal.InputHalService;
import com.android.car.user.CarUserService;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.app.AssistUtils;
import com.android.internal.app.IVoiceInteractionSessionShowCallback;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.Collections;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.IntSupplier;
import java.util.function.Supplier;

/**
 * CarInputService monitors and handles input event through vehicle HAL.
 */
public class CarInputService extends ICarInput.Stub
        implements CarServiceBase, InputHalService.InputListener {

    /** An interface to receive {@link KeyEvent}s as they occur. */
    public interface KeyEventListener {
        /** Called when a key event occurs. */
        void onKeyEvent(KeyEvent event);
    }

    private final class KeyPressTimer {
        private final Runnable mLongPressRunnable;
        private final Runnable mCallback = this::onTimerExpired;
        private final IntSupplier mLongPressDelaySupplier;

        @GuardedBy("CarInputService.this.mLock")
        private final Handler mHandler;
        @GuardedBy("CarInputService.this.mLock")
        private boolean mDown;
        @GuardedBy("CarInputService.this.mLock")
        private boolean mLongPress = false;

        KeyPressTimer(
                Handler handler, IntSupplier longPressDelaySupplier, Runnable longPressRunnable) {
            mHandler = handler;
            mLongPressRunnable = longPressRunnable;
            mLongPressDelaySupplier = longPressDelaySupplier;
        }

        /** Marks that a key was pressed, and starts the long-press timer. */
        void keyDown() {
            synchronized (mLock) {
                mDown = true;
                mLongPress = false;
                mHandler.removeCallbacks(mCallback);
                mHandler.postDelayed(mCallback, mLongPressDelaySupplier.getAsInt());
            }
        }

        /**
         * Marks that a key was released, and stops the long-press timer.
         *
         * Returns true if the press was a long-press.
         */
        boolean keyUp() {
            synchronized (mLock) {
                mHandler.removeCallbacks(mCallback);
                mDown = false;
                return mLongPress;
            }
        }

        private void onTimerExpired() {
            synchronized (mLock) {
                // If the timer expires after key-up, don't retroactively make the press long.
                if (!mDown) {
                    return;
                }
                mLongPress = true;
            }
            mLongPressRunnable.run();
        }
    }

    private final IVoiceInteractionSessionShowCallback mShowCallback =
            new IVoiceInteractionSessionShowCallback.Stub() {
                @Override
                public void onFailed() {
                    Log.w(CarLog.TAG_INPUT, "Failed to show VoiceInteractionSession");
                }

                @Override
                public void onShown() {
                    if (DBG) {
                        Log.d(CarLog.TAG_INPUT, "IVoiceInteractionSessionShowCallback onShown()");
                    }
                }
            };

    private static final boolean DBG = false;
    @VisibleForTesting
    static final String EXTRA_CAR_PUSH_TO_TALK =
            "com.android.car.input.EXTRA_CAR_PUSH_TO_TALK";

    private final Context mContext;
    private final InputHalService mInputHalService;
    private final CarUserService mUserService;
    private final TelecomManager mTelecomManager;
    private final AssistUtils mAssistUtils;
    // The ComponentName of the CarInputListener service. Can be changed via resource overlay,
    // or overridden directly for testing.
    @Nullable
    private final ComponentName mCustomInputServiceComponent;
    // The default handler for main-display input events. By default, injects the events into
    // the input queue via InputManager, but can be overridden for testing.
    private final KeyEventListener mMainDisplayHandler;
    // The supplier for the last-called number. By default, gets the number from the call log.
    // May be overridden for testing.
    private final Supplier<String> mLastCalledNumberSupplier;
    // The supplier for the system long-press delay, in milliseconds. By default, gets the value
    // from Settings.Secure for the current user, falling back to the system-wide default
    // long-press delay defined in ViewConfiguration. May be overridden for testing.
    private final IntSupplier mLongPressDelaySupplier;
    // ComponentName of the RotaryService.
    private final String mRotaryServiceComponentName;

    private final BooleanSupplier mShouldCallButtonEndOngoingCallSupplier;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private CarProjectionManager.ProjectionKeyEventHandler mProjectionKeyEventHandler;

    @GuardedBy("mLock")
    private final BitSet mProjectionKeyEventsSubscribed = new BitSet();

    private final KeyPressTimer mVoiceKeyTimer;
    private final KeyPressTimer mCallKeyTimer;

    @GuardedBy("mLock")
    private KeyEventListener mInstrumentClusterKeyListener;

    @GuardedBy("mLock")
    @VisibleForTesting
    ICarInputListener mCarInputListener;

    @GuardedBy("mLock")
    private boolean mCarInputListenerBound = false;

    // Maps display -> keycodes handled.
    @GuardedBy("mLock")
    private final SetMultimap<Integer, Integer> mHandledKeys = new SetMultimap<>();

    private final InputCaptureClientController mCaptureController;

    private final Binder mCallback = new Binder() {
        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
            if (code == CarInputHandlingService.INPUT_CALLBACK_BINDER_CODE) {
                data.setDataPosition(0);
                InputFilter[] handledKeys = (InputFilter[]) data.createTypedArray(
                        InputFilter.CREATOR);
                if (handledKeys != null) {
                    setHandledKeys(handledKeys);
                }
                return true;
            }
            return false;
        }
    };

    private final ServiceConnection mInputServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder binder) {
            if (DBG) {
                Log.d(CarLog.TAG_INPUT, "onServiceConnected, name: "
                        + name + ", binder: " + binder);
            }
            synchronized (mLock) {
                mCarInputListener = ICarInputListener.Stub.asInterface(binder);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.d(CarLog.TAG_INPUT, "onServiceDisconnected, name: " + name);
            synchronized (mLock) {
                mCarInputListener = null;
            }
        }
    };

    private final BluetoothAdapter mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

    // BluetoothHeadsetClient set through mBluetoothProfileServiceListener, and used by
    // launchBluetoothVoiceRecognition().
    @GuardedBy("mLock")
    private BluetoothHeadsetClient mBluetoothHeadsetClient;

    private final BluetoothProfile.ServiceListener mBluetoothProfileServiceListener =
            new BluetoothProfile.ServiceListener() {
        @Override
        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            if (profile == BluetoothProfile.HEADSET_CLIENT) {
                Log.d(CarLog.TAG_INPUT, "Bluetooth proxy connected for HEADSET_CLIENT profile");
                synchronized (mLock) {
                    mBluetoothHeadsetClient = (BluetoothHeadsetClient) proxy;
                }
            }
        }

        @Override
        public void onServiceDisconnected(int profile) {
            if (profile == BluetoothProfile.HEADSET_CLIENT) {
                Log.d(CarLog.TAG_INPUT, "Bluetooth proxy disconnected for HEADSET_CLIENT profile");
                synchronized (mLock) {
                    mBluetoothHeadsetClient = null;
                }
            }
        }
    };

    private final CarUserManager.UserLifecycleListener mUserLifecycleListener = event -> {
        Log.d(CarLog.TAG_INPUT, "CarInputService.onEvent(" + event + ")");
        if (CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING == event.getEventType()) {
            updateRotaryServiceSettings(event.getUserId());
        }
    };

    @Nullable
    private static ComponentName getDefaultInputComponent(Context context) {
        String carInputService = context.getString(R.string.inputService);
        return TextUtils.isEmpty(carInputService)
                ? null : ComponentName.unflattenFromString(carInputService);
    }

    private static int getViewLongPressDelay(ContentResolver cr) {
        return Settings.Secure.getIntForUser(
                cr,
                Settings.Secure.LONG_PRESS_TIMEOUT,
                ViewConfiguration.getLongPressTimeout(),
                UserHandle.USER_CURRENT);
    }

    public CarInputService(Context context, InputHalService inputHalService,
            CarUserService userService) {
        this(context, inputHalService, userService, new Handler(Looper.getMainLooper()),
                context.getSystemService(TelecomManager.class), new AssistUtils(context),
                event ->
                        context.getSystemService(InputManager.class)
                                .injectInputEvent(event, INJECT_INPUT_EVENT_MODE_ASYNC),
                () -> Calls.getLastOutgoingCall(context),
                getDefaultInputComponent(context),
                () -> getViewLongPressDelay(context.getContentResolver()),
                () -> context.getResources().getBoolean(R.bool.config_callButtonEndsOngoingCall));
    }

    @VisibleForTesting
    CarInputService(Context context, InputHalService inputHalService, CarUserService userService,
            Handler handler, TelecomManager telecomManager, AssistUtils assistUtils,
            KeyEventListener mainDisplayHandler, Supplier<String> lastCalledNumberSupplier,
            @Nullable ComponentName customInputServiceComponent,
            IntSupplier longPressDelaySupplier,
            BooleanSupplier shouldCallButtonEndOngoingCallSupplier) {
        mContext = context;
        mCaptureController = new InputCaptureClientController(context);
        mInputHalService = inputHalService;
        mUserService = userService;
        mTelecomManager = telecomManager;
        mAssistUtils = assistUtils;
        mMainDisplayHandler = mainDisplayHandler;
        mLastCalledNumberSupplier = lastCalledNumberSupplier;
        mCustomInputServiceComponent = customInputServiceComponent;
        mLongPressDelaySupplier = longPressDelaySupplier;

        mVoiceKeyTimer =
                new KeyPressTimer(
                        handler, longPressDelaySupplier, this::handleVoiceAssistLongPress);
        mCallKeyTimer =
                new KeyPressTimer(handler, longPressDelaySupplier, this::handleCallLongPress);

        mRotaryServiceComponentName = mContext.getString(R.string.rotaryService);
        mShouldCallButtonEndOngoingCallSupplier = shouldCallButtonEndOngoingCallSupplier;
    }

    @VisibleForTesting
    void setHandledKeys(InputFilter[] handledKeys) {
        synchronized (mLock) {
            mHandledKeys.clear();
            for (InputFilter handledKey : handledKeys) {
                mHandledKeys.put(handledKey.mTargetDisplay, handledKey.mKeyCode);
            }
        }
    }

    /**
     * Set projection key event listener. If null, unregister listener.
     */
    public void setProjectionKeyEventHandler(
            @Nullable CarProjectionManager.ProjectionKeyEventHandler listener,
            @Nullable BitSet events) {
        synchronized (mLock) {
            mProjectionKeyEventHandler = listener;
            mProjectionKeyEventsSubscribed.clear();
            if (events != null) {
                mProjectionKeyEventsSubscribed.or(events);
            }
        }
    }

    public void setInstrumentClusterKeyListener(KeyEventListener listener) {
        synchronized (mLock) {
            mInstrumentClusterKeyListener = listener;
        }
    }

    @Override
    public void init() {
        if (!mInputHalService.isKeyInputSupported()) {
            Log.w(CarLog.TAG_INPUT, "Hal does not support key input.");
            return;
        } else if (DBG) {
            Log.d(CarLog.TAG_INPUT, "Hal supports key input.");
        }

        mInputHalService.setInputListener(this);
        synchronized (mLock) {
            mCarInputListenerBound = bindCarInputService();
        }
        if (mBluetoothAdapter != null) {
            mBluetoothAdapter.getProfileProxy(
                    mContext, mBluetoothProfileServiceListener, BluetoothProfile.HEADSET_CLIENT);
        }
        if (!TextUtils.isEmpty(mRotaryServiceComponentName)) {
            mUserService.addUserLifecycleListener(mUserLifecycleListener);
        }
    }

    @Override
    public void release() {
        synchronized (mLock) {
            mProjectionKeyEventHandler = null;
            mProjectionKeyEventsSubscribed.clear();
            mInstrumentClusterKeyListener = null;
            if (mCarInputListenerBound) {
                mContext.unbindService(mInputServiceConnection);
                mCarInputListenerBound = false;
            }
            if (mBluetoothHeadsetClient != null) {
                mBluetoothAdapter.closeProfileProxy(
                        BluetoothProfile.HEADSET_CLIENT, mBluetoothHeadsetClient);
                mBluetoothHeadsetClient = null;
            }
        }
        if (!TextUtils.isEmpty(mRotaryServiceComponentName)) {
            mUserService.removeUserLifecycleListener(mUserLifecycleListener);
        }
    }

    @Override
    public void onKeyEvent(KeyEvent event, int targetDisplay) {
        // Give a car specific input listener the opportunity to intercept any input from the car
        ICarInputListener carInputListener;
        synchronized (mLock) {
            carInputListener = mCarInputListener;
        }
        if (carInputListener != null && isCustomEventHandler(event, targetDisplay)) {
            try {
                carInputListener.onKeyEvent(event, targetDisplay);
            } catch (RemoteException e) {
                Log.e(CarLog.TAG_INPUT, "Error while calling car input service", e);
            }
            // Custom input service handled the event, nothing more to do here.
            return;
        }

        // Special case key code that have special "long press" handling for automotive
        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_VOICE_ASSIST:
                handleVoiceAssistKey(event);
                return;
            case KeyEvent.KEYCODE_CALL:
                handleCallKey(event);
                return;
            default:
                break;
        }

        // Allow specifically targeted keys to be routed to the cluster
        if (targetDisplay == InputHalService.DISPLAY_INSTRUMENT_CLUSTER) {
            handleInstrumentClusterKey(event);
        } else {
            if (mCaptureController.onKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, event)) {
                return;
            }
            mMainDisplayHandler.onKeyEvent(event);
        }
    }

    @Override
    public void onRotaryEvent(RotaryEvent event, int targetDisplay) {
        if (!mCaptureController.onRotaryEvent(targetDisplay, event)) {
            List<KeyEvent> keyEvents = rotaryEventToKeyEvents(event);
            for (KeyEvent keyEvent : keyEvents) {
                onKeyEvent(keyEvent, targetDisplay);
            }
        }
    }

    private static List<KeyEvent> rotaryEventToKeyEvents(RotaryEvent event) {
        int numClicks = event.getNumberOfClicks();
        int numEvents = numClicks * 2; // up / down per each click
        boolean clockwise = event.isClockwise();
        int keyCode;
        switch (event.getInputType()) {
            case CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION:
                keyCode = clockwise
                        ? KeyEvent.KEYCODE_NAVIGATE_NEXT
                        : KeyEvent.KEYCODE_NAVIGATE_PREVIOUS;
                break;
            case CarInputManager.INPUT_TYPE_ROTARY_VOLUME:
                keyCode = clockwise
                        ? KeyEvent.KEYCODE_VOLUME_UP
                        : KeyEvent.KEYCODE_VOLUME_DOWN;
                break;
            default:
                Log.e(CarLog.TAG_INPUT, "Unknown rotary input type: " + event.getInputType());
                return Collections.EMPTY_LIST;
        }
        ArrayList<KeyEvent> keyEvents = new ArrayList<>(numEvents);
        for (int i = 0; i < numClicks; i++) {
            long uptime = event.getUptimeMillisForClick(i);
            KeyEvent downEvent = createKeyEvent(/* down= */ true, uptime, uptime, keyCode);
            KeyEvent upEvent = createKeyEvent(/* down= */ false, uptime, uptime, keyCode);
            keyEvents.add(downEvent);
            keyEvents.add(upEvent);
        }
        return keyEvents;
    }

    private static KeyEvent createKeyEvent(boolean down, long downTime, long eventTime,
            int keyCode) {
        return new KeyEvent(
                downTime,
                eventTime,
                /* action= */ down ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP,
                keyCode,
                /* repeat= */ 0,
                /* metaState= */ 0,
                /* deviceId= */ 0,
                /* scancode= */ 0,
                /* flags= */ 0,
                InputDevice.SOURCE_CLASS_BUTTON);
    }

    @Override
    public int requestInputEventCapture(ICarInputCallback callback, int targetDisplayType,
            int[] inputTypes, int requestFlags) {
        return mCaptureController.requestInputEventCapture(callback, targetDisplayType, inputTypes,
                requestFlags);
    }

    @Override
    public void releaseInputEventCapture(ICarInputCallback callback, int targetDisplayType) {
        mCaptureController.releaseInputEventCapture(callback, targetDisplayType);
    }

    private boolean isCustomEventHandler(KeyEvent event, int targetDisplay) {
        synchronized (mLock) {
            return mHandledKeys.containsEntry(targetDisplay, event.getKeyCode());
        }
    }

    private void handleVoiceAssistKey(KeyEvent event) {
        int action = event.getAction();
        if (action == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
            mVoiceKeyTimer.keyDown();
            dispatchProjectionKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_KEY_DOWN);
        } else if (action == KeyEvent.ACTION_UP) {
            if (mVoiceKeyTimer.keyUp()) {
                // Long press already handled by handleVoiceAssistLongPress(), nothing more to do.
                // Hand it off to projection, if it's interested, otherwise we're done.
                dispatchProjectionKeyEvent(
                        CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_UP);
                return;
            }

            if (dispatchProjectionKeyEvent(
                    CarProjectionManager.KEY_EVENT_VOICE_SEARCH_SHORT_PRESS_KEY_UP)) {
                return;
            }

            launchDefaultVoiceAssistantHandler();
        }
    }

    private void handleVoiceAssistLongPress() {
        // If projection wants this event, let it take it.
        if (dispatchProjectionKeyEvent(
                CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_DOWN)) {
            return;
        }
        // Otherwise, try to launch voice recognition on a BT device.
        if (launchBluetoothVoiceRecognition()) {
            return;
        }
        // Finally, fallback to the default voice assist handling.
        launchDefaultVoiceAssistantHandler();
    }

    private void handleCallKey(KeyEvent event) {
        int action = event.getAction();
        if (action == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
            mCallKeyTimer.keyDown();
            dispatchProjectionKeyEvent(CarProjectionManager.KEY_EVENT_CALL_KEY_DOWN);
        } else if (action == KeyEvent.ACTION_UP) {
            if (mCallKeyTimer.keyUp()) {
                // Long press already handled by handleCallLongPress(), nothing more to do.
                // Hand it off to projection, if it's interested, otherwise we're done.
                dispatchProjectionKeyEvent(CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_UP);
                return;
            }

            if (acceptCallIfRinging()) {
                // Ringing call answered, nothing more to do.
                return;
            }

            if (mShouldCallButtonEndOngoingCallSupplier.getAsBoolean() && endCall()) {
                // On-going call ended, nothing more to do.
                return;
            }

            if (dispatchProjectionKeyEvent(
                    CarProjectionManager.KEY_EVENT_CALL_SHORT_PRESS_KEY_UP)) {
                return;
            }

            launchDialerHandler();
        }
    }

    private void handleCallLongPress() {
        // Long-press answers call if ringing, same as short-press.
        if (acceptCallIfRinging()) {
            return;
        }

        if (mShouldCallButtonEndOngoingCallSupplier.getAsBoolean() && endCall()) {
            return;
        }

        if (dispatchProjectionKeyEvent(CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_DOWN)) {
            return;
        }

        dialLastCallHandler();
    }

    private boolean dispatchProjectionKeyEvent(@CarProjectionManager.KeyEventNum int event) {
        CarProjectionManager.ProjectionKeyEventHandler projectionKeyEventHandler;
        synchronized (mLock) {
            projectionKeyEventHandler = mProjectionKeyEventHandler;
            if (projectionKeyEventHandler == null || !mProjectionKeyEventsSubscribed.get(event)) {
                // No event handler, or event handler doesn't want this event - we're done.
                return false;
            }
        }

        projectionKeyEventHandler.onKeyEvent(event);
        return true;
    }

    private void launchDialerHandler() {
        Log.i(CarLog.TAG_INPUT, "call key, launch dialer intent");
        Intent dialerIntent = new Intent(Intent.ACTION_DIAL);
        mContext.startActivityAsUser(dialerIntent, null, UserHandle.CURRENT_OR_SELF);
    }

    private void dialLastCallHandler() {
        Log.i(CarLog.TAG_INPUT, "call key, dialing last call");

        String lastNumber = mLastCalledNumberSupplier.get();
        if (!TextUtils.isEmpty(lastNumber)) {
            Intent callLastNumberIntent = new Intent(Intent.ACTION_CALL)
                    .setData(Uri.fromParts("tel", lastNumber, null))
                    .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivityAsUser(callLastNumberIntent, null, UserHandle.CURRENT_OR_SELF);
        }
    }

    private boolean acceptCallIfRinging() {
        if (mTelecomManager != null && mTelecomManager.isRinging()) {
            Log.i(CarLog.TAG_INPUT, "call key while ringing. Answer the call!");
            mTelecomManager.acceptRingingCall();
            return true;
        }
        return false;
    }

    private boolean endCall() {
        if (mTelecomManager != null && mTelecomManager.isInCall()) {
            Log.i(CarLog.TAG_INPUT, "End the call!");
            mTelecomManager.endCall();
            return true;
        }
        return false;
    }

    private boolean isBluetoothVoiceRecognitionEnabled() {
        Resources res = mContext.getResources();
        return res.getBoolean(R.bool.enableLongPressBluetoothVoiceRecognition);
    }

    private boolean launchBluetoothVoiceRecognition() {
        synchronized (mLock) {
            if (mBluetoothHeadsetClient == null || !isBluetoothVoiceRecognitionEnabled()) {
                return false;
            }
            // getConnectedDevices() does not make any guarantees about the order of the returned
            // list. As of 2019-02-26, this code is only triggered through a long-press of the
            // voice recognition key, so handling of multiple connected devices that support voice
            // recognition is not expected to be a primary use case.
            List<BluetoothDevice> devices = mBluetoothHeadsetClient.getConnectedDevices();
            if (devices != null) {
                for (BluetoothDevice device : devices) {
                    Bundle bundle = mBluetoothHeadsetClient.getCurrentAgFeatures(device);
                    if (bundle == null || !bundle.getBoolean(
                            BluetoothHeadsetClient.EXTRA_AG_FEATURE_VOICE_RECOGNITION)) {
                        continue;
                    }
                    if (mBluetoothHeadsetClient.startVoiceRecognition(device)) {
                        Log.d(CarLog.TAG_INPUT, "started voice recognition on BT device at "
                                + device.getAddress());
                        return true;
                    }
                }
            }
        }
        return false;
    }

    private void launchDefaultVoiceAssistantHandler() {
        Log.i(CarLog.TAG_INPUT, "voice key, invoke AssistUtils");

        if (mAssistUtils.getAssistComponentForUser(ActivityManager.getCurrentUser()) == null) {
            Log.w(CarLog.TAG_INPUT, "Unable to retrieve assist component for current user");
            return;
        }

        final Bundle args = new Bundle();
        args.putBoolean(EXTRA_CAR_PUSH_TO_TALK, true);

        mAssistUtils.showSessionForActiveService(args,
                SHOW_SOURCE_PUSH_TO_TALK, mShowCallback, null /*activityToken*/);
    }

    private void handleInstrumentClusterKey(KeyEvent event) {
        KeyEventListener listener = null;
        synchronized (mLock) {
            listener = mInstrumentClusterKeyListener;
        }
        if (listener == null) {
            return;
        }
        listener.onKeyEvent(event);
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*Input Service*");
        writer.println("mCustomInputServiceComponent: " + mCustomInputServiceComponent);
        synchronized (mLock) {
            writer.println("mCarInputListenerBound: " + mCarInputListenerBound);
            writer.println("mCarInputListener: " + mCarInputListener);
        }
        writer.println("Long-press delay: " + mLongPressDelaySupplier.getAsInt() + "ms");
        writer.println("Call button ends ongoing call: "
                + mShouldCallButtonEndOngoingCallSupplier.getAsBoolean());
        mCaptureController.dump(writer);
    }

    private boolean bindCarInputService() {
        if (mCustomInputServiceComponent == null) {
            Log.i(CarLog.TAG_INPUT, "Custom input service was not configured");
            return false;
        }

        Log.d(CarLog.TAG_INPUT, "bindCarInputService, component: " + mCustomInputServiceComponent);

        Intent intent = new Intent();
        Bundle extras = new Bundle();
        extras.putBinder(CarInputHandlingService.INPUT_CALLBACK_BINDER_KEY, mCallback);
        intent.putExtras(extras);
        intent.setComponent(mCustomInputServiceComponent);
        return mContext.bindService(intent, mInputServiceConnection, Context.BIND_AUTO_CREATE);
    }

    private void updateRotaryServiceSettings(@UserIdInt int userId) {
        if (UserHelper.isHeadlessSystemUser(userId)) {
            return;
        }
        ContentResolver contentResolver = mContext.getContentResolver();
        Settings.Secure.putStringForUser(contentResolver,
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
                mRotaryServiceComponentName,
                userId);
        Settings.Secure.putStringForUser(contentResolver,
                Settings.Secure.ACCESSIBILITY_ENABLED,
                "1",
                userId);
    }
}
