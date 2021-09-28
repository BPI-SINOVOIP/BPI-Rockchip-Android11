package com.android.tools.metalava

import com.android.SdkConstants.ATTR_VALUE
import com.android.tools.metalava.model.AnnotationAttribute
import com.android.tools.metalava.model.AnnotationItem
import com.android.tools.metalava.model.DefaultAnnotationAttribute

interface AnnotationFilter {
    // tells whether an annotation is included by the filter
    fun matches(annotation: AnnotationItem): Boolean
    // tells whether an annotation is included by this filter
    fun matches(annotationSource: String): Boolean

    // Returns a list of fully qualified annotation names that may be included by this filter.
    // Note that this filter might incorporate parameters but this function strips them.
    fun getIncludedAnnotationNames(): List<String>
    // Tells whether there exists an annotation that is accepted by this filter and that
    // ends with the given suffix
    fun matchesSuffix(annotationSuffix: String): Boolean
    // Returns true if nothing is matched by this filter
    fun isEmpty(): Boolean
    // Returns true if some annotation is matched by this filter
    fun isNotEmpty(): Boolean
    // Returns the fully-qualified class name of the first annotation matched by this filter
    fun firstQualifiedName(): String
}

// Mutable implementation of AnnotationFilter
class MutableAnnotationFilter : AnnotationFilter {
    private val inclusionExpressions = mutableListOf<AnnotationFilterEntry>()

    // Adds the given source as a fully qualified annotation name to match with this filter
    // Can be "androidx.annotation.RestrictTo"
    // Can be "androidx.annotation.RestrictTo(androidx.annotation.RestrictTo.Scope.LIBRARY_GROUP)"
    // Note that the order of calls to this method could affect the return from
    // {@link #firstQualifiedName} .
    fun add(source: String) {
        inclusionExpressions.add(AnnotationFilterEntry.fromSource(source))
    }

    override fun matches(annotationSource: String): Boolean {
        val annotationText = annotationSource.replace("@", "")
        val wrapper = AnnotationFilterEntry.fromSource(annotationText)
        return matches(wrapper)
    }

    override fun matches(annotation: AnnotationItem): Boolean {
        if (annotation.qualifiedName() == null) {
            return false
        }
        val wrapper = AnnotationFilterEntry.fromAnnotationItem(annotation)
        return matches(wrapper)
    }

    private fun matches(annotation: AnnotationFilterEntry): Boolean {
        return inclusionExpressions.any { includedAnnotation ->
            annotationsMatch(includedAnnotation, annotation)
        }
    }

    override fun getIncludedAnnotationNames(): List<String> {
        val annotationNames = mutableListOf<String>()
        for (expression in inclusionExpressions) {
            annotationNames.add(expression.qualifiedName)
        }
        return annotationNames
    }

    override fun matchesSuffix(annotationSuffix: String): Boolean {
        return inclusionExpressions.any { included ->
            included.qualifiedName.endsWith(annotationSuffix)
        }
    }

    override fun isEmpty(): Boolean {
        return inclusionExpressions.isEmpty()
    }

    override fun isNotEmpty(): Boolean {
        return !isEmpty()
    }

    override fun firstQualifiedName(): String {
        val inclusion = inclusionExpressions.first()
        return inclusion.qualifiedName
    }

    private fun annotationsMatch(filter: AnnotationFilterEntry, existingAnnotation: AnnotationFilterEntry): Boolean {
        if (filter.qualifiedName != existingAnnotation.qualifiedName) {
            return false
        }
        if (filter.attributes.count() > existingAnnotation.attributes.count()) {
            return false
        }
        for (attribute in filter.attributes) {
            val existingValue = existingAnnotation.findAttribute(attribute.name)?.value?.toSource()
            if (existingValue != attribute.value.toSource()) {
                return false
            }
        }
        return true
    }
}

// An AnnotationFilterEntry filters for annotations having a certain qualifiedName and
// possibly certain attributes.
// An AnnotationFilterEntry doesn't necessarily have a Codebase like an AnnotationItem does
private class AnnotationFilterEntry(
    val qualifiedName: String,
    val attributes: List<AnnotationAttribute>
) {
    fun findAttribute(name: String?): AnnotationAttribute? {
        val actualName = name ?: ATTR_VALUE
        return attributes.firstOrNull { it.name == actualName }
    }

    companion object {
        fun fromSource(source: String): AnnotationFilterEntry {
            val text = source.replace("@", "")
            val index = text.indexOf("(")

            val qualifiedName = if (index == -1) {
                text
            } else {
                text.substring(0, index)
            }

            val attributes: List<AnnotationAttribute> = if (index == -1) {
                emptyList()
            } else {
                DefaultAnnotationAttribute.createList(
                    text.substring(index + 1, text.lastIndexOf(')'))
                )
            }
            return AnnotationFilterEntry(qualifiedName, attributes)
        }

        fun fromAnnotationItem(annotationItem: AnnotationItem): AnnotationFilterEntry {
            // Have to call toSource to resolve attribute values into fully qualified class names.
            // For example: resolving RestrictTo(LIBRARY_GROUP) into
            // RestrictTo(androidx.annotation.RestrictTo.Scope.LIBRARY_GROUP)
            // In addition, toSource (with the default argument showDefaultAttrs=true) retrieves
            // default attributes from the definition of the annotation. For example,
            // @SystemApi actually is converted into @android.annotation.SystemApi(\
            // client=android.annotation.SystemApi.Client.PRIVILEGED_APPS,\
            // process=android.annotation.SystemApi.Process.ALL)
            return AnnotationFilterEntry.fromSource(annotationItem.toSource())
        }
    }
}
