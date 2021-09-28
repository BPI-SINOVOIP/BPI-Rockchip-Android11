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

import android.graphics.Color;
import android.util.SparseArray;
import android.view.inspector.PropertyReader;

import java.util.Objects;
import java.util.Set;


class TestPropertyReader implements PropertyReader {
    private final TestPropertyMapper mPropertyMapper;
    private final SparseArray<Object> mProperties = new SparseArray<>();

    TestPropertyReader(TestPropertyMapper propertyMapper) {
        mPropertyMapper = Objects.requireNonNull(propertyMapper);
    }

    Object get(String name) {
        return mProperties.get(mPropertyMapper.getId(name));
    }

    String getIntEnum(String name) {
        return mPropertyMapper.getIntEnumMapping(name).apply((Integer) get(name));
    }

    Set<String> getIntFlag(String name) {
        return mPropertyMapper.getIntFlagMapping(name).apply((Integer) get(name));
    }

    @Override
    public void readBoolean(int id, boolean value) {
        mProperties.put(id, value);
    }

    @Override
    public void readByte(int id, byte value) {
        mProperties.put(id, value);
    }

    @Override
    public void readChar(int id, char value) {
        mProperties.put(id, value);
    }

    @Override
    public void readDouble(int id, double value) {
        mProperties.put(id, value);
    }

    @Override
    public void readFloat(int id, float value) {
        mProperties.put(id, value);
    }

    @Override
    public void readInt(int id, int value) {
        mProperties.put(id, value);
    }

    @Override
    public void readLong(int id, long value) {
        mProperties.put(id, value);
    }

    @Override
    public void readShort(int id, short value) {
        mProperties.put(id, value);
    }

    @Override
    public void readObject(int id, Object value) {
        mProperties.put(id, value);
    }

    @Override
    public void readColor(int id, int value) {
        mProperties.put(id, value);
    }

    @Override
    public void readColor(int id, long value) {
        mProperties.put(id, value);
    }

    @Override
    public void readColor(int id, Color value) {
        mProperties.put(id, value);
    }

    @Override
    public void readGravity(int id, int value) {
        mProperties.put(id, value);
    }

    @Override
    public void readIntEnum(int id, int value) {
        mProperties.put(id, value);
    }

    @Override
    public void readIntFlag(int id, int value) {
        mProperties.put(id, value);
    }

    @Override
    public void readResourceId(int id, int value) {
        mProperties.put(id, value);
    }
}
