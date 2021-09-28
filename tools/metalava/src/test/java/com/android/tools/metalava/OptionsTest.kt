/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tools.metalava

import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.defaultConfiguration
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

@Suppress("PrivatePropertyName")
class OptionsTest : DriverTest() {
    private val DESCRIPTION = """
$PROGRAM_NAME extracts metadata from source code to generate artifacts such as the signature files, the SDK stub files,
external annotations etc.
""".trimIndent()

    private val FLAGS = """
Usage: metalava <flags>


General:
--help                                       
                                             This message.
--version                                    
                                             Show the version of metalava.
--quiet                                      
                                             Only include vital output
--verbose                                    
                                             Include extra diagnostic output
--color                                      
                                             Attempt to colorize the output (defaults to true if ${"$"}TERM is xterm)
--no-color                                   
                                             Do not attempt to colorize the output
--no-docs                                    
                                             Cancel any other documentation flags supplied to metalava. This is here to
                                             make it easier customize build system tasks.
--only-update-api                            
                                             Cancel any other "action" flags other than generating signature files. This
                                             is here to make it easier customize build system tasks, particularly for
                                             the "make update-api" task.
--only-check-api                             
                                             Cancel any other "action" flags other than checking signature files. This
                                             is here to make it easier customize build system tasks, particularly for
                                             the "make checkapi" task.


API sources:
--source-files <files>                       
                                             A comma separated list of source files to be parsed. Can also be @ followed
                                             by a path to a text file containing paths to the full set of files to
                                             parse.
--source-path <paths>                        
                                             One or more directories (separated by `:`) containing source files (within
                                             a package hierarchy). If --strict-input-files, --strict-input-files:warn,
                                             or --strict-input-files:stack are used, files accessed under --source-path
                                             that are not explicitly specified in --source-files are reported as
                                             violations.
--classpath <paths>                          
                                             One or more directories or jars (separated by `:`) containing classes that
                                             should be on the classpath when parsing the source files
--merge-qualifier-annotations <file>         
                                             An external annotations file to merge and overlay the sources, or a
                                             directory of such files. Should be used for annotations intended for
                                             inclusion in the API to be written out, e.g. nullability. Formats supported
                                             are: IntelliJ's external annotations database format, .jar or .zip files
                                             containing those, Android signature files, and Java stub files.
--merge-inclusion-annotations <file>         
                                             An external annotations file to merge and overlay the sources, or a
                                             directory of such files. Should be used for annotations which determine
                                             inclusion in the API to be written out, i.e. show and hide. The only format
                                             supported is Java stub files.
--validate-nullability-from-merged-stubs     
                                             Triggers validation of nullability annotations for any class where
                                             --merge-qualifier-annotations includes a Java stub file.
--validate-nullability-from-list             
                                             Triggers validation of nullability annotations for any class listed in the
                                             named file (one top-level class per line, # prefix for comment line).
--nullability-warnings-txt <file>            
                                             Specifies where to write warnings encountered during validation of
                                             nullability annotations. (Does not trigger validation by itself.)
--nullability-errors-non-fatal               
                                             Specifies that errors encountered during validation of nullability
                                             annotations should not be treated as errors. They will be written out to
                                             the file specified in --nullability-warnings-txt instead.
--input-api-jar <file>                       
                                             A .jar file to read APIs from directly
--manifest <file>                            
                                             A manifest file, used to for check permissions to cross check APIs
--replace-documentation <p> <r> <t>          
                                             Amongst nonempty documentation of items from Java packages <p> and their
                                             subpackages, replaces any matches of regular expression <r> with
                                             replacement text <t>. <p> is given as a nonempty list of Java package names
                                             separated by ':' (e.g. "java:android.util"); <t> may contain backreferences
                                             ($1, $2 etc.) to matching groups from <r>.
--hide-package <package>                     
                                             Remove the given packages from the API even if they have not been marked
                                             with @hide
--show-annotation <annotation class>         
                                             Unhide any hidden elements that are also annotated with the given
                                             annotation
--show-single-annotation <annotation>        
                                             Like --show-annotation, but does not apply to members; these must also be
                                             explicitly annotated
--hide-annotation <annotation class>         
                                             Treat any elements annotated with the given annotation as hidden
--hide-meta-annotation <meta-annotation class>
                                             Treat as hidden any elements annotated with an annotation which is itself
                                             annotated with the given meta-annotation
--show-unannotated                           
                                             Include un-annotated public APIs in the signature file as well
--java-source <level>                        
                                             Sets the source level for Java source files; default is 1.8.
--stub-packages <package-list>               
                                             List of packages (separated by :) which will be used to filter out
                                             irrelevant code. If specified, only code in these packages will be included
                                             in signature files, stubs, etc. (This is not limited to just the stubs; the
                                             name is historical.) You can also use ".*" at the end to match subpackages,
                                             so `foo.*` will match both `foo` and `foo.bar`.
--subtract-api <api file>                    
                                             Subtracts the API in the given signature or jar file from the current API
                                             being emitted via --api, --stubs, --doc-stubs, etc. Note that the
                                             subtraction only applies to classes; it does not subtract members.
--typedefs-in-signatures <ref|inline>        
                                             Whether to include typedef annotations in signature files.
                                             `--typedefs-in-signatures ref` will include just a reference to the typedef
                                             class, which is not itself part of the API and is not included as a class,
                                             and `--typedefs-in-signatures inline` will include the constants themselves
                                             into each usage site. You can also supply `--typedefs-in-signatures none`
                                             to explicitly turn it off, if the default ever changes.
--ignore-classes-on-classpath                
                                             Prevents references to classes on the classpath from being added to the
                                             generated stub files.


Documentation:
--public                                     
                                             Only include elements that are public
--protected                                  
                                             Only include elements that are public or protected
--package                                    
                                             Only include elements that are public, protected or package protected
--private                                    
                                             Include all elements except those that are marked hidden
--hidden                                     
                                             Include all elements, including hidden


Extracting Signature Files:
--api <file>                                 
                                             Generate a signature descriptor file
--private-api <file>                         
                                             Generate a signature descriptor file listing the exact private APIs
--dex-api <file>                             
                                             Generate a DEX signature descriptor file listing the APIs
--private-dex-api <file>                     
                                             Generate a DEX signature descriptor file listing the exact private APIs
--dex-api-mapping <file>                     
                                             Generate a DEX signature descriptor along with file and line numbers
--removed-api <file>                         
                                             Generate a signature descriptor file for APIs that have been removed
--format=<v1,v2,v3,...>                      
                                             Sets the output signature file format to be the given version.
--output-kotlin-nulls[=yes|no]               
                                             Controls whether nullness annotations should be formatted as in Kotlin
                                             (with "?" for nullable types, "" for non nullable types, and "!" for
                                             unknown. The default is yes.
--output-default-values[=yes|no]             
                                             Controls whether default values should be included in signature files. The
                                             default is yes.
--compatible-output=[yes|no]                 
                                             Controls whether to keep signature files compatible with the historical
                                             format (with its various quirks) or to generate the new format (which will
                                             also include annotations that are part of the API, etc.)
--omit-common-packages[=yes|no]              
                                             Skip common package prefixes like java.lang.* and kotlin.* in signature
                                             files, along with packages for well known annotations like @Nullable and
                                             @NonNull.
--include-signature-version[=yes|no]         
                                             Whether the signature files should include a comment listing the format
                                             version of the signature file.
--proguard <file>                            
                                             Write a ProGuard keep file for the API
--sdk-values <dir>                           
                                             Write SDK values files to the given directory


Generating Stubs:
--stubs <dir>                                
                                             Generate stub source files for the API
--doc-stubs <dir>                            
                                             Generate documentation stub source files for the API. Documentation stub
                                             files are similar to regular stub files, but there are some differences.
                                             For example, in the stub files, we'll use special annotations like
                                             @RecentlyNonNull instead of @NonNull to indicate that an element is
                                             recently marked as non null, whereas in the documentation stubs we'll just
                                             list this as @NonNull. Another difference is that @doconly elements are
                                             included in documentation stubs, but not regular stubs, etc.
--include-annotations                        
                                             Include annotations such as @Nullable in the stub files.
--exclude-annotations                        
                                             Exclude annotations such as @Nullable from the stub files; the default.
--pass-through-annotation <annotation classes>
                                             A comma separated list of fully qualified names of annotation classes that
                                             must be passed through unchanged.
--exclude-documentation-from-stubs           
                                             Exclude element documentation (javadoc and kdoc) from the generated stubs.
                                             (Copyright notices are not affected by this, they are always included.
                                             Documentation stubs (--doc-stubs) are not affected.)
--write-stubs-source-list <file>             
                                             Write the list of generated stub files into the given source list file. If
                                             generating documentation stubs and you haven't also specified
                                             --write-doc-stubs-source-list, this list will refer to the documentation
                                             stubs; otherwise it's the non-documentation stubs.
--write-doc-stubs-source-list <file>         
                                             Write the list of generated doc stub files into the given source list file
--register-artifact <api-file> <id>          
                                             Registers the given id for the packages found in the given signature file.
                                             metalava will inject an @artifactId <id> tag into every top level stub
                                             class in that API.


Diffs and Checks:
--input-kotlin-nulls[=yes|no]                
                                             Whether the signature file being read should be interpreted as having
                                             encoded its types using Kotlin style types: a suffix of "?" for nullable
                                             types, no suffix for non nullable types, and "!" for unknown. The default
                                             is no.
--check-compatibility:type:state <file>      
                                             Check compatibility. Type is one of 'api' and 'removed', which checks
                                             either the public api or the removed api. State is one of 'current' and
                                             'released', to check either the currently in development API or the last
                                             publicly released API, respectively. Different compatibility checks apply
                                             in the two scenarios. For example, to check the code base against the
                                             current public API, use --check-compatibility:api:current.
--api-lint [api file]                        
                                             Check API for Android API best practices. If a signature file is provided,
                                             only the APIs that are new since the API will be checked.
--api-lint-ignore-prefix [prefix]            
                                             A list of package prefixes to ignore API issues in when running with
                                             --api-lint.
--check-kotlin-interop                       
                                             Check API intended to be used from both Kotlin and Java for
                                             interoperability issues
--migrate-nullness <api file>                
                                             Compare nullness information with the previous stable API and mark newly
                                             annotated APIs as under migration.
--warnings-as-errors                         
                                             Promote all warnings to errors
--lints-as-errors                            
                                             Promote all API lint warnings to errors
--error <id>                                 
                                             Report issues of the given id as errors
--warning <id>                               
                                             Report issues of the given id as warnings
--lint <id>                                  
                                             Report issues of the given id as having lint-severity
--hide <id>                                  
                                             Hide/skip issues of the given id
--report-even-if-suppressed <file>           
                                             Write all issues into the given file, even if suppressed (via annotation or
                                             baseline) but not if hidden (by '--hide')
--baseline <file>                            
                                             Filter out any errors already reported in the given baseline file, or
                                             create if it does not already exist
--update-baseline [file]                     
                                             Rewrite the existing baseline file with the current set of warnings. If
                                             some warnings have been fixed, this will delete them from the baseline
                                             files. If a file is provided, the updated baseline is written to the given
                                             file; otherwise the original source baseline file is updated.
--baseline:api-lint <file> --update-baseline:api-lint [file]
                                             Same as --baseline and --update-baseline respectively, but used
                                             specifically for API lint issues performed by --api-lint.
--baseline:compatibility:released <file> --update-baseline:compatibility:released [file]
                                             Same as --baseline and --update-baseline respectively, but used
                                             specifically for API compatibility issues performed by
                                             --check-compatibility:api:released and
                                             --check-compatibility:removed:released.
--merge-baseline [file]                      
                                             Like --update-baseline, but instead of always replacing entries in the
                                             baseline, it will merge the existing baseline with the new baseline. This
                                             is useful if metalava runs multiple times on the same source tree with
                                             different flags at different times, such as occasionally with --api-lint.
--pass-baseline-updates                      
                                             Normally, encountering error will fail the build, even when updating
                                             baselines. This flag allows you to tell metalava to continue without
                                             errors, such that all the baselines in the source tree can be updated in
                                             one go.
--delete-empty-baselines                     
                                             Whether to delete baseline files if they are updated and there is nothing
                                             to include.
--error-message:api-lint <message>           
                                             If set, metalava shows it when errors are detected in --api-lint.
--error-message:compatibility:released <message>
                                             If set, metalava shows it  when errors are detected in
                                             --check-compatibility:api:released and
                                             --check-compatibility:removed:released.
--error-message:compatibility:current <message>
                                             If set, metalava shows it  when errors are detected in
                                             --check-compatibility:api:current and
                                             --check-compatibility:removed:current.


JDiff:
--api-xml <file>                             
                                             Like --api, but emits the API in the JDiff XML format instead
--convert-to-jdiff <sig> <xml>               
                                             Reads in the given signature file, and writes it out in the JDiff XML
                                             format. Can be specified multiple times.
--convert-new-to-jdiff <old> <new> <xml>     
                                             Reads in the given old and new api files, computes the difference, and
                                             writes out only the new parts of the API in the JDiff XML format.
--convert-to-v1 <sig> <sig>                  
                                             Reads in the given signature file and writes it out as a signature file in
                                             the original v1/doclava format.
--convert-to-v2 <sig> <sig>                  
                                             Reads in the given signature file and writes it out as a signature file in
                                             the new signature format, v2.
--convert-new-to-v2 <old> <new> <sig>        
                                             Reads in the given old and new api files, computes the difference, and
                                             writes out only the new parts of the API in the v2 format.


Statistics:
--annotation-coverage-stats                  
                                             Whether metalava should emit coverage statistics for annotations, listing
                                             the percentage of the API that has been annotated with nullness
                                             information.
--annotation-coverage-of <paths>             
                                             One or more jars (separated by `:`) containing existing apps that we want
                                             to measure annotation coverage statistics for. The set of API usages in
                                             those apps are counted up and the most frequently used APIs that are
                                             missing annotation metadata are listed in descending order.
--skip-java-in-coverage-report               
                                             In the coverage annotation report, skip java.** and kotlin.** to narrow the
                                             focus down to the Android framework APIs.
--write-class-coverage-to <path>             
                                             Specifies a file to write the annotation coverage report for classes to.
--write-member-coverage-to <path>            
                                             Specifies a file to write the annotation coverage report for members to.


Extracting Annotations:
--extract-annotations <zipfile>              
                                             Extracts source annotations from the source files and writes them into the
                                             given zip file
--include-annotation-classes <dir>           
                                             Copies the given stub annotation source files into the generated stub
                                             sources; <dir> is typically metalava/stub-annotations/src/main/java/.
--rewrite-annotations <dir/jar>              
                                             For a bytecode folder or output jar, rewrites the androidx annotations to
                                             be package private
--force-convert-to-warning-nullability-annotations <package1:-package2:...>
                                             On every API declared in a class referenced by the given filter, makes
                                             nullability issues appear to callers as warnings rather than errors by
                                             replacing @Nullable/@NonNull in these APIs with
                                             @RecentlyNullable/@RecentlyNonNull
--copy-annotations <source> <dest>           
                                             For a source folder full of annotation sources, generates corresponding
                                             package private versions of the same annotations.
--include-source-retention                   
                                             If true, include source-retention annotations in the stub files. Does not
                                             apply to signature files. Source retention annotations are extracted into
                                             the external annotations files instead.


Injecting API Levels:
--apply-api-levels <api-versions.xml>        
                                             Reads an XML file containing API level descriptions and merges the
                                             information into the documentation


Extracting API Levels:
--generate-api-levels <xmlfile>              
                                             Reads android.jar SDK files and generates an XML file recording the API
                                             level for each class, method and field
--android-jar-pattern <pattern>              
                                             Patterns to use to locate Android JAR files. The default is
                                             ${"$"}ANDROID_HOME/platforms/android-%/android.jar.
--current-version                            
                                             Sets the current API level of the current source code
--current-codename                           
                                             Sets the code name for the current source code
--current-jar                                
                                             Points to the current API jar, if any


Sandboxing:
--no-implicit-root                           
                                             Disable implicit root directory detection. Otherwise, metalava adds in
                                             source roots implied by the source files
--strict-input-files <file>                  
                                             Do not read files that are not explicitly specified in the command line.
                                             All violations are written to the given file. Reads on directories are
                                             always allowed, but metalava still tracks reads on directories that are not
                                             specified in the command line, and write them to the file.
--strict-input-files:warn <file>             
                                             Warn when files not explicitly specified on the command line are read. All
                                             violations are written to the given file. Reads on directories not
                                             specified in the command line are allowed but also logged.
--strict-input-files:stack <file>            
                                             Same as --strict-input-files but also print stacktraces.
--strict-input-files-exempt <files or dirs>  
                                             Used with --strict-input-files. Explicitly allow access to files and/or
                                             directories (separated by `:). Can also be @ followed by a path to a text
                                             file containing paths to the full set of files and/or directories.


Environment Variables:
METALAVA_DUMP_ARGV                           
                                             Set to true to have metalava emit all the arguments it was invoked with.
                                             Helpful when debugging or reproducing under a debugger what the build
                                             system is doing.
METALAVA_PREPEND_ARGS                        
                                             One or more arguments (concatenated by space) to insert into the command
                                             line, before the documentation flags.
METALAVA_APPEND_ARGS                         
                                             One or more arguments (concatenated by space) to append to the end of the
                                             command line, after the generate documentation flags.

""".trimIndent()

