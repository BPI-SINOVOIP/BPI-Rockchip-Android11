package org.jetbrains.dokka.tests.format

import org.jetbrains.dokka.Formats.DacAsJavaFormatDescriptor
import org.jetbrains.dokka.Formats.DacFormatDescriptor
import org.jetbrains.dokka.Formats.JavaLayoutHtmlFormatDescriptorBase
import org.junit.Test

class DacFormatTest: DacFormatTestCase() {
    val dacFormatDescriptor = DacFormatDescriptor()
    val dacAsJavaFormatDescriptor = DacAsJavaFormatDescriptor()
    val dacFormat = "dac"
    val dacAsJavaFormat = "dac-as-java"

    private fun verifyBothFormats(directory: String) {
        verifyDirectory(directory, dacFormatDescriptor, dacFormat)
        verifyDirectory(directory, dacAsJavaFormatDescriptor, dacAsJavaFormat)
    }

    @Test fun javaSeeTag() {
        verifyBothFormats("javaSeeTag")
    }

    @Test fun javaConstructor() {
        verifyBothFormats("javaConstructor")
    }

    @Test
    fun javaSeeTagAsJava() {
        verifyBothFormats("javaSeeTag")
    }

    @Test
    fun javaConstructorAsJava() {
        verifyBothFormats("javaConstructor")
    }

    @Test
    fun javaDefaultConstructor() {
        verifyBothFormats("javaDefaultConstructor")
    }

    @Test
    fun javaInheritedMethods() {
        verifyBothFormats("inheritedMethods")
    }

    @Test fun javaMethodVisibilities() {
        verifyBothFormats("javaMethodVisibilities")
    }

    @Test fun javaClassLinks() {
        verifyBothFormats("javaClassLinks")
    }

    @Test fun deprecation() {
        verifyBothFormats("deprecation")
    }
}