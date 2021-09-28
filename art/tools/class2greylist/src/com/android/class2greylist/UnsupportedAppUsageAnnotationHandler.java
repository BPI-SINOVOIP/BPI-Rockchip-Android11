package com.android.class2greylist;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableSet;

import org.apache.bcel.Const;
import org.apache.bcel.classfile.AnnotationEntry;
import org.apache.bcel.classfile.ElementValue;
import org.apache.bcel.classfile.ElementValuePair;
import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.Method;
import org.apache.bcel.classfile.SimpleElementValue;

import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;

/**
 * Processes {@code UnsupportedAppUsage} annotations to generate greylist
 * entries.
 *
 * Any annotations with a {@link #EXPECTED_SIGNATURE_PROPERTY} property will have their
 * generated signature verified against this, and an error will be reported if
 * it does not match. Exclusions are made for bridge methods.
 *
 * Any {@link #MAX_TARGET_SDK_PROPERTY} properties will be validated against the given
 * set of valid values, then passed through to the greylist consumer.
 */
public class UnsupportedAppUsageAnnotationHandler extends AnnotationHandler {

    // properties of greylist annotations:
    private static final String EXPECTED_SIGNATURE_PROPERTY = "expectedSignature";
    private static final String MAX_TARGET_SDK_PROPERTY = "maxTargetSdk";
    private static final String IMPLICIT_MEMBER_PROPERTY = "implicitMember";
    private static final String PUBLIC_ALTERNATIVES_PROPERTY = "publicAlternatives";

    private final Status mStatus;
    private final Predicate<ClassMember> mClassMemberFilter;
    private final Map<Integer, String> mSdkVersionToFlagMap;
    private final AnnotationConsumer mAnnotationConsumer;

    private ApiResolver mApiResolver;

    /**
     * Represents a member of a class file (a field or method).
     */
    @VisibleForTesting
    public static class ClassMember {

        /**
         * Signature of this class member.
         */
        public final String signature;

        /**
         * Indicates if this is a synthetic bridge method.
         */
        public final boolean isBridgeMethod;

        public ClassMember(String signature, boolean isBridgeMethod) {
            this.signature = signature;
            this.isBridgeMethod = isBridgeMethod;
        }
    }

    public UnsupportedAppUsageAnnotationHandler(Status status,
            AnnotationConsumer annotationConsumer, Set<String> publicApis,
            Map<Integer, String> sdkVersionToFlagMap) {
        this(status, annotationConsumer,
                member -> !(member.isBridgeMethod && publicApis.contains(member.signature)),
                sdkVersionToFlagMap);
        mApiResolver = new ApiResolver(publicApis);
    }

    @VisibleForTesting
    public UnsupportedAppUsageAnnotationHandler(Status status,
            AnnotationConsumer annotationConsumer, Predicate<ClassMember> memberFilter,
            Map<Integer, String> sdkVersionToFlagMap) {
        mStatus = status;
        mAnnotationConsumer = annotationConsumer;
        mClassMemberFilter = memberFilter;
        mSdkVersionToFlagMap = sdkVersionToFlagMap;
        mApiResolver = new ApiResolver();
    }

    @Override
    public void handleAnnotation(AnnotationEntry annotation, AnnotationContext context) {
        boolean isBridgeMethod = false;
        if (context instanceof AnnotatedMemberContext) {
            AnnotatedMemberContext memberContext = (AnnotatedMemberContext) context;
            FieldOrMethod member = memberContext.member;
            isBridgeMethod = (member instanceof Method) &&
                    (member.getAccessFlags() & Const.ACC_BRIDGE) != 0;
            if (isBridgeMethod) {
                mStatus.debug("Member is a bridge method");
            }
        }

        String signature = context.getMemberDescriptor();
        Integer maxTargetSdk = null;
        String implicitMemberSignature = null;
        String publicAlternativesString = null;

        for (ElementValuePair property : annotation.getElementValuePairs()) {
            switch (property.getNameString()) {
                case EXPECTED_SIGNATURE_PROPERTY:
                    String expected = property.getValue().stringifyValue();
                    // Don't enforce for bridge methods; they're generated so won't match.
                    if (!isBridgeMethod && !signature.equals(expected)) {
                        context.reportError("Expected signature does not match generated:\n"
                                + "Expected:  %s\n"
                                + "Generated: %s", expected, signature);
                        return;
                    }
                    break;
                case MAX_TARGET_SDK_PROPERTY:
                    if (property.getValue().getElementValueType() != ElementValue.PRIMITIVE_INT) {
                        context.reportError("Expected property %s to be of type int; got %d",
                                property.getNameString(),
                                property.getValue().getElementValueType());
                        return;
                    }

                    maxTargetSdk = ((SimpleElementValue) property.getValue()).getValueInt();
                    break;
                case IMPLICIT_MEMBER_PROPERTY:
                    implicitMemberSignature = property.getValue().stringifyValue();
                    if (context instanceof AnnotatedClassContext) {
                        signature = String.format("L%s;->%s",
                                context.getClassDescriptor(), implicitMemberSignature);
                    } else {
                        context.reportError(
                                "Expected annotation with an %s property to be on a class but is "
                                        + "on %s",
                                IMPLICIT_MEMBER_PROPERTY,
                                signature);
                        return;
                    }
                    break;
                case PUBLIC_ALTERNATIVES_PROPERTY:
                    publicAlternativesString = property.getValue().stringifyValue();
                    break;
            }
        }

        if (context instanceof AnnotatedClassContext && implicitMemberSignature == null) {
            context.reportError(
                    "Missing property %s on annotation on class %s",
                    IMPLICIT_MEMBER_PROPERTY,
                    signature);
            return;
        }

        // Verify that maxTargetSdk is valid.
        if (!mSdkVersionToFlagMap.containsKey(maxTargetSdk)) {
            context.reportError("Invalid value for %s: got %d, expected one of [%s]",
                    MAX_TARGET_SDK_PROPERTY,
                    maxTargetSdk,
                    mSdkVersionToFlagMap.keySet());
            return;
        }

        try {
            mApiResolver.resolvePublicAlternatives(publicAlternativesString, signature,
                    maxTargetSdk);
        } catch (JavadocLinkSyntaxError | AlternativeNotFoundError e) {
            context.reportError(e.toString());
        } catch (RequiredAlternativeNotSpecifiedError e) {
            context.reportError("Signature %s moved to %s without specifying public "
                            + "alternatives; Refer to go/unsupportedappusage-public-alternatives "
                            + "for details.",
                    signature, mSdkVersionToFlagMap.get(maxTargetSdk));
        }

        // Consume this annotation if it matches the predicate.
        if (mClassMemberFilter.test(new ClassMember(signature, isBridgeMethod))) {
            mAnnotationConsumer.consume(signature, stringifyAnnotationProperties(annotation),
                    ImmutableSet.of(mSdkVersionToFlagMap.get(maxTargetSdk)));
        }
    }
}
