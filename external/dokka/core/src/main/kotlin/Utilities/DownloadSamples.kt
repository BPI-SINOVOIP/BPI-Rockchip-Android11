/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.jetbrains.dokka.Utilities

import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.io.FileOutputStream

object DownloadSamples {

    /** HTTP Client to make requests **/
    val client = OkHttpClient()

    /**
     * Function that downloads samples based on the directory structure described in hashmap
     */
    fun downloadSamples(): Boolean {

        //loop through each directory of AOSP code in SamplesPathsToURLs.kt
        filepathsToUrls.forEach { (filepath, url) ->

            //build request using each URL
            val request = Request.Builder()
                .url(url)
                .build()

            val response = client.newCall(request).execute()

            if (response.isSuccessful) {

                //save .tar.gz file to filepath designated by map
                val currentFile = File(filepath)
                currentFile.mkdirs()

                val fos = FileOutputStream("$filepath.tar.gz")
                fos.write(response.body?.bytes())
                fos.close()

                //Unzip, Untar, and delete compressed file after
                extractFiles(filepath)

            } else {
                println("Error Downloading Samples: $response")
                return false
            }
        }

        println("Successfully completed download of samples.")
        return true

    }

    /**
     * Execute bash commands to extract file, then delete archive file
     */
    private fun extractFiles(pathToFile: String) {

        ProcessBuilder()
            .command("tar","-zxf", "$pathToFile.tar.gz", "-C", pathToFile)
            .redirectError(ProcessBuilder.Redirect.INHERIT)
            .redirectOutput(ProcessBuilder.Redirect.INHERIT)
            .start()
            .waitFor()

        ProcessBuilder()
            .command("rm", "$pathToFile.tar.gz")
            .redirectError(ProcessBuilder.Redirect.INHERIT)
            .redirectOutput(ProcessBuilder.Redirect.INHERIT)
            .start()
            .waitFor()
    }

}