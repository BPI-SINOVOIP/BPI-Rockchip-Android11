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
import com.android.json.stream.NewlineDelimitedJsonWriter;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.Collection;
import java.util.Map;

public class JsonPrinter {
    private String mOutputFilePathPrefix;
    private ReleaseContent mRelContent;
    private NewlineDelimitedJsonWriter mJsonWriter;

    public JsonPrinter(ReleaseContent relContent, String outputFilePathPrefix) {
        mRelContent = relContent;
        mOutputFilePathPrefix = outputFilePathPrefix;
    }

    public void write() {
        try {
            open(mOutputFilePathPrefix + "-ReleaseContent.json");
            writeReleaseContent();
            close();

            open(mOutputFilePathPrefix + "-AppInfo.json");
            writeAppInfo();
            close();
            open(mOutputFilePathPrefix + "-ApiDep.json");
            writeApiDep(mRelContent.getEntries().values());
            close();
        } catch (IOException ex) {
            System.err.println("Unable to write json files: " + mOutputFilePathPrefix);
            ex.printStackTrace();
        }
    }

    private void writeReleaseContent() throws IOException {
        mJsonWriter.beginObject();
        writeReleaseIDs();
        mJsonWriter.name("name").value(mRelContent.getName());
        mJsonWriter.name("version").value(mRelContent.getVersion());
        mJsonWriter.name("build_number").value(mRelContent.getBuildNumber());
        mJsonWriter.name("fullname").value(mRelContent.getFullname());
        mJsonWriter.name("size").value(mRelContent.getSize());
        mJsonWriter.name("release_type").value(mRelContent.getReleaseType().toString());
        mJsonWriter.name("test_suite_tradefed").value(mRelContent.getTestSuiteTradefed());
        mJsonWriter.name("target_arch").value(mRelContent.getTargetArch());
        writeProperties(mRelContent.getProperties());
        writeTargetFileInfo(mRelContent.getEntries().values());
        mJsonWriter.endObject();
        mJsonWriter.newlineDelimited();
    }

    private void writeReleaseIDs() throws IOException {
        mJsonWriter.name("release_id").value(mRelContent.getReleaseId());
        mJsonWriter.name("content_id").value(mRelContent.getContentId());
    }

    private void writeProperties(Map<String, String> pMap) throws IOException {
        mJsonWriter.name("properties");
        mJsonWriter.beginArray();
        for (Map.Entry<String, String> pSet : pMap.entrySet()) {
            mJsonWriter.beginObject();
            mJsonWriter.name("key").value(pSet.getKey());
            mJsonWriter.name("value").value(pSet.getValue());
            mJsonWriter.endObject();
        }
        mJsonWriter.endArray();
    }

    private void writeTargetFileInfo(Collection<Entry> entries) throws IOException {
        mJsonWriter.name("files");
        mJsonWriter.beginArray();
        for (Entry entry : entries) {
            mJsonWriter.beginObject();
            mJsonWriter.name("name").value(entry.getName());
            mJsonWriter.name("type").value(entry.getType().toString());
            mJsonWriter.name("size").value(entry.getSize());
            mJsonWriter.name("content_id").value(entry.getContentId());
            mJsonWriter.name("code_id").value(entry.getCodeId());
            mJsonWriter.name("abi_architecture").value(entry.getAbiArchitecture());
            mJsonWriter.name("abi_bits").value(entry.getAbiBits());
            mJsonWriter.name("parent_folder").value(entry.getParentFolder());
            mJsonWriter.name("relative_path").value(entry.getRelativePath());
            writeStringCollection("dependencies", entry.getDependenciesList());
            writeStringCollection(
                    "dynamic_loading_dependencies", entry.getDynamicLoadingDependenciesList());
            mJsonWriter.endObject();
        }
        mJsonWriter.endArray();
    }

    private void writeAppInfo() throws IOException {
        mJsonWriter.beginObject();
        writeReleaseIDs();

        mJsonWriter.name("apps");
        mJsonWriter.beginArray();
        Collection<Entry> entries = mRelContent.getEntries().values();
        for (Entry entry : entries) {
            if (entry.getType() == Entry.EntryType.APK) {
                AppInfo appInfo = entry.getAppInfo();
                mJsonWriter.beginObject();
                mJsonWriter.name("name").value(entry.getName());
                mJsonWriter.name("content_id").value(entry.getContentId());
                // from AppInfo Message
                mJsonWriter.name("package_name").value(appInfo.getPackageName());
                mJsonWriter.name("version_code").value(appInfo.getVersionCode());
                mJsonWriter.name("version_name").value(appInfo.getVersionName());
                mJsonWriter.name("sdk_version").value(appInfo.getSdkVersion());
                mJsonWriter.name("target_sdk_version").value(appInfo.getTargetSdkVersion());
                writeFeatureCollection("uses_features", appInfo.getUsesFeaturesList());
                writeLibraryCollection("uses_libraries", appInfo.getUsesLibrariesList());
                writeStringCollection("native_code", appInfo.getNativeCodeList());
                writeStringCollection("uses_permissions", appInfo.getUsesPermissionsList());
                writeStringCollection("activities", appInfo.getActivitiesList());
                writeStringCollection("services", appInfo.getServicesList());
                writeStringCollection("provideries", appInfo.getProvidersList());
                writeProperties(appInfo.getProperties());
                writePackageFileContent("package_file_content", appInfo.getPackageFileContent());
                mJsonWriter.name("package_signature").value(appInfo.getPackageSignature());
                mJsonWriter.endObject();
            }
        }
        mJsonWriter.endArray();
        mJsonWriter.endObject();
        mJsonWriter.newlineDelimited();
    }

