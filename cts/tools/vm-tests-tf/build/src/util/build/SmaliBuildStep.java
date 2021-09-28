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

package util.build;

import org.jf.smali.Smali;
import org.jf.smali.SmaliOptions;

import java.io.File;
import java.util.List;

/**
 * BuildStep that invokes the Smali Java API to
 * assemble test cases written in Smali.
 */
class SmaliBuildStep extends BuildStep {

    List<String> inputFiles;

    SmaliBuildStep(List<String> inputFiles, File outputFile) {
        super(outputFile);
        this.inputFiles = inputFiles;
    }

    @Override
    boolean build() {
        SmaliOptions options = new SmaliOptions();
        options.verboseErrors = true;
        options.outputDexFile = outputFile.fileName.getAbsolutePath();
        try {
            File destDir = outputFile.folder;
            if (!destDir.exists()) {
                destDir.mkdirs();
            }
            return Smali.assemble(options, inputFiles);
        } catch(Exception e) {
             if(BuildDalvikSuite.DEBUG)
                 e.printStackTrace();
             System.err.println("Exception <" + e.getClass().getName() + ">" + e.getMessage());
             return false;
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (super.equals(obj)) {
            SmaliBuildStep other = (SmaliBuildStep) obj;
            return inputFiles.equals(other.inputFiles)
                    && outputFile.equals(other.outputFile);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return inputFiles.hashCode() ^ outputFile.hashCode();
    }
}
