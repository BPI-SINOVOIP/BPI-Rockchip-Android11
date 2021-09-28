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
package android.signature.cts;

import android.signature.cts.JDiffClassDescription.JDiffField;
import android.signature.cts.ReflectionHelper.DefaultTypeComparator;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Formatter;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

/**
 * Checks that the runtime representation of a class matches the API representation of a class.
 */
public class ApiComplianceChecker extends AbstractApiChecker {

    /**
     * A set of method signatures whose abstract modifier should be ignored.
     *
     * <p>If a class is not intended to be created or extended by application developers and all
     * instances are created and supplied by Android itself then the abstract modifier has no
     * impact on runtime compatibility.
     */
    private static final Set<String> IGNORE_METHOD_ABSTRACT_MODIFIER_WHITE_LIST = new HashSet<>();
    static {
        // This method was previously abstract and is now not abstract. As the
        // CtsSystemApiSignatureTestCases package tests both the old and new specifications, with
        // and without the abstract modifier this needs to ignore the abstract modifier.
        IGNORE_METHOD_ABSTRACT_MODIFIER_WHITE_LIST.add(
                "public int android.service.euicc.EuiccService.onDownloadSubscription("
                        + "int,android.telephony.euicc.DownloadableSubscription,boolean,boolean)");
    }

    /** Indicates that the class is an annotation. */
    private static final int CLASS_MODIFIER_ANNOTATION = 0x00002000;

    /** Indicates that the class is an enum. */
    private static final int CLASS_MODIFIER_ENUM       = 0x00004000;

    /** Indicates that the method is a bridge method. */
    private static final int METHOD_MODIFIER_BRIDGE    = 0x00000040;

    /** Indicates that the method is takes a variable number of arguments. */
    private static final int METHOD_MODIFIER_VAR_ARGS  = 0x00000080;

    /** Indicates that the method is a synthetic method. */
    private static final int METHOD_MODIFIER_SYNTHETIC = 0x00001000;

    /** Indicates that a field is an enum value. */
    public static final int FIELD_MODIFIER_ENUM_VALUE = 0x00004000;

    private final InterfaceChecker interfaceChecker;

    public ApiComplianceChecker(ResultObserver resultObserver, ClassProvider classProvider) {
        super(classProvider, resultObserver);
        interfaceChecker = new InterfaceChecker(resultObserver, classProvider);
    }

    @Override
    public void checkDeferred() {
        interfaceChecker.checkQueued();
    }

    @Override
    protected boolean checkClass(JDiffClassDescription classDescription, Class<?> runtimeClass) {
        if (JDiffClassDescription.JDiffType.INTERFACE.equals(classDescription.getClassType())) {
            // Queue the interface for deferred checking.
            interfaceChecker.queueForDeferredCheck(classDescription, runtimeClass);
        }

        String reason;
        if ((reason = checkClassModifiersCompliance(classDescription, runtimeClass)) != null) {
            resultObserver.notifyFailure(FailureType.mismatch(classDescription),
                    classDescription.getAbsoluteClassName(),
                    String.format("Non-compatible class found when looking for %s - because %s",
                            classDescription.toSignatureString(), reason));
            return false;
        }

        if (!checkClassAnnotationCompliance(classDescription, runtimeClass)) {
            resultObserver.notifyFailure(FailureType.mismatch(classDescription),
                    classDescription.getAbsoluteClassName(), "Annotation mismatch");
            return false;
        }

        if (!runtimeClass.isAnnotation()) {
            // check father class
            if (!checkClassExtendsCompliance(classDescription, runtimeClass)) {
                resultObserver.notifyFailure(FailureType.mismatch(classDescription),
                        classDescription.getAbsoluteClassName(),
                        "Extends mismatch, expected " + classDescription.getExtendedClass());
                return false;
            }

            // check implements interface
            if (!checkClassImplementsCompliance(classDescription, runtimeClass)) {
                resultObserver.notifyFailure(FailureType.mismatch(classDescription),
                        classDescription.getAbsoluteClassName(),
                        "Implements mismatch, expected " + classDescription.getImplInterfaces());
                return false;
            }
        }
        return true;
    }

