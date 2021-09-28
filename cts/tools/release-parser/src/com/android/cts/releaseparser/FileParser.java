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

import com.android.compatibility.common.util.ReadElf;
import com.android.cts.releaseparser.ReleaseProto.*;
import com.google.protobuf.TextFormat;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Base64;
import java.util.List;

public class FileParser {
    private static final String NO_ID = "";
    protected static final int READ_BLOCK_SIZE = 1024;

    // Target File Extensions
    public static final String APK_EXT_TAG = ".apk";
    public static final String JAR_EXT_TAG = ".jar";
    public static final String DEX_EXT_TAG = ".dex";
    public static final String ODEX_EXT_TAG = ".odex";
    public static final String VDEX_EXT_TAG = ".vdex";
    public static final String ART_EXT_TAG = ".art";
    public static final String OAT_EXT_TAG = ".oat";
    public static final String SO_EXT_TAG = ".so";
    public static final String BUILD_PROP_EXT_TAG = "build.prop";
    public static final String RC_EXT_TAG = ".rc";
    public static final String XML_EXT_TAG = ".xml";
    public static final String IMG_EXT_TAG = ".img";
    public static final String TEST_SUITE_TRADEFED_TAG = "-tradefed.jar";
    public static final String CONFIG_EXT_TAG = ".config";
    public static final String ANDROID_MANIFEST_TAG = "AndroidManifest.xml";
    // Code Id format: [0xchecksum1] [...
    public static final String CODE_ID_FORMAT = "%x ";

    protected File mFile;
    protected String mContentId;
    protected String mCodeId;
    protected Entry.Builder mFileEntryBuilder;

    public static FileParser getParser(File file) {
        String fName = file.getName();
        // Starts with SymbolicLink
        if (isSymbolicLink(file)) {
            return new SymbolicLinkParser(file);
        } else if (fName.endsWith(APK_EXT_TAG)) {
            return new ApkParser(file);
        } else if (fName.endsWith(CONFIG_EXT_TAG)) {
            return new TestModuleConfigParser(file);
        } else if (fName.endsWith(TEST_SUITE_TRADEFED_TAG)) {
            return new TestSuiteTradefedParser(file);
        } else if (fName.endsWith(JAR_EXT_TAG)) {
            // keeps this after TEST_SUITE_TRADEFED_TAG to avoid missing it
            return new JarParser(file);
        } else if (fName.endsWith(SO_EXT_TAG)) {
            return new SoParser(file);
        } else if (fName.endsWith(ART_EXT_TAG)) {
            return new ArtParser(file);
        } else if (fName.endsWith(OAT_EXT_TAG)) {
            return new OatParser(file);
        } else if (fName.endsWith(ODEX_EXT_TAG)) {
            return new OdexParser(file);
        } else if (fName.endsWith(VDEX_EXT_TAG)) {
            return new VdexParser(file);
        } else if (fName.endsWith(BUILD_PROP_EXT_TAG)) {
            // ToDo prop.default & etc in system/core/init/property_service.cpp
            return new BuildPropParser(file);
        } else if (fName.endsWith(RC_EXT_TAG)) {
            return new RcParser(file);
        } else if (fName.endsWith(XML_EXT_TAG)) {
            return new XmlParser(file);
        } else if (fName.endsWith(IMG_EXT_TAG)) {
            return new ImgParser(file);
        } else if (ReadElf.isElf(file)) {
            // keeps this in the end as no Exe Ext name
            return new ExeParser(file);
        } else {
            // Common File Parser
            return new FileParser(file);
        }
    }

    FileParser(File file) {
        mFile = file;
        mCodeId = NO_ID;
        mContentId = NO_ID;
        mFileEntryBuilder = null;
    }

    public File getFile() {
        return mFile;
    }

    public String getFileName() {
        return mFile.getName();
    }

    public Entry.Builder getFileEntryBuilder() {
        if (mFileEntryBuilder == null) {
            parse();
        }
        return mFileEntryBuilder;
    }

    public Entry.EntryType getType() {
        return Entry.EntryType.FILE;
    }

    public String getFileContentId() {
        if (NO_ID.equals(mContentId)) {
            try {
                MessageDigest md = MessageDigest.getInstance("SHA-256");
                FileInputStream fis = new FileInputStream(mFile);
                byte[] dataBytes = new byte[READ_BLOCK_SIZE];
                int nread = 0;
                while ((nread = fis.read(dataBytes)) != -1) {
                    md.update(dataBytes, 0, nread);
                }
                // Converts to Base64 String
                mContentId = Base64.getEncoder().encodeToString(md.digest());
            } catch (IOException e) {
                System.err.println("IOException:" + e.getMessage());
            } catch (NoSuchAlgorithmException e) {
                System.err.println("NoSuchAlgorithmException:" + e.getMessage());
            }
        }
        return mContentId;
    }

    public int getAbiBits() {
        return 0;
    }

    public String getAbiArchitecture() {
        return NO_ID;
    }

    public String getCodeId() {
        return mCodeId;
    }

    public List<String> getDependencies() {
        return null;
    }

    public List<String> getDynamicLoadingDependencies() {
        return null;
    }

    /** For subclass parser to add file type specific info into the entry. */
    public void setAdditionalInfo() {}

    private static boolean isSymbolicLink(File f) {
        // Assumes 0b files are Symbolic Link
        return (f.length() == 0);
    }

    public static int getIntLittleEndian(byte[] byteArray, int start) {
        int answer = 0;
        // Assume Little-Endian. ToDo: if to care about big-endian
        for (int i = start + 3; i >= start; --i) {
            answer = (answer << 8) | (byteArray[i] & 0xff);
        }
        return answer;
    }

    public static void writeTextFormatMessage(String outputFileName, Entry fileEntry)
            throws IOException {
        if (outputFileName != null) {
            FileOutputStream txtOutput = new FileOutputStream(outputFileName);
            txtOutput.write(TextFormat.printToString(fileEntry).getBytes(Charset.forName("UTF-8")));
            txtOutput.flush();
            txtOutput.close();
        } else {
            System.out.println(TextFormat.printToString(fileEntry));
        }
    }

    private void parse() {
        mFileEntryBuilder = Entry.newBuilder();
        mFileEntryBuilder.setName(getFileName());
        mFileEntryBuilder.setSize(getFile().length());
        mFileEntryBuilder.setContentId(getFileContentId());
        mFileEntryBuilder.setCodeId(getCodeId());
        mFileEntryBuilder.setType(getType());
        setAdditionalInfo();
    }
}
