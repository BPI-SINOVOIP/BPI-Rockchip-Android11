/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static java.lang.Integer.toHexString;

import android.annotation.Nullable;
import android.car.diagnostic.CarDiagnosticEvent;
import android.car.diagnostic.CarDiagnosticManager;
import android.car.hardware.CarSensorManager;
import android.hardware.automotive.vehicle.V2_0.DiagnosticFloatSensorIndex;
import android.hardware.automotive.vehicle.V2_0.DiagnosticIntegerSensorIndex;
import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyChangeMode;
import android.os.ServiceSpecificException;
import android.util.Log;
import android.util.SparseArray;

import com.android.car.CarLog;
import com.android.car.CarServiceUtils;
import com.android.car.vehiclehal.VehiclePropValueBuilder;
import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;
import java.util.BitSet;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArraySet;

/**
 * Diagnostic HAL service supporting gathering diagnostic info from VHAL and translating it into
 * higher-level semantic information
 */
public class DiagnosticHalService extends HalServiceBase {
    static final int OBD2_SELECTIVE_FRAME_CLEAR = 1;
    static final boolean DEBUG = false;

    private static final int[] SUPPORTED_PROPERTIES = new int[]{
            VehicleProperty.OBD2_LIVE_FRAME,
            VehicleProperty.OBD2_FREEZE_FRAME,
            VehicleProperty.OBD2_FREEZE_FRAME_INFO,
            VehicleProperty.OBD2_FREEZE_FRAME_CLEAR
    };

    private final Object mLock = new Object();
    private final VehicleHal mVehicleHal;

    @GuardedBy("mLock")
    private boolean mIsReady = false;

    public static class DiagnosticCapabilities {
        private final CopyOnWriteArraySet<Integer> mProperties = new CopyOnWriteArraySet<>();

        void setSupported(int propertyId) {
            mProperties.add(propertyId);
        }

        boolean isSupported(int propertyId) {
            return mProperties.contains(propertyId);
        }

        public boolean isLiveFrameSupported() {
            return isSupported(VehicleProperty.OBD2_LIVE_FRAME);
        }

        public boolean isFreezeFrameSupported() {
            return isSupported(VehicleProperty.OBD2_FREEZE_FRAME);
        }

        public boolean isFreezeFrameInfoSupported() {
            return isSupported(VehicleProperty.OBD2_FREEZE_FRAME_INFO);
        }

        public boolean isFreezeFrameClearSupported() {
            return isSupported(VehicleProperty.OBD2_FREEZE_FRAME_CLEAR);
        }

        public boolean isSelectiveClearFreezeFramesSupported() {
            return isSupported(OBD2_SELECTIVE_FRAME_CLEAR);
        }

        void clear() {
            mProperties.clear();
        }
    }

    @GuardedBy("mLock")
    private final DiagnosticCapabilities mDiagnosticCapabilities = new DiagnosticCapabilities();

    @GuardedBy("mLock")
    private DiagnosticListener mDiagnosticListener;

    @GuardedBy("mLock")
    protected final SparseArray<VehiclePropConfig> mVehiclePropertyToConfig = new SparseArray<>();

    @GuardedBy("mLock")
    protected final SparseArray<VehiclePropConfig> mSensorTypeToConfig = new SparseArray<>();

    public DiagnosticHalService(VehicleHal hal) {
        mVehicleHal = hal;

    }

    @Override
    public int[] getAllSupportedProperties() {
        return SUPPORTED_PROPERTIES;
    }

    @Override
    public void takeProperties(Collection<VehiclePropConfig> properties) {
        if (DEBUG) {
            Log.d(CarLog.TAG_DIAGNOSTIC, "takeSupportedProperties");
        }
        for (VehiclePropConfig vp : properties) {
            int sensorType = getTokenForProperty(vp);
            if (sensorType == NOT_SUPPORTED_PROPERTY) {
                if (DEBUG) {
                    Log.d(CarLog.TAG_DIAGNOSTIC, new StringBuilder()
                                .append("0x")
                                .append(toHexString(vp.prop))
                                .append(" ignored")
                                .toString());
                }
            } else {
                synchronized (mLock) {
                    mSensorTypeToConfig.append(sensorType, vp);
                }
            }
        }
    }