    /**
     * Checks if the class under test has compliant modifiers compared to the API.
     *
     * @param classDescription a description of a class in an API.
     * @param runtimeClass the runtime class corresponding to {@code classDescription}.
     * @return null if modifiers are compliant otherwise a reason why they are not.
     */
    private static String checkClassModifiersCompliance(JDiffClassDescription classDescription,
            Class<?> runtimeClass) {
        int reflectionModifiers = runtimeClass.getModifiers();
        int apiModifiers = classDescription.getModifier();

        // If the api class isn't abstract
        if (((apiModifiers & Modifier.ABSTRACT) == 0) &&
                // but the reflected class is
                ((reflectionModifiers & Modifier.ABSTRACT) != 0) &&
                // interfaces are implicitly abstract (JLS 9.1.1.1)
                classDescription.getClassType() != JDiffClassDescription.JDiffType.INTERFACE &&
                // and it isn't an enum
                !classDescription.isEnumType()) {
            // that is a problem
            return "description is abstract but class is not and is not an enum";
        }
        // ABSTRACT check passed, so mask off ABSTRACT
        reflectionModifiers &= ~Modifier.ABSTRACT;
        apiModifiers &= ~Modifier.ABSTRACT;

        if (classDescription.isAnnotation()) {
            reflectionModifiers &= ~CLASS_MODIFIER_ANNOTATION;
        }
        if (runtimeClass.isInterface()) {
            reflectionModifiers &= ~(Modifier.INTERFACE);
        }
        if (classDescription.isEnumType() && runtimeClass.isEnum()) {
            reflectionModifiers &= ~CLASS_MODIFIER_ENUM;

            // Most enums are marked as final, however enums that have one or more constants that
            // override a method from the class cannot be marked as final because those constants
            // are represented as a subclass. As enum classes cannot be extended (except for its own
            // constants) there is no benefit in checking final modifier so just ignore them.
            reflectionModifiers &= ~Modifier.FINAL;
            apiModifiers &= ~Modifier.FINAL;
        }

        if ((reflectionModifiers == apiModifiers)
                && (classDescription.isEnumType() == runtimeClass.isEnum())) {
            return null;
        } else {
            return String.format("modifier mismatch - description (%s), class (%s)",
                    getModifierString(apiModifiers), getModifierString(reflectionModifiers));
        }
    }

    /**
     * Checks if the class under test is compliant with regards to
     * annnotations when compared to the API.
     *
     * @param classDescription a description of a class in an API.
     * @param runtimeClass the runtime class corresponding to {@code classDescription}.
     * @return true if the class is compliant
     */
    private static boolean checkClassAnnotationCompliance(JDiffClassDescription classDescription,
            Class<?> runtimeClass) {
        if (runtimeClass.isAnnotation()) {
            // check annotation
            for (String inter : classDescription.getImplInterfaces()) {
                if ("java.lang.annotation.Annotation".equals(inter)) {
                    return true;
                }
            }
            return false;
        }
        return true;
    }

    /**
     * Checks if the class under test extends the proper classes
     * according to the API.
     *
     * @param classDescription a description of a class in an API.
     * @param runtimeClass the runtime class corresponding to {@code classDescription}.
     * @return true if the class is compliant.
     */
    private static boolean checkClassExtendsCompliance(JDiffClassDescription classDescription,
            Class<?> runtimeClass) {
        // Nothing to check if it doesn't extend anything.
        if (classDescription.getExtendedClass() != null) {
            Class<?> superClass = runtimeClass.getSuperclass();

            while (superClass != null) {
                if (superClass.getCanonicalName().equals(classDescription.getExtendedClass())) {
                    return true;
                }
                superClass = superClass.getSuperclass();
            }
            // Couldn't find a matching superclass.
            return false;
        }
        return true;
    }

