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

package com.android.cts.releaseparser;

import com.android.cts.releaseparser.ReleaseProto.*;
import com.google.protobuf.TextFormat;

import org.jf.dexlib2.DexFileFactory;
import org.jf.dexlib2.Opcodes;
import org.jf.dexlib2.ReferenceType;
import org.jf.dexlib2.ValueType;
import org.jf.dexlib2.dexbacked.DexBackedClassDef;
import org.jf.dexlib2.dexbacked.DexBackedDexFile;
import org.jf.dexlib2.dexbacked.DexBackedField;
import org.jf.dexlib2.dexbacked.DexBackedMethod;
import org.jf.dexlib2.dexbacked.reference.DexBackedFieldReference;
import org.jf.dexlib2.dexbacked.reference.DexBackedMethodReference;
import org.jf.dexlib2.dexbacked.reference.DexBackedTypeReference;
import org.jf.dexlib2.iface.Annotation;
import org.jf.dexlib2.iface.AnnotationElement;
import org.jf.dexlib2.iface.reference.*;
import org.jf.dexlib2.iface.value.*;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;

public class DexParser extends FileParser {
    private ApiPackage.Builder mExternalApiPackageBuilder;
    private HashMap<String, ApiClass.Builder> mExternalApiClassBuilderMap;
    private ApiPackage.Builder mInternalApiPackageBuilder;
    private HashMap<String, ApiClass.Builder> mInternalApiClassBuilderMap;
    private String mPackageName;
    private boolean mParseInternalApi;

    public DexParser(File file) {
        super(file);
        // default is the file name with out extenion
        mPackageName = getFileName().split("\\.")[0];
    }

    @Override
    public Entry.EntryType getType() {
        return Entry.EntryType.APK;
    }

    public void setPackageName(String name) {
        mPackageName = name;
    }

    public void setParseInternalApi(boolean parseInternalApi) {
        mParseInternalApi = parseInternalApi;
    }

    public ApiPackage getExternalApiPackage() {
        if (mExternalApiPackageBuilder == null) {
            parse();
        }
        return mExternalApiPackageBuilder.build();
    }

    public ApiPackage getInternalApiPackage() {
        if (mInternalApiPackageBuilder == null) {
            parse();
        }
        return mInternalApiPackageBuilder.build();
    }

    private void parse() {
        mExternalApiPackageBuilder = ApiPackage.newBuilder();
        mExternalApiPackageBuilder.setName(mPackageName);
        mExternalApiClassBuilderMap = new HashMap<String, ApiClass.Builder>();
        mInternalApiPackageBuilder = ApiPackage.newBuilder();
        mInternalApiPackageBuilder.setName(mPackageName);
        mInternalApiClassBuilderMap = new HashMap<String, ApiClass.Builder>();
        DexBackedDexFile dexFile = null;

        // Loads a Dex file
        System.out.println("dexFile: " + getFile().getAbsoluteFile());
        try {
            dexFile = DexFileFactory.loadDexFile(getFile().getAbsoluteFile(), Opcodes.getDefault());

            // Iterates through all clesses in the Dex file
            for (DexBackedClassDef classDef : dexFile.getClasses()) {
                // Still need to build the map to filter out internal classes later
                ApiClass.Builder classBuilder =
                        ClassUtils.getApiClassBuilder(
                                mInternalApiClassBuilderMap, classDef.getType());
                if (mParseInternalApi) {
                    classBuilder.setAccessFlags(classDef.getAccessFlags());
                    classBuilder.setSuperClass(
                            ClassUtils.getCanonicalName(classDef.getSuperclass()));
                    classBuilder.addAllInterfaces(
                            classDef.getInterfaces()
                                    .stream()
                                    .map(iType -> ClassUtils.getCanonicalName(iType))
                                    .collect(Collectors.toList()));

                    List<ApiAnnotation> annLst = getAnnotationList(classDef.getAnnotations());
                    if (!annLst.isEmpty()) {
                        classBuilder.addAllAnnotations(annLst);
                    }

                    for (DexBackedField dxField : classDef.getFields()) {
                        ApiField.Builder fieldBuilder = ApiField.newBuilder();
                        fieldBuilder.setName(dxField.getName());
                        fieldBuilder.setType(ClassUtils.getCanonicalName(dxField.getType()));
                        fieldBuilder.setAccessFlags(dxField.getAccessFlags());
                        annLst = getAnnotationList(dxField.getAnnotations());
                        if (!annLst.isEmpty()) {
                            fieldBuilder.addAllAnnotations(annLst);
                        }
                        classBuilder.addFields(fieldBuilder.build());
                    }

                    for (DexBackedMethod dxMethod : classDef.getMethods()) {
                        ApiMethod.Builder methodBuilder = ApiMethod.newBuilder();
                        methodBuilder.setName(dxMethod.getName());
                        methodBuilder.setAccessFlags(dxMethod.getAccessFlags());
                        for (String parameter : dxMethod.getParameterTypes()) {
                            methodBuilder.addParameters(ClassUtils.getCanonicalName(parameter));
                        }
                        methodBuilder.setReturnType(
                                ClassUtils.getCanonicalName(dxMethod.getReturnType()));
                        annLst = getAnnotationList(dxMethod.getAnnotations());
                        if (!annLst.isEmpty()) {
                            methodBuilder.addAllAnnotations(annLst);
                        }
                        classBuilder.addMethods(methodBuilder.build());
                    }
                }
            }

            dexFile.getReferences(ReferenceType.FIELD)
                    .stream()
                    .map(f -> (DexBackedFieldReference) f)
                    .filter(f -> (!mInternalApiClassBuilderMap.containsKey(f.getDefiningClass())))
                    .forEach(f -> processField(f));

            dexFile.getReferences(ReferenceType.METHOD)
                    .stream()
                    .map(m -> (DexBackedMethodReference) m)
                    .filter(m -> (!mInternalApiClassBuilderMap.containsKey(m.getDefiningClass())))
                    .filter(
                            m ->
                                    !(m.getDefiningClass().startsWith("[")
                                            && m.getName().equals("clone")))
                    .forEach(m -> processMethod(m));

            ClassUtils.addAllApiClasses(mExternalApiClassBuilderMap, mExternalApiPackageBuilder);
            if (mParseInternalApi) {
                ClassUtils.addAllApiClasses(
                        mInternalApiClassBuilderMap, mInternalApiPackageBuilder);
            }
        } catch (IOException | DexFileFactory.DexFileNotFoundException ex) {
            String error = "Unable to load dex file: " + getFile().getAbsoluteFile();
            mExternalApiPackageBuilder.setError(error);
            System.err.println(error);
            ex.printStackTrace();
        }
    }

