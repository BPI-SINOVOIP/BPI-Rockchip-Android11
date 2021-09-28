/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.car.hal;


import static android.hardware.automotive.vehicle.V2_0.VehicleProperty.AP_POWER_STATE_REPORT;
import static android.hardware.automotive.vehicle.V2_0.VehicleProperty.AP_POWER_STATE_REQ;
import static android.hardware.automotive.vehicle.V2_0.VehicleProperty.DISPLAY_BRIGHTNESS;

import android.annotation.Nullable;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateConfigFlag;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateReport;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateReq;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateReqIndex;
import android.hardware.automotive.vehicle.V2_0.VehicleApPowerStateShutdownParam;
import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.os.ServiceSpecificException;
import android.util.Log;

import com.android.car.CarLog;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;

public class PowerHalService extends HalServiceBase {
    // Set display brightness from 0-100%
    public static final int MAX_BRIGHTNESS = 100;

    private static final int[] SUPPORTED_PROPERTIES = new int[]{
            AP_POWER_STATE_REQ,
            AP_POWER_STATE_REPORT,
            DISPLAY_BRIGHTNESS
    };

    @VisibleForTesting
    public static final int SET_WAIT_FOR_VHAL = VehicleApPowerStateReport.WAIT_FOR_VHAL;
    @VisibleForTesting
    public static final int SET_DEEP_SLEEP_ENTRY = VehicleApPowerStateReport.DEEP_SLEEP_ENTRY;
    @VisibleForTesting
    public static final int SET_DEEP_SLEEP_EXIT = VehicleApPowerStateReport.DEEP_SLEEP_EXIT;
    @VisibleForTesting
    public static final int SET_SHUTDOWN_POSTPONE = VehicleApPowerStateReport.SHUTDOWN_POSTPONE;
    @VisibleForTesting
    public static final int SET_SHUTDOWN_START = VehicleApPowerStateReport.SHUTDOWN_START;
    @VisibleForTesting
    public static final int SET_ON = VehicleApPowerStateReport.ON;
    @VisibleForTesting
    public static final int SET_SHUTDOWN_PREPARE = VehicleApPowerStateReport.SHUTDOWN_PREPARE;
    @VisibleForTesting
    public static final int SET_SHUTDOWN_CANCELLED = VehicleApPowerStateReport.SHUTDOWN_CANCELLED;

    @VisibleForTesting
    public static final int SHUTDOWN_CAN_SLEEP = VehicleApPowerStateShutdownParam.CAN_SLEEP;
    @VisibleForTesting
    public static final int SHUTDOWN_IMMEDIATELY =
            VehicleApPowerStateShutdownParam.SHUTDOWN_IMMEDIATELY;
    @VisibleForTesting
    public static final int SHUTDOWN_ONLY = VehicleApPowerStateShutdownParam.SHUTDOWN_ONLY;

    private final Object mLock = new Object();

    private static String powerStateReportName(int state) {
        String baseName;
        switch(state) {
            case SET_WAIT_FOR_VHAL:      baseName = "WAIT_FOR_VHAL";      break;
            case SET_DEEP_SLEEP_ENTRY:   baseName = "DEEP_SLEEP_ENTRY";   break;
            case SET_DEEP_SLEEP_EXIT:    baseName = "DEEP_SLEEP_EXIT";    break;
            case SET_SHUTDOWN_POSTPONE:  baseName = "SHUTDOWN_POSTPONE";  break;
            case SET_SHUTDOWN_START:     baseName = "SHUTDOWN_START";     break;
            case SET_ON:                 baseName = "ON";                 break;
            case SET_SHUTDOWN_PREPARE:   baseName = "SHUTDOWN_PREPARE";   break;
            case SET_SHUTDOWN_CANCELLED: baseName = "SHUTDOWN_CANCELLED"; break;
            default:                     baseName = "<unknown>";          break;
        }
        return baseName + "(" + state + ")";
    }