    @Test
    fun `Test invalid arguments`() {
        val args = listOf(ARG_NO_COLOR, "--blah-blah-blah")

        val stdout = StringWriter()
        val stderr = StringWriter()
        run(
            originalArgs = args.toTypedArray(),
            stdout = PrintWriter(stdout),
            stderr = PrintWriter(stderr)
        )
        assertEquals(BANNER + "\n\n", stdout.toString())
        assertEquals(
            """

Invalid argument --blah-blah-blah

$FLAGS

""".trimIndent(), stderr.toString()
        )
    }

    @Test
    fun `Test help`() {
        val args = listOf(ARG_NO_COLOR, "--help")

        val stdout = StringWriter()
        val stderr = StringWriter()
        run(
            originalArgs = args.toTypedArray(),
            stdout = PrintWriter(stdout),
            stderr = PrintWriter(stderr)
        )
        assertEquals("", stderr.toString())
        assertEquals(
            """
$BANNER


$DESCRIPTION

$FLAGS

""".trimIndent(), stdout.toString()
        )
    }

    @Test
    fun `Test issue severity options`() {
        check(
            extraArguments = arrayOf(
                "--hide",
                "StartWithLower",
                "--lint",
                "EndsWithImpl",
                "--warning",
                "StartWithUpper",
                "--error",
                "ArrayReturn"
            )
        )
        assertEquals(Severity.HIDDEN, defaultConfiguration.getSeverity(Issues.START_WITH_LOWER))
        assertEquals(Severity.LINT, defaultConfiguration.getSeverity(Issues.ENDS_WITH_IMPL))
        assertEquals(Severity.WARNING, defaultConfiguration.getSeverity(Issues.START_WITH_UPPER))
        assertEquals(Severity.ERROR, defaultConfiguration.getSeverity(Issues.ARRAY_RETURN))
    }

