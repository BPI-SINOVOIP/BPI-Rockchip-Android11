/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.processor.compat.changeid;

import static javax.lang.model.element.ElementKind.CLASS;
import static javax.lang.model.element.ElementKind.PARAMETER;
import static javax.tools.Diagnostic.Kind.ERROR;
import static javax.tools.StandardLocation.CLASS_OUTPUT;

import android.processor.compat.SingleAnnotationProcessor;
import android.processor.compat.SourcePosition;

import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Table;

import java.io.IOException;
import java.io.OutputStream;
import java.util.List;
import java.util.Objects;
import java.util.regex.Pattern;

import javax.annotation.processing.SupportedAnnotationTypes;
import javax.annotation.processing.SupportedSourceVersion;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.AnnotationValue;
import javax.lang.model.element.Element;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.TypeElement;
import javax.lang.model.element.VariableElement;
import javax.lang.model.type.TypeKind;
import javax.tools.FileObject;

/**
 * Annotation processor for ChangeId annotations.
 *
 * This processor outputs an XML file containing all the changeIds defined by this
 * annotation. The file is bundled into the pratform image and used by the system server.
 * Design doc: go/gating-and-logging.
 */
@SupportedAnnotationTypes({"android.compat.annotation.ChangeId"})
@SupportedSourceVersion(SourceVersion.RELEASE_9)
public class ChangeIdProcessor extends SingleAnnotationProcessor {

    private static final String CONFIG_XML = "compat_config.xml";

    private static final String IGNORED_CLASS = "android.compat.Compatibility";
    private static final ImmutableSet<String> IGNORED_METHOD_NAMES =
            ImmutableSet.of("reportChange", "isChangeEnabled");

    private static final String CHANGE_ID_QUALIFIED_CLASS_NAME =
            "android.compat.annotation.ChangeId";

    private static final String DISABLED_CLASS_NAME = "android.compat.annotation.Disabled";
    private static final String ENABLED_AFTER_CLASS_NAME = "android.compat.annotation.EnabledAfter";
    private static final String LOGGING_CLASS_NAME = "android.compat.annotation.LoggingOnly";
    private static final String TARGET_SDK_VERSION = "targetSdkVersion";

    private static final Pattern JAVADOC_SANITIZER = Pattern.compile("^\\s", Pattern.MULTILINE);
    private static final Pattern HIDE_TAG_MATCHER = Pattern.compile("(\\s|^)@hide(\\s|$)");

    @Override
    protected void process(TypeElement annotation,
            Table<PackageElement, String, List<Element>> annotatedElements) {
        for (PackageElement packageElement : annotatedElements.rowKeySet()) {
            for (String enclosingElementName : annotatedElements.row(packageElement).keySet()) {
                XmlWriter writer = new XmlWriter();
                for (Element element : annotatedElements.get(packageElement,
                        enclosingElementName)) {
                    Change change =
                            createChange(packageElement.toString(), enclosingElementName, element);
                    writer.addChange(change);
                }

                try {
                    FileObject resource = processingEnv.getFiler().createResource(
                            CLASS_OUTPUT, packageElement.toString(),
                            enclosingElementName + "_" + CONFIG_XML);
                    try (OutputStream outputStream = resource.openOutputStream()) {
                        writer.write(outputStream);
                    }
                } catch (IOException e) {
                    messager.printMessage(ERROR, "Failed to write output: " + e);
                }
            }
        }
    }

    @Override
    protected boolean ignoreAnnotatedElement(Element element, AnnotationMirror mirror) {
        // Ignore the annotations on method parameters in known methods in package android.compat
        // (libcore/luni/src/main/java/android/compat/Compatibility.java)
        // without generating an error.
        if (element.getKind() == PARAMETER) {
            Element enclosingMethod = element.getEnclosingElement();
            Element enclosingElement = enclosingMethod.getEnclosingElement();
            if (enclosingElement.getKind() == CLASS) {
                if (enclosingElement.toString().equals(IGNORED_CLASS) &&
                        IGNORED_METHOD_NAMES.contains(enclosingMethod.getSimpleName().toString())) {
                    return true;
                }
            }
        }
        return !isValidChangeId(element);
    }

