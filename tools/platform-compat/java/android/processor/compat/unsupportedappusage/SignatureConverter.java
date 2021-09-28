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
package android.processor.compat.unsupportedappusage;

import static javax.tools.Diagnostic.Kind.ERROR;

import com.google.common.collect.ImmutableMap;
import com.sun.tools.javac.code.Type;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

import javax.annotation.processing.Messager;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.Element;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.TypeElement;
import javax.lang.model.element.VariableElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.DeclaredType;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.lang.model.util.Types;

/**
 * Builds a dex signature for a given method or field.
 */
final class SignatureConverter {

    private static final Map<TypeKind, String> TYPE_MAP = ImmutableMap.<TypeKind, String>builder()
            .put(TypeKind.BOOLEAN, "Z")
            .put(TypeKind.BYTE, "B")
            .put(TypeKind.CHAR, "C")
            .put(TypeKind.DOUBLE, "D")
            .put(TypeKind.FLOAT, "F")
            .put(TypeKind.INT, "I")
            .put(TypeKind.LONG, "J")
            .put(TypeKind.SHORT, "S")
            .put(TypeKind.VOID, "V")
            .build();

    private final Messager messager;

    SignatureConverter(Messager messager) {
        this.messager = messager;
    }

    /**
     * Creates the signature for an annotated element.
     */
    String getSignature(Types types, TypeElement annotation, Element element) {
        try {
            String signature;
            switch (element.getKind()) {
                case METHOD:
                    signature = buildMethodSignature((ExecutableElement) element);
                    break;
                case CONSTRUCTOR:
                    signature = buildConstructorSignature((ExecutableElement) element);
                    break;
                case FIELD:
                    signature = buildFieldSignature((VariableElement) element);
                    break;
                default:
                    return null;
            }
            return verifyExpectedSignature(types, signature, element, annotation);
        } catch (SignatureConverterException problem) {
            messager.printMessage(ERROR, problem.getMessage(), element);
            return null;
        }
    }

    /**
     * Returns a list of enclosing elements for the given element, with the package first, and
     * excluding the element itself.
     */
    private List<Element> getEnclosingElements(Element element) {
        List<Element> enclosing = new ArrayList<>();
        element = element.getEnclosingElement(); // don't include the element itself.
        while (element != null) {
            enclosing.add(element);
            element = element.getEnclosingElement();
        }
        Collections.reverse(enclosing);
        return enclosing;
    }

    /**
     * Get the dex signature for a clazz, in format "Lpackage/name/Outer$Inner;"
     */
    private String getClassSignature(TypeElement clazz) {
        StringBuilder signature = new StringBuilder("L");
        for (Element enclosing : getEnclosingElements(clazz)) {
            switch (enclosing.getKind()) {
                case MODULE:
                    // ignore
                    break;
                case PACKAGE:
                    signature.append(((PackageElement) enclosing)
                            .getQualifiedName()
                            .toString()
                            .replace('.', '/'));
                    signature.append('/');
                    break;
                case CLASS:
                case INTERFACE:
                    signature.append(enclosing.getSimpleName()).append('$');
                    break;
                default:
                    throw new IllegalStateException(
                            "Unexpected enclosing element " + enclosing.getKind());
            }
        }
        return signature
                .append(clazz.getSimpleName())
                .append(";")
                .toString();
    }

    /**
     * Returns the type signature for a given type.
     *
     * <p>For primitive types, a single character. For classes, the class signature. For arrays, a
     * "[" preceding the component type.
     */
    private String getTypeSignature(TypeMirror type) throws SignatureConverterException {
        TypeKind kind = type.getKind();
        if (TYPE_MAP.containsKey(kind)) {
            return TYPE_MAP.get(kind);
        }
        switch (kind) {
            case ARRAY:
                return "[" + getTypeSignature(((ArrayType) type).getComponentType());
            case DECLARED:
                Element declaring = ((DeclaredType) type).asElement();
                if (!(declaring instanceof TypeElement)) {
                    throw new SignatureConverterException(
                            "Can't handle declared type of kind " + declaring.getKind());
                }
                return getClassSignature((TypeElement) declaring);
            case TYPEVAR:
                Type.TypeVar typeVar = (Type.TypeVar) type;
                if (typeVar.getLowerBound().getKind() != TypeKind.NULL) {
                    return getTypeSignature(typeVar.getLowerBound());
                } else if (typeVar.getUpperBound().getKind() != TypeKind.NULL) {
                    return getTypeSignature(typeVar.getUpperBound());
                } else {
                    throw new SignatureConverterException("Can't handle typevar with no bound");
                }
            default:
                throw new SignatureConverterException("Can't handle type of kind " + kind);
        }
    }

    /**
     * Get the signature for an executable, either a method or a constructor.
     *
     * @param name   "<init>" for  constructor, else the method name
     * @param method The executable element in question.
     */
    private String getExecutableSignature(CharSequence name, ExecutableElement method)
            throws SignatureConverterException {
        StringBuilder signature = new StringBuilder();
        signature.append(getClassSignature((TypeElement) method.getEnclosingElement()))
                .append("->")
                .append(name)
                .append("(");
        for (VariableElement param : method.getParameters()) {
            signature.append(getTypeSignature(param.asType()));
        }
        signature
                .append(")")
                .append(getTypeSignature(method.getReturnType()));
        return signature.toString();
    }

    private String buildMethodSignature(ExecutableElement method)
            throws SignatureConverterException {
        return getExecutableSignature(method.getSimpleName(), method);
    }

    private String buildConstructorSignature(ExecutableElement constructor)
            throws SignatureConverterException {
        return getExecutableSignature("<init>", constructor);
    }

    private String buildFieldSignature(VariableElement field) throws SignatureConverterException {
        return new StringBuilder()
                .append(getClassSignature((TypeElement) field.getEnclosingElement()))
                .append("->")
                .append(field.getSimpleName())
                .append(":")
                .append(getTypeSignature(field.asType()))
                .toString();
    }

    /** If we have an expected signature on the annotation, warn if it doesn't match. */
    private String verifyExpectedSignature(Types types, String signature, Element element,
            TypeElement annotation) throws SignatureConverterException {
        AnnotationMirror annotationMirror = null;
        for (AnnotationMirror mirror : element.getAnnotationMirrors()) {
            if (types.isSameType(annotation.asType(), mirror.getAnnotationType())) {
                annotationMirror = mirror;
                break;
            }
        }
        if (annotationMirror == null) {
            throw new SignatureConverterException(
                    "Element doesn't have any UnsupportedAppUsage annotation");
        }
        String expectedSignature = annotationMirror.getElementValues().entrySet().stream()
                .filter(e -> e.getKey().getSimpleName().contentEquals("expectedSignature"))
                .map(e -> (String) e.getValue().getValue())
                .findAny()
                .orElse(signature);
        if (!signature.equals(expectedSignature)) {
            throw new SignatureConverterException(String.format(
                    "Expected signature doesn't match generated signature.\n"
                            + " Expected:  %s\n Generated: %s",
                    expectedSignature,
                    signature));
        }
        return signature;
    }

    /**
     * Exception used internally when we can't build a signature.
     */
    private static class SignatureConverterException extends Exception {
        SignatureConverterException(String message) {
            super(message);
        }
    }
}