    @Test
    fun `Test multiple issue severity options`() {
        check(
            extraArguments = arrayOf("--hide", "StartWithLower,StartWithUpper,ArrayReturn")
        )
        assertEquals(Severity.HIDDEN, defaultConfiguration.getSeverity(Issues.START_WITH_LOWER))
        assertEquals(Severity.HIDDEN, defaultConfiguration.getSeverity(Issues.START_WITH_UPPER))
        assertEquals(Severity.HIDDEN, defaultConfiguration.getSeverity(Issues.ARRAY_RETURN))
    }

    @Test
    fun `Test issue severity options with inheriting issues`() {
        check(
            extraArguments = arrayOf("--error", "RemovedClass")
        )
        assertEquals(Severity.ERROR, defaultConfiguration.getSeverity(Issues.REMOVED_CLASS))
        assertEquals(Severity.ERROR, defaultConfiguration.getSeverity(Issues.REMOVED_DEPRECATED_CLASS))
    }

    @Test
    fun `Test issue severity options by numeric id`() {
        check(
            extraArguments = arrayOf("--hide", "366"),
            expectedIssues = "warning: Issue lookup by numeric id is deprecated, use --hide ArrayReturn instead of --hide 366 [DeprecatedOption]"
        )
        assertEquals(Severity.HIDDEN, defaultConfiguration.getSeverity(Issues.ARRAY_RETURN))
    }

