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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.TreeMap;

public class DepPrinter {
    private ReleaseContent mRelContent;
    private PrintWriter mPWriter;
    private HashMap<String, Integer> mLibMap;
    private HashMap<String, Integer> mDepPathMap;
    private TreeMap<String, Entry> mTreeEntryMap;
    private ReleaseContent mBRelContent;
    private int mBits;
    private String mTitle;
    private int mCurLevel;

    private static String getTitle(ReleaseContent relContent) {
        return relContent.getName() + relContent.getVersion() + relContent.getBuildNumber();
    }

    public DepPrinter(ReleaseContent relContent) {
        mRelContent = relContent;
        mTitle = getTitle(relContent);
        mBRelContent = null;
        mTreeEntryMap = new TreeMap<String, Entry>(mRelContent.getEntries());
    }

    public void writeDeltaDigraphs(ReleaseContent bRelContent, String dirName) {
        mBRelContent = bRelContent;
        mTitle = mTitle + "_vs_" + getTitle(bRelContent);
        compareEntries(dirName);
    }

    public void compareEntries(String dirName) {
        for (Entry entry : mRelContent.getEntries().values()) {
            if (entry.getType() == Entry.EntryType.EXE) {
                String exeName = entry.getName();
                String fileName = String.format("%s/%s.gv", dirName, exeName);
                writeDigraph(entry, fileName);
            }
        }
    }

    public void writeDeltaDigraph(String exeName, ReleaseContent bRelContent, String fileName) {
        mBRelContent = bRelContent;
        mTitle = mTitle + "_vs_" + getTitle(bRelContent);
        compareEntry(exeName, fileName);
    }

    public void compareEntry(String exeName, String fileName) {
        for (Entry entry : mRelContent.getEntries().values()) {
            if (entry.getName().equals(exeName)) {
                writeDigraph(entry, fileName);
                break;
            }
        }
    }

    public void writeDigraph(Entry entry, String fileName) {
        try {
            String exeName = entry.getName();
            FileWriter fWriter = new FileWriter(fileName);
            mPWriter = new PrintWriter(fWriter);
            mLibMap = new HashMap<String, Integer>();
            mDepPathMap = new HashMap<String, Integer>();
            mBits = entry.getAbiBits();
            // Header
            mPWriter.println("digraph {");
            mPWriter.println(
                    String.format(
                            "a [label=\"%s %dbits\" shape=plaintext]", getNodeName(mTitle), mBits));

            String sourceNode = getNodeName(exeName);
            mPWriter.printf(String.format("%s [label=\"%s\"", sourceNode, exeName));
            checkDelta(entry);
            mPWriter.println("]");

            mCurLevel = 0;
            printDep(entry, sourceNode);

            mPWriter.println("}");
            mPWriter.flush();
            mPWriter.close();
        } catch (IOException e) {
            System.err.println("IOException:" + e.getMessage());
        }
    }

    public void writeRcDeltaDigraphs(ReleaseContent bRelContent, String dirName) {
        mBRelContent = bRelContent;
        mTitle = mTitle + "_vs_" + getTitle(bRelContent);
        compareRcEntries(dirName);
    }

    public void compareRcEntries(String dirName) {
        String fileName = String.format("%s/%s.gv", dirName, "RC-files");
        try {
            FileWriter fWriter = new FileWriter(fileName);
            mPWriter = new PrintWriter(fWriter);
            String rootNode = "root";
            mPWriter.println("digraph {");
            mPWriter.println("rankdir=LR;");
            mPWriter.println("node [shape = box]");
            mPWriter.println(String.format("%s [label=\"%s\"]", rootNode, getNodeName(mTitle)));

            for (Entry entry : mRelContent.getEntries().values()) {
                if (entry.getType() == Entry.EntryType.RC) {
                    // only care if a RC starts Services
                    if (entry.getDependenciesList().size() > 0) {
                        writeRcDigraph(entry, rootNode);
                    }
                }
            }

            mPWriter.println("}");
            mPWriter.flush();
            mPWriter.close();
        } catch (IOException e) {
            System.err.println("IOException:" + e.getMessage());
        }
    }