    private List<ApiAnnotation> getAnnotationList(Set<? extends Annotation> annotations) {
        List<ApiAnnotation> apiAnnotationList = new ArrayList<ApiAnnotation>();
        for (Annotation annotation : annotations) {
            ApiAnnotation.Builder apiAnnotationBuilder = ApiAnnotation.newBuilder();
            apiAnnotationBuilder.setType(ClassUtils.getCanonicalName(annotation.getType()));
            Set<? extends AnnotationElement> elements = annotation.getElements();
            for (AnnotationElement ele : elements) {
                Element.Builder elementBuilder = Element.newBuilder();
                elementBuilder.setName(ele.getName());
                elementBuilder.setValue(getEncodedValueString(ele.getValue()));
                apiAnnotationBuilder.addElements(elementBuilder.build());
            }
            apiAnnotationList.add(apiAnnotationBuilder.build());
        }
        return apiAnnotationList;
    }

    private String getEncodedValueString(EncodedValue encodedValue) {
        switch (encodedValue.getValueType()) {
            case ValueType.BYTE:
                return String.format("0x%X", ((ByteEncodedValue) encodedValue).getValue());
            case ValueType.SHORT:
                return String.format("%d", ((ShortEncodedValue) encodedValue).getValue());
            case ValueType.CHAR:
                return String.format("%c", ((CharEncodedValue) encodedValue).getValue());
            case ValueType.INT:
                return String.format("%d", ((IntEncodedValue) encodedValue).getValue());
            case ValueType.LONG:
                return String.format("%d", ((LongEncodedValue) encodedValue).getValue());
            case ValueType.FLOAT:
                return String.format("%f", ((FloatEncodedValue) encodedValue).getValue());
            case ValueType.DOUBLE:
                return String.format("%f", ((DoubleEncodedValue) encodedValue).getValue());
            case ValueType.STRING:
                return ((StringEncodedValue) encodedValue).getValue();
            case ValueType.NULL:
                return "null";
            case ValueType.BOOLEAN:
                return Boolean.toString(((BooleanEncodedValue) encodedValue).getValue());
            case ValueType.TYPE:
                return ClassUtils.getCanonicalName(((TypeEncodedValue) encodedValue).getValue());
            case ValueType.ARRAY:
                ArrayList<String> lst = new ArrayList<String>();
                for (EncodedValue eValue : ((ArrayEncodedValue) encodedValue).getValue()) {
                    lst.add(getEncodedValueString(eValue));
                }
                return String.join(",", lst);
            case ValueType.FIELD:
                return ((FieldReference) ((FieldEncodedValue) encodedValue).getValue()).getName();
            case ValueType.METHOD:
                return ((MethodReference) ((MethodEncodedValue) encodedValue).getValue()).getName();
            case ValueType.ENUM:
                return ((FieldReference) ((EnumEncodedValue) encodedValue).getValue()).getName();
            default:
                getLogger()
                        .log(
                                Level.WARNING,
                                String.format(
                                        "ToDo,Encoded Type,0x%X", encodedValue.getValueType()));
                return String.format("Encoded Type:%x", encodedValue.getValueType());
        }
    }

