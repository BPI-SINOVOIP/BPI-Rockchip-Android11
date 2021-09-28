package org.jetbrains.dokka.Samples

import com.google.inject.Inject
import com.intellij.psi.PsiElement
import org.jetbrains.dokka.*
import org.jetbrains.kotlin.name.Name
import org.jetbrains.kotlin.psi.*
import org.jetbrains.kotlin.utils.addIfNotNull

open class DevsiteSampleProcessingService
@Inject constructor(
    options: DocumentationOptions,
    logger: DokkaLogger,
    resolutionFacade: DokkaResolutionFacade
) : DefaultSampleProcessingService(options, logger, resolutionFacade) {

    override fun processImports(psiElement: PsiElement): ContentBlockCode {
        // List of expression calls inside this sample, so we can trim the imports to only show relevant expressions
        val sampleExpressionCalls = mutableSetOf<String>()
        val psiFile = psiElement.containingFile
        (psiElement as KtDeclarationWithBody).bodyExpression!!.accept(object : KtTreeVisitorVoid() {
            override fun visitCallExpression(expression: KtCallExpression) {
                sampleExpressionCalls.addIfNotNull(expression.calleeExpression?.text)
                super.visitCallExpression(expression)
            }
        })
        val androidxPackage = Name.identifier("androidx")
        if (psiFile is KtFile) {
            val filteredImports = psiFile.importList?.imports?.filter { element ->
                val fqImportName = element.importPath?.fqName ?: return@filter false

                val shortName = fqImportName.shortName().identifier
                // Hide all non-androidx imports
                if (!fqImportName.startsWith(androidxPackage)) return@filter false

                sampleExpressionCalls.any { call ->
                    call == shortName
                }
            }

            return ContentBlockCode("kotlin").apply {
                filteredImports?.forEach { import ->
                    if (import != filteredImports.first()) {
                        append(ContentText("\n"))
                    }
                    append(ContentText(import.text))
                }
            }
        }
        return super.processImports(psiElement)
    }
}
