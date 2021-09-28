/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.processor.compat;

import com.google.common.base.Preconditions;
import com.google.common.collect.HashBasedTable;
import com.google.common.collect.Iterables;
import com.google.common.collect.Table;
import com.sun.source.tree.CompilationUnitTree;
import com.sun.source.tree.LineMap;
import com.sun.source.tree.Tree;
import com.sun.source.util.SourcePositions;
import com.sun.source.util.TreePath;
import com.sun.source.util.Trees;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import javax.annotation.Nullable;
import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.Messager;
import javax.annotation.processing.ProcessingEnvironment;
import javax.annotation.processing.RoundEnvironment;
import javax.lang.model.element.AnnotationMirror;
import javax.lang.model.element.AnnotationValue;
import javax.lang.model.element.Element;
import javax.lang.model.element.PackageElement;
import javax.lang.model.element.QualifiedNameable;
import javax.lang.model.element.TypeElement;
import javax.lang.model.util.Elements;
import javax.lang.model.util.Types;

/**
 * Abstract annotation processor that goes over annotated elements in bulk (per .class file).
 *
 * <p>It expects only one supported annotation, i.e. {@link #getSupportedAnnotationTypes()} must
 * return one annotation type only.
 *
 * <p>Annotated elements are pre-filtered by {@link #ignoreAnnotatedElement(Element,
 * AnnotationMirror)}. Afterwards, the table with package and enclosing element name into list of
 * elements is generated and passed to {@link #process(TypeElement, Table)}.
 */
public abstract class SingleAnnotationProcessor extends AbstractProcessor {

    protected Elements elements;
    protected Messager messager;
    protected SourcePositions sourcePositions;
    protected Trees trees;
    protected Types types;

    @Override
    public synchronized void init(ProcessingEnvironment processingEnv) {
        super.init(processingEnv);

        this.elements = processingEnv.getElementUtils();
        this.messager = processingEnv.getMessager();
        this.trees = Trees.instance(processingEnv);
        this.types = processingEnv.getTypeUtils();

        this.sourcePositions = trees.getSourcePositions();
    }

    @Override
    public boolean process(
            Set<? extends TypeElement> annotations, RoundEnvironment roundEnvironment) {
        if (annotations.size() == 0) {
            // no annotations to process, doesn't really matter what we return here.
            return true;
        }

        TypeElement annotation = Iterables.getOnlyElement(annotations);
        String supportedAnnotation = Iterables.getOnlyElement(getSupportedAnnotationTypes());
        Preconditions.checkState(supportedAnnotation.equals(annotation.toString()));

        Table<PackageElement, String, List<Element>> annotatedElements = HashBasedTable.create();
        for (Element annotatedElement : roundEnvironment.getElementsAnnotatedWith(annotation)) {
            AnnotationMirror annotationMirror =
                    getSupportedAnnotationMirror(annotation, annotatedElement);
            if (ignoreAnnotatedElement(annotatedElement, annotationMirror)) {
                continue;
            }

            PackageElement packageElement = elements.getPackageOf(annotatedElement);
            String enclosingElementName = getEnclosingElementName(annotatedElement);
            Preconditions.checkNotNull(packageElement);
            Preconditions.checkNotNull(enclosingElementName);

            if (!annotatedElements.contains(packageElement, enclosingElementName)) {
                annotatedElements.put(packageElement, enclosingElementName, new ArrayList<>());
            }
            annotatedElements.get(packageElement, enclosingElementName).add(annotatedElement);
        }

        process(annotation, annotatedElements);
        return true;
    }

    /**
     * Processes a set of elements annotated with supported annotation and not ignored via {@link
     * #ignoreAnnotatedElement(Element, AnnotationMirror)}.
     *
     * @param annotation        {@link TypeElement} of the supported annotation
     * @param annotatedElements table with {@code package}, {@code enclosing elements name}, and the
     *                          list of elements
     */
    protected abstract void process(TypeElement annotation,
            Table<PackageElement, String, List<Element>> annotatedElements);

    /** Whether to process given element with the annotation mirror. */
    protected boolean ignoreAnnotatedElement(Element element, AnnotationMirror mirror) {
        return false;
    }

    /**
     * Returns the annotation mirror for the supported annotation on the given element.
     *
     * <p>We are not using a class to avoid choosing which Java 9 Module to select from, in case
     * the annotation is present in base module and unnamed module.
     */
    protected final AnnotationMirror getSupportedAnnotationMirror(TypeElement annotation,
            Element element) {
        for (AnnotationMirror mirror : element.getAnnotationMirrors()) {
            if (types.isSameType(annotation.asType(), mirror.getAnnotationType())) {
                return mirror;
            }
        }
        return null;
    }

    /**
     * Returns {@link SourcePosition} of an annotation on the given element or null if position is
     * not found.
     */
    @Nullable
    protected final SourcePosition getSourcePosition(Element element,
            AnnotationMirror annotationMirror) {
        TreePath path = trees.getPath(element, annotationMirror);
        if (path == null) {
            return null;
        }
        CompilationUnitTree compilationUnit = path.getCompilationUnit();
        Tree tree = path.getLeaf();
        long startPosition = sourcePositions.getStartPosition(compilationUnit, tree);
        long endPosition = sourcePositions.getEndPosition(compilationUnit, tree);

        LineMap lineMap = path.getCompilationUnit().getLineMap();
        return new SourcePosition(
                compilationUnit.getSourceFile().getName(),
                lineMap.getLineNumber(startPosition),
                lineMap.getColumnNumber(startPosition),
                lineMap.getLineNumber(endPosition),
                lineMap.getColumnNumber(endPosition));
    }

    @Nullable
    protected final AnnotationValue getAnnotationValue(
            Element element, AnnotationMirror annotation, String propertyName) {
        return annotation.getElementValues().keySet().stream()
                .filter(key -> propertyName.equals(key.getSimpleName().toString()))
                .map(key -> annotation.getElementValues().get(key))
                .reduce((a, b) -> {
                    throw new IllegalStateException(
                            String.format("Only one %s expected, found %s in %s",
                                    propertyName, annotation, element));
                })
                .orElse(null);
    }

    /**
     * Returns a name of an enclosing element without the package name.
     *
     * <p>This would return names of all enclosing classes, e.g. <code>Outer.Inner.Foo</code>.
     */
    private String getEnclosingElementName(Element element) {
        String fullQualifiedName =
                ((QualifiedNameable) element.getEnclosingElement()).getQualifiedName().toString();
        String packageName = elements.getPackageOf(element).toString();
        return fullQualifiedName.substring(packageName.length() + 1);
    }

}