    private void processField(DexBackedFieldReference f) {
        ApiField.Builder fieldBuilder = ApiField.newBuilder();
        fieldBuilder.setName(f.getName());
        fieldBuilder.setType(ClassUtils.getCanonicalName(f.getType()));
        ApiClass.Builder classBuilder =
                ClassUtils.getApiClassBuilder(mExternalApiClassBuilderMap, f.getDefiningClass());
        classBuilder.addFields(fieldBuilder.build());
    }

    private void processMethod(DexBackedMethodReference m) {
        ApiMethod.Builder methodBuilder = ApiMethod.newBuilder();
        methodBuilder.setName(m.getName());
        for (String parameter : m.getParameterTypes()) {
            methodBuilder.addParameters(ClassUtils.getCanonicalName(parameter));
        }
        methodBuilder.setReturnType(ClassUtils.getCanonicalName(m.getReturnType()));
        ApiClass.Builder classBuilder =
                ClassUtils.getApiClassBuilder(mExternalApiClassBuilderMap, m.getDefiningClass());
        classBuilder.addMethods(methodBuilder.build());
    }

    private boolean isInternal(DexBackedTypeReference t) {
        if (t.getType().length() == 1) {
            // primitive class
            return true;
        } else if (t.getType().charAt(0) == ClassUtils.TYPE_ARRAY) {
            return true;
        }
        return false;
    }

    private String getSignature(DexBackedTypeReference f) {
        return f.getType();
    }

    private String getSignature(DexBackedMethod m) {
        return m.getDefiningClass()
                + "."
                + m.getName()
                + ","
                + String.join(",", m.getParameterTypes())
                + ","
                + m.getReturnType();
    }

    private String getSignature(DexBackedField f) {
        return ClassUtils.getCanonicalName(f.getDefiningClass())
                + "."
                + f.getName()
                + "."
                + f.getType();
    }

    private String getCanonicalName(DexBackedTypeReference f) {
        return ClassUtils.getCanonicalName(f.getType());
    }

    private String getCanonicalName(DexBackedFieldReference f) {
        return ClassUtils.getCanonicalName(f.getDefiningClass())
                + "."
                + f.getName()
                + " : "
                + ClassUtils.getCanonicalName(f.getType());
    }

    private String getCanonicalName(DexBackedMethodReference m) {
        return ClassUtils.getCanonicalName(m.getDefiningClass())
                + "."
                + m.getName()
                + " ("
                + toParametersString(m.getParameterTypes())
                + ")"
                + ClassUtils.getCanonicalName(m.getReturnType());
    }

    private String toParametersString(List<String> pList) {
        return pList.stream()
                .map(p -> ClassUtils.getCanonicalName(p))
                .collect(Collectors.joining(", "));
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar "
                    + DexParser.class.getCanonicalName()
                    + " [-options <parameter>]...\n"
                    + "           to prase APK file Dex data\n"
                    + "Options:\n"
                    + "\t-i PATH\t The file path of the file to be parsed.\n"
                    + "\t-pi \t Parses internal methods and fields too. Output will be large when parsing multiple files in a release.\n"
                    + "\t-of PATH\t The file path of the output file instead of printing to System.out.\n";

    public static void main(String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String fileName = argParser.getParameterElement("i", 0);
            String outputFileName = argParser.getParameterElement("of", 0);
            boolean parseInternalApi = argParser.containsOption("pi");

            String apkFileName = null;
            File apkFile = new File(fileName);
            DexParser aParser = new DexParser(apkFile);
            aParser.setParseInternalApi(parseInternalApi);

            if (outputFileName != null) {
                FileOutputStream txtOutput = new FileOutputStream(outputFileName);
                txtOutput.write(
                        TextFormat.printToString(aParser.getExternalApiPackage())
                                .getBytes(Charset.forName("UTF-8")));
                if (parseInternalApi) {
                    txtOutput.write(
                            TextFormat.printToString(aParser.getInternalApiPackage())
                                    .getBytes(Charset.forName("UTF-8")));
                }
                txtOutput.flush();
                txtOutput.close();
            } else {
                System.out.println(TextFormat.printToString(aParser.getExternalApiPackage()));
                if (parseInternalApi) {
                    System.out.println(TextFormat.printToString(aParser.getInternalApiPackage()));
                }
            }
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    private static Logger getLogger() {
        return Logger.getLogger(DexParser.class.getSimpleName());
    }
}
