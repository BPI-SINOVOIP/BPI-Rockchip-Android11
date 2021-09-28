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

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

public class FileUtil {

    public static String readFile(File f) {
        if (f.length() > Integer.MAX_VALUE) {
            throw new IllegalArgumentException(f.toString());
        }
        byte tmp[] = new byte[(int)f.length()];
        try (FileInputStream fis = new FileInputStream(f)) {
            int pos = 0;
            while (pos != tmp.length) {
                int read = fis.read(tmp, pos, tmp.length - pos);
                if (read == -1) {
                    throw new IOException("Unexpected EOF");
                }
                pos += read;
            }
            return new String(tmp);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

}
