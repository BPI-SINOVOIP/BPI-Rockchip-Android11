/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static com.android.car.hal.CarPropertyUtils.toCarPropertyValue;
import static com.android.car.hal.CarPropertyUtils.toMixedCarPropertyValue;
import static com.android.car.hal.CarPropertyUtils.toMixedVehiclePropValue;
import static com.android.car.hal.CarPropertyUtils.toVehiclePropValue;

import static java.lang.Integer.toHexString;

import android.annotation.Nullable;
import android.car.VehiclePropertyIds;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyEvent;
import android.car.hardware.property.CarPropertyManager;
import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyType;
import android.os.Build;
import android.util.Log;
import android.util.SparseArray;

import com.android.car.CarServiceUtils;
import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Common interface for HAL services that send Vehicle Properties back and forth via ICarProperty.
 * Services that communicate by passing vehicle properties back and forth via ICarProperty should
 * extend this class.
 */
public class PropertyHalService extends HalServiceBase {
    private final boolean mDbg = true;
    private final LinkedList<CarPropertyEvent> mEventsToDispatch = new LinkedList<>();
    @GuardedBy("mLock")
    private final Map<Integer, CarPropertyConfig<?>> mMgrPropIdToCarPropConfig = new HashMap<>();
    @GuardedBy("mLock")
    private final SparseArray<VehiclePropConfig> mHalPropIdToVehiclePropConfig =
            new SparseArray<>();
    // Only contains propId if the property Id is different in HAL and manager
    private static final Map<Integer, Integer> PROPERTY_ID_HAL_TO_MANAGER = Map.of(
            VehicleProperty.VEHICLE_SPEED_DISPLAY_UNITS,
            VehiclePropertyIds.VEHICLE_SPEED_DISPLAY_UNITS);
    // Only contains propId if the property Id is different in HAL and manager
    private static final Map<Integer, Integer> PROPERTY_ID_MANAGER_TO_HAL = Map.of(
            VehiclePropertyIds.VEHICLE_SPEED_DISPLAY_UNITS,
            VehicleProperty.VEHICLE_SPEED_DISPLAY_UNITS);
    private static final String TAG = "PropertyHalService";
    private final VehicleHal mVehicleHal;
    private final PropertyHalServiceIds mPropIds;

    @GuardedBy("mLock")
    private PropertyHalListener mListener;
    @GuardedBy("mLock")
    private Set<Integer> mSubscribedHalPropIds;

    private final Object mLock = new Object();

    /**
     * Converts manager property ID to Vehicle HAL property ID.
     */
    private int managerToHalPropId(int mgrPropId) {
        return PROPERTY_ID_MANAGER_TO_HAL.getOrDefault(mgrPropId, mgrPropId);
    }

    /**
     * Converts Vehicle HAL property ID to manager property ID.
     */
    private int halToManagerPropId(int halPropId) {
        return PROPERTY_ID_HAL_TO_MANAGER.getOrDefault(halPropId, halPropId);
    }

    // Checks if the property exists in this VHAL before calling methods in IVehicle.
    private boolean isPropertySupportedInVehicle(int halPropId) {
        return mHalPropIdToVehiclePropConfig.contains(halPropId);
    }

    /**
     * PropertyHalListener used to send events to CarPropertyService
     */
    public interface PropertyHalListener {
        /**
         * This event is sent whenever the property value is updated
         * @param events
         */
        void onPropertyChange(List<CarPropertyEvent> events);
        /**
         * This event is sent when the set property call fails
         * @param property
         * @param area
         */
        void onPropertySetError(int property, int area,
                @CarPropertyManager.CarSetPropertyErrorCode int errorCode);

    }

    public PropertyHalService(VehicleHal vehicleHal) {
        mPropIds = new PropertyHalServiceIds();
        mSubscribedHalPropIds = new HashSet<Integer>();
        mVehicleHal = vehicleHal;
        if (mDbg) {
            Log.d(TAG, "started PropertyHalService");
        }
    }

    /**
     * Set the listener for the HAL service
     * @param listener
     */
    public void setListener(PropertyHalListener listener) {
        synchronized (mLock) {
            mListener = listener;
        }
    }

    /**
     *
     * @return List<CarPropertyConfig> List of configs available.
     */
    public Map<Integer, CarPropertyConfig<?>> getPropertyList() {
        if (mDbg) {
            Log.d(TAG, "getPropertyList");
        }
        synchronized (mLock) {
            if (mMgrPropIdToCarPropConfig.size() == 0) {
                for (int i = 0; i < mHalPropIdToVehiclePropConfig.size(); i++) {
                    VehiclePropConfig p = mHalPropIdToVehiclePropConfig.valueAt(i);
                    int mgrPropId = halToManagerPropId(p.prop);
                    CarPropertyConfig config = CarPropertyUtils.toCarPropertyConfig(p, mgrPropId);
                    mMgrPropIdToCarPropConfig.put(mgrPropId, config);
                }
            }
        }
        return mMgrPropIdToCarPropConfig;
    }