    public void writeRcDigraph(Entry entry, String rootNode) {
        String rcName = entry.getRelativePath();
        String sourceNode = getNodeName(rcName);
        mPWriter.printf(String.format("%s [label=\"%s\"", sourceNode, rcName));
        checkDelta(entry);
        mPWriter.println("]");

        mPWriter.println(String.format("%s -> %s", rootNode, sourceNode));

        for (Service target : entry.getServicesList()) {
            String targetFile =
                    target.getFile().replace("/system/", "SYSTEM/").replace("/vendor/", "VENDOR/");
            String targetNode = getNodeName(targetFile);
            mPWriter.printf(String.format("%s [label=\"%s\"", targetNode, targetFile));
            Entry targetEntry = mRelContent.getEntries().get(targetFile);
            if (targetEntry != null) {
                checkDelta(targetEntry);
            }
            mPWriter.println("]");

            String depPath = String.format("%s -> %s", sourceNode, targetNode);
            mPWriter.println(depPath);
        }
    }

    private void checkDelta(Entry srcEntry) {
        // compare
        if (mBRelContent != null) {
            // System.err.println(srcEntry.getRelativePath());
            Entry bEntry = mBRelContent.getEntries().get(srcEntry.getRelativePath());
            if (bEntry == null) {
                // New Entry
                mPWriter.printf(" fillcolor=\"gold1\" style=\"filled\"");
            } else {
                if (srcEntry.getContentId().equals(bEntry.getContentId())) {
                    // Same
                    mPWriter.printf(" fillcolor=\"green\" style=\"filled\"");
                } else {
                    // Different
                    mPWriter.printf(" fillcolor=\"red\" style=\"filled\"");
                }
            }
        }
    }

    private String getNodeName(String note) {
        return note.replace(".", "_")
                .replace("@", "")
                .replace("+", "p")
                .replace("-", "_")
                .replace("/", "_");
    }

    private void printDep(Entry srcEntry, String sourceNode) {
        mCurLevel += 1;
        ArrayList<String> allDepList = new ArrayList<>();
        allDepList.addAll(srcEntry.getDependenciesList());
        int depCnt = allDepList.size();
        allDepList.addAll(srcEntry.getDynamicLoadingDependenciesList());
        int no = 0;

        for (String dep : allDepList) {
            boolean goFurther = false;
            String targetNode;
            Entry depEntry = null;
            String filePath = dep;

            // libEGL*.so to VENDOR/lib64/egl/libEGL_adreno.so
            int idx = dep.indexOf("*.so");
            if (idx > -1) {
                if (mBits == 32) {
                    filePath = "VENDOR/lib/egl/" + dep.substring(0, idx);
                } else {
                    filePath = "VENDOR/lib64/egl/" + dep.substring(0, idx);
                }
                depEntry = mTreeEntryMap.tailMap(filePath, false).firstEntry().getValue();
                dep = depEntry.getName();
            }
            targetNode = getNodeName(dep);

            String depPath = String.format("%s -> %s", sourceNode, targetNode);
            if (no < depCnt) {
                depPath = String.format("%s -> %s", sourceNode, targetNode);
            } else {
                // This is Dyanmic Loading
                depPath = String.format("%s -> %s [color=\"blue\"]", sourceNode, targetNode);
            }

            if (mDepPathMap.get(depPath) == null) {
                // Print path once only
                mDepPathMap.put(depPath, 1);
                mPWriter.println(depPath);
            }

            if (depEntry == null) {
                if (dep.startsWith("/system")) {
                    depEntry = mRelContent.getEntries().get(dep.replace("/system/", "SYSTEM/"));
                } else if (dep.startsWith("/vendor")) {
                    depEntry = mRelContent.getEntries().get(dep.replace("/vendor/", "VENDOR/"));
                } else {
                    if (mBits == 32) {
                        filePath = String.format("SYSTEM/lib/%s", dep);
                    } else {
                        filePath = String.format("SYSTEM/lib64/%s", dep);
                    }

                    depEntry = mRelContent.getEntries().get(filePath);

                    if (depEntry == null) {
                        // try Vendor
                        if (mBits == 32) {
                            filePath = String.format("VENDOR/lib/%s", dep);
                        } else {
                            filePath = String.format("VENDOR/lib64/%s", dep);
                        }
                        depEntry = mRelContent.getEntries().get(filePath);
                    }

                    if (depEntry == null) {
                        // try Vendor
                        if (mBits == 32) {
                            filePath = String.format("VENDOR/lib/%s", dep);
                        } else {
                            filePath = String.format("VENDOR/lib64/%s", dep);
                        }
                        depEntry = mRelContent.getEntries().get(filePath);
                    }
                }
            }

            if (depEntry == null && dep.endsWith("libGLES_android.so")) {
                // try Vendor
                if (mBits == 32) {
                    filePath = "SYSTEM/lib/egl/libGLES_android.so";
                } else {
                    filePath = "SYSTEM/lib64/egl/libGLES_android.so";
                }
                depEntry = mRelContent.getEntries().get(filePath);
            }

            if (depEntry != null) {
                if (mLibMap.get(targetNode) == null) {
                    // Print Entry node once only
                    mLibMap.put(targetNode, 1);
                    mPWriter.printf(String.format("%s [label=\"%s\"", targetNode, dep));
                    checkDelta(depEntry);

                    // Try to patch symbolic link to the target file
                    if (depEntry.getType() == Entry.EntryType.SYMBOLIC_LINK) {
                        mPWriter.printf(", shape=diamond");
                        filePath = depEntry.getParentFolder() + "/egl/" + depEntry.getName();
                        Entry sDepEntry = mRelContent.getEntries().get(filePath);
                        if (sDepEntry == null) {
                            System.err.println(
                                    "cannot find a target file for symbolic link: "
                                            + depEntry.getRelativePath());
                        } else {
                            depEntry = sDepEntry;
                        }
                    }
                    mPWriter.println("]");
                    // Only visit once
                    goFurther = true;
                } else {
                    // Record max Level for a target
                    int i = mLibMap.get(targetNode);
                    mLibMap.put(targetNode, Math.max(i, mCurLevel));
                }

                if (goFurther) {
                    printDep(depEntry, targetNode);
                }
            } else {
                System.err.println("cannot find: " + filePath);
            }
            no++;
        }
        mCurLevel -= 1;
    }