    private void writeFeatureCollection(String name, Collection<UsesFeature> features)
            throws IOException {
        mJsonWriter.name(name);
        mJsonWriter.beginArray();
        for (UsesFeature feature : features) {
            mJsonWriter.beginObject();
            mJsonWriter.name("name").value(feature.getName());
            mJsonWriter.name("required").value(feature.getRequired());
            mJsonWriter.endObject();
        }
        mJsonWriter.endArray();
    }

    private void writeLibraryCollection(String name, Collection<UsesLibrary> libraries)
            throws IOException {
        mJsonWriter.name(name);
        mJsonWriter.beginArray();
        for (UsesLibrary library : libraries) {
            mJsonWriter.beginObject();
            mJsonWriter.name("name").value(library.getName());
            mJsonWriter.name("required").value(library.getRequired());
            mJsonWriter.endObject();
        }
        mJsonWriter.endArray();
    }

    private void writeStringCollection(String name, Collection<String> strings) throws IOException {
        mJsonWriter.name(name);
        mJsonWriter.beginArray();
        for (String str : strings) {
            mJsonWriter.value(str);
        }
        mJsonWriter.endArray();
    }

    private void writePackageFileContent(String name, PackageFileContent packageFileContent)
            throws IOException {
        mJsonWriter.name(name);
        mJsonWriter.beginArray();

        Collection<Entry> entries = packageFileContent.getEntries().values();
        for (Entry entry : entries) {
            mJsonWriter.beginObject();
            mJsonWriter.name("name").value(entry.getName());
            mJsonWriter.name("size").value(entry.getSize());
            mJsonWriter.name("content_id").value(entry.getContentId());
            mJsonWriter.endObject();
        }
        mJsonWriter.endArray();
    }

    private void writeApiDep(Collection<Entry> entries) throws IOException {
        for (Entry entry : entries) {
            if (entry.getType() == Entry.EntryType.APK) {
                writeApiPkg(entry);
            }
        }
    }

    private void writeApiPkg(Entry entry) throws IOException {
        for (ApiPackage pkg : entry.getAppInfo().getExternalApiPackagesList()) {
            for (ApiClass clazz : pkg.getClassesList()) {
                for (ApiMethod method : clazz.getMethodsList()) {
                    mJsonWriter.beginObject();
                    mJsonWriter.name("content_id").value(entry.getContentId());
                    mJsonWriter.name("api_signature").value(getApiSignature(pkg, clazz, method));
                    mJsonWriter.name("class").value(clazz.getName());
                    mJsonWriter.name("method").value(method.getName());
                    mJsonWriter.name("file_name").value(entry.getName());
                    mJsonWriter.name("release_id").value(ReleaseParser.getReleaseId(mRelContent));
                    mJsonWriter.endObject();
                    mJsonWriter.newlineDelimited();
                }
            }
        }
    }

    private String getApiSignature(ApiPackage pkg, ApiClass clazz, ApiMethod method) {
        StringBuilder signatureBuilder = new StringBuilder();
        signatureBuilder.append(method.getReturnType());
        signatureBuilder.append(" ");
        signatureBuilder.append(clazz.getName());
        signatureBuilder.append(".");
        signatureBuilder.append(method.getName());
        signatureBuilder.append("(");
        signatureBuilder.append(String.join(",", method.getParametersList()));
        signatureBuilder.append(")");
        return signatureBuilder.toString();
    }

    private void open(String fileName) throws IOException {
        File jsonFile;

        jsonFile = new File(fileName);
        FileOutputStream fOutStrem = new FileOutputStream(jsonFile);
        mJsonWriter =
                new NewlineDelimitedJsonWriter(
                        new OutputStreamWriter(fOutStrem, StandardCharsets.UTF_8));
    }

    private void close() throws IOException {
        mJsonWriter.flush();
        mJsonWriter.close();
    }
}