    /**
     * Checks if the class under test implements the proper interfaces
     * according to the API.
     *
     * @param classDescription a description of a class in an API.
     * @param runtimeClass the runtime class corresponding to {@code classDescription}.
     * @return true if the class is compliant
     */
    private static boolean checkClassImplementsCompliance(JDiffClassDescription classDescription,
            Class<?> runtimeClass) {
        Set<String> interFaceSet = new HashSet<>();

        addInterfacesToSetByName(runtimeClass, interFaceSet);

        for (String inter : classDescription.getImplInterfaces()) {
            if (!interFaceSet.contains(inter)) {
                return false;
            }
        }
        return true;
    }

    private static void addInterfacesToSetByName(Class<?> runtimeClass, Set<String> interFaceSet) {
        Class<?>[] interfaces = runtimeClass.getInterfaces();
        for (Class<?> c : interfaces) {
            interFaceSet.add(c.getCanonicalName());
            // Add grandparent interfaces in case the parent interface is hidden.
            addInterfacesToSetByName(c, interFaceSet);
        }

        // Add the interfaces that the super class implements as well just in case the super class
        // is hidden.
        Class<?> superClass = runtimeClass.getSuperclass();
        if (superClass != null) {
            addInterfacesToSetByName(superClass, interFaceSet);
        }
    }

    @Override
    protected void checkField(JDiffClassDescription classDescription, Class<?> runtimeClass,
            JDiffField fieldDescription, Field field) {
        int expectedModifiers = fieldDescription.mModifier;
        int actualModifiers = field.getModifiers();
        if (actualModifiers != expectedModifiers) {
            resultObserver.notifyFailure(FailureType.MISMATCH_FIELD,
                    fieldDescription.toReadableString(classDescription.getAbsoluteClassName()),
                    String.format(
                            "Incompatible field modifiers, expected %s, found %s",
                            getModifierString(expectedModifiers),
                            getModifierString(actualModifiers)));
        }

        String expectedFieldType = fieldDescription.mFieldType;
        String actualFieldType = ReflectionHelper.typeToString(field.getGenericType());
        if (!DefaultTypeComparator.INSTANCE.compare(expectedFieldType, actualFieldType)) {
            resultObserver.notifyFailure(
                    FailureType.MISMATCH_FIELD,
                    fieldDescription.toReadableString(classDescription.getAbsoluteClassName()),
                    String.format("Incompatible field type found, expected %s, found %s",
                            expectedFieldType, actualFieldType));
        }

        String message = checkFieldValueCompliance(fieldDescription, field);
        if (message != null) {
            resultObserver.notifyFailure(FailureType.MISMATCH_FIELD,
                    fieldDescription.toReadableString(classDescription.getAbsoluteClassName()),
                    message);
        }
    }

    private static final int BRIDGE    = 0x00000040;
    private static final int VARARGS   = 0x00000080;
    private static final int SYNTHETIC = 0x00001000;
    private static final int ANNOTATION  = 0x00002000;
    private static final int ENUM      = 0x00004000;
    private static final int MANDATED  = 0x00008000;

    private static String getModifierString(int modifiers) {
        Formatter formatter = new Formatter();
        String m = Modifier.toString(modifiers);
        formatter.format("<%s", m);
        String sep = m.isEmpty() ? "" : " ";
        if ((modifiers & BRIDGE) != 0) {
            formatter.format("%senum", sep);
            sep = " ";
        }
        if ((modifiers & VARARGS) != 0) {
            formatter.format("%svarargs", sep);
            sep = " ";
        }
        if ((modifiers & SYNTHETIC) != 0) {
            formatter.format("%ssynthetic", sep);
            sep = " ";
        }
        if ((modifiers & ANNOTATION) != 0) {
            formatter.format("%sannotation", sep);
            sep = " ";
        }
        if ((modifiers & ENUM) != 0) {
            formatter.format("%senum", sep);
            sep = " ";
        }
        if ((modifiers & MANDATED) != 0) {
            formatter.format("%smandated", sep);
        }
        return formatter.format("> (0x%x)", modifiers).toString();
    }