    private static String powerStateReqName(int state) {
        String baseName;
        switch(state) {
            case VehicleApPowerStateReq.ON:               baseName = "ON";               break;
            case VehicleApPowerStateReq.SHUTDOWN_PREPARE: baseName = "SHUTDOWN_PREPARE"; break;
            case VehicleApPowerStateReq.CANCEL_SHUTDOWN:  baseName = "CANCEL_SHUTDOWN";  break;
            case VehicleApPowerStateReq.FINISHED:         baseName = "FINISHED";         break;
            default:                                      baseName = "<unknown>";        break;
        }
        return baseName + "(" + state + ")";
    }

    public interface PowerEventListener {
        /**
         * Received power state change event.
         * @param state One of STATE_*
         */
        void onApPowerStateChange(PowerState state);
        /**
         * Received display brightness change event.
         * @param brightness in percentile. 100% full.
         */
        void onDisplayBrightnessChange(int brightness);
    }

    public static final class PowerState {
        /**
         * One of STATE_*
         */
        public final int mState;
        public final int mParam;

        public PowerState(int state, int param) {
            this.mState = state;
            this.mParam = param;
        }

        /**
         * Whether the current PowerState allows deep sleep or not. Calling this for
         * power state other than STATE_SHUTDOWN_PREPARE will trigger exception.
         * @return
         * @throws IllegalStateException
         */
        public boolean canEnterDeepSleep() {
            if (mState != VehicleApPowerStateReq.SHUTDOWN_PREPARE) {
                throw new IllegalStateException("wrong state");
            }
            return (mParam == VehicleApPowerStateShutdownParam.CAN_SLEEP
                    || mParam == VehicleApPowerStateShutdownParam.SLEEP_IMMEDIATELY);
        }

        /**
         * Whether the current PowerState allows postponing or not. Calling this for
         * power state other than STATE_SHUTDOWN_PREPARE will trigger exception.
         * @return
         * @throws IllegalStateException
         */
        public boolean canPostponeShutdown() {
            if (mState != VehicleApPowerStateReq.SHUTDOWN_PREPARE) {
                throw new IllegalStateException("wrong state");
            }
            return (mParam != VehicleApPowerStateShutdownParam.SHUTDOWN_IMMEDIATELY
                    && mParam != VehicleApPowerStateShutdownParam.SLEEP_IMMEDIATELY);
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (!(o instanceof PowerState)) {
                return false;
            }
            PowerState that = (PowerState) o;
            return this.mState == that.mState && this.mParam == that.mParam;
        }

        @Override
        public String toString() {
            return "PowerState state:" + mState + ", param:" + mParam;
        }
    }

    @GuardedBy("mLock")
    private final HashMap<Integer, VehiclePropConfig> mProperties = new HashMap<>();
    private final VehicleHal mHal;
    @GuardedBy("mLock")
    private LinkedList<VehiclePropValue> mQueuedEvents;
    @GuardedBy("mLock")
    private PowerEventListener mListener;
    @GuardedBy("mLock")
    private int mMaxDisplayBrightness;

    public PowerHalService(VehicleHal hal) {
        mHal = hal;
    }

    public void setListener(PowerEventListener listener) {
        LinkedList<VehiclePropValue> eventsToDispatch = null;
        synchronized (mLock) {
            mListener = listener;
            if (mQueuedEvents != null && mQueuedEvents.size() > 0) {
                eventsToDispatch = mQueuedEvents;
            }
            mQueuedEvents = null;
        }
        // do this outside lock
        if (eventsToDispatch != null) {
            dispatchEvents(eventsToDispatch, listener);
        }
    }

    /**
     * Send WaitForVhal message to VHAL
     */
    public void sendWaitForVhal() {
        Log.i(CarLog.TAG_POWER, "send wait for vhal");
        setPowerState(VehicleApPowerStateReport.WAIT_FOR_VHAL, 0);
    }

    /**
     * Send SleepEntry message to VHAL
     * @param wakeupTimeSec Notify VHAL when system wants to be woken from sleep.
     */
    public void sendSleepEntry(int wakeupTimeSec) {
        Log.i(CarLog.TAG_POWER, "send sleep entry");
        setPowerState(VehicleApPowerStateReport.DEEP_SLEEP_ENTRY, wakeupTimeSec);
    }

