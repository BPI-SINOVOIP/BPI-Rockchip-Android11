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
package android.signature.cts;

import android.signature.cts.JDiffClassDescription.JDiffConstructor;
import android.signature.cts.JDiffClassDescription.JDiffField;
import android.signature.cts.JDiffClassDescription.JDiffMethod;
import android.util.Log;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Modifier;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.Spliterator;
import java.util.function.Consumer;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;
import java.util.zip.GZIPInputStream;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

/**
 * Parser for the XML representation of an API specification.
 */
class XmlApiParser extends ApiParser {

    private static final String TAG_ROOT = "api";

    private static final String TAG_PACKAGE = "package";

    private static final String TAG_CLASS = "class";

    private static final String TAG_INTERFACE = "interface";

    private static final String TAG_IMPLEMENTS = "implements";

    private static final String TAG_CONSTRUCTOR = "constructor";

    private static final String TAG_METHOD = "method";

    private static final String TAG_PARAM = "parameter";

    private static final String TAG_EXCEPTION = "exception";

    private static final String TAG_FIELD = "field";

    private static final String ATTRIBUTE_NAME = "name";

    private static final String ATTRIBUTE_TYPE = "type";

    private static final String ATTRIBUTE_VALUE = "value";

    private static final String ATTRIBUTE_EXTENDS = "extends";

    private static final String ATTRIBUTE_RETURN = "return";

    private static final String MODIFIER_ABSTRACT = "abstract";

    private static final String MODIFIER_FINAL = "final";

    private static final String MODIFIER_NATIVE = "native";

    private static final String MODIFIER_PRIVATE = "private";

    private static final String MODIFIER_PROTECTED = "protected";

    private static final String MODIFIER_PUBLIC = "public";

    private static final String MODIFIER_STATIC = "static";

    private static final String MODIFIER_SYNCHRONIZED = "synchronized";

    private static final String MODIFIER_TRANSIENT = "transient";

    private static final String MODIFIER_VOLATILE = "volatile";

    private static final String MODIFIER_VISIBILITY = "visibility";

    private static final String MODIFIER_ENUM_CONSTANT = "metalava:enumConstant";

    private static final Set<String> KEY_TAG_SET;

    static {
        KEY_TAG_SET = new HashSet<>();
        Collections.addAll(KEY_TAG_SET,
                TAG_PACKAGE,
                TAG_CLASS,
                TAG_INTERFACE,
                TAG_IMPLEMENTS,
                TAG_CONSTRUCTOR,
                TAG_METHOD,
                TAG_PARAM,
                TAG_EXCEPTION,
                TAG_FIELD);
    }

    private final String tag;
    private final boolean gzipped;

    private final XmlPullParserFactory factory;