    private static final String USAGE_MESSAGE =
            "Usage: java -cp releaseparser.jar com.android.cts.releaseparser.DepPrinter [-options]\n"
                    + "           to compare A B builds dependency for X \n"
                    + "Options:\n"
                    + "\t-a A-Release.pb\t A release Content Protobuf file \n"
                    + "\t-b B-Release.pb\t B release Content Protobuf file \n"
                    + "\t-e Exe Name\t generates the Delta Dependency Digraph for the Execuable \n"
                    + "\t-r \t generates RC file Delta Dependency Digraphs \n"
                    + "\t \t without -e & -t, it will generate all Delta Dependency Digraphs \n";

    /** Get the argument or print out the usage and exit. */
    private static void printUsage() {
        System.out.printf(USAGE_MESSAGE);
        System.exit(1);
    }

    /** Get the argument or print out the usage and exit. */
    private static String getExpectedArg(String[] args, int index) {
        if (index < args.length) {
            return args[index];
        } else {
            printUsage();
            return null; // Never will happen because printUsage will call exit(1)
        }
    }

    public static void main(final String[] args) {
        String aPB = null;
        String bPB = null;
        String exeName = null;
        boolean processRcOnly = false;

        for (int i = 0; i < args.length; i++) {
            if (args[i].startsWith("-")) {
                if ("-a".equals(args[i])) {
                    aPB = getExpectedArg(args, ++i);
                } else if ("-b".equals(args[i])) {
                    bPB = getExpectedArg(args, ++i);
                } else if ("-e".equals(args[i])) {
                    exeName = getExpectedArg(args, ++i);
                } else if ("-r".equals(args[i])) {
                    processRcOnly = true;
                } else {
                    printUsage();
                }
            }
        }
        if (aPB == null || bPB == null) {
            printUsage();
        }

        try {
            ReleaseContent aRelContent = ReleaseContent.parseFrom(new FileInputStream(aPB));
            DepPrinter depPrinter = new DepPrinter(aRelContent);
            ReleaseContent bRelContent = ReleaseContent.parseFrom(new FileInputStream(bPB));
            String dirName =
                    String.format("%s-vs-%s", getTitle(aRelContent), getTitle(bRelContent));
            File dir = new File(dirName);
            dir.mkdir();
            if (processRcOnly) {
                // General RC delta digraphs
                depPrinter.writeRcDeltaDigraphs(bRelContent, dirName);
            } else if (exeName == null) {
                // General all execuable delta digraphs
                depPrinter.writeDeltaDigraphs(bRelContent, dirName);
            } else {
                depPrinter.writeDeltaDigraph(exeName, bRelContent, String.format("%s/%s.gv", dirName, exeName));
            }

        } catch (Exception e) {
            e.printStackTrace();
            System.err.println(e);
        }
    }
}