    /**
     * Send SleepExit message to VHAL
     * Notifies VHAL when SOC has woken.
     */
    public void sendSleepExit() {
        Log.i(CarLog.TAG_POWER, "send sleep exit");
        setPowerState(VehicleApPowerStateReport.DEEP_SLEEP_EXIT, 0);
    }

    /**
     * Send Shutdown Postpone message to VHAL
     */
    public void sendShutdownPostpone(int postponeTimeMs) {
        Log.i(CarLog.TAG_POWER, "send shutdown postpone, time:" + postponeTimeMs);
        setPowerState(VehicleApPowerStateReport.SHUTDOWN_POSTPONE, postponeTimeMs);
    }

    /**
     * Send Shutdown Start message to VHAL
     */
    public void sendShutdownStart(int wakeupTimeSec) {
        Log.i(CarLog.TAG_POWER, "send shutdown start");
        setPowerState(VehicleApPowerStateReport.SHUTDOWN_START, wakeupTimeSec);
    }

    /**
     * Send On message to VHAL
     */
    public void sendOn() {
        Log.i(CarLog.TAG_POWER, "send on");
        setPowerState(VehicleApPowerStateReport.ON, 0);
    }

    /**
     * Send Shutdown Prepare message to VHAL
     */
    public void sendShutdownPrepare() {
        Log.i(CarLog.TAG_POWER, "send shutdown prepare");
        setPowerState(VehicleApPowerStateReport.SHUTDOWN_PREPARE, 0);
    }

    /**
     * Send Shutdown Cancel message to VHAL
     */
    public void sendShutdownCancel() {
        Log.i(CarLog.TAG_POWER, "send shutdown cancel");
        setPowerState(VehicleApPowerStateReport.SHUTDOWN_CANCELLED, 0);
    }

    /**
     * Sets the display brightness for the vehicle.
     * @param brightness value from 0 to 100.
     */
    public void sendDisplayBrightness(int brightness) {
        if (brightness < 0) {
            brightness = 0;
        } else if (brightness > 100) {
            brightness = 100;
        }
        VehiclePropConfig prop = mProperties.get(DISPLAY_BRIGHTNESS);
        if (prop == null) {
            return;
        }
        try {
            mHal.set(VehicleProperty.DISPLAY_BRIGHTNESS, 0).to(brightness);
            Log.i(CarLog.TAG_POWER, "send display brightness = " + brightness);
        } catch (ServiceSpecificException | IllegalArgumentException e) {
            Log.e(CarLog.TAG_POWER, "cannot set DISPLAY_BRIGHTNESS", e);
        }
    }

    private void setPowerState(int state, int additionalParam) {
        if (isPowerStateSupported()) {
            int[] values = { state, additionalParam };
            try {
                mHal.set(VehicleProperty.AP_POWER_STATE_REPORT, 0).to(values);
                Log.i(CarLog.TAG_POWER, "setPowerState=" + powerStateReportName(state)
                        + " param=" + additionalParam);
            } catch (ServiceSpecificException e) {
                Log.e(CarLog.TAG_POWER, "cannot set to AP_POWER_STATE_REPORT", e);
            }
        }
    }

    @Nullable
    public PowerState getCurrentPowerState() {
        int[] state;
        try {
            state = mHal.get(int[].class, VehicleProperty.AP_POWER_STATE_REQ);
        } catch (ServiceSpecificException e) {
            Log.e(CarLog.TAG_POWER, "Cannot get AP_POWER_STATE_REQ", e);
            return null;
        }
        return new PowerState(state[VehicleApPowerStateReqIndex.STATE],
                state[VehicleApPowerStateReqIndex.ADDITIONAL]);
    }

    /**
     * Determines if the current properties describe a valid power state
     * @return true if both the power state request and power state report are valid
     */
    public boolean isPowerStateSupported() {
        synchronized (mLock) {
            return (mProperties.get(VehicleProperty.AP_POWER_STATE_REQ) != null)
                    && (mProperties.get(VehicleProperty.AP_POWER_STATE_REPORT) != null);
        }
    }

    private boolean isConfigFlagSet(int flag) {
        VehiclePropConfig config;
        synchronized (mLock) {
            config = mProperties.get(VehicleProperty.AP_POWER_STATE_REQ);
        }
        if (config == null || config.configArray.size() < 1) {
            return false;
        }
        return (config.configArray.get(0) & flag) != 0;
    }

