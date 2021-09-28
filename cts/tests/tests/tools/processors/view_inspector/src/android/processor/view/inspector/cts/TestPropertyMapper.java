/*
 * Copyright 2019 The Android Open Source Project
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
 * limitations under the License
 */

package android.processor.view.inspector.cts;

import android.view.inspector.InspectableProperty.ValueType;
import android.view.inspector.PropertyMapper;

import java.util.HashMap;
import java.util.Set;
import java.util.function.IntFunction;

class TestPropertyMapper implements PropertyMapper {
    private final HashMap<String, Integer> mPropertyIds = new HashMap<>();
    private final HashMap<String, Integer> mAttributeIds = new HashMap<>();
    private final HashMap<String, ValueType> mValueTypes = new HashMap<>();
    private final HashMap<String, IntFunction<String>> mIntEnumMappings = new HashMap<>();
    private final HashMap<String, IntFunction<Set<String>>> mIntFlagMappings = new HashMap<>();
    private int mNextId = 1;

    int getId(String name) {
        return mPropertyIds.getOrDefault(name, 0);
    }

    int getAttributeId(String name) {
        return mAttributeIds.getOrDefault(name, 0);
    }

    ValueType getValueType(String name) {
        return mValueTypes.getOrDefault(name, ValueType.NONE);
    }

    IntFunction<String> getIntEnumMapping(String name) {
        return mIntEnumMappings.get(name);
    }

    IntFunction<Set<String>> getIntFlagMapping(String name) {
        return mIntFlagMappings.get(name);
    }

    @Override
    public int mapBoolean(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapByte(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapChar(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapDouble(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapFloat(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapInt(String name, int attributeId) { return map(name, attributeId); }

    @Override
    public int mapLong(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapShort(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapObject(String name, int attributeId) {
        return map(name, attributeId);
    }

    @Override
    public int mapColor(String name, int attributeId) {
        return map(name, attributeId, ValueType.COLOR);
    }

    @Override
    public int mapGravity(String name, int attributeId) {
        return map(name, attributeId, ValueType.GRAVITY);
    }

    @Override
    public int mapIntEnum(String name, int attributeId, IntFunction<String> intEnumMapping) {
        mIntEnumMappings.put(name, intEnumMapping);
        return map(name, attributeId, ValueType.INT_ENUM);
    }

    @Override
    public int mapIntFlag(String name, int attributeId, IntFunction<Set<String>> intFlagMapping) {
        mIntFlagMappings.put(name, intFlagMapping);
        return map(name, attributeId, ValueType.INT_FLAG);
    }

    @Override
    public int mapResourceId(String name, int attributeId) {
        return map(name, attributeId, ValueType.RESOURCE_ID);
    }

    private int map(String name, int attributeId) {
        return map(name, attributeId, ValueType.NONE);
    }

    private int map(String name, int attributeId, ValueType valueType) {
        if (mPropertyIds.containsKey(name)) {
            throw new PropertyConflictException(name, "all", "all");
        }
        mAttributeIds.put(name, attributeId);
        mValueTypes.put(name, valueType);
        return mPropertyIds.computeIfAbsent(name, n -> mNextId++);
    }
}
