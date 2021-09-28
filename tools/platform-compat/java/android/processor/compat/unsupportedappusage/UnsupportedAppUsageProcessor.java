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
import static javax.tools.StandardLocation.CLASS_OUTPUT;

import android.processor.compat.SingleAnnotationProcessor;
import android.processor.compat.SourcePosition;

import com.google.common.base.Joiner;
import com.google.common.collect.Table;

import java.io.IOException;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

import javax.annotation.Nullable;
import javax.annotation.processing.SupportedAnnotationTypes;
import javax.annotation.processing.SupportedSourceVersion;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.AnnotationValue;
import javax.lang.model.element.Element;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.TypeElement;
import javax.tools.FileObject;

/**
 * Annotation processor for {@code UnsupportedAppUsage} annotation.
 *
 * <p>This processor generates a CSV file with a mapping of dex signatures of elements annotated
 * with @UnsupportedAppUsage to corresponding source positions for their UnsupportedAppUsage
 * annotation.
 */
@SupportedAnnotationTypes({"android.compat.annotation.UnsupportedAppUsage"})
@SupportedSourceVersion(SourceVersion.RELEASE_9)
public final class UnsupportedAppUsageProcessor extends SingleAnnotationProcessor {

    private static final String GENERATED_INDEX_FILE_EXTENSION = ".uau";

    private static final String OVERRIDE_SOURCE_POSITION_PROPERTY = "overrideSourcePosition";
    private static final Pattern OVERRIDE_SOURCE_POSITION_PROPERTY_PATTERN = Pattern.compile(
            "^[^:]+:\\d+:\\d+:\\d+:\\d+$");

    /**
     * CSV header line for the columns returned by {@link #getAnnotationIndex(String, TypeElement,
     * Element)}.
     */
    private static final String CSV_HEADER = Joiner.on(',').join(
            "signature",
            "file",
            "startline",
            "startcol",
            "endline",
            "endcol",
            "properties"
    );

    @Override
    protected void process(TypeElement annotation,
            Table<PackageElement, String, List<Element>> annotatedElements) {
        SignatureConverter signatureConverter = new SignatureConverter(messager);

        for (PackageElement packageElement : annotatedElements.rowKeySet()) {
            Map<String, List<Element>> row = annotatedElements.row(packageElement);
            for (String enclosingElementName : row.keySet()) {
                List<String> content = new ArrayList<>();
                for (Element annotatedElement : row.get(enclosingElementName)) {
                    String signature = signatureConverter.getSignature(
                            types, annotation, annotatedElement);
                    if (signature != null) {
                        String annotationIndex = getAnnotationIndex(signature, annotation,
                                annotatedElement);
                        if (annotationIndex != null) {
                            content.add(annotationIndex);
                        }
                    }
                }

                if (content.isEmpty()) {
                    continue;
                }

                try {
                    FileObject resource = processingEnv.getFiler().createResource(
                            CLASS_OUTPUT,
                            packageElement.toString(),
                            enclosingElementName + GENERATED_INDEX_FILE_EXTENSION);
                    try (PrintStream outputStream = new PrintStream(resource.openOutputStream())) {
                        outputStream.println(CSV_HEADER);
                        content.forEach(outputStream::println);
                    }
                } catch (IOException exception) {
                    messager.printMessage(ERROR, "Could not write CSV file: " + exception);
                }
            }
        }
    }

    @Override
    protected boolean ignoreAnnotatedElement(Element element, AnnotationMirror mirror) {
        // Implicit member refers to member not present in code, ignore.
        return hasElement(mirror, "implicitMember");
    }

    /**
     * Maps an annotated element to the source position of the @UnsupportedAppUsage annotation
     * attached to it.
     *
     * <p>It returns CSV in the format:
     * dex-signature,filename,start-line,start-col,end-line,end-col,properties
     *
     * <p>The positions refer to the annotation itself, *not* the annotated member. This can
     * therefore be used to read just the annotation from the file, and to perform in-place
     * edits on it.
     *
     * @return A single line of CSV text
     */
    @Nullable
    private String getAnnotationIndex(String signature, TypeElement annotation, Element element) {
        AnnotationMirror annotationMirror = getSupportedAnnotationMirror(annotation, element);
        String position = getSourcePositionOverride(element, annotationMirror);
        if (position == null) {
            SourcePosition sourcePosition = getSourcePosition(element, annotationMirror);
            if (sourcePosition == null) {
                return null;
            }
            position = Joiner.on(",").join(
                    sourcePosition.getFilename(),
                    sourcePosition.getStartLineNumber(),
                    sourcePosition.getStartColumnNumber(),
                    sourcePosition.getEndLineNumber(),
                    sourcePosition.getEndColumnNumber());
        }
        return Joiner.on(",").join(
                signature,
                position,
                getAllProperties(annotationMirror));
    }

    @Nullable
    private String getSourcePositionOverride(Element element, AnnotationMirror annotation) {
        AnnotationValue annotationValue =
                getAnnotationValue(element, annotation, OVERRIDE_SOURCE_POSITION_PROPERTY);
        if (annotationValue == null) {
            return null;
        }

        String parameterValue = annotationValue.getValue().toString();
        if (!OVERRIDE_SOURCE_POSITION_PROPERTY_PATTERN.matcher(parameterValue).matches()) {
            messager.printMessage(ERROR, String.format(
                    "Expected %s to have format string:int:int:int:int",
                    OVERRIDE_SOURCE_POSITION_PROPERTY), element, annotation);
            return null;
        }

        return parameterValue.replace(':', ',');
    }

    private boolean hasElement(AnnotationMirror annotation, String elementName) {
        return annotation.getElementValues().keySet().stream().anyMatch(
                key -> elementName.equals(key.getSimpleName().toString()));
    }

    private String getAllProperties(AnnotationMirror annotation) {
        return annotation.getElementValues().keySet().stream()
                .filter(key -> !key.getSimpleName().toString().equals(
                        OVERRIDE_SOURCE_POSITION_PROPERTY))
                .map(key -> String.format(
                        "%s=%s",
                        key.getSimpleName(),
                        getAnnotationElementValue(annotation, key)))
                .collect(Collectors.joining("&"));
    }

    private String getAnnotationElementValue(AnnotationMirror annotation,
            ExecutableElement element) {
        try {
            return URLEncoder.encode(annotation.getElementValues().get(element).toString(),
                    "UTF-8");
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
    }

}