    /**
     * Returns a unique token to be used to map this property to a higher-level sensor
     * This token will be stored in {@link DiagnosticHalService#mSensorTypeToConfig} to allow
     * callers to go from unique sensor identifiers to VehiclePropConfig objects
     * @param propConfig
     * @return SENSOR_TYPE_INVALID or a locally unique token
     */
    protected int getTokenForProperty(VehiclePropConfig propConfig) {
        switch (propConfig.prop) {
            case VehicleProperty.OBD2_LIVE_FRAME:
                mDiagnosticCapabilities.setSupported(propConfig.prop);
                mVehiclePropertyToConfig.put(propConfig.prop, propConfig);
                Log.i(CarLog.TAG_DIAGNOSTIC, String.format("configArray for OBD2_LIVE_FRAME is %s",
                    propConfig.configArray));
                return CarDiagnosticManager.FRAME_TYPE_LIVE;
            case VehicleProperty.OBD2_FREEZE_FRAME:
                mDiagnosticCapabilities.setSupported(propConfig.prop);
                mVehiclePropertyToConfig.put(propConfig.prop, propConfig);
                Log.i(CarLog.TAG_DIAGNOSTIC, String.format("configArray for OBD2_FREEZE_FRAME is %s",
                    propConfig.configArray));
                return CarDiagnosticManager.FRAME_TYPE_FREEZE;
            case VehicleProperty.OBD2_FREEZE_FRAME_INFO:
                mDiagnosticCapabilities.setSupported(propConfig.prop);
                return propConfig.prop;
            case VehicleProperty.OBD2_FREEZE_FRAME_CLEAR:
                mDiagnosticCapabilities.setSupported(propConfig.prop);
                Log.i(CarLog.TAG_DIAGNOSTIC, String.format(
                        "configArray for OBD2_FREEZE_FRAME_CLEAR is %s", propConfig.configArray));
                if (propConfig.configArray.size() < 1) {
                    Log.e(CarLog.TAG_DIAGNOSTIC, String.format(
                            "property 0x%x does not specify whether it supports selective " +
                            "clearing of freeze frames. assuming it does not.", propConfig.prop));
                } else {
                    if (propConfig.configArray.get(0) == 1) {
                        mDiagnosticCapabilities.setSupported(OBD2_SELECTIVE_FRAME_CLEAR);
                    }
                }
                return propConfig.prop;
            default:
                return NOT_SUPPORTED_PROPERTY;
        }
    }

    @Override
    public void init() {
        if (DEBUG) {
            Log.d(CarLog.TAG_DIAGNOSTIC, "init()");
        }
        synchronized (mLock) {
            mIsReady = true;
        }
    }

    @Override
    public void release() {
        synchronized (mLock) {
            mDiagnosticCapabilities.clear();
            mIsReady = false;
        }
    }

    /**
     * Returns the status of Diagnostic HAL.
     * @return true if Diagnostic HAL is ready after init call.
     */
    public boolean isReady() {
        return mIsReady;
    }

    /**
     * Returns an array of diagnostic property Ids implemented by this vehicle.
     *
     * @return Array of diagnostic property Ids implemented by this vehicle. Empty array if
     * no property available.
     */
    public int[] getSupportedDiagnosticProperties() {
        int[] supportedDiagnosticProperties;
        synchronized (mLock) {
            supportedDiagnosticProperties = new int[mSensorTypeToConfig.size()];
            for (int i = 0; i < supportedDiagnosticProperties.length; i++) {
                supportedDiagnosticProperties[i] = mSensorTypeToConfig.keyAt(i);
            }
        }
        return supportedDiagnosticProperties;
    }

    /**
     * Start to request diagnostic information.
     * @param sensorType
     * @param rate
     * @return true if request successfully. otherwise return false
     */
    public boolean requestDiagnosticStart(int sensorType, int rate) {
        VehiclePropConfig propConfig;
        synchronized (mLock) {
            propConfig = mSensorTypeToConfig.get(sensorType);
        }
        if (propConfig == null) {
            Log.e(CarLog.TAG_DIAGNOSTIC, new StringBuilder()
                    .append("VehiclePropConfig not found, propertyId: 0x")
                    .append(toHexString(propConfig.prop))
                    .toString());
            return false;
        }
        if (DEBUG) {
            Log.d(CarLog.TAG_DIAGNOSTIC, new StringBuilder()
                    .append("requestDiagnosticStart, propertyId: 0x")
                    .append(toHexString(propConfig.prop))
                    .append(", rate: ")
                    .append(rate)
                    .toString());
        }
        mVehicleHal.subscribeProperty(this, propConfig.prop,
                fixSamplingRateForProperty(propConfig, rate));
        return true;
    }