    /**
     * Checks whether the field values are compatible.
     *
     * @param apiField The field as defined by the platform API.
     * @param deviceField The field as defined by the device under test.
     */
    private static String checkFieldValueCompliance(JDiffField apiField, Field deviceField) {
        if ((apiField.mModifier & Modifier.FINAL) == 0 ||
                (apiField.mModifier & Modifier.STATIC) == 0) {
            // Only final static fields can have fixed values.
            return null;
        }
        String apiFieldValue = apiField.getValueString();
        if (apiFieldValue == null) {
            // If we don't define a constant value for it, then it can be anything.
            return null;
        }

        // Convert char into a number to match the value returned from device field. The device
        // field does not
        if (deviceField.getType() == char.class) {
            apiFieldValue = convertCharToCanonicalValue(apiFieldValue.charAt(0));
        }

        String deviceFieldValue = getFieldValueAsString(deviceField);
        if (!Objects.equals(apiFieldValue, deviceFieldValue)) {
            return String.format("Incorrect field value, expected <%s>, found <%s>",
                    apiFieldValue, deviceFieldValue);

        }

        return null;
    }

    private static String getFieldValueAsString(Field deviceField) {
        // Some fields may be protected or package-private
        deviceField.setAccessible(true);
        try {
            Class<?> fieldType = deviceField.getType();
            if (fieldType == byte.class) {
                return Byte.toString(deviceField.getByte(null));
            } else if (fieldType == char.class) {
                return convertCharToCanonicalValue(deviceField.getChar(null));
            } else if (fieldType == short.class) {
                return  Short.toString(deviceField.getShort(null));
            } else if (fieldType == int.class) {
                return  Integer.toString(deviceField.getInt(null));
            } else if (fieldType == long.class) {
                return Long.toString(deviceField.getLong(null));
            } else if (fieldType == float.class) {
                return  canonicalizeFloatingPoint(
                                Float.toString(deviceField.getFloat(null)));
            } else if (fieldType == double.class) {
                return  canonicalizeFloatingPoint(
                                Double.toString(deviceField.getDouble(null)));
            } else if (fieldType == boolean.class) {
                return  Boolean.toString(deviceField.getBoolean(null));
            } else if (fieldType == java.lang.String.class) {
                return (String) deviceField.get(null);
            } else {
                return null;
            }
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
    }

    private static String convertCharToCanonicalValue(char c) {
        return String.format("'%c' (0x%x)", c, (int) c);
    }

    /**
     * Canonicalize the string representation of floating point numbers.
     *
     * This needs to be kept in sync with the doclava canonicalization.
     */
    private static String canonicalizeFloatingPoint(String val) {
        switch (val) {
            case "Infinity":
            case "-Infinity":
            case "NaN":
                return val;
        }

        if (val.indexOf('E') != -1) {
            return val;
        }

        // 1.0 is the only case where a trailing "0" is allowed.
        // 1.00 is canonicalized as 1.0.
        int i = val.length() - 1;
        int d = val.indexOf('.');
        while (i >= d + 2 && val.charAt(i) == '0') {
            val = val.substring(0, i--);
        }
        return val;
    }

    @Override
    protected void checkConstructor(JDiffClassDescription classDescription, Class<?> runtimeClass,
            JDiffClassDescription.JDiffConstructor ctorDescription, Constructor<?> ctor) {
        if (ctor.isVarArgs()) {// some method's parameter are variable args
            ctorDescription.mModifier |= METHOD_MODIFIER_VAR_ARGS;
        }
        if (ctor.getModifiers() != ctorDescription.mModifier) {
            resultObserver.notifyFailure(
                    FailureType.MISMATCH_METHOD,
                    ctorDescription.toReadableString(classDescription.getAbsoluteClassName()),
                    "Non-compatible method found when looking for " +
                            ctorDescription.toSignatureString());
        }
    }

    @Override
    protected void checkMethod(JDiffClassDescription classDescription, Class<?> runtimeClass,
            JDiffClassDescription.JDiffMethod methodDescription, Method method) {
        if (method.isVarArgs()) {
            methodDescription.mModifier |= METHOD_MODIFIER_VAR_ARGS;
        }
        if (method.isBridge()) {
            methodDescription.mModifier |= METHOD_MODIFIER_BRIDGE;
        }
        if (method.isSynthetic()) {
            methodDescription.mModifier |= METHOD_MODIFIER_SYNTHETIC;
        }

        // FIXME: A workaround to fix the final mismatch on enumeration
        if (runtimeClass.isEnum() && methodDescription.mName.equals("values")) {
            return;
        }

        String reason;
        if ((reason = areMethodsModifierCompatible(
                classDescription, methodDescription, method)) != null) {
            resultObserver.notifyFailure(FailureType.MISMATCH_METHOD,
                    methodDescription.toReadableString(classDescription.getAbsoluteClassName()),
                    String.format("Non-compatible method found when looking for %s - because %s",
                            methodDescription.toSignatureString(), reason));
        }
    }

    /**
     * Checks to ensure that the modifiers value for two methods are compatible.
     *
     * Allowable differences are:
     *   - the native modifier is ignored
     *
     * @param classDescription a description of a class in an API.
     * @param apiMethod the method read from the api file.
     * @param reflectedMethod the method found via reflection.
     * @return null if the method modifiers are compatible otherwise the reason why not.
     */
    private static String areMethodsModifierCompatible(
            JDiffClassDescription classDescription,
            JDiffClassDescription.JDiffMethod apiMethod,
            Method reflectedMethod) {

        // Mask off NATIVE since it is a don't care.  Also mask off
        // SYNCHRONIZED since it is not considered API significant (b/112626813)
        int ignoredMods = (Modifier.NATIVE | Modifier.SYNCHRONIZED | Modifier.STRICT);
        int reflectionModifiers = reflectedMethod.getModifiers() & ~ignoredMods;
        int apiModifiers = apiMethod.mModifier & ~ignoredMods;

        // A method can become non-abstract
        if ((reflectionModifiers & Modifier.ABSTRACT) == 0) {
            apiModifiers &= ~Modifier.ABSTRACT;
        }

        // We can ignore FINAL for classes
        if ((classDescription.getModifier() & Modifier.FINAL) != 0) {
            reflectionModifiers &= ~Modifier.FINAL;
            apiModifiers &= ~Modifier.FINAL;
        }

        String genericString = reflectedMethod.toGenericString();
        if (IGNORE_METHOD_ABSTRACT_MODIFIER_WHITE_LIST.contains(genericString)) {
            reflectionModifiers &= ~Modifier.ABSTRACT;
            apiModifiers &= ~Modifier.ABSTRACT;
        }

        if (reflectionModifiers == apiModifiers) {
            return null;
        } else {
            return String.format("modifier mismatch - description (%s), method (%s), for %s",
                    getModifierString(apiModifiers), getModifierString(reflectionModifiers), genericString);
        }
    }

    public void addBaseClass(JDiffClassDescription classDescription) {
        // Keep track of all the base interfaces that may by extended.
        if (classDescription.getClassType() == JDiffClassDescription.JDiffType.INTERFACE) {
            try {
                Class<?> runtimeClass =
                        ReflectionHelper.findMatchingClass(classDescription, classProvider);
                interfaceChecker.queueForDeferredCheck(classDescription, runtimeClass);
            } catch (ClassNotFoundException e) {
                // Do nothing.
            }
        }
    }
}
