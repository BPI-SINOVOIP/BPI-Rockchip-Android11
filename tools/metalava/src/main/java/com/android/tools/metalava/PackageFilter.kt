package com.android.tools.metalava

import com.android.tools.metalava.model.PackageItem
import java.io.File
import java.util.function.Predicate

/**
 *
 * We permit a number of different styles:
 * - exact match (foo)
 * - prefix match (foo*, probably not intentional)
 * - subpackage match (foo.*)
 * - package and subpackage match (foo:foo.*)
 * - explicit addition (+foo.*)
 * - subtraction (+*:-foo.*)
 *
 * Real examples:
 * args: "-stubpackages com.android.test.power ",
 * args: "-stubpackages android.car* ",
 * args: "-stubpackages com.android.ahat:com.android.ahat.*",
 * args: "-force-convert-to-warning-nullability-annotations +*:-android.*:+android.icu.*:-dalvik.*
 *
 * Note that doclava does *not* include subpackages by default: -stubpackage foo
 * will match only foo, not foo.bar. Note also that "foo.*" will not match "foo",
 * so doclava required you to supply both: "foo:foo.*".
 *
 * In metalava we've changed that: it's not likely that you want to
 * match any subpackage of foo but not foo itself, so foo.* is taken
 * to mean "foo" and "foo.*".
 */
class PackageFilter() {
    val components: MutableList<PackageFilterComponent> = mutableListOf()

    fun matches(qualifiedName: String): Boolean {
        for (component in components.reversed()) {
            if (component.filter.test(qualifiedName)) {
                return component.treatAsPositiveMatch
            }
        }
        return false
    }

    fun addPackages(path: String) {
        for (arg in path.split(File.pathSeparatorChar)) {
            val treatAsPositiveMatch = !arg.startsWith("-")
            val pkg = arg.removePrefix("-").removePrefix("+")
            val index = pkg.indexOf('*')
            if (index != -1) {
                if (index < pkg.length - 1) {
                    throw DriverException(stderr = "Wildcards in stub packages must be at the end of the package: $pkg)")
                }
                val prefix = pkg.removeSuffix("*")
                if (prefix.endsWith(".")) {
                    // In doclava, "foo.*" does not match "foo", but we want to do that.
                    val exact = prefix.substring(0, prefix.length - 1)
                    add(StringEqualsPredicate(exact), treatAsPositiveMatch)
                }
                add(StringPrefixPredicate(prefix), treatAsPositiveMatch)
            } else {
                add(StringEqualsPredicate(pkg), treatAsPositiveMatch)
            }
        }
    }

    fun add(predicate: Predicate<String>, treatAsPositiveMatch: Boolean) {
        components.add(PackageFilterComponent(predicate, treatAsPositiveMatch))
    }

    fun matches(packageItem: PackageItem): Boolean {
        return matches(packageItem.qualifiedName())
    }

    companion object {
        fun parse(path: String): PackageFilter {
            val filter = PackageFilter()
            filter.addPackages(path)
            return filter
        }
    }
}

class StringPrefixPredicate(val acceptedPrefix: String) : Predicate<String> {
    override fun test(candidatePackage: String): Boolean {
        return candidatePackage.startsWith(acceptedPrefix)
    }
}

class StringEqualsPredicate(val acceptedPackage: String) : Predicate<String> {
    override fun test(candidatePackage: String): Boolean {
        return candidatePackage == acceptedPackage
    }
}

/**
 * One element of a PackageFilter.
 * Detects packages and either either includes or excludes them from the filter
 */
class PackageFilterComponent(val filter: Predicate<String>, val treatAsPositiveMatch: Boolean)
