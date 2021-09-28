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

package android.signature.cts.api;

import android.os.Bundle;
import android.signature.cts.DexApiDocumentParser;
import android.signature.cts.DexField;
import android.signature.cts.DexMember;
import android.signature.cts.DexMemberChecker;
import android.signature.cts.DexMethod;
import android.signature.cts.FailureType;
import android.signature.cts.VirtualPath;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Set;
import java.util.function.Predicate;

/**
 * Checks that it is not possible to access hidden APIs.
 */
public class HiddenApiTest extends AbstractApiTest {

    private String[] hiddenapiFiles;
    private String[] hiddenapiTestFlags;

    @Override
    protected void initializeFromArgs(Bundle instrumentationArgs) {
        hiddenapiFiles = getCommaSeparatedList(instrumentationArgs, "hiddenapi-files");
        hiddenapiTestFlags = getCommaSeparatedList(instrumentationArgs, "hiddenapi-test-flags");
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        DexMemberChecker.init();
    }

    // We have four methods to split up the load, keeping individual test runs small.

    private final static Predicate<DexMember> METHOD_FILTER =
            dexMember -> (dexMember instanceof DexMethod);

    private final static Predicate<DexMember> FIELD_FILTER =
            dexMember -> (dexMember instanceof DexField);

    public void testSignatureMethodsThroughReflection() {
        doTestSignature(METHOD_FILTER,/* reflection= */ true, /* jni= */ false);
    }

    public void testSignatureMethodsThroughJni() {
        doTestSignature(METHOD_FILTER, /* reflection= */ false, /* jni= */ true);
    }

    public void testSignatureFieldsThroughReflection() {
        doTestSignature(FIELD_FILTER, /* reflection= */ true, /* jni= */ false);
    }

    public void testSignatureFieldsThroughJni() {
        doTestSignature(FIELD_FILTER, /* reflection= */ false, /* jni= */ true);
    }

    /**
     * Tests that the device does not expose APIs on the provided lists of
     * DEX signatures.
     *
     * Will check the entire API, and then report the complete list of failures
     */
    private void doTestSignature(Predicate<DexMember> memberFilter, boolean reflection,
            boolean jni) {
        runWithTestResultObserver(resultObserver -> {
            DexMemberChecker.Observer observer = new DexMemberChecker.Observer() {
                @Override
                public void classAccessible(boolean accessible, DexMember member) {
                }

                @Override
                public void fieldAccessibleViaReflection(boolean accessible, DexField field) {
                    if (accessible) {
                        synchronized(resultObserver) {
                            resultObserver.notifyFailure(
                                    FailureType.EXTRA_FIELD,
                                    field.toString(),
                                    "Hidden field accessible through reflection");
                        }
                    }
                }

                @Override
                public void fieldAccessibleViaJni(boolean accessible, DexField field) {
                    if (accessible) {
                        synchronized(resultObserver) {
                            resultObserver.notifyFailure(
                                    FailureType.EXTRA_FIELD,
                                    field.toString(),
                                    "Hidden field accessible through JNI");
                        }
                    }
                }

                @Override
                public void methodAccessibleViaReflection(boolean accessible, DexMethod method) {
                    if (accessible) {
                        synchronized(resultObserver) {
                            resultObserver.notifyFailure(
                                    FailureType.EXTRA_METHOD,
                                    method.toString(),
                                    "Hidden method accessible through reflection");
                        }
                    }
                }

                @Override
                public void methodAccessibleViaJni(boolean accessible, DexMethod method) {
                    if (accessible) {
                        synchronized(resultObserver) {
                            resultObserver.notifyFailure(
                                    FailureType.EXTRA_METHOD,
                                    method.toString(),
                                    "Hidden method accessible through JNI");
                        }
                    }
                }
            };

            for (String apiFile : hiddenapiFiles) {
                VirtualPath.ResourcePath resourcePath =
                        VirtualPath.get(getClass().getClassLoader(), apiFile);
                BufferedReader reader = new BufferedReader(
                        new InputStreamReader(resourcePath.newInputStream()));
                int lineIndex = 1;
                String line = reader.readLine();
                while (line != null) {
                    DexMember dexMember = DexApiDocumentParser.parseLine(line, lineIndex);
                    if (memberFilter.test(dexMember) && shouldTestMember(dexMember)) {
                        DexMemberChecker.checkSingleMember(dexMember, reflection, jni,
                                observer);
                    }
                    line = reader.readLine();
                    lineIndex++;
                }
            }
        });
    }

    protected boolean shouldTestMember(DexMember member) {
        Set<String> flags = member.getHiddenapiFlags();
        for (String testFlag : hiddenapiTestFlags) {
            if (flags.contains(testFlag)) {
                return true;
            }
        }
        return false;
    }

}
