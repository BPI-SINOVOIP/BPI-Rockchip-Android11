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
 * limitations under the License.
 */

package android.processor.view.inspector.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.R;
import android.content.res.Resources;
import android.graphics.Color;
import android.view.inspector.InspectableProperty;
import android.view.inspector.InspectableProperty.EnumEntry;
import android.view.inspector.InspectableProperty.ValueType;
import android.view.inspector.InspectionCompanion;

import androidx.annotation.AnimRes;
import androidx.annotation.AnimatorRes;
import androidx.annotation.AnyRes;
import androidx.annotation.ArrayRes;
import androidx.annotation.BoolRes;
import androidx.annotation.ColorInt;
import androidx.annotation.ColorLong;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.FontRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntegerRes;
import androidx.annotation.InterpolatorRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.MenuRes;
import androidx.annotation.NavigationRes;
import androidx.annotation.PluralsRes;
import androidx.annotation.RawRes;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.annotation.StyleableRes;
import androidx.annotation.TransitionRes;
import androidx.annotation.XmlRes;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Objects;
import java.util.Random;
import java.util.Set;

/**
 * Behavioral tests for {@link android.processor.view.inspector.PlatformInspectableProcessor}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class PlatformInspectableProcessorTest {
    private Random mRandom;
    private TestPropertyMapper mPropertyMapper;
    private TestPropertyReader mPropertyReader;

    @Before
    public void setup() {
        mRandom = new Random();
        mPropertyMapper = new TestPropertyMapper();
        mPropertyReader = new TestPropertyReader(mPropertyMapper);
    }

    class IntPropertyTest {
        private final int mValue;

        IntPropertyTest(Random seed) {
            mValue = seed.nextInt();
        }

        @InspectableProperty
        public int getValue() {
            return mValue;
        }
    }

    @Test
    public void testMapAndReadInt() {
        IntPropertyTest node = new IntPropertyTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getValue(), mPropertyReader.get("value"));
    }

    @Test
    public void testInferredAttributeId() {
        loadCompanion(IntPropertyTest.class).mapProperties(mPropertyMapper);
        assertEquals(R.attr.value, mPropertyMapper.getAttributeId("value"));
    }

    @Test(expected = InspectionCompanion.UninitializedPropertyMapException.class)
    public void testUninitializedPropertyMap() {
        IntPropertyTest node = new IntPropertyTest(mRandom);
        loadCompanion(IntPropertyTest.class).readProperties(node, mPropertyReader);
    }

    class NamedPropertyTest {
        private final int mValue;

        NamedPropertyTest(Random seed) {
            mValue = seed.nextInt();
        }

        @InspectableProperty(name = "myNamedValue", hasAttributeId = false)
        public int getValue() {
            return mValue;
        }
    }

    @Test
    public void testNamedProperty() {
        NamedPropertyTest node = new NamedPropertyTest(mRandom);
        mapAndRead(node);
        assertEquals(0, mPropertyMapper.getId("value"));
        assertEquals(node.getValue(), mPropertyReader.get("myNamedValue"));
    }

    class HasAttributeIdFalseTest {
        @InspectableProperty(hasAttributeId = false)
        public int getValue() {
            return 0;
        }
    }

    @Test
    public void testHasAttributeIdFalse() {
        loadCompanion(HasAttributeIdFalseTest.class).mapProperties(mPropertyMapper);
        assertEquals(Resources.ID_NULL, mPropertyMapper.getAttributeId("value"));
    }

    class AttributeIdEqualsTest {
        @InspectableProperty(attributeId = 0xdecafbad)
        public int getValue() {
            return 0;
        }
    }

    @Test
    public void testAttributeIdEquals() {
        loadCompanion(AttributeIdEqualsTest.class).mapProperties(mPropertyMapper);
        assertEquals(0xdecafbad, mPropertyMapper.getAttributeId("value"));
    }

    class InferredPropertyNameTest {
        private final int mValueA;
        private final int mValueB;
        private final int mValueC;

        InferredPropertyNameTest(Random seed) {
            mValueA = seed.nextInt();
            mValueB = seed.nextInt();
            mValueC = seed.nextInt();
        }

        @InspectableProperty(hasAttributeId = false)
        public int getValueA() {
            return mValueA;
        }

        @InspectableProperty(hasAttributeId = false)
        public int isValueB() {
            return mValueB;
        }

        @InspectableProperty(hasAttributeId = false)
        public int obtainValueC() {
            return mValueC;
        }
    }

    @Test
    public void testInferredPropertyName() {
        InferredPropertyNameTest node = new InferredPropertyNameTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getValueA(), mPropertyReader.get("valueA"));
        assertEquals(node.isValueB(), mPropertyReader.get("isValueB"));
        assertEquals(node.obtainValueC(), mPropertyReader.get("obtainValueC"));
    }

    class InferredBooleanNameTest {
        private final boolean mValueA;
        private final boolean mValueB;
        private final boolean mValueC;

        InferredBooleanNameTest(Random seed) {
            mValueA = seed.nextBoolean();
            mValueB = seed.nextBoolean();
            mValueC = seed.nextBoolean();
        }

        @InspectableProperty(hasAttributeId = false)
        public boolean getValueA() {
            return mValueA;
        }

        @InspectableProperty(hasAttributeId = false)
        public boolean isValueB() {
            return mValueB;
        }

        @InspectableProperty(hasAttributeId = false)
        public boolean obtainValueC() {
            return mValueC;
        }
    }

    @Test
    public void testInferredBooleanName() {
        InferredBooleanNameTest node = new InferredBooleanNameTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getValueA(), mPropertyReader.get("valueA"));
        assertEquals(node.isValueB(), mPropertyReader.get("valueB"));
        assertEquals(node.obtainValueC(), mPropertyReader.get("obtainValueC"));
    }

    class ColorTest {
        private final int mColorInt;
        private final long mColorLong;

        private final Color mColorObject;

        ColorTest(Random seed) {
            mColorInt = seed.nextInt();
            mColorLong = Color.pack(seed.nextInt());
            mColorObject = Color.valueOf(seed.nextInt());
        }

        @InspectableProperty(hasAttributeId = false)
        @ColorInt
        public int getColorInt() {
            return mColorInt;
        }

        @InspectableProperty(hasAttributeId = false)
        @ColorLong
        public long getColorLong() {
            return mColorLong;
        }

        @InspectableProperty(hasAttributeId = false)
        public Color getColorObject() {
            return mColorObject;
        }
    }

    @Test
    public void testColorTypeInference() {
        ColorTest node = new ColorTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getColorInt(), mPropertyReader.get("colorInt"));
        assertEquals(node.getColorLong(), mPropertyReader.get("colorLong"));
        assertEquals(node.getColorObject(), mPropertyReader.get("colorObject"));
        assertEquals(ValueType.COLOR, mPropertyMapper.getValueType("colorInt"));
        assertEquals(ValueType.COLOR, mPropertyMapper.getValueType("colorLong"));
        assertEquals(ValueType.COLOR, mPropertyMapper.getValueType("colorObject"));
    }

    class ValueTypeTest {
        private final int mColor;
        private final int mGravity;
        private final int mValue;

        ValueTypeTest(Random seed) {
            mColor = seed.nextInt();
            mGravity = seed.nextInt();
            mValue = seed.nextInt();
        }

        @InspectableProperty(valueType = ValueType.COLOR)
        public int getColor() {
            return mColor;
        }

        @InspectableProperty(valueType = ValueType.GRAVITY)
        public int getGravity() {
            return mGravity;
        }

        @InspectableProperty(valueType = ValueType.NONE)
        @ColorInt
        public int getValue() {
            return mValue;
        }
    }

    @Test
    public void testValueTypeEquals() {
        ValueTypeTest node = new ValueTypeTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getColor(), mPropertyReader.get("color"));
        assertEquals(node.getGravity(), mPropertyReader.get("gravity"));
        assertEquals(node.getValue(), mPropertyReader.get("value"));
        assertEquals(ValueType.COLOR, mPropertyMapper.getValueType("color"));
        assertEquals(ValueType.GRAVITY, mPropertyMapper.getValueType("gravity"));
        assertEquals(ValueType.NONE, mPropertyMapper.getValueType("value"));
    }

    class PrimitivePropertiesTest {
        private final boolean mBoolean;
        private final byte mByte;
        private final char mChar;
        private final double mDouble;
        private final float mFloat;
        private final int mInt;
        private final long mLong;
        private final short mShort;

        PrimitivePropertiesTest(Random seed) {
            mBoolean = seed.nextBoolean();
            mByte = (byte) seed.nextInt();
            mChar = randomLetter(seed);
            mDouble = seed.nextDouble();
            mFloat = seed.nextFloat();
            mInt = seed.nextInt();
            mLong = seed.nextLong();
            mShort = (short) seed.nextInt();
        }

        @InspectableProperty(hasAttributeId = false)
        public boolean getBoolean() {
            return mBoolean;
        }

        @InspectableProperty(hasAttributeId = false)
        public byte getByte() {
            return mByte;
        }

        @InspectableProperty(hasAttributeId = false)
        public char getChar() {
            return mChar;
        }

        @InspectableProperty(hasAttributeId = false)
        public double getDouble() {
            return mDouble;
        }

        @InspectableProperty(hasAttributeId = false)
        public float getFloat() {
            return mFloat;
        }

        @InspectableProperty(hasAttributeId = false)
        public int getInt() {
            return mInt;
        }

        @InspectableProperty(hasAttributeId = false)
        public long getLong() {
            return mLong;
        }

        @InspectableProperty(hasAttributeId = false)
        public short getShort() {
            return mShort;
        }
    }

    @Test
    public void testPrimitiveProperties() {
        PrimitivePropertiesTest node = new PrimitivePropertiesTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getBoolean(), mPropertyReader.get("boolean"));
        assertEquals(node.getByte(), mPropertyReader.get("byte"));
        assertEquals(node.getChar(), mPropertyReader.get("char"));
        assertEquals(node.getDouble(), mPropertyReader.get("double"));
        assertEquals(node.getFloat(), mPropertyReader.get("float"));
        assertEquals(node.getInt(), mPropertyReader.get("int"));
        assertEquals(node.getLong(), mPropertyReader.get("long"));
        assertEquals(node.getShort(), mPropertyReader.get("short"));
    }

    class ObjectPropertiesTest {
        private final String mText;

        ObjectPropertiesTest(Random seed) {
            final StringBuilder stringBuilder = new StringBuilder();
            final int length = seed.nextInt(8) + 8;

            for (int i = 0; i < length; i++) {
                stringBuilder.append(randomLetter(seed));
            }

            mText = stringBuilder.toString();
        }

        @InspectableProperty
        public String getText() {
            return mText;
        }

        @InspectableProperty(hasAttributeId = false)
        public Objects getNull() {
            return null;
        }
    }

    @Test
    public void testObjectProperties() {
        ObjectPropertiesTest node = new ObjectPropertiesTest(mRandom);
        mapAndRead(node);
        assertEquals(node.getText(), mPropertyReader.get("text"));
        assertNull(mPropertyReader.get("null"));
        assertNotEquals(0, mPropertyMapper.getId("null"));
    }

    class IntEnumTest {
        private int mValue;

        @InspectableProperty(enumMapping = {
                @InspectableProperty.EnumEntry(name = "ONE", value = 1),
                @EnumEntry(name = "TWO", value = 2)})
        public int getValue() {
            return mValue;
        }

        public void setValue(int value) {
            mValue = value;
        }
    }

    @Test
    public void testIntEnum() {
        IntEnumTest node = new IntEnumTest();
        InspectionCompanion<IntEnumTest> companion = loadCompanion(IntEnumTest.class);
        companion.mapProperties(mPropertyMapper);

        node.setValue(1);
        companion.readProperties(node, mPropertyReader);
        assertEquals("ONE", mPropertyReader.getIntEnum("value"));

        node.setValue(2);
        companion.readProperties(node, mPropertyReader);
        assertEquals("TWO", mPropertyReader.getIntEnum("value"));

        node.setValue(3);
        companion.readProperties(node, mPropertyReader);
        assertNull(mPropertyReader.getIntEnum("value"));
    }

    class IntFlagTest {
        private int mValue;

        @InspectableProperty(flagMapping = {
                @InspectableProperty.FlagEntry(name = "ONE", target = 0x1, mask = 0x3),
                @InspectableProperty.FlagEntry(name = "TWO", target = 0x2, mask = 0x3),
                @InspectableProperty.FlagEntry(name = "THREE", target = 0x3, mask = 0x3),
                @InspectableProperty.FlagEntry(name = "FOUR", target = 0x4)})
        public int getValue() {
            return mValue;
        }

        public void setValue(int value) {
            mValue = value;
        }
    }

    @Test
    public void testIntFlag() {
        IntFlagTest node = new IntFlagTest();
        InspectionCompanion<IntFlagTest> companion = loadCompanion(IntFlagTest.class);
        companion.mapProperties(mPropertyMapper);

        node.setValue(0);
        companion.readProperties(node, mPropertyReader);
        assertTrue(mPropertyReader.getIntFlag("value").isEmpty());

        node.setValue(1);
        companion.readProperties(node, mPropertyReader);
        assertEquals(setOf("ONE"), mPropertyReader.getIntFlag("value"));

        node.setValue(2);
        companion.readProperties(node, mPropertyReader);
        assertEquals(setOf("TWO"), mPropertyReader.getIntFlag("value"));

        node.setValue(3);
        companion.readProperties(node, mPropertyReader);
        assertEquals(setOf("THREE"), mPropertyReader.getIntFlag("value"));

        node.setValue(4);
        companion.readProperties(node, mPropertyReader);
        assertEquals(setOf("FOUR"), mPropertyReader.getIntFlag("value"));

        node.setValue(5);
        companion.readProperties(node, mPropertyReader);
        assertEquals(setOf("FOUR", "ONE"), mPropertyReader.getIntFlag("value"));
    }

    class PublicFieldTest {
        @InspectableProperty
        public final int value;

        PublicFieldTest(Random seed) {
            value = seed.nextInt();
        }
    }

    @Test
    public void testPublicField() {
        PublicFieldTest node = new PublicFieldTest(mRandom);
        mapAndRead(node);
        assertEquals(node.value, mPropertyReader.get("value"));
    }

    class ResourceIdTest {
        @AnimatorRes private final int mAnimatorId;
        @AnimRes private final int mAnimId;
        @AnyRes private final int mAnyId;
        @ArrayRes private final int mArrayId;
        @BoolRes private final int mBoolId;
        @DimenRes private final int mDimenId;
        @DrawableRes private final int mDrawableId;
        @FontRes private final int mFontId;
        @IdRes private final int mIdId;
        @IntegerRes private final int mIntegerId;
        @InterpolatorRes private final int mInterpolatorId;
        @LayoutRes private final int mLayoutId;
        @MenuRes private final int mMenuId;
        @NavigationRes private final int mNavigationId;
        @PluralsRes private final int mPluralsId;
        @RawRes private final int mRawId;
        @StringRes private final int mStringId;
        @StyleableRes private final int mStyleableId;
        @StyleRes private final int mStyleId;
        @TransitionRes private final int mTransitionId;
        @XmlRes private final int mXmlId;
        private final int mUnannotatedId;

        ResourceIdTest(Random seed) {
            mAnimatorId = seed.nextInt();
            mAnimId = seed.nextInt();
            mAnyId = seed.nextInt();
            mArrayId = seed.nextInt();
            mBoolId = seed.nextInt();
            mDimenId = seed.nextInt();
            mDrawableId = seed.nextInt();
            mFontId = seed.nextInt();
            mIdId = seed.nextInt();
            mIntegerId = seed.nextInt();
            mInterpolatorId = seed.nextInt();
            mLayoutId = seed.nextInt();
            mMenuId = seed.nextInt();
            mNavigationId = seed.nextInt();
            mPluralsId = seed.nextInt();
            mRawId = seed.nextInt();
            mStringId = seed.nextInt();
            mStyleableId = seed.nextInt();
            mStyleId = seed.nextInt();
            mTransitionId = seed.nextInt();
            mXmlId = seed.nextInt();
            mUnannotatedId = seed.nextInt();
        }

        @InspectableProperty(hasAttributeId = false)
        @AnimatorRes
        public int getAnimatorId() {
            return mAnimatorId;
        }

        @InspectableProperty(hasAttributeId = false)
        @AnimRes
        public int getAnimId() {
            return mAnimId;
        }

        @InspectableProperty(hasAttributeId = false)
        @AnyRes
        public int getAnyId() {
            return mAnimId;
        }

        @InspectableProperty(hasAttributeId = false)
        @ArrayRes
        public int getArrayId() {
            return mArrayId;
        }

        @InspectableProperty(hasAttributeId = false)
        @BoolRes
        public int getBoolId() {
            return mBoolId;
        }

        @InspectableProperty(hasAttributeId = false)
        @DimenRes
        public int getDimenId() {
            return mDimenId;
        }

        @InspectableProperty(hasAttributeId = false)
        @DrawableRes
        public int getDrawableId() {
            return mDrawableId;
        }

        @InspectableProperty(hasAttributeId = false)
        @FontRes
        public int getFontId() {
            return mFontId;
        }

        @InspectableProperty(hasAttributeId = false)
        @IdRes
        public int getIdId() {
            return mIdId;
        }

        @InspectableProperty(hasAttributeId = false)
        @IntegerRes
        public int getIntegerId() {
            return mIntegerId;
        }

        @InspectableProperty(hasAttributeId = false)
        @InterpolatorRes
        public int getInterpolatorId() {
            return mInterpolatorId;
        }

        @InspectableProperty(hasAttributeId = false)
        @LayoutRes
        public int getLayoutId() {
            return mLayoutId;
        }

        @InspectableProperty(hasAttributeId = false)
        @MenuRes
        public int getMenuId() {
            return mMenuId;
        }

        @InspectableProperty(hasAttributeId = false)
        @NavigationRes
        public int getNavigationId() {
            return mNavigationId;
        }

        @InspectableProperty(hasAttributeId = false)
        @PluralsRes
        public int getPluralsId() {
            return mPluralsId;
        }

        @InspectableProperty(hasAttributeId = false)
        @RawRes
        public int getRawId() {
            return mRawId;
        }

        @InspectableProperty(hasAttributeId = false)
        @StringRes
        public int getStringId() {
            return mStringId;
        }

        @InspectableProperty(hasAttributeId = false)
        @StyleableRes
        public int getStyleableId() {
            return mStyleableId;
        }

        @InspectableProperty(hasAttributeId = false)
        @StyleRes
        public int getStyleId() {
            return mStyleId;
        }

        @InspectableProperty(hasAttributeId = false)
        @TransitionRes
        public int getTransitionId() {
            return mTransitionId;
        }

        @InspectableProperty(hasAttributeId = false)
        @XmlRes
        public int getXmlId() {
            return mXmlId;
        }

        @InspectableProperty(hasAttributeId = false, valueType = ValueType.RESOURCE_ID)
        public int getUnannotatedId() {
            return mUnannotatedId;
        }
    }

    @Test
    public void testResourceId() {
        ResourceIdTest node = new ResourceIdTest(mRandom);
        mapAndRead(node);

        assertEquals(node.getAnimatorId(), mPropertyReader.get("animatorId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("animatorId"));

        assertEquals(node.getAnimId(), mPropertyReader.get("animId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("animId"));

        assertEquals(node.getAnyId(), mPropertyReader.get("anyId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("anyId"));

        assertEquals(node.getArrayId(), mPropertyReader.get("arrayId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("arrayId"));

        assertEquals(node.getBoolId(), mPropertyReader.get("boolId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("boolId"));

        assertEquals(node.getDimenId(), mPropertyReader.get("dimenId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("dimenId"));

        assertEquals(node.getDrawableId(), mPropertyReader.get("drawableId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("drawableId"));

        assertEquals(node.getFontId(), mPropertyReader.get("fontId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("fontId"));

        assertEquals(node.getIdId(), mPropertyReader.get("idId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("idId"));

        assertEquals(node.getIntegerId(), mPropertyReader.get("integerId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("integerId"));

        assertEquals(node.getInterpolatorId(), mPropertyReader.get("interpolatorId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("interpolatorId"));

        assertEquals(node.getLayoutId(), mPropertyReader.get("layoutId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("layoutId"));

        assertEquals(node.getMenuId(), mPropertyReader.get("menuId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("menuId"));

        assertEquals(node.getNavigationId(), mPropertyReader.get("navigationId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("navigationId"));

        assertEquals(node.getPluralsId(), mPropertyReader.get("pluralsId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("pluralsId"));

        assertEquals(node.getRawId(), mPropertyReader.get("rawId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("rawId"));

        assertEquals(node.getStringId(), mPropertyReader.get("stringId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("stringId"));

        assertEquals(node.getStyleableId(), mPropertyReader.get("styleableId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("styleableId"));

        assertEquals(node.getStyleId(), mPropertyReader.get("styleId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("styleId"));

        assertEquals(node.getTransitionId(), mPropertyReader.get("transitionId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("transitionId"));

        assertEquals(node.getXmlId(), mPropertyReader.get("xmlId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("xmlId"));

        assertEquals(node.getUnannotatedId(), mPropertyReader.get("unannotatedId"));
        assertEquals(ValueType.RESOURCE_ID, mPropertyMapper.getValueType("unannotatedId"));
    }

    private static <T> Set<T> setOf(T... items) {
        Set<T> set = new HashSet<>(items.length);

        for (T item : items) {
            set.add(item);
        }

        return set;
    }

    @SuppressWarnings("unchecked")
    private <T> void mapAndRead(T node) {
        InspectionCompanion<T> companion = loadCompanion((Class<T>) node.getClass());
        companion.mapProperties(mPropertyMapper);
        companion.readProperties(node, mPropertyReader);
    }

    @SuppressWarnings("unchecked")
    private <T> InspectionCompanion<T> loadCompanion(Class<T> cls) {
        final ClassLoader classLoader = cls.getClassLoader();
        final String companionName = String.format("%s$InspectionCompanion", cls.getName());

        try {
            final Class<InspectionCompanion<T>> companion =
                    (Class<InspectionCompanion<T>>) classLoader.loadClass(companionName);
            return companion.newInstance();
        } catch (ClassNotFoundException e) {
            fail(String.format("Unable to load companion for %s", cls.getCanonicalName()));
        } catch (InstantiationException | IllegalAccessException e) {
            fail(String.format("Unable to instantiate companion for %s", cls.getCanonicalName()));
        }

        return null;
    }

    private char randomLetter(Random random) {
        final String alphabet = "abcdefghijklmnopqrstuvwxyz";
        return alphabet.charAt(random.nextInt(alphabet.length()));
    }
}