    XmlApiParser(String tag, boolean gzipped) {
        this.tag = tag;
        this.gzipped = gzipped;
        try {
            factory = XmlPullParserFactory.newInstance();
        } catch (XmlPullParserException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Load field information from xml to memory.
     *
     * @param currentClass
     *         of the class being examined which will be shown in error messages
     * @param parser
     *         The XmlPullParser which carries the xml information.
     * @return the new field
     */
    private static JDiffField loadFieldInfo(
            JDiffClassDescription currentClass, XmlPullParser parser) {
        String fieldName = parser.getAttributeValue(null, ATTRIBUTE_NAME);
        String fieldType = canonicalizeType(parser.getAttributeValue(null, ATTRIBUTE_TYPE));
        int modifier = jdiffModifierToReflectionFormat(currentClass.getClassName(), parser);
        String value = parser.getAttributeValue(null, ATTRIBUTE_VALUE);

        // Canonicalize the expected value to ensure that it is consistent with the values obtained
        // using reflection by ApiComplianceChecker.getFieldValueAsString(...).
        if (value != null) {

            // An unquoted null String value actually means null. It cannot be confused with a
            // String containing the word null as that would be surrounded with double quotes.
            if (value.equals("null")) {
                value = null;
            } else {
                switch (fieldType) {
                    case "java.lang.String":
                        value = unescapeFieldStringValue(value);
                        break;

                    case "char":
                        // A character may be encoded in XML as its numeric value. Convert it to a
                        // string containing the single character.
                        try {
                            char c = (char) Integer.parseInt(value);
                            value = String.valueOf(c);
                        } catch (NumberFormatException e) {
                            // If not, it must be a string "'?'". Extract the second character,
                            // but we need to unescape it.
                            int len = value.length();
                            if (value.charAt(0) == '\'' && value.charAt(len - 1) == '\'') {
                                String sub = value.substring(1, len - 1);
                                value = unescapeFieldStringValue(sub);
                            } else {
                                throw new NumberFormatException(String.format(
                                        "Cannot parse the value of field '%s': invalid number '%s'",
                                        fieldName, value));
                            }
                        }
                        break;

                    case "double":
                        switch (value) {
                            case "(-1.0/0.0)":
                                value = "-Infinity";
                                break;
                            case "(0.0/0.0)":
                                value = "NaN";
                                break;
                            case "(1.0/0.0)":
                                value = "Infinity";
                                break;
                        }
                        break;

                    case "float":
                        switch (value) {
                            case "(-1.0f/0.0f)":
                                value = "-Infinity";
                                break;
                            case "(0.0f/0.0f)":
                                value = "NaN";
                                break;
                            case "(1.0f/0.0f)":
                                value = "Infinity";
                                break;
                            default:
                                // Remove the trailing f.
                                if (value.endsWith("f")) {
                                    value = value.substring(0, value.length() - 1);
                                }
                        }
                        break;

                    case "long":
                        // Remove the trailing L.
                        if (value.endsWith("L")) {
                            value = value.substring(0, value.length() - 1);
                        }
                        break;
                }
            }
        }

        return new JDiffField(fieldName, fieldType, modifier, value);
    }

    /**
     * Load method information from xml to memory.
     *
     * @param className
     *         of the class being examined which will be shown in error messages
     * @param parser
     *         The XmlPullParser which carries the xml information.
     * @return the newly loaded method.
     */
    private static JDiffMethod loadMethodInfo(String className, XmlPullParser parser) {
        String methodName = parser.getAttributeValue(null, ATTRIBUTE_NAME);
        String returnType = parser.getAttributeValue(null, ATTRIBUTE_RETURN);
        int modifier = jdiffModifierToReflectionFormat(className, parser);
        return new JDiffMethod(methodName, modifier, canonicalizeType(returnType));
    }

    /**
     * Load constructor information from xml to memory.
     *
     * @param parser
     *         The XmlPullParser which carries the xml information.
     * @param currentClass
     *         the current class being loaded.
     * @return the new constructor
     */
    private static JDiffConstructor loadConstructorInfo(
            XmlPullParser parser, JDiffClassDescription currentClass) {
        String name = currentClass.getClassName();
        int modifier = jdiffModifierToReflectionFormat(name, parser);
        return new JDiffConstructor(name, modifier);
    }

    /**
     * Load class or interface information to memory.
     *
     * @param parser
     *         The XmlPullParser which carries the xml information.
     * @param isInterface
     *         true if the current class is an interface, otherwise is false.
     * @param pkg
     *         the name of the java package this class can be found in.
     * @return the new class description.
     */
    private static JDiffClassDescription loadClassInfo(
            XmlPullParser parser, boolean isInterface, String pkg) {
        String className = parser.getAttributeValue(null, ATTRIBUTE_NAME);
        JDiffClassDescription currentClass = new JDiffClassDescription(pkg, className);

        currentClass.setType(isInterface ? JDiffClassDescription.JDiffType.INTERFACE :
                JDiffClassDescription.JDiffType.CLASS);

        String superClass = stripGenericsArgs(parser.getAttributeValue(null, ATTRIBUTE_EXTENDS));
        int modifiers = jdiffModifierToReflectionFormat(className, parser);
        if (isInterface) {
            if (superClass != null) {
                currentClass.addImplInterface(superClass);
            }
        } else {
            if ("java.lang.annotation.Annotation".equals(superClass)) {
                // ApiComplianceChecker expects "java.lang.annotation.Annotation" to be in
                // the "impl interfaces".
                currentClass.addImplInterface(superClass);
            } else {
                currentClass.setExtendsClass(superClass);
            }
        }
        currentClass.setModifier(modifiers);
        return currentClass;
    }

    /**
     * Transfer string modifier to int one.
     *
     * @param name
     *         of the class/method/field being examined which will be shown in error messages
     * @param parser
     *         XML resource parser
     * @return converted modifier
     */
    private static int jdiffModifierToReflectionFormat(String name, XmlPullParser parser) {
        int modifier = 0;
        for (int i = 0; i < parser.getAttributeCount(); i++) {
            modifier |= modifierDescriptionToReflectedType(name, parser.getAttributeName(i),
                    parser.getAttributeValue(i));
        }
        return modifier;
    }

    /**
     * Convert string modifier to int modifier.
     *
     * @param name
     *         of the class/method/field being examined which will be shown in error messages
     * @param key
     *         modifier name
     * @param value
     *         modifier value
     * @return converted modifier value
     */
    private static int modifierDescriptionToReflectedType(String name, String key, String value) {
        switch (key) {
            case MODIFIER_ABSTRACT:
                return value.equals("true") ? Modifier.ABSTRACT : 0;
            case MODIFIER_FINAL:
                return value.equals("true") ? Modifier.FINAL : 0;
            case MODIFIER_NATIVE:
                return value.equals("true") ? Modifier.NATIVE : 0;
            case MODIFIER_STATIC:
                return value.equals("true") ? Modifier.STATIC : 0;
            case MODIFIER_SYNCHRONIZED:
                return value.equals("true") ? Modifier.SYNCHRONIZED : 0;
            case MODIFIER_TRANSIENT:
                return value.equals("true") ? Modifier.TRANSIENT : 0;
            case MODIFIER_VOLATILE:
                return value.equals("true") ? Modifier.VOLATILE : 0;
            case MODIFIER_VISIBILITY:
                switch (value) {
                    case MODIFIER_PRIVATE:
                        throw new RuntimeException("Private visibility found in API spec: " + name);
                    case MODIFIER_PROTECTED:
                        return Modifier.PROTECTED;
                    case MODIFIER_PUBLIC:
                        return Modifier.PUBLIC;
                    case "":
                        // If the visibility is "", it means it has no modifier.
                        // which is package private. We should return 0 for this modifier.
                        return 0;
                    default:
                        throw new RuntimeException("Unknown modifier found in API spec: " + value);
                }
            case MODIFIER_ENUM_CONSTANT:
                return value.equals("true") ? ApiComplianceChecker.FIELD_MODIFIER_ENUM_VALUE : 0;
        }
        return 0;
    }

    @Override
    public Stream<JDiffClassDescription> parseAsStream(VirtualPath path) {
        XmlPullParser parser;
        try {
            parser = factory.newPullParser();
            InputStream input = path.newInputStream();
            if (gzipped) {
                input = new GZIPInputStream(input);
            }
            parser.setInput(input, null);
            return StreamSupport
                    .stream(new ClassDescriptionSpliterator(parser), false);
        } catch (XmlPullParserException | IOException e) {
            throw new RuntimeException("Could not parse " + path, e);
        }
    }

    private static String stripGenericsArgs(String typeName) {
        return typeName == null ? null : typeName.replaceFirst("<.*", "");
    }

    private class ClassDescriptionSpliterator implements Spliterator<JDiffClassDescription> {

        private final XmlPullParser parser;

        JDiffClassDescription currentClass = null;

        String currentPackage = "";

        JDiffMethod currentMethod = null;

        ClassDescriptionSpliterator(XmlPullParser parser)
                throws IOException, XmlPullParserException {
            this.parser = parser;
            logd(String.format("Name: %s", parser.getName()));
            logd(String.format("Text: %s", parser.getText()));
            logd(String.format("Namespace: %s", parser.getNamespace()));
            logd(String.format("Line Number: %s", parser.getLineNumber()));
            logd(String.format("Column Number: %s", parser.getColumnNumber()));
            logd(String.format("Position Description: %s", parser.getPositionDescription()));
            beginDocument(parser);
        }

        @Override
        public boolean tryAdvance(Consumer<? super JDiffClassDescription> action) {
            JDiffClassDescription classDescription;
            try {
                classDescription = next();
            } catch (IOException | XmlPullParserException e) {
                throw new RuntimeException(e);
            }

            if (classDescription == null) {
                return false;
            }
            action.accept(classDescription);
            return true;
        }

        @Override
        public Spliterator<JDiffClassDescription> trySplit() {
            return null;
        }

        @Override
        public long estimateSize() {
            return Long.MAX_VALUE;
        }

        @Override
        public int characteristics() {
            return ORDERED | DISTINCT | NONNULL | IMMUTABLE;
        }

        private void beginDocument(XmlPullParser parser)
                throws XmlPullParserException, IOException {
            int type;
            do {
                type = parser.next();
            } while (type != XmlPullParser.START_TAG && type != XmlPullParser.END_DOCUMENT);

            if (type != XmlPullParser.START_TAG) {
                throw new XmlPullParserException("No start tag found");
            }

            if (!parser.getName().equals(TAG_ROOT)) {
                throw new XmlPullParserException("Unexpected start tag: found " + parser.getName() +
                        ", expected " + TAG_ROOT);
            }
        }

        private JDiffClassDescription next() throws IOException, XmlPullParserException {
            int type;
            while (true) {
                do {
                    type = parser.next();
                } while (type != XmlPullParser.START_TAG && type != XmlPullParser.END_DOCUMENT
                        && type != XmlPullParser.END_TAG);

                if (type == XmlPullParser.END_DOCUMENT) {
                    logd("Reached end of document");
                    break;
                }

                String tagname = parser.getName();
                if (type == XmlPullParser.END_TAG) {
                    if (TAG_CLASS.equals(tagname) || TAG_INTERFACE.equals(tagname)) {
                        logd("Reached end of class: " + currentClass);
                        return currentClass;
                    } else if (TAG_PACKAGE.equals(tagname)) {
                        currentPackage = "";
                    }
                    continue;
                }

                if (!KEY_TAG_SET.contains(tagname)) {
                    continue;
                }

                switch (tagname) {
                    case TAG_PACKAGE:
                        currentPackage = parser.getAttributeValue(null, ATTRIBUTE_NAME);
                        break;

                    case TAG_CLASS:
                        currentClass = loadClassInfo(parser, false, currentPackage);
                        break;

                    case TAG_INTERFACE:
                        currentClass = loadClassInfo(parser, true, currentPackage);
                        break;

                    case TAG_IMPLEMENTS:
                        currentClass.addImplInterface(stripGenericsArgs(
                                parser.getAttributeValue(null, ATTRIBUTE_NAME)));
                        break;

                    case TAG_CONSTRUCTOR:
                        JDiffConstructor constructor =
                                loadConstructorInfo(parser, currentClass);
                        currentClass.addConstructor(constructor);
                        currentMethod = constructor;
                        break;

                    case TAG_METHOD:
                        currentMethod = loadMethodInfo(currentClass.getClassName(), parser);
                        currentClass.addMethod(currentMethod);
                        break;

                    case TAG_PARAM:
                        String paramType = parser.getAttributeValue(null, ATTRIBUTE_TYPE);
                        currentMethod.addParam(canonicalizeType(paramType));
                        break;

                    case TAG_EXCEPTION:
                        currentMethod.addException(parser.getAttributeValue(null, ATTRIBUTE_TYPE));
                        break;

                    case TAG_FIELD:
                        JDiffField field = loadFieldInfo(currentClass, parser);
                        currentClass.addField(field);
                        break;

                    default:
                        throw new RuntimeException("unknown tag exception:" + tagname);
                }

                if (currentPackage != null) {
                    logd(String.format("currentPackage: %s", currentPackage));
                }
                if (currentClass != null) {
                    logd(String.format("currentClass: %s", currentClass.toSignatureString()));
                }
                if (currentMethod != null) {
                    logd(String.format("currentMethod: %s", currentMethod.toSignatureString()));
                }
            }

            return null;
        }
    }

    private void logd(String msg) {
        Log.d(tag, msg);
    }

    // This unescapes the string format used by doclava and so needs to be kept in sync with any
    // changes made to that format.
    private static String unescapeFieldStringValue(String str) {
        // Skip over leading and trailing ".
        int start = 0;
        if (str.charAt(start) == '"') {
            ++start;
        }
        int end = str.length();
        if (str.charAt(end - 1) == '"') {
            --end;
        }

        // If there's no special encoding strings in the string then just return it without the
        // leading and trailing "s.
        if (str.indexOf('\\') == -1) {
            return str.substring(start, end);
        }

        final StringBuilder buf = new StringBuilder(str.length());
        char escaped = 0;
        final int START = 0;
        final int CHAR1 = 1;
        final int CHAR2 = 2;
        final int CHAR3 = 3;
        final int CHAR4 = 4;
        final int ESCAPE = 5;
        int state = START;

        for (int i = start; i < end; i++) {
            final char c = str.charAt(i);
            switch (state) {
                case START:
                    if (c == '\\') {
                        state = ESCAPE;
                    } else {
                        buf.append(c);
                    }
                    break;
                case ESCAPE:
                    switch (c) {
                        case '\\':
                            buf.append('\\');
                            state = START;
                            break;
                        case 't':
                            buf.append('\t');
                            state = START;
                            break;
                        case 'b':
                            buf.append('\b');
                            state = START;
                            break;
                        case 'r':
                            buf.append('\r');
                            state = START;
                            break;
                        case 'n':
                            buf.append('\n');
                            state = START;
                            break;
                        case 'f':
                            buf.append('\f');
                            state = START;
                            break;
                        case '\'':
                            buf.append('\'');
                            state = START;
                            break;
                        case '\"':
                            buf.append('\"');
                            state = START;
                            break;
                        case 'u':
                            state = CHAR1;
                            escaped = 0;
                            break;
                    }
                    break;
                case CHAR1:
                case CHAR2:
                case CHAR3:
                case CHAR4:
                    escaped <<= 4;
                    if (c >= '0' && c <= '9') {
                        escaped |= c - '0';
                    } else if (c >= 'a' && c <= 'f') {
                        escaped |= 10 + (c - 'a');
                    } else if (c >= 'A' && c <= 'F') {
                        escaped |= 10 + (c - 'A');
                    } else {
                        throw new RuntimeException(
                                "bad escape sequence: '" + c + "' at pos " + i + " in: \""
                                        + str + "\"");
                    }
                    if (state == CHAR4) {
                        buf.append(escaped);
                        state = START;
                    } else {
                        state++;
                    }
                    break;
            }
        }
        if (state != START) {
            throw new RuntimeException("unfinished escape sequence: " + str);
        }
        return buf.toString();
    }

    /**
     * Canonicalize a possibly generic type.
     */
    private static String canonicalizeType(String type) {
        // Remove trailing spaces after commas.
        return type.replace(", ", ",");
    }
}