    /**
     * Stop requesting diagnostic information.
     * @param sensorType
     */
    public void requestDiagnosticStop(int sensorType) {
        VehiclePropConfig propConfig;
        synchronized (mLock) {
            propConfig = mSensorTypeToConfig.get(sensorType);
        }
        if (propConfig == null) {
            Log.e(CarLog.TAG_DIAGNOSTIC, new StringBuilder()
                    .append("VehiclePropConfig not found, propertyId: 0x")
                    .append(toHexString(propConfig.prop))
                    .toString());
            return;
        }
        if (DEBUG) {
            Log.d(CarLog.TAG_DIAGNOSTIC, new StringBuilder()
                    .append("requestDiagnosticStop, propertyId: 0x")
                    .append(toHexString(propConfig.prop))
                    .toString());
        }
        mVehicleHal.unsubscribeProperty(this, propConfig.prop);

    }

    /**
     * Query current diagnostic value
     * @param sensorType
     * @return VehiclePropValue of the property
     */
    @Nullable
    public VehiclePropValue getCurrentDiagnosticValue(int sensorType) {
        VehiclePropConfig propConfig;
        synchronized (mLock) {
            propConfig = mSensorTypeToConfig.get(sensorType);
        }
        if (propConfig == null) {
            Log.e(CarLog.TAG_DIAGNOSTIC, new StringBuilder()
                    .append("property not available 0x")
                    .append(toHexString(propConfig.prop))
                    .toString());
            return null;
        }
        try {
            return mVehicleHal.get(propConfig.prop);
        } catch (ServiceSpecificException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC,
                    "property not ready 0x" + toHexString(propConfig.prop), e);
            return null;
        }

    }

    private VehiclePropConfig getPropConfig(int halPropId) {
        VehiclePropConfig config;
        synchronized (mLock) {
            config = mVehiclePropertyToConfig.get(halPropId, null);
        }
        return config;
    }

    private List<Integer> getPropConfigArray(int halPropId) {
        VehiclePropConfig propConfig = getPropConfig(halPropId);
        return propConfig.configArray;
    }

    private int getNumIntegerSensors(int halPropId) {
        int count = DiagnosticIntegerSensorIndex.LAST_SYSTEM_INDEX + 1;
        List<Integer> configArray = getPropConfigArray(halPropId);
        if(configArray.size() < 2) {
            Log.e(CarLog.TAG_DIAGNOSTIC, String.format(
                    "property 0x%x does not specify the number of vendor-specific properties." +
                            "assuming 0.", halPropId));
        }
        else {
            count += configArray.get(0);
        }
        return count;
    }

    private int getNumFloatSensors(int halPropId) {
        int count = DiagnosticFloatSensorIndex.LAST_SYSTEM_INDEX + 1;
        List<Integer> configArray = getPropConfigArray(halPropId);
        if(configArray.size() < 2) {
            Log.e(CarLog.TAG_DIAGNOSTIC, String.format(
                "property 0x%x does not specify the number of vendor-specific properties." +
                    "assuming 0.", halPropId));
        }
        else {
            count += configArray.get(1);
        }
        return count;
    }

    private CarDiagnosticEvent createCarDiagnosticEvent(VehiclePropValue value) {
        if (null == value)
            return null;

        final boolean isFreezeFrame = value.prop == VehicleProperty.OBD2_FREEZE_FRAME;

        CarDiagnosticEvent.Builder builder =
                (isFreezeFrame
                                ? CarDiagnosticEvent.Builder.newFreezeFrameBuilder()
                                : CarDiagnosticEvent.Builder.newLiveFrameBuilder())
                        .atTimestamp(value.timestamp);

        BitSet bitset = BitSet.valueOf(CarServiceUtils.toByteArray(value.value.bytes));

        int numIntegerProperties = getNumIntegerSensors(value.prop);
        int numFloatProperties = getNumFloatSensors(value.prop);

        for (int i = 0; i < numIntegerProperties; ++i) {
            if (bitset.get(i)) {
                builder.withIntValue(i, value.value.int32Values.get(i));
            }
        }

        for (int i = 0; i < numFloatProperties; ++i) {
            if (bitset.get(numIntegerProperties + i)) {
                builder.withFloatValue(i, value.value.floatValues.get(i));
            }
        }

        builder.withDtc(value.value.stringValue);

        return builder.build();
    }

    /** Listener for monitoring diagnostic event. */
    public interface DiagnosticListener {
        /**
         * Diagnostic events are available.
         *
         * @param events
         */
        void onDiagnosticEvents(List<CarDiagnosticEvent> events);
    }

    // Should be used only inside handleHalEvents method.
    private final LinkedList<CarDiagnosticEvent> mEventsToDispatch = new LinkedList<>();

    @Override
    public void onHalEvents(List<VehiclePropValue> values) {
        for (VehiclePropValue value : values) {
            CarDiagnosticEvent event = createCarDiagnosticEvent(value);
            if (event != null) {
                mEventsToDispatch.add(event);
            }
        }

        DiagnosticListener listener = null;
        synchronized (mLock) {
            listener = mDiagnosticListener;
        }
        if (listener != null) {
            listener.onDiagnosticEvents(mEventsToDispatch);
        }
        mEventsToDispatch.clear();
    }

    /**
     * Set DiagnosticListener.
     * @param listener
     */
    public void setDiagnosticListener(DiagnosticListener listener) {
        synchronized (mLock) {
            mDiagnosticListener = listener;
        }
    }

    public DiagnosticListener getDiagnosticListener() {
        return mDiagnosticListener;
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*Diagnostic HAL*");
    }

    protected float fixSamplingRateForProperty(VehiclePropConfig prop, int carSensorManagerRate) {
        switch (prop.changeMode) {
            case VehiclePropertyChangeMode.ON_CHANGE:
                return 0;
        }
        float rate = 1.0f;
        switch (carSensorManagerRate) {
            case CarSensorManager.SENSOR_RATE_FASTEST:
            case CarSensorManager.SENSOR_RATE_FAST:
                rate = 10f;
                break;
            case CarSensorManager.SENSOR_RATE_UI:
                rate = 5f;
                break;
            default: // fall back to default.
                break;
        }
        if (rate > prop.maxSampleRate) {
            rate = prop.maxSampleRate;
        }
        if (rate < prop.minSampleRate) {
            rate = prop.minSampleRate;
        }
        return rate;
    }

    public DiagnosticCapabilities getDiagnosticCapabilities() {
        return mDiagnosticCapabilities;
    }

    @Nullable
    public CarDiagnosticEvent getCurrentLiveFrame() {
        try {
            VehiclePropValue value = mVehicleHal.get(VehicleProperty.OBD2_LIVE_FRAME);
            return createCarDiagnosticEvent(value);
        } catch (ServiceSpecificException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC, "Failed to read OBD2_LIVE_FRAME.", e);
            return null;
        } catch (IllegalArgumentException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC, "illegal argument trying to read OBD2_LIVE_FRAME", e);
            return null;
        }
    }

    @Nullable
    public long[] getFreezeFrameTimestamps() {
        try {
            VehiclePropValue value = mVehicleHal.get(VehicleProperty.OBD2_FREEZE_FRAME_INFO);
            long[] timestamps = new long[value.value.int64Values.size()];
            for (int i = 0; i < timestamps.length; ++i) {
                timestamps[i] = value.value.int64Values.get(i);
            }
            return timestamps;
        } catch (ServiceSpecificException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC, "Failed to read OBD2_FREEZE_FRAME_INFO.", e);
            return null;
        } catch (IllegalArgumentException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC,
                    "illegal argument trying to read OBD2_FREEZE_FRAME_INFO", e);
            return null;
        }
    }

    @Nullable
    public CarDiagnosticEvent getFreezeFrame(long timestamp) {
        VehiclePropValueBuilder builder = VehiclePropValueBuilder.newBuilder(
            VehicleProperty.OBD2_FREEZE_FRAME);
        builder.setInt64Value(timestamp);
        try {
            VehiclePropValue value = mVehicleHal.get(builder.build());
            return createCarDiagnosticEvent(value);
        } catch (ServiceSpecificException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC, "Failed to read OBD2_FREEZE_FRAME.", e);
            return null;
        } catch (IllegalArgumentException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC,
                    "illegal argument trying to read OBD2_FREEZE_FRAME", e);
            return null;
        }
    }

    public void clearFreezeFrames(long... timestamps) {
        VehiclePropValueBuilder builder = VehiclePropValueBuilder.newBuilder(
            VehicleProperty.OBD2_FREEZE_FRAME_CLEAR);
        builder.setInt64Value(timestamps);
        try {
            mVehicleHal.set(builder.build());
        } catch (ServiceSpecificException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC, "Failed to write OBD2_FREEZE_FRAME_CLEAR.", e);
        } catch (IllegalArgumentException e) {
            Log.e(CarLog.TAG_DIAGNOSTIC,
                "illegal argument trying to write OBD2_FREEZE_FRAME_CLEAR", e);
        }
    }
}