    /**
     * Returns property or null if property is not ready yet.
     * @param mgrPropId
     * @param areaId
     */
    @Nullable
    public CarPropertyValue getProperty(int mgrPropId, int areaId) {
        int halPropId = managerToHalPropId(mgrPropId);
        if (!isPropertySupportedInVehicle(halPropId)) {
            throw new IllegalArgumentException("Invalid property Id : 0x" + toHexString(mgrPropId));
        }

        // CarPropertyManager catches and rethrows exception, no need to handle here.
        VehiclePropValue value = mVehicleHal.get(halPropId, areaId);
        if (isMixedTypeProperty(halPropId)) {
            VehiclePropConfig propConfig;
            synchronized (mLock) {
                propConfig = mHalPropIdToVehiclePropConfig.get(halPropId);
            }
            boolean containBooleanType = propConfig.configArray.get(1) == 1;
            return value == null ? null : toMixedCarPropertyValue(value,
                    mgrPropId, containBooleanType);
        }
        return value == null ? null : toCarPropertyValue(value, mgrPropId);
    }

    /**
     * Returns sample rate for the property
     * @param mgrPropId
     */
    public float getSampleRate(int mgrPropId) {
        int halPropId = managerToHalPropId(mgrPropId);
        if (!isPropertySupportedInVehicle(halPropId)) {
            throw new IllegalArgumentException("Invalid property Id : 0x" + toHexString(mgrPropId));
        }
        return mVehicleHal.getSampleRate(halPropId);
    }

    /**
     * Get the read permission string for the property.
     * @param mgrPropId
     */
    @Nullable
    public String getReadPermission(int mgrPropId) {
        int halPropId = managerToHalPropId(mgrPropId);
        return mPropIds.getReadPermission(halPropId);
    }

    /**
     * Get the write permission string for the property.
     * @param mgrPropId
     */
    @Nullable
    public String getWritePermission(int mgrPropId) {
        int halPropId = managerToHalPropId(mgrPropId);
        return mPropIds.getWritePermission(halPropId);
    }

    /**
     * Return true if property is a display_units property
     * @param mgrPropId
     */
    public boolean isDisplayUnitsProperty(int mgrPropId) {
        int halPropId = managerToHalPropId(mgrPropId);
        return mPropIds.isPropertyToChangeUnits(halPropId);
    }

    /**
     * Set the property value.
     * @param prop
     */
    public void setProperty(CarPropertyValue prop) {
        int halPropId = managerToHalPropId(prop.getPropertyId());
        if (!isPropertySupportedInVehicle(halPropId)) {
            throw new IllegalArgumentException("Invalid property Id : 0x"
                    + toHexString(prop.getPropertyId()));
        }

        VehiclePropValue halProp;
        if (isMixedTypeProperty(halPropId)) {
            // parse mixed type property value.
            VehiclePropConfig propConfig;
            synchronized (mLock) {
                propConfig = mHalPropIdToVehiclePropConfig.get(prop.getPropertyId());
            }
            int[] configArray = propConfig.configArray.stream().mapToInt(i->i).toArray();
            halProp = toMixedVehiclePropValue(prop, halPropId, configArray);
        } else {
            halProp = toVehiclePropValue(prop, halPropId);
        }
        // CarPropertyManager catches and rethrows exception, no need to handle here.
        mVehicleHal.set(halProp);
    }

    /**
     * Subscribe to this property at the specified update rate.
     * @param mgrPropId
     * @param rate
     */
    public void subscribeProperty(int mgrPropId, float rate) {
        if (mDbg) {
            Log.d(TAG, "subscribeProperty propId=0x" + toHexString(mgrPropId) + ", rate=" + rate);
        }
        int halPropId = managerToHalPropId(mgrPropId);
        if (!isPropertySupportedInVehicle(halPropId)) {
            throw new IllegalArgumentException("Invalid property Id : 0x"
                    + toHexString(mgrPropId));
        }
        synchronized (mLock) {
            VehiclePropConfig cfg = mHalPropIdToVehiclePropConfig.get(halPropId);
            if (rate > cfg.maxSampleRate) {
                rate = cfg.maxSampleRate;
            } else if (rate < cfg.minSampleRate) {
                rate = cfg.minSampleRate;
            }
            mSubscribedHalPropIds.add(halPropId);
        }

        mVehicleHal.subscribeProperty(this, halPropId, rate);
    }