    public boolean isDeepSleepAllowed() {
        return isConfigFlagSet(VehicleApPowerStateConfigFlag.ENABLE_DEEP_SLEEP_FLAG);
    }

    public boolean isTimedWakeupAllowed() {
        return isConfigFlagSet(VehicleApPowerStateConfigFlag.CONFIG_SUPPORT_TIMER_POWER_ON_FLAG);
    }

    @Override
    public void init() {
        synchronized (mLock) {
            for (VehiclePropConfig config : mProperties.values()) {
                if (VehicleHal.isPropertySubscribable(config)) {
                    mHal.subscribeProperty(this, config.prop);
                }
            }
            VehiclePropConfig brightnessProperty = mProperties.get(DISPLAY_BRIGHTNESS);
            if (brightnessProperty != null) {
                mMaxDisplayBrightness = brightnessProperty.areaConfigs.size() > 0
                        ? brightnessProperty.areaConfigs.get(0).maxInt32Value : 0;
                if (mMaxDisplayBrightness <= 0) {
                    Log.w(CarLog.TAG_POWER, "Max display brightness from vehicle HAL is invalid:"
                            + mMaxDisplayBrightness);
                    mMaxDisplayBrightness = 1;
                }
            }
        }
    }

    @Override
    public void release() {
        synchronized (mLock) {
            mProperties.clear();
        }
    }

    @Override
    public int[] getAllSupportedProperties() {
        return SUPPORTED_PROPERTIES;
    }

    @Override
    public void takeProperties(Collection<VehiclePropConfig> properties) {
        if (properties.isEmpty()) {
            return;
        }
        synchronized (mLock) {
            for (VehiclePropConfig config : properties) {
                mProperties.put(config.prop, config);
            }
        }
    }

    @Override
    public void onHalEvents(List<VehiclePropValue> values) {
        PowerEventListener listener;
        synchronized (mLock) {
            if (mListener == null) {
                if (mQueuedEvents == null) {
                    mQueuedEvents = new LinkedList<>();
                }
                mQueuedEvents.addAll(values);
                return;
            }
            listener = mListener;
        }
        dispatchEvents(values, listener);
    }

    private void dispatchEvents(List<VehiclePropValue> values, PowerEventListener listener) {
        for (VehiclePropValue v : values) {
            switch (v.prop) {
                case AP_POWER_STATE_REPORT:
                    // Ignore this property event. It was generated inside of CarService.
                    break;
                case AP_POWER_STATE_REQ:
                    int state = v.value.int32Values.get(VehicleApPowerStateReqIndex.STATE);
                    int param = v.value.int32Values.get(VehicleApPowerStateReqIndex.ADDITIONAL);
                    Log.i(CarLog.TAG_POWER, "Received AP_POWER_STATE_REQ="
                            + powerStateReqName(state) + " param=" + param);
                    listener.onApPowerStateChange(new PowerState(state, param));
                    break;
                case DISPLAY_BRIGHTNESS:
                {
                    int maxBrightness;
                    synchronized (mLock) {
                        maxBrightness = mMaxDisplayBrightness;
                    }
                    int brightness = v.value.int32Values.get(0) * MAX_BRIGHTNESS / maxBrightness;
                    if (brightness < 0) {
                        Log.e(CarLog.TAG_POWER, "invalid brightness: " + brightness + ", set to 0");
                        brightness = 0;
                    } else if (brightness > MAX_BRIGHTNESS) {
                        Log.e(CarLog.TAG_POWER, "invalid brightness: " + brightness + ", set to "
                                + MAX_BRIGHTNESS);
                        brightness = MAX_BRIGHTNESS;
                    }
                    Log.i(CarLog.TAG_POWER, "Received DISPLAY_BRIGHTNESS=" + brightness);
                    listener.onDisplayBrightnessChange(brightness);
                }
                    break;
            }
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*Power HAL*");
        writer.println("isPowerStateSupported:" + isPowerStateSupported() +
                ",isDeepSleepAllowed:" + isDeepSleepAllowed());
    }
}