    /**
     * Checks if the provided java element is a valid change id (i.e. a long parameter with a
     * constant value).
     *
     * @param element java element to check.
     * @return true if the provided element is a legal change id that should be added to the
     * produced XML file. If true is returned it's guaranteed that the following operations are
     * safe.
     */
    private boolean isValidChangeId(Element element) {
        if (element.getKind() != ElementKind.FIELD) {
            messager.printMessage(
                    ERROR,
                    "Non FIELD element annotated with @ChangeId.",
                    element);
            return false;
        }
        if (!(element instanceof VariableElement)) {
            messager.printMessage(
                    ERROR,
                    "Non variable annotated with @ChangeId.",
                    element);
            return false;
        }
        if (((VariableElement) element).getConstantValue() == null) {
            messager.printMessage(
                    ERROR,
                    "Non constant/final variable annotated with @ChangeId.",
                    element);
            return false;
        }
        if (element.asType().getKind() != TypeKind.LONG) {
            messager.printMessage(
                    ERROR,
                    "Variables annotated with @ChangeId must be of type long.",
                    element);
            return false;
        }
        if (!element.getModifiers().contains(Modifier.STATIC)) {
            messager.printMessage(
                    ERROR,
                    "Non static variable annotated with @ChangeId.",
                    element);
            return false;
        }
        return true;
    }

    private Change createChange(String packageName, String enclosingElementName, Element element) {
        Change.Builder builder = new Change.Builder()
                .id((Long) ((VariableElement) element).getConstantValue())
                .name(element.getSimpleName().toString());

        AnnotationMirror changeId = null;
        for (AnnotationMirror mirror : element.getAnnotationMirrors()) {
            String type =
                    ((TypeElement) mirror.getAnnotationType().asElement()).getQualifiedName().toString();
            switch (type) {
                case DISABLED_CLASS_NAME:
                    builder.disabled();
                    break;
                case LOGGING_CLASS_NAME:
                    builder.loggingOnly();
                    break;
                case ENABLED_AFTER_CLASS_NAME:
                    AnnotationValue value = getAnnotationValue(element, mirror, TARGET_SDK_VERSION);
                    builder.enabledAfter((Integer)(Objects.requireNonNull(value).getValue()));
                    break;
                case CHANGE_ID_QUALIFIED_CLASS_NAME:
                    changeId = mirror;
                    break;
            }
        }

        String comment =
                elements.getDocComment(element);
        if (comment != null) {
            comment = HIDE_TAG_MATCHER.matcher(comment).replaceAll("");
            comment = JAVADOC_SANITIZER.matcher(comment).replaceAll("");
            comment = comment.replaceAll("\\n", " ");
            builder.description(comment.trim());
        }

        return verifyChange(element,
                builder.javaClass(enclosingElementName)
                        .javaPackage(packageName)
                        .qualifiedClass(packageName + "." + enclosingElementName)
                        .sourcePosition(getLineNumber(element, changeId))
                        .build());
    }

    private String getLineNumber(Element element, AnnotationMirror mirror) {
        SourcePosition position = Objects.requireNonNull(getSourcePosition(element, mirror));
        return String.format("%s:%d", position.getFilename(), position.getStartLineNumber());
    }

    private Change verifyChange(Element element, Change change) {
        if (change.disabled && change.enabledAfter != null) {
            messager.printMessage(
                    ERROR,
                    "ChangeId cannot be annotated with both @Disabled and @EnabledAfter.",
                    element);
        }
        if (change.loggingOnly && (change.disabled || change.enabledAfter != null)) {
            messager.printMessage(
                    ERROR,
                    "ChangeId cannot be annotated with both @LoggingOnly and "
                            + "(@EnabledAfter | @Disabled).",
                    element);
        }
        return change;
    }
}