    /**
     * Unsubscribe the property and turn off update events for it.
     * @param mgrPropId
     */
    public void unsubscribeProperty(int mgrPropId) {
        if (mDbg) {
            Log.d(TAG, "unsubscribeProperty propId=0x" + toHexString(mgrPropId));
        }
        int halPropId = managerToHalPropId(mgrPropId);
        if (!isPropertySupportedInVehicle(halPropId)) {
            throw new IllegalArgumentException("Invalid property Id : 0x"
                    + toHexString(mgrPropId));
        }
        synchronized (mLock) {
            if (mSubscribedHalPropIds.contains(halPropId)) {
                mSubscribedHalPropIds.remove(halPropId);
                mVehicleHal.unsubscribeProperty(this, halPropId);
            }
        }
    }

    @Override
    public void init() {
        if (mDbg) {
            Log.d(TAG, "init()");
        }
    }

    @Override
    public void release() {
        if (mDbg) {
            Log.d(TAG, "release()");
        }
        synchronized (mLock) {
            for (Integer halProp : mSubscribedHalPropIds) {
                mVehicleHal.unsubscribeProperty(this, halProp);
            }
            mSubscribedHalPropIds.clear();
            mHalPropIdToVehiclePropConfig.clear();
            mMgrPropIdToCarPropConfig.clear();
            mListener = null;
        }
    }

    @Override
    public boolean isSupportedProperty(int propId) {
        return mPropIds.isSupportedProperty(propId);
    }

    @Override
    public int[] getAllSupportedProperties() {
        return CarServiceUtils.EMPTY_INT_ARRAY;
    }

    @Override
    public void takeProperties(Collection<VehiclePropConfig> allProperties) {
        for (VehiclePropConfig p : allProperties) {
            if (mPropIds.isSupportedProperty(p.prop)) {
                synchronized (mLock) {
                    mHalPropIdToVehiclePropConfig.put(p.prop, p);
                }
                if (mDbg) {
                    Log.d(TAG, "takeSupportedProperties: " + toHexString(p.prop));
                }
            }
        }
        if (mDbg) {
            Log.d(TAG, "takeSupportedProperties() took " + allProperties.size()
                    + " properties");
        }
        // If vehicle hal support to select permission for vendor properties.
        VehiclePropConfig customizePermission;
        synchronized (mLock) {
            customizePermission = mHalPropIdToVehiclePropConfig.get(
                    VehicleProperty.SUPPORT_CUSTOMIZE_VENDOR_PERMISSION);
        }
        if (customizePermission != null) {
            mPropIds.customizeVendorPermission(customizePermission.configArray);
        }
    }

    @Override
    public void onHalEvents(List<VehiclePropValue> values) {
        PropertyHalListener listener;
        synchronized (mLock) {
            listener = mListener;
        }
        if (listener != null) {
            for (VehiclePropValue v : values) {
                if (v == null) {
                    continue;
                }
                if (!isPropertySupportedInVehicle(v.prop)) {
                    Log.e(TAG, "Property is not supported: 0x" + toHexString(v.prop));
                    continue;
                }
                // Check payload if it is a userdebug build.
                if (Build.IS_DEBUGGABLE && !mPropIds.checkPayload(v)) {
                    Log.e(TAG, "Drop event for property: " + v + " because it is failed "
                            + "in payload checking.");
                    continue;
                }
                int mgrPropId = halToManagerPropId(v.prop);
                CarPropertyValue<?> propVal;
                if (isMixedTypeProperty(v.prop)) {
                    // parse mixed type property value.
                    VehiclePropConfig propConfig;
                    synchronized (mLock) {
                        propConfig = mHalPropIdToVehiclePropConfig.get(v.prop);
                    }
                    boolean containBooleanType = propConfig.configArray.get(1) == 1;
                    propVal = toMixedCarPropertyValue(v, mgrPropId, containBooleanType);
                } else {
                    propVal = toCarPropertyValue(v, mgrPropId);
                }
                CarPropertyEvent event = new CarPropertyEvent(
                        CarPropertyEvent.PROPERTY_EVENT_PROPERTY_CHANGE, propVal);
                mEventsToDispatch.add(event);
            }
            listener.onPropertyChange(mEventsToDispatch);
            mEventsToDispatch.clear();
        }
    }

    @Override
    public void onPropertySetError(int halPropId, int area,
            @CarPropertyManager.CarSetPropertyErrorCode int errorCode) {
        PropertyHalListener listener;
        synchronized (mLock) {
            listener = mListener;
        }
        if (listener != null) {
            int mgrPropId = halToManagerPropId(halPropId);
            listener.onPropertySetError(mgrPropId, area, errorCode);
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println(TAG);
        writer.println("  Properties available:");
        synchronized (mLock) {
            for (int i = 0; i < mHalPropIdToVehiclePropConfig.size(); i++) {
                VehiclePropConfig p = mHalPropIdToVehiclePropConfig.valueAt(i);
                writer.println("    " + p);
            }
        }
    }

    private static boolean isMixedTypeProperty(int halPropId) {
        return (halPropId & VehiclePropertyType.MASK) == VehiclePropertyType.MIXED;
    }
}
