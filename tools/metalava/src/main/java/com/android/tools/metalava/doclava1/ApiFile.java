/*
 * Copyright (C) 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.tools.metalava.doclava1;

import com.android.tools.lint.checks.infrastructure.ClassNameKt;
import com.android.tools.metalava.FileFormat;
import com.android.tools.metalava.model.AnnotationItem;
import com.android.tools.metalava.model.DefaultModifierList;
import com.android.tools.metalava.model.TypeParameterList;
import com.android.tools.metalava.model.VisibilityLevel;
import com.android.tools.metalava.model.text.TextClassItem;
import com.android.tools.metalava.model.text.TextConstructorItem;
import com.android.tools.metalava.model.text.TextFieldItem;
import com.android.tools.metalava.model.text.TextMethodItem;
import com.android.tools.metalava.model.text.TextModifiers;
import com.android.tools.metalava.model.text.TextPackageItem;
import com.android.tools.metalava.model.text.TextParameterItem;
import com.android.tools.metalava.model.text.TextParameterItemKt;
import com.android.tools.metalava.model.text.TextPropertyItem;
import com.android.tools.metalava.model.text.TextTypeItem;
import com.android.tools.metalava.model.text.TextTypeParameterList;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.io.Files;
import kotlin.Pair;
import kotlin.text.StringsKt;
import org.jetbrains.annotations.Nullable;

import javax.annotation.Nonnull;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import static com.android.tools.metalava.ConstantsKt.ANDROIDX_NONNULL;
import static com.android.tools.metalava.ConstantsKt.ANDROIDX_NULLABLE;
import static com.android.tools.metalava.ConstantsKt.JAVA_LANG_ANNOTATION;
import static com.android.tools.metalava.ConstantsKt.JAVA_LANG_ENUM;
import static com.android.tools.metalava.ConstantsKt.JAVA_LANG_STRING;
import static com.android.tools.metalava.model.FieldItemKt.javaUnescapeString;
import static kotlin.text.Charsets.UTF_8;

//
// Copied from doclava1, but adapted to metalava's code model (plus tweaks to handle
// metalava's richer files, e.g. annotations)
//
public class ApiFile {
    /**
     * Same as {@link #parseApi(List, boolean)}}, but take a single file for convenience.
     *
     * @param file input signature file
     * @param kotlinStyleNulls if true, we assume the input has a kotlin style nullability markers (e.g. "?").
     *                         Even if false, we'll allow them if the file format supports them/
     */
    public static TextCodebase parseApi(@Nonnull File file, boolean kotlinStyleNulls) throws ApiParseException {
        final List<File> files = new ArrayList<>(1);
        files.add(file);
        return parseApi(files, kotlinStyleNulls);
    }

    /**
     * Read API signature files into a {@link TextCodebase}.
     *
     * Note: when reading from them multiple files, {@link TextCodebase#getLocation} would refer to the first
     * file specified. each {@link com.android.tools.metalava.model.text.TextItem#getPosition} would correctly
     * point out the source file of each item.
     *
     * @param files input signature files
     * @param kotlinStyleNulls if true, we assume the input has a kotlin style nullability markers (e.g. "?").
     *                         Even if false, we'll allow them if the file format supports them/
     */
    public static TextCodebase parseApi(@Nonnull List<File> files, boolean kotlinStyleNulls)
        throws ApiParseException {
        if (files.size() == 0) {
            throw new IllegalArgumentException("files must not be empty");
        }
        final TextCodebase api = new TextCodebase(files.get(0));
        final StringBuilder description = new StringBuilder("Codebase loaded from ");

        boolean first = true;
        for (File file : files) {
            if (!first) {
                description.append(", ");
            }
            description.append(file.getPath());

            final String apiText;
            try {
                apiText = Files.asCharSource(file, UTF_8).read();
            } catch (IOException ex) {
                throw new ApiParseException("Error reading API file", file.getPath(), ex);
            }
            parseApiSingleFile(api, !first, file.getPath(), apiText, kotlinStyleNulls);
            first = false;
        }
        api.setDescription(description.toString());
        api.postProcess();
        return api;
    }

    /** @deprecated Exists only for external callers. */
    @Deprecated
    public static TextCodebase parseApi(@Nonnull String filename, @Nonnull String apiText,
                                        Boolean kotlinStyleNulls) throws ApiParseException {
        return parseApi(filename, apiText, kotlinStyleNulls != null && kotlinStyleNulls);
    }

    /**
     * Entry point fo test. Take a filename and content separately.
     */
    @VisibleForTesting
    public static TextCodebase parseApi(@Nonnull String filename, @Nonnull String apiText,
                                        boolean kotlinStyleNulls) throws ApiParseException {
        final TextCodebase api = new TextCodebase(new File(filename));
        api.setDescription("Codebase loaded from " + filename);
        parseApiSingleFile(api, false, filename, apiText, kotlinStyleNulls);
        api.postProcess();
        return api;
    }

    private static void parseApiSingleFile(TextCodebase api, boolean appending, String filename, String apiText,
                                           boolean kotlinStyleNulls) throws ApiParseException {
        // Infer the format.
        FileFormat format = FileFormat.Companion.parseHeader(apiText);

        // If it's the first file, set the format. Otherwise, make sure the format is the same as the prior files.
        if (!appending) {
            // This is the first file to process.
            api.setFormat(format);
        } else {
            // If we're appending to another API file, make sure the format is the same.
            if (!format.equals(api.getFormat())) {
                throw new ApiParseException(String.format(
                    "Cannot merge different formats of signature files. First file format=%s, current file format=%s: file=%s",
                    api.getFormat(), format, filename));
            }
            // When we're appending, and the content is empty, nothing to do.
            if (StringsKt.isBlank(apiText)) {
                return;
            }
        }

        // Even if kotlinStyleNulls is false, still allow kotlin nullability markers, if the format allows them.
        if (format.isSignatureFormat()) {
            if (!kotlinStyleNulls) {
                kotlinStyleNulls = format.useKotlinStyleNulls();
            }
        } else if (StringsKt.isBlank(apiText)) {
            // Sometimes, signature files are empty, and we do want to accept them.
        } else {
            throw new ApiParseException("Unknown file format of " + filename);
        }

        if (kotlinStyleNulls) {
            api.setKotlinStyleNulls(true);
        }

        // Remove the block comments.
        if (apiText.contains("/*")) {
            apiText = ClassNameKt.stripComments(apiText, false); // line comments are used to stash field constants
        }

        final Tokenizer tokenizer = new Tokenizer(filename, apiText.toCharArray());
        while (true) {
            String token = tokenizer.getToken();
            if (token == null) {
                break;
            }
            // TODO: Accept annotations on packages.
            if ("package".equals(token)) {
                parsePackage(api, tokenizer);
            } else {
                throw new ApiParseException("expected package got " + token, tokenizer);
            }
        }
    }

    private static void parsePackage(TextCodebase api, Tokenizer tokenizer)
        throws ApiParseException {
        String token;
        String name;
        TextPackageItem pkg;

        token = tokenizer.requireToken();

        // Metalava: including annotations in file now
        List<String> annotations = getAnnotations(tokenizer, token);
        TextModifiers modifiers = new TextModifiers(api, DefaultModifierList.PUBLIC, null);
        if (annotations != null) {
            modifiers.addAnnotations(annotations);
        }

        token = tokenizer.getCurrent();

        assertIdent(tokenizer, token);
        name = token;

        // If the same package showed up multiple times, make sure they have the same modifiers.
        // (Packages can't have public/private/etc, but they can have annotations, which are part of ModifierList.)
        // ModifierList doesn't provide equals(), neither does AnnotationItem which ModifilerList contains,
        // so we just use toString() here for equality comparison.
        // However, ModifierList.toString() throws if the owner is not yet set, so we have to instantiate an
        // (owner) TextPackageItem here.
        // If it's a duplicate package, then we'll replace pkg with the existing one in the following if block.

        // TODO: However, currently this parser can't handle annotations on packages, so we will never hit this case.
        // Once the parser supports that, we should add a test case for this too.
        pkg = new TextPackageItem(api, name, modifiers, tokenizer.pos());

        final TextPackageItem existing = api.findPackage(name);
        if (existing != null) {
            if (!pkg.getModifiers().toString().equals(existing.getModifiers().toString())) {
                throw new ApiParseException(String.format(
                    "Contradicting declaration of package %s. Previously seen with modifiers \"%s\", but now with \"%s\"",
                    name, pkg.getModifiers(), modifiers), tokenizer);
            }
            pkg = existing;
        }

        token = tokenizer.requireToken();
        if (!"{".equals(token)) {
            throw new ApiParseException("expected '{' got " + token, tokenizer);
        }
        while (true) {
            token = tokenizer.requireToken();
            if ("}".equals(token)) {
                break;
            } else {
                parseClass(api, pkg, tokenizer, token);
            }
        }
        api.addPackage(pkg);
    }

    private static void parseClass(TextCodebase api, TextPackageItem pkg, Tokenizer tokenizer, String token)
        throws ApiParseException {
        boolean isInterface = false;
        boolean isAnnotation = false;
        boolean isEnum = false;
        String name;
        String qualifiedName;
        String ext = null;
        TextClassItem cl;

        // Metalava: including annotations in file now
        List<String> annotations = getAnnotations(tokenizer, token);
        token = tokenizer.getCurrent();

        TextModifiers modifiers = parseModifiers(api, tokenizer, token, annotations);
        token = tokenizer.getCurrent();

        if ("class".equals(token)) {
            token = tokenizer.requireToken();
        } else if ("interface".equals(token)) {
            isInterface = true;
            modifiers.setAbstract(true);
            token = tokenizer.requireToken();
        } else if ("@interface".equals(token)) {
            // Annotation
            modifiers.setAbstract(true);
            isAnnotation = true;
            token = tokenizer.requireToken();
        } else if ("enum".equals(token)) {
            isEnum = true;
            modifiers.setFinal(true);
            modifiers.setStatic(true);
            ext = JAVA_LANG_ENUM;
            token = tokenizer.requireToken();
        } else {
            throw new ApiParseException("missing class or interface. got: " + token, tokenizer);
        }
        assertIdent(tokenizer, token);
        name = token;
        qualifiedName = qualifiedName(pkg.name(), name);

        if (api.findClass(qualifiedName) != null) {
            throw new ApiParseException("Duplicate class found: " + qualifiedName, tokenizer);
        }

        final TextTypeItem typeInfo = api.obtainTypeFromString(qualifiedName);
        // Simple type info excludes the package name (but includes enclosing class names)

        String rawName = name;
        int variableIndex = rawName.indexOf('<');
        if (variableIndex != -1) {
            rawName = rawName.substring(0, variableIndex);
        }

        token = tokenizer.requireToken();

        cl = new TextClassItem(api, tokenizer.pos(), modifiers, isInterface, isEnum, isAnnotation,
            typeInfo.toErasedTypeString(null), typeInfo.qualifiedTypeName(),
            rawName, annotations);
        cl.setContainingPackage(pkg);
        cl.setTypeInfo(typeInfo);
        cl.setDeprecated(modifiers.isDeprecated());
        if ("extends".equals(token)) {
            token = tokenizer.requireToken();
            assertIdent(tokenizer, token);
            ext = token;
            token = tokenizer.requireToken();
        }
        // Resolve superclass after done parsing
        api.mapClassToSuper(cl, ext);
        if ("implements".equals(token) || "extends".equals(token) ||
                isInterface && ext != null && !token.equals("{")) {
            if (!token.equals("implements") && !token.equals("extends")) {
                api.mapClassToInterface(cl, token);
            }
            while (true) {
                token = tokenizer.requireToken();
                if ("{".equals(token)) {
                    break;
                } else {
                    /// TODO
                    if (!",".equals(token)) {
                        api.mapClassToInterface(cl, token);
                    }
                }
            }
        }
        if (JAVA_LANG_ENUM.equals(ext)) {
            cl.setIsEnum(true);
            // Above we marked all enums as static but for a top level class it's implicit
            if (!cl.fullName().contains(".")) {
                cl.getModifiers().setStatic(false);
            }
        } else if (isAnnotation) {
            api.mapClassToInterface(cl, JAVA_LANG_ANNOTATION);
        } else if (api.implementsInterface(cl, JAVA_LANG_ANNOTATION)) {
            cl.setIsAnnotationType(true);
        }
        if (!"{".equals(token)) {
            throw new ApiParseException("expected {, was " + token, tokenizer);
        }
        token = tokenizer.requireToken();
        while (true) {
            if ("}".equals(token)) {
                break;
            } else if ("ctor".equals(token)) {
                token = tokenizer.requireToken();
                parseConstructor(api, tokenizer, cl, token);
            } else if ("method".equals(token)) {
                token = tokenizer.requireToken();
                parseMethod(api, tokenizer, cl, token);
            } else if ("field".equals(token)) {
                token = tokenizer.requireToken();
                parseField(api, tokenizer, cl, token, false);
            } else if ("enum_constant".equals(token)) {
                token = tokenizer.requireToken();
                parseField(api, tokenizer, cl, token, true);
            } else if ("property".equals(token)) {
                token = tokenizer.requireToken();
                parseProperty(api, tokenizer, cl, token);
            } else {
                throw new ApiParseException("expected ctor, enum_constant, field or method", tokenizer);
            }
            token = tokenizer.requireToken();
        }
        pkg.addClass(cl);
    }

    private static Pair<String, List<String>> processKotlinTypeSuffix(TextCodebase api, String type, List<String> annotations) throws ApiParseException {
        boolean varArgs = false;
        if (type.endsWith("...")) {
            type = type.substring(0, type.length() - 3);
            varArgs = true;
        }
        if (api.getKotlinStyleNulls()) {
            if (type.endsWith("?")) {
                type = type.substring(0, type.length() - 1);
                annotations = mergeAnnotations(annotations, ANDROIDX_NULLABLE);
            } else if (type.endsWith("!")) {
                type = type.substring(0, type.length() - 1);
            } else if (!type.endsWith("!")) {
                if (!TextTypeItem.Companion.isPrimitive(type)) { // Don't add nullness on primitive types like void
                    annotations = mergeAnnotations(annotations, ANDROIDX_NONNULL);
                }
            }
        } else if (type.endsWith("?") || type.endsWith("!")) {
            throw new ApiParseException("Did you forget to supply --input-kotlin-nulls? Found Kotlin-style null type suffix when parser was not configured " +
                "to interpret signature file that way: " + type);
        }
        if (varArgs) {
            type = type + "...";
        }
        return new Pair<>(type, annotations);
    }

    private static List<String> getAnnotations(Tokenizer tokenizer, String token) throws ApiParseException {
        List<String> annotations = null;

        while (true) {
            if (token.startsWith("@")) {
                // Annotation
                String annotation = token;

                // Restore annotations that were shortened on export
                annotation = AnnotationItem.Companion.unshortenAnnotation(annotation);

                token = tokenizer.requireToken();
                if (token.equals("(")) {
                    // Annotation arguments; potentially nested
                    int balance = 0;
                    int start = tokenizer.offset() - 1;
                    while (true) {
                        if (token.equals("(")) {
                            balance++;
                        } else if (token.equals(")")) {
                            balance--;
                            if (balance == 0) {
                                break;
                            }
                        }
                        token = tokenizer.requireToken();
                    }
                    annotation += tokenizer.getStringFromOffset(start);
                    token = tokenizer.requireToken();
                }
                if (annotations == null) {
                    annotations = new ArrayList<>();
                }
                annotations.add(annotation);
            } else {
                break;
            }
        }

        return annotations;
    }

    private static void parseConstructor(TextCodebase api, Tokenizer tokenizer, TextClassItem cl, String token)
        throws ApiParseException {
        String name;
        TextConstructorItem method;

        // Metalava: including annotations in file now
        List<String> annotations = getAnnotations(tokenizer, token);
        token = tokenizer.getCurrent();

        TextModifiers modifiers = parseModifiers(api, tokenizer, token, annotations);
        token = tokenizer.getCurrent();

        assertIdent(tokenizer, token);
        name = token.substring(token.lastIndexOf('.') + 1); // For inner classes, strip outer classes from name
        token = tokenizer.requireToken();
        if (!"(".equals(token)) {
            throw new ApiParseException("expected (", tokenizer);
        }
        method = new TextConstructorItem(api, name, cl, modifiers, cl.asTypeInfo(), tokenizer.pos());
        method.setDeprecated(modifiers.isDeprecated());
        parseParameterList(api, tokenizer, method);
        token = tokenizer.requireToken();
        if ("throws".equals(token)) {
            token = parseThrows(tokenizer, method);
        }
        if (!";".equals(token)) {
            throw new ApiParseException("expected ; found " + token, tokenizer);
        }
        cl.addConstructor(method);
    }

    private static void parseMethod(TextCodebase api, Tokenizer tokenizer, TextClassItem cl, String token)
        throws ApiParseException {
        TextTypeItem returnType;
        String name;
        TextMethodItem method;
        TypeParameterList typeParameterList = TypeParameterList.Companion.getNONE();

        // Metalava: including annotations in file now
        List<String> annotations = getAnnotations(tokenizer, token);
        token = tokenizer.getCurrent();

        TextModifiers modifiers = parseModifiers(api, tokenizer, token, null);
        token = tokenizer.getCurrent();

        if ("<".equals(token)) {
            typeParameterList = parseTypeParameterList(api, tokenizer);
            token = tokenizer.requireToken();
        }
        assertIdent(tokenizer, token);

        Pair<String, List<String>> kotlinTypeSuffix = processKotlinTypeSuffix(api, token, annotations);
        token = kotlinTypeSuffix.getFirst();
        annotations = kotlinTypeSuffix.getSecond();
        modifiers.addAnnotations(annotations);
        String returnTypeString = token;

        token = tokenizer.requireToken();

        if (returnTypeString.contains("@") && (returnTypeString.indexOf('<') == -1 ||
                returnTypeString.indexOf('@') < returnTypeString.indexOf('<'))) {
            returnTypeString += " " + token;
            token = tokenizer.requireToken();
        }
        while (true) {
            if (token.contains("@") && (token.indexOf('<') == -1 ||
                   token.indexOf('@') < token.indexOf('<'))) {
                // Type-use annotations in type; keep accumulating
                returnTypeString += " " + token;
                token = tokenizer.requireToken();
                if (token.startsWith("[")) { // TODO: This isn't general purpose; make requireToken smarter!
                    returnTypeString += " " + token;
                    token = tokenizer.requireToken();
                }
            } else {
                break;
            }
        }

        returnType = api.obtainTypeFromString(returnTypeString, cl, typeParameterList);

        assertIdent(tokenizer, token);
        name = token;
        method = new TextMethodItem(api, name, cl, modifiers, returnType, tokenizer.pos());
        method.setDeprecated(modifiers.isDeprecated());
        if (cl.isInterface() && !modifiers.isDefault() && !modifiers.isStatic()) {
            modifiers.setAbstract(true);
        }
        method.setTypeParameterList(typeParameterList);
        if (typeParameterList instanceof TextTypeParameterList) {
            ((TextTypeParameterList) typeParameterList).setOwner(method);
        }
        token = tokenizer.requireToken();
        if (!"(".equals(token)) {
            throw new ApiParseException("expected (, was " + token, tokenizer);
        }
        parseParameterList(api, tokenizer, method);
        token = tokenizer.requireToken();
        if ("throws".equals(token)) {
            token = parseThrows(tokenizer, method);
        }
        if ("default".equals(token)) {
            token = parseDefault(tokenizer, method);
        }
        if (!";".equals(token)) {
            throw new ApiParseException("expected ; found " + token, tokenizer);
        }
        cl.addMethod(method);
    }

    private static List<String> mergeAnnotations(List<String> annotations, String annotation) {
        if (annotations == null) {
            annotations = new ArrayList<>();
        }
        // Reverse effect of TypeItem.shortenTypes(...)
        String qualifiedName = annotation.indexOf('.') == -1
            ? "@androidx.annotation" + annotation
            : "@" + annotation;

        annotations.add(qualifiedName);
        return annotations;
    }

    private static void parseField(TextCodebase api, Tokenizer tokenizer, TextClassItem cl, String token, boolean isEnum)
        throws ApiParseException {
        List<String> annotations = getAnnotations(tokenizer, token);
        token = tokenizer.getCurrent();

        TextModifiers modifiers = parseModifiers(api, tokenizer, token, null);
        token = tokenizer.getCurrent();
        assertIdent(tokenizer, token);

        Pair<String, List<String>> kotlinTypeSuffix = processKotlinTypeSuffix(api, token, annotations);
        token = kotlinTypeSuffix.getFirst();
        annotations = kotlinTypeSuffix.getSecond();
        modifiers.addAnnotations(annotations);

        String type = token;
        TextTypeItem typeInfo = api.obtainTypeFromString(type);

        token = tokenizer.requireToken();
        assertIdent(tokenizer, token);
        String name = token;
        token = tokenizer.requireToken();
        Object value = null;
        if ("=".equals(token)) {
            token = tokenizer.requireToken(false);
            value = parseValue(type, token);
            token = tokenizer.requireToken();
        }
        if (!";".equals(token)) {
            throw new ApiParseException("expected ; found " + token, tokenizer);
        }
        TextFieldItem field = new TextFieldItem(api, name, cl, modifiers, typeInfo, value, tokenizer.pos());
        field.setDeprecated(modifiers.isDeprecated());
        if (isEnum) {
            cl.addEnumConstant(field);
        } else {
            cl.addField(field);
        }
    }

    private static TextModifiers parseModifiers(
        TextCodebase api,
        Tokenizer tokenizer,
        String token,
        List<String> annotations) throws ApiParseException {

        TextModifiers modifiers = new TextModifiers(api, DefaultModifierList.PACKAGE_PRIVATE, null);

        processModifiers:
        while (true) {
            switch (token) {
                case "public":
                    modifiers.setVisibilityLevel(VisibilityLevel.PUBLIC);
                    token = tokenizer.requireToken();
                    break;
                case "protected":
                    modifiers.setVisibilityLevel(VisibilityLevel.PROTECTED);
                    token = tokenizer.requireToken();
                    break;
                case "private":
                    modifiers.setVisibilityLevel(VisibilityLevel.PRIVATE);
                    token = tokenizer.requireToken();
                    break;
                case "internal":
                    modifiers.setVisibilityLevel(VisibilityLevel.INTERNAL);
                    token = tokenizer.requireToken();
                    break;
                case "static":
                    modifiers.setStatic(true);
                    token = tokenizer.requireToken();
                    break;
                case "final":
                    modifiers.setFinal(true);
                    token = tokenizer.requireToken();
                    break;
                case "deprecated":
                    modifiers.setDeprecated(true);
                    token = tokenizer.requireToken();
                    break;
                case "abstract":
                    modifiers.setAbstract(true);
                    token = tokenizer.requireToken();
                    break;
                case "transient":
                    modifiers.setTransient(true);
                    token = tokenizer.requireToken();
                    break;
                case "volatile":
                    modifiers.setVolatile(true);
                    token = tokenizer.requireToken();
                    break;
                case "sealed":
                    modifiers.setSealed(true);
                    token = tokenizer.requireToken();
                    break;
                case "default":
                    modifiers.setDefault(true);
                    token = tokenizer.requireToken();
                    break;
                case "synchronized":
                    modifiers.setSynchronized(true);
                    token = tokenizer.requireToken();
                    break;
                case "native":
                    modifiers.setNative(true);
                    token = tokenizer.requireToken();
                    break;
                case "strictfp":
                    modifiers.setStrictFp(true);
                    token = tokenizer.requireToken();
                    break;
                case "infix":
                    modifiers.setInfix(true);
                    token = tokenizer.requireToken();
                    break;
                case "operator":
                    modifiers.setOperator(true);
                    token = tokenizer.requireToken();
                    break;
                case "inline":
                    modifiers.setInline(true);
                    token = tokenizer.requireToken();
                    break;
                case "suspend":
                    modifiers.setSuspend(true);
                    token = tokenizer.requireToken();
                    break;
                case "vararg":
                    modifiers.setVarArg(true);
                    token = tokenizer.requireToken();
                    break;
                default:
                    break processModifiers;
            }
        }

        if (annotations != null) {
            modifiers.addAnnotations(annotations);
        }

        return modifiers;
    }

    private static Object parseValue(String type, String val) {
        if (val != null) {
            switch (type) {
                case "boolean":
                    return "true".equals(val) ? Boolean.TRUE : Boolean.FALSE;
                case "byte":
                    return Integer.valueOf(val);
                case "short":
                    return Integer.valueOf(val);
                case "int":
                    return Integer.valueOf(val);
                case "long":
                    return Long.valueOf(val.substring(0, val.length() - 1));
                case "float":
                    switch (val) {
                        case "(1.0f/0.0f)":
                        case "(1.0f / 0.0f)":
                            return Float.POSITIVE_INFINITY;
                        case "(-1.0f/0.0f)":
                        case "(-1.0f / 0.0f)":
                            return Float.NEGATIVE_INFINITY;
                        case "(0.0f/0.0f)":
                        case "(0.0f / 0.0f)":
                            return Float.NaN;
                        default:
                            return Float.valueOf(val);
                    }
                case "double":
                    switch (val) {
                        case "(1.0/0.0)":
                        case "(1.0 / 0.0)":
                            return Double.POSITIVE_INFINITY;
                        case "(-1.0/0.0)":
                        case "(-1.0 / 0.0)":
                            return Double.NEGATIVE_INFINITY;
                        case "(0.0/0.0)":
                        case "(0.0 / 0.0)":
                            return Double.NaN;
                        default:
                            return Double.valueOf(val);
                    }
                case "char":
                    return (char) Integer.parseInt(val);
                case JAVA_LANG_STRING:
                case "String":
                    if ("null".equals(val)) {
                        return null;
                    } else {
                        return javaUnescapeString(val.substring(1, val.length() - 1));
                    }
                case "null":
                    return null;
                default:
                    return val;
            }
        }
        return null;
    }

    private static void parseProperty(TextCodebase api, Tokenizer tokenizer, TextClassItem cl, String token)
        throws ApiParseException {
        String type;
        String name;

        // Metalava: including annotations in file now
        List<String> annotations = getAnnotations(tokenizer, token);
        token = tokenizer.getCurrent();

        TextModifiers modifiers = parseModifiers(api, tokenizer, token, null);
        token = tokenizer.getCurrent();
        assertIdent(tokenizer, token);

        Pair<String, List<String>> kotlinTypeSuffix = processKotlinTypeSuffix(api, token, annotations);
        token = kotlinTypeSuffix.getFirst();
        annotations = kotlinTypeSuffix.getSecond();
        modifiers.addAnnotations(annotations);
        type = token;
        TextTypeItem typeInfo = api.obtainTypeFromString(type);

        token = tokenizer.requireToken();
        assertIdent(tokenizer, token);
        name = token;
        token = tokenizer.requireToken();
        if (!";".equals(token)) {
            throw new ApiParseException("expected ; found " + token, tokenizer);
        }

        TextPropertyItem property = new TextPropertyItem(api, name, cl, modifiers, typeInfo, tokenizer.pos());
        property.setDeprecated(modifiers.isDeprecated());
        cl.addProperty(property);
    }

    private static TypeParameterList parseTypeParameterList(TextCodebase codebase, Tokenizer tokenizer) throws ApiParseException {
        String token;

        int start = tokenizer.offset() - 1;
        int balance = 1;
        while (balance > 0) {
            token = tokenizer.requireToken();
            if (token.equals("<")) {
                balance++;
            } else if (token.equals(">")) {
                balance--;
            }
        }

        String typeParameterList = tokenizer.getStringFromOffset(start);
        if (typeParameterList.isEmpty()) {
            return TypeParameterList.Companion.getNONE();
        } else {
            return TextTypeParameterList.Companion.create(codebase, null, typeParameterList);
        }
    }

    private static void parseParameterList(TextCodebase api, Tokenizer tokenizer, TextMethodItem method)
                                           throws ApiParseException {
        String token = tokenizer.requireToken();
        int index = 0;
        while (true) {
            if (")".equals(token)) {
                return;
            }

            // Each item can be
            // annotations optional-modifiers type-with-use-annotations-and-generics optional-name optional-equals-default-value

            // Metalava: including annotations in file now
            List<String> annotations = getAnnotations(tokenizer, token);
            token = tokenizer.getCurrent();

            TextModifiers modifiers = parseModifiers(api, tokenizer, token, null);
            token = tokenizer.getCurrent();

            // Token should now represent the type
            String type = token;
            token = tokenizer.requireToken();
            if (token.startsWith("@")) {
                // Type use annotations within the type, which broke up the tokenizer;
                // put it back together
                type += " " + token;
                token = tokenizer.requireToken();
                if (token.startsWith("[")) { // TODO: This isn't general purpose; make requireToken smarter!
                    type += " " + token;
                    token = tokenizer.requireToken();
                }
            }

            Pair<String, List<String>> kotlinTypeSuffix = processKotlinTypeSuffix(api, type, annotations);
            String typeString = kotlinTypeSuffix.getFirst();
            annotations = kotlinTypeSuffix.getSecond();
            modifiers.addAnnotations(annotations);
            if (typeString.endsWith("...")) {
                modifiers.setVarArg(true);
            }
            TextTypeItem typeInfo = api.obtainTypeFromString(typeString,
                (TextClassItem) method.containingClass(),
                method.typeParameterList());

            String name;
            String publicName;
            if (isIdent(token) && !token.equals("=")) {
                name = token;
                publicName = name;
                token = tokenizer.requireToken();
            } else {
                name = "arg" + (index + 1);
                publicName = null;
            }

            String defaultValue = TextParameterItemKt.NO_DEFAULT_VALUE;
            if ("=".equals(token)) {
                defaultValue = tokenizer.requireToken(true);
                StringBuilder sb = new StringBuilder(defaultValue);
                if (defaultValue.equals("{")) {
                    int balance = 1;
                    while (balance > 0) {
                        token = tokenizer.requireToken(false, false);
                        sb.append(token);
                        if (token.equals("{")) {
                            balance++;
                        } else if (token.equals("}")) {
                            balance--;
                            if (balance == 0) {
                                break;
                            }
                        }
                    }
                    token = tokenizer.requireToken();
                } else {
                    int balance = defaultValue.equals("(") ? 1 : 0;
                    while (true) {
                        token = tokenizer.requireToken(true, false);
                        if ((token.endsWith(",") || token.endsWith(")")) && balance <= 0) {
                            if (token.length() > 1) {
                                sb.append(token, 0, token.length() - 1);
                                token = Character.toString(token.charAt(token.length() - 1));
                            }
                            break;
                        }
                        sb.append(token);
                        if (token.equals("(")) {
                            balance++;
                        } else if (token.equals(")")) {
                            balance--;
                        }
                    }
                }
                defaultValue = sb.toString();
            }

            if (",".equals(token)) {
                token = tokenizer.requireToken();
            } else if (")".equals(token)) {
            } else {
                throw new ApiParseException("expected , or ), found " + token, tokenizer);
            }

            method.addParameter(new TextParameterItem(api, method, name, publicName, defaultValue, index,
                typeInfo, modifiers, tokenizer.pos()));
            if (modifiers.isVarArg()) {
                method.setVarargs(true);
            }
            index++;
        }
    }

    private static String parseDefault(Tokenizer tokenizer, TextMethodItem method)
        throws ApiParseException {
        StringBuilder sb = new StringBuilder();
        while (true) {
            String token = tokenizer.requireToken();
            if (";".equals(token)) {
                method.setAnnotationDefault(sb.toString());
                return token;
            } else {
                sb.append(token);
            }
        }
    }

    private static String parseThrows(Tokenizer tokenizer, TextMethodItem method)
        throws ApiParseException {
        String token = tokenizer.requireToken();
        boolean comma = true;
        while (true) {
            if (";".equals(token)) {
                return token;
            } else if (",".equals(token)) {
                if (comma) {
                    throw new ApiParseException("Expected exception, got ','", tokenizer);
                }
                comma = true;
            } else {
                if (!comma) {
                    throw new ApiParseException("Expected ',' or ';' got " + token, tokenizer);
                }
                comma = false;
                method.addException(token);
            }
            token = tokenizer.requireToken();
        }
    }

    private static String qualifiedName(String pkg, String className) {
        return pkg + "." + className;
    }

    private static boolean isIdent(String token) {
        return isIdent(token.charAt(0));
    }

    private static void assertIdent(Tokenizer tokenizer, String token) throws ApiParseException {
        if (!isIdent(token.charAt(0))) {
            throw new ApiParseException("Expected identifier: " + token, tokenizer);
        }
    }

    static class Tokenizer {
        final char[] mBuf;
        final String mFilename;
        int mPos;
        int mLine = 1;

        Tokenizer(String filename, char[] buf) {
            mFilename = filename;
            mBuf = buf;
        }

        SourcePositionInfo pos() {
            return new SourcePositionInfo(mFilename, mLine);
        }

        public int getLine() {
            return mLine;
        }

        boolean eatWhitespace() {
            boolean ate = false;
            while (mPos < mBuf.length && isSpace(mBuf[mPos])) {
                if (mBuf[mPos] == '\n') {
                    mLine++;
                }
                mPos++;
                ate = true;
            }
            return ate;
        }

        boolean eatComment() {
            if (mPos + 1 < mBuf.length) {
                if (mBuf[mPos] == '/' && mBuf[mPos + 1] == '/') {
                    mPos += 2;
                    while (mPos < mBuf.length && !isNewline(mBuf[mPos])) {
                        mPos++;
                    }
                    return true;
                }
            }
            return false;
        }

        void eatWhitespaceAndComments() {
            while (eatWhitespace() || eatComment()) {
            }
        }

        String requireToken() throws ApiParseException {
            return requireToken(true);
        }

        String requireToken(boolean parenIsSep) throws ApiParseException {
            return requireToken(parenIsSep, true);
        }

        String requireToken(boolean parenIsSep, boolean eatWhitespace) throws ApiParseException {
            final String token = getToken(parenIsSep, eatWhitespace);
            if (token != null) {
                return token;
            } else {
                throw new ApiParseException("Unexpected end of file", this);
            }
        }

        String getToken() throws ApiParseException {
            return getToken(true);
        }

        int offset() {
            return mPos;
        }

        String getStringFromOffset(int offset) {
            return new String(mBuf, offset, mPos - offset);
        }

        String getToken(boolean parenIsSep) throws ApiParseException {
            return getToken(parenIsSep, true);
        }

        String getCurrent() {
            return mCurrent;
        }

        private String mCurrent = null;

        String getToken(boolean parenIsSep, boolean eatWhitespace) throws ApiParseException {
            if (eatWhitespace) {
                eatWhitespaceAndComments();
            }
            if (mPos >= mBuf.length) {
                return null;
            }
            final int line = mLine;
            final char c = mBuf[mPos];
            final int start = mPos;
            mPos++;
            if (c == '"') {
                final int STATE_BEGIN = 0;
                final int STATE_ESCAPE = 1;
                int state = STATE_BEGIN;
                while (true) {
                    if (mPos >= mBuf.length) {
                        throw new ApiParseException("Unexpected end of file for \" starting at " + line, this);
                    }
                    final char k = mBuf[mPos];
                    if (k == '\n' || k == '\r') {
                        throw new ApiParseException("Unexpected newline for \" starting at " + line +" in " + mFilename, this);
                    }
                    mPos++;
                    switch (state) {
                        case STATE_BEGIN:
                            switch (k) {
                                case '\\':
                                    state = STATE_ESCAPE;
                                    break;
                                case '"':
                                    mCurrent = new String(mBuf, start, mPos - start);
                                    return mCurrent;
                            }
                            break;
                        case STATE_ESCAPE:
                            state = STATE_BEGIN;
                            break;
                    }
                }
            } else if (isSeparator(c, parenIsSep)) {
                mCurrent = Character.toString(c);
                return mCurrent;
            } else {
                int genericDepth = 0;
                do {
                    while (mPos < mBuf.length) {
                        char d = mBuf[mPos];
                        if (isSpace(d) || isSeparator(d, parenIsSep)) {
                            break;
                        } else if (d == '"') {
                            // String literal in token: skip the full thing
                            mPos++;
                            while (mPos < mBuf.length) {
                                if (mBuf[mPos] == '"') {
                                    mPos++;
                                    break;
                                } else if (mBuf[mPos] == '\\') {
                                    mPos++;
                                }
                                mPos++;
                            }
                            continue;
                        }
                        mPos++;
                    }
                    if (mPos < mBuf.length) {
                        if (mBuf[mPos] == '<') {
                            genericDepth++;
                            mPos++;
                        } else if (genericDepth != 0) {
                            if (mBuf[mPos] == '>') {
                                genericDepth--;
                            }
                            mPos++;
                        }
                    }
                } while (mPos < mBuf.length
                    && ((!isSpace(mBuf[mPos]) && !isSeparator(mBuf[mPos], parenIsSep)) || genericDepth != 0));
                if (mPos >= mBuf.length) {
                    throw new ApiParseException("Unexpected end of file for \" starting at " + line, this);
                }
                mCurrent = new String(mBuf, start, mPos - start);
                return mCurrent;
            }
        }

        @Nullable
        public String getFileName() {
            return mFilename;
        }
    }

    private static boolean isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    private static boolean isNewline(char c) {
        return c == '\n' || c == '\r';
    }

    private static boolean isSeparator(char c, boolean parenIsSep) {
        if (parenIsSep) {
            if (c == '(' || c == ')') {
                return true;
            }
        }
        return c == '{' || c == '}' || c == ',' || c == ';' || c == '<' || c == '>';
    }

    private static boolean isIdent(char c) {
        return c != '"' && !isSeparator(c, true);
    }
}
