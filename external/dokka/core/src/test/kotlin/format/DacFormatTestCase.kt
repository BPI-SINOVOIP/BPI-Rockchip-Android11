package org.jetbrains.dokka.tests.format

import com.google.inject.Guice
import com.google.inject.Injector
import com.google.inject.Module
import com.google.inject.name.Names
import org.jetbrains.dokka.DocumentationOptions
import org.jetbrains.dokka.DokkaLogger
import org.jetbrains.dokka.Formats.JavaLayoutHtmlFormatDescriptorBase
import org.jetbrains.dokka.Formats.JavaLayoutHtmlFormatGenerator
import org.jetbrains.dokka.Generator
import org.jetbrains.dokka.Utilities.bind
import org.jetbrains.dokka.tests.assertEqualsIgnoringSeparators
import org.jetbrains.dokka.tests.verifyModel
import org.jetbrains.kotlin.cli.common.config.KotlinSourceRoot
import org.jetbrains.kotlin.cli.jvm.config.JavaSourceRoot
import org.junit.Rule
import org.junit.rules.TemporaryFolder
import java.io.File
import java.net.URI

abstract class DacFormatTestCase {
    @get:Rule
    var folder = TemporaryFolder()

    protected fun verifyDirectory(directory: String, formatDescriptor: JavaLayoutHtmlFormatDescriptorBase, dokkaFormat: String) {
        val injector: Injector by lazy {
            val options =
                DocumentationOptions(
                    folder.toString(),
                    dokkaFormat,
                    apiVersion = null,
                    languageVersion = null,
                    generateClassIndexPage = false,
                    generatePackageIndexPage = false,
                    noStdlibLink = false,
                    noJdkLink = false,
                    collectInheritedExtensionsFromLibraries = true
                )

            Guice.createInjector(Module { binder ->

                binder.bind<Boolean>().annotatedWith(Names.named("generateClassIndex")).toInstance(false)
                binder.bind<Boolean>().annotatedWith(Names.named("generatePackageIndex")).toInstance(false)

                binder.bind<String>().annotatedWith(Names.named("dacRoot")).toInstance("")
                binder.bind<String>().annotatedWith(Names.named("outlineRoot")).toInstance("")
                binder.bind<File>().annotatedWith(Names.named("outputDir")).toInstance(folder.root)

                binder.bind<DocumentationOptions>().toProvider { options }
                binder.bind<DokkaLogger>().toInstance(object : DokkaLogger {
                    override fun info(message: String) {
                        println(message)
                    }

                    override fun warn(message: String) {
                        println("WARN: $message")
                    }

                    override fun error(message: String) {
                        println("ERROR: $message")
                    }
                })

                formatDescriptor.configureOutput(binder)
            })
        }


        val directoryFile = File("testdata/format/dac/$directory")
        verifyModel(
            JavaSourceRoot(directoryFile, null), KotlinSourceRoot(directoryFile.path, false),
            format = dokkaFormat
        ) { documentationModule ->
            val nodes = documentationModule.members.single().members
            with(injector.getInstance(Generator::class.java)) {
                this as JavaLayoutHtmlFormatGenerator
                buildPages(listOf(documentationModule))
                val byLocations = nodes.groupBy { mainUri(it) }
                val tmpFolder = folder.root.toURI().resolve("${documentationModule.name}/")
                byLocations.forEach { (loc, node) ->
                    val output = StringBuilder()
                    output.append(tmpFolder.resolve(URI("/").relativize(loc)).toURL().readText())
                    val expectedFile = File(File(directoryFile, dokkaFormat), "${node.first().name}.html")
                    assertEqualsIgnoringSeparators(expectedFile, output.toString())
                }
            }
        }
    }
}