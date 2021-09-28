package org.jetbrains.dokka.Formats

import com.intellij.psi.PsiMember
import com.intellij.psi.PsiParameter
import org.jetbrains.dokka.*
import org.jetbrains.dokka.ExternalDocumentationLinkResolver.Companion.DOKKA_PARAM_PREFIX
import org.jetbrains.kotlin.asJava.toLightElements
import org.jetbrains.kotlin.descriptors.*
import org.jetbrains.kotlin.descriptors.impl.EnumEntrySyntheticClassDescriptor
import org.jetbrains.kotlin.load.java.descriptors.JavaMethodDescriptor
import org.jetbrains.kotlin.load.java.descriptors.JavaPropertyDescriptor
import org.jetbrains.kotlin.psi.KtDeclaration
import org.jetbrains.kotlin.resolve.descriptorUtil.fqNameUnsafe
import org.jetbrains.kotlin.resolve.descriptorUtil.isCompanionObject
import org.jetbrains.kotlin.types.KotlinType

class JavaLayoutHtmlPackageListService: PackageListService {

    private fun StringBuilder.appendParam(name: String, value: String) {
        append(DOKKA_PARAM_PREFIX)
        append(name)
        append(":")
        appendln(value)
    }

    override fun formatPackageList(module: DocumentationModule): String {
        val packages = module.members(NodeKind.Package).map { it.name }

        return buildString {
            appendParam("format", "java-layout-html")
            appendParam("mode", "kotlin")
            for (p in packages) {
                appendln(p)
            }
        }
    }

}

class JavaLayoutHtmlInboundLinkResolutionService(private val paramMap: Map<String, List<String>>,
                                                 private val resolutionFacade: DokkaResolutionFacade) : InboundExternalLinkResolutionService {

    constructor(asJava: Boolean, resolutionFacade: DokkaResolutionFacade) :
            this(mapOf("mode" to listOf(if (asJava) "java" else "kotlin")), resolutionFacade)


    private val isJavaMode = paramMap["mode"]!!.single() == "java"

    private fun getContainerPath(symbol: DeclarationDescriptor): String? {
        return when (symbol) {
            is PackageFragmentDescriptor -> symbol.fqName.asString().replace('.', '/') + "/"
            is ClassifierDescriptor -> getContainerPath(symbol.findPackage()) + symbol.nameWithOuter() + ".html"
            else -> null
        }
    }

    private fun DeclarationDescriptor.findPackage(): PackageFragmentDescriptor =
        generateSequence(this) { it.containingDeclaration }.filterIsInstance<PackageFragmentDescriptor>().first()

    private fun ClassifierDescriptor.nameWithOuter(): String =
        generateSequence(this) { it.containingDeclaration as? ClassifierDescriptor }
            .toList().asReversed().joinToString(".") { it.name.asString() }

    private fun getJavaPagePath(symbol: DeclarationDescriptor): String? {

        val sourcePsi = symbol.sourcePsi() ?: return null
        val source = (if (sourcePsi is KtDeclaration) {
            sourcePsi.toLightElements().firstOrNull()
        } else {
            sourcePsi
        }) as? PsiMember ?: return null
        val desc = source.getJavaMemberDescriptor(resolutionFacade) ?: return null
        return getPagePath(desc)
    }

    private fun getPagePath(symbol: DeclarationDescriptor): String? {
        return when (symbol) {
            is PackageFragmentDescriptor -> getContainerPath(symbol) + "package-summary.html"
            is EnumEntrySyntheticClassDescriptor -> getContainerPath(symbol.containingDeclaration) + "#" + symbol.signatureForAnchorUrlEncoded()
            is ClassifierDescriptor -> getContainerPath(symbol) + "#"
            is FunctionDescriptor, is PropertyDescriptor -> getContainerPath(symbol.containingDeclaration!!) + "#" + symbol.signatureForAnchorUrlEncoded()
            else -> null
        }
    }

    private fun DeclarationDescriptor.signatureForAnchor(): String? {

        fun ReceiverParameterDescriptor.extractReceiverName(): String {
            var receiverClass: DeclarationDescriptor = type.constructor.declarationDescriptor!!
            if (receiverClass.isCompanionObject()) {
                receiverClass = receiverClass.containingDeclaration!!
            } else if (receiverClass is TypeParameterDescriptor) {
                val upperBoundClass = receiverClass.upperBounds.singleOrNull()?.constructor?.declarationDescriptor
                if (upperBoundClass != null) {
                    receiverClass = upperBoundClass
                }
            }

            return receiverClass.name.asString()
        }

        fun KotlinType.qualifiedNameForSignature(): String {
            val desc = constructor.declarationDescriptor
            return desc?.fqNameUnsafe?.asString() ?: "<ERROR TYPE NAME>"
        }

        fun StringBuilder.appendReceiverAndCompanion(desc: CallableDescriptor) {
            if (desc.containingDeclaration.isCompanionObject()) {
                append("Companion.")
            }
            desc.extensionReceiverParameter?.let {
                append("(")
                append(it.extractReceiverName())
                append(").")
            }
        }

        return when (this) {
            is EnumEntrySyntheticClassDescriptor -> buildString {
                append("ENUM_VALUE:")
                append(name.asString())
            }
            is JavaMethodDescriptor -> buildString {
                append(name.asString())
                valueParameters.joinTo(this, prefix = "(", postfix = ")") {
                    val param = it.sourcePsi() as PsiParameter
                    param.type.canonicalText
                }
            }
            is JavaPropertyDescriptor -> buildString {
                append(name.asString())
            }
            is FunctionDescriptor -> buildString {
                appendReceiverAndCompanion(this@signatureForAnchor)
                append(name.asString())
                valueParameters.joinTo(this, prefix = "(", postfix = ")") {
                    it.type.qualifiedNameForSignature()
                }
            }
            is PropertyDescriptor -> buildString {
                appendReceiverAndCompanion(this@signatureForAnchor)
                append(name.asString())
                append(":")

                append(returnType?.qualifiedNameForSignature())
            }
            else -> null
        }
    }

    private fun DeclarationDescriptor.signatureForAnchorUrlEncoded(): String? = signatureForAnchor()?.anchorEncoded()

    override fun getPath(symbol: DeclarationDescriptor) = if (isJavaMode) getJavaPagePath(symbol) else getPagePath(symbol)
}