    @Test
    fun `Test issue severity options with case insensitive names`() {
        check(
            extraArguments = arrayOf("--hide", "arrayreturn"),
            expectedIssues = "warning: Case-insensitive issue matching is deprecated, use --hide ArrayReturn instead of --hide arrayreturn [DeprecatedOption]"
        )
        assertEquals(Severity.HIDDEN, defaultConfiguration.getSeverity(Issues.ARRAY_RETURN))
    }

    @Test
    fun `Test issue severity options with non-existing issue`() {
        check(
            extraArguments = arrayOf("--hide", "ThisIssueDoesNotExist"),
            expectedFail = "Unknown issue id: --hide ThisIssueDoesNotExist"
        )
    }

    @Test
    fun `Test for --strict-input-files-exempt`() {
        val top = temporaryFolder.newFolder()

        val dir = File(top, "childdir").apply { mkdirs() }
        val grandchild1 = File(dir, "grandchiild1").apply { createNewFile() }
        val grandchild2 = File(dir, "grandchiild2").apply { createNewFile() }
        val file1 = File(top, "file1").apply { createNewFile() }
        val file2 = File(top, "file2").apply { createNewFile() }

        try {
            check(
                extraArguments = arrayOf("--strict-input-files-exempt",
                    file1.path + File.pathSeparatorChar + dir.path)
            )

            assertTrue(FileReadSandbox.isAccessAllowed(file1))
            assertTrue(FileReadSandbox.isAccessAllowed(grandchild1))
            assertTrue(FileReadSandbox.isAccessAllowed(grandchild2))

            assertFalse(FileReadSandbox.isAccessAllowed(file2)) // Access *not* allowed
        } finally {
            FileReadSandbox.reset()
        }
    }
}
