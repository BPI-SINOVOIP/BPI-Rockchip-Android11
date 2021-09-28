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

package com.android.tools.metalava.model.psi

import com.android.SdkConstants
import com.android.tools.metalava.ANDROIDX_NONNULL
import com.android.tools.metalava.ANDROIDX_NULLABLE
import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.DefaultCodebase
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.PackageDocs
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.model.PackageList
import com.android.tools.metalava.model.TypeItem
import com.android.tools.metalava.options
import com.android.tools.metalava.reporter
import com.android.tools.metalava.tick
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Disposer
import com.intellij.psi.JavaPsiFacade
import com.intellij.psi.JavaRecursiveElementVisitor
import com.intellij.psi.PsiAnnotation
import com.intellij.psi.PsiArrayType
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiClassOwner
import com.intellij.psi.PsiClassType
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiErrorElement
import com.intellij.psi.PsiField
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiJavaCodeReferenceElement
import com.intellij.psi.PsiJavaFile
import com.intellij.psi.PsiMethod
import com.intellij.psi.PsiPackage
import com.intellij.psi.PsiSubstitutor
import com.intellij.psi.PsiType
import com.intellij.psi.TypeAnnotationProvider
import com.intellij.psi.javadoc.PsiDocComment
import com.intellij.psi.javadoc.PsiDocTag
import com.intellij.psi.search.GlobalSearchScope
import com.intellij.psi.util.PsiTreeUtil
import org.jetbrains.kotlin.resolve.BindingContext
import org.jetbrains.uast.UFile
import org.jetbrains.uast.UastContext
import java.io.File
import java.io.IOException
import java.util.ArrayList
import java.util.HashMap
import java.util.zip.ZipFile

const val PACKAGE_ESTIMATE = 500
const val CLASS_ESTIMATE = 15000
const val METHOD_ESTIMATE = 1000

open class PsiBasedCodebase(location: File, override var description: String = "Unknown") : DefaultCodebase(location) {
    lateinit var project: Project

    var bindingContext: BindingContext? = null

    /** Map from class name to class item */
    private val classMap: MutableMap<String, PsiClassItem> = HashMap(CLASS_ESTIMATE)

    /** Map from psi type to type item */
    private val typeMap: MutableMap<PsiType, TypeItem> = HashMap(400)

    /**
     * Map from classes to the set of methods for each (but only for classes where we've
     * called [findMethod]
     */
    private lateinit var methodMap: MutableMap<PsiClassItem, MutableMap<PsiMethod, PsiMethodItem>>

    /** Map from package name to the corresponding package item */
    private lateinit var packageMap: MutableMap<String, PsiPackageItem>

    /** Map from package name to list of classes in that package */
    private lateinit var packageClasses: MutableMap<String, MutableList<PsiClassItem>>

    /** A set of packages to hide */
    private lateinit var hiddenPackages: MutableMap<String, Boolean?>

    /**
     * A list of the top-level classes declared in the codebase's source (rather than on its
     * classpath).
     */
    private lateinit var topLevelClassesFromSource: MutableList<ClassItem>

    private var initializing = false

    override fun trustedApi(): Boolean = false

    private var packageDocs: PackageDocs? = null

    private var hideClassesFromJars = true

    private lateinit var emptyPackage: PsiPackageItem

    fun initialize(project: Project, units: List<PsiFile>, packages: PackageDocs) {
        initializing = true
        this.units = units
        packageDocs = packages

        this.project = project
        // there are currently ~230 packages in the public SDK, but here we need to account for internal ones too
        val hiddenPackages: MutableSet<String> = packages.hiddenPackages
        val packageDocs: MutableMap<String, String> = packages.packageDocs
        this.hiddenPackages = HashMap(100)
        for (pkgName in hiddenPackages) {
            this.hiddenPackages[pkgName] = true
        }

        packageMap = HashMap(PACKAGE_ESTIMATE)
        packageClasses = HashMap(PACKAGE_ESTIMATE)
        packageClasses[""] = ArrayList()
        this.methodMap = HashMap(METHOD_ESTIMATE)
        topLevelClassesFromSource = ArrayList(CLASS_ESTIMATE)

        // Make sure we only process the units once; sometimes there's overlap in the source lists
        for (unit in units.asSequence().distinct()) {
            tick() // show progress

            var classes = (unit as? PsiClassOwner)?.classes?.toList() ?: emptyList()
            if (classes.isEmpty()) {
                val uastContext = project.getComponent(UastContext::class.java)
                val uFile = uastContext.convertElementWithParent(unit, UFile::class.java) as? UFile?
                classes = uFile?.classes?.map { it }?.toList() ?: emptyList()
            }
            var packageName: String? = null
            if (classes.isEmpty() && unit is PsiJavaFile) {
                // package-info.java ?
                val packageStatement = unit.packageStatement
                // Look for javadoc on the package statement; this is NOT handed to us on
                // the PsiPackage!
                if (packageStatement != null) {
                    packageName = packageStatement.packageName
                    val comment = PsiTreeUtil.getPrevSiblingOfType(packageStatement, PsiDocComment::class.java)
                    if (comment != null) {
                        val text = comment.text
                        if (text.contains("@hide")) {
                            this.hiddenPackages[packageName] = true
                        }
                        if (packageDocs[packageName] != null) {
                            reporter.report(
                                Issues.BOTH_PACKAGE_INFO_AND_HTML,
                                unit,
                                "It is illegal to provide both a package-info.java file and a " +
                                    "package.html file for the same package"
                            )
                        }
                        packageDocs[packageName] = text
                    }
                }
            } else {
                for (psiClass in classes) {
                    psiClass.accept(object : JavaRecursiveElementVisitor() {
                        override fun visitErrorElement(element: PsiErrorElement?) {
                            super.visitErrorElement(element)
                            reporter.report(
                                Issues.INVALID_SYNTAX,
                                element,
                                "Syntax error: `${element?.errorDescription}`"
                            )
                        }
                    })

                    val classItem = createClass(psiClass)
                    topLevelClassesFromSource.add(classItem)

                    if (packageName == null) {
                        packageName = getPackageName(psiClass)
                    }
                }
            }
        }

        // Next construct packages
        for ((pkgName, classes) in packageClasses) {
            tick() // show progress
            val psiPackage = JavaPsiFacade.getInstance(project).findPackage(pkgName)
            if (psiPackage == null) {
                println("Could not find package $pkgName")
                continue
            }

            val sortedClasses = classes.toMutableList().sortedWith(ClassItem.fullNameComparator)
            registerPackage(psiPackage, sortedClasses, packageDocs[pkgName], pkgName)
        }

        initializing = false

        emptyPackage = findPackage("")!!

        // Finish initialization
        val initialPackages = ArrayList(packageMap.values)
        var registeredCount = packageMap.size // classes added after this point will have indices >= original
        for (cls in initialPackages) {
            cls.finishInitialization()
        }

        // Finish initialization of any additional classes that were registered during
        // the above initialization (recursively)
        while (registeredCount < packageMap.size) {
            val added = packageMap.values.minus(initialPackages)
            registeredCount = packageMap.size
            for (pkg in added) {
                pkg.finishInitialization()
            }
        }

        // Point to "parent" packages, since doclava treats packages as nested (e.g. an @hide on
        // android.foo will also apply to android.foo.bar)
        addParentPackages(packageMap.values)
    }

    override fun dispose() {
        Disposer.dispose(project)
        super.dispose()
    }

    private fun addParentPackages(packages: Collection<PsiPackageItem>) {
        val missingPackages = packages.mapNotNull {
            val name = it.qualifiedName()
            val index = name.lastIndexOf('.')
            val parent = if (index != -1) {
                name.substring(0, index)
            } else {
                ""
            }
            if (packageMap.containsKey(parent)) {
                // Already registered
                null
            } else {
                parent
            }
        }.toSet()

        // Create PackageItems for any packages that weren't in the source
        for (pkgName in missingPackages) {
            val psiPackage = JavaPsiFacade.getInstance(project).findPackage(pkgName) ?: continue
            val sortedClasses = emptyList<PsiClassItem>()
            val packageHtml = null
            val pkg = registerPackage(psiPackage, sortedClasses, packageHtml, pkgName)
            pkg.emit = false // don't expose these packages in the API signature files, stubs, etc
        }

        // Connect up all the package items
        for (pkg in packageMap.values) {
            var name = pkg.qualifiedName()
            // Find parent package; we have to loop since we don't always find a PSI package
            // for intermediate elements; e.g. we may jump from java.lang straight up to the default
            // package
            while (name.isNotEmpty()) {
                val index = name.lastIndexOf('.')
                name = if (index != -1) {
                    name.substring(0, index)
                } else {
                    ""
                }
                val parent = findPackage(name) ?: continue
                pkg.containingPackageField = parent
                break
            }
        }
    }

    private fun registerPackage(
        psiPackage: PsiPackage,
        sortedClasses: List<PsiClassItem>?,
        packageHtml: String?,
        pkgName: String
    ): PsiPackageItem {
        val packageItem = PsiPackageItem.create(this, psiPackage, packageHtml)
        packageMap[pkgName] = packageItem
        if (isPackageHidden(pkgName)) {
            packageItem.hidden = true
        }

        sortedClasses?.let { packageItem.addClasses(it) }
        return packageItem
    }

    fun initialize(project: Project, jarFile: File, preFiltered: Boolean = false) {
        this.preFiltered = preFiltered
        initializing = true
        hideClassesFromJars = false

        this.project = project

        // Find all classes referenced from the class
        val facade = JavaPsiFacade.getInstance(project)
        val scope = GlobalSearchScope.allScope(project)

        hiddenPackages = HashMap(100)
        packageMap = HashMap(PACKAGE_ESTIMATE)
        packageClasses = HashMap(PACKAGE_ESTIMATE)
        packageClasses[""] = ArrayList()
        this.methodMap = HashMap(1000)
        val packageToClasses: MutableMap<String, MutableList<PsiClassItem>> = HashMap(
            PACKAGE_ESTIMATE
        )
        packageToClasses[""] = ArrayList() // ensure we construct one for the default package

        topLevelClassesFromSource = ArrayList(CLASS_ESTIMATE)

        try {
            ZipFile(jarFile).use { jar ->
                val enumeration = jar.entries()
                while (enumeration.hasMoreElements()) {
                    val entry = enumeration.nextElement()
                    val fileName = entry.name
                    if (fileName.contains("$")) {
                        // skip inner classes
                        continue
                    }
                    if (fileName.endsWith(SdkConstants.DOT_CLASS)) {
                        val qualifiedName = fileName.removeSuffix(SdkConstants.DOT_CLASS).replace('/', '.')
                        if (qualifiedName.endsWith(".package-info")) {
                            // Ensure we register a package for this, even if empty
                            val packageName = qualifiedName.removeSuffix(".package-info")
                            var list = packageToClasses[packageName]
                            if (list == null) {
                                list = mutableListOf()
                                packageToClasses[packageName] = list
                            }
                            continue
                        } else {
                            val psiClass = facade.findClass(qualifiedName, scope) ?: continue

                            val classItem = createClass(psiClass)
                            topLevelClassesFromSource.add(classItem)

                            val packageName = getPackageName(psiClass)
                            var list = packageToClasses[packageName]
                            if (list == null) {
                                list = mutableListOf(classItem)
                                packageToClasses[packageName] = list
                            } else {
                                list.add(classItem)
                            }
                        }
                    }
                }
            }
        } catch (e: IOException) {
            reporter.report(Issues.IO_ERROR, jarFile, e.message ?: e.toString())
        }

        // Next construct packages
        for ((pkgName, packageClasses) in packageToClasses) {
            val psiPackage = JavaPsiFacade.getInstance(project).findPackage(pkgName)
            if (psiPackage == null) {
                println("Could not find package $pkgName")
                continue
            }

            packageClasses.sortWith(ClassItem.fullNameComparator)
            // TODO: How do we obtain the package docs? We generally don't have them, but it *would* be
            // nice if we picked up "overview.html" bundled files and added them. But since the docs
            // are generally missing for all elements *anyway*, let's not bother.
            val docs = packageDocs?.packageDocs
            val packageHtml: String? =
                if (docs != null) {
                    docs[pkgName]
                } else {
                    null
                }
            registerPackage(psiPackage, packageClasses, packageHtml, pkgName)
        }

        emptyPackage = findPackage("")!!

        initializing = false
        hideClassesFromJars = true

        // Finish initialization
        for (pkg in packageMap.values) {
            pkg.finishInitialization()
        }
    }

    fun dumpStats() {
        options.stdout.println(
            "INTERNAL STATS: Size of classMap=${classMap.size} and size of " +
                "methodMap=${methodMap.size} and size of packageMap=${packageMap.size}, and the " +
                "typemap size is ${typeMap.size}, and the packageClasses size is ${packageClasses.size} "
        )
    }

    private fun registerPackageClass(packageName: String, cls: PsiClassItem) {
        var list = packageClasses[packageName]
        if (list == null) {
            list = ArrayList()
            packageClasses[packageName] = list
        }

        list.add(cls)
    }

    private fun isPackageHidden(packageName: String): Boolean {
        val hidden = hiddenPackages[packageName]
        if (hidden == true) {
            return true
        } else if (hidden == null) {
            // Compute for all prefixes of this package
            var pkg = packageName
            while (true) {
                if (hiddenPackages[pkg] != null) {
                    hiddenPackages[packageName] = hiddenPackages[pkg]
                    if (hiddenPackages[pkg] == true) {
                        return true
                    }
                }
                val last = pkg.lastIndexOf('.')
                if (last == -1) {
                    hiddenPackages[packageName] = false
                    break
                } else {
                    pkg = pkg.substring(0, last)
                }
            }
        }

        return false
    }

    private fun createClass(clz: PsiClass): PsiClassItem {
        val classItem = PsiClassItem.create(this, clz)

        if (!initializing && options.hideClasspathClasses) {
            // This class is found while we're no longer initializing all the source units:
            // that means it must be found on the classpath instead. These should be treated
            // as hidden; we don't want to generate code for them.
            classItem.emit = false

            // Workaround: we're pulling in .aidl files from .jar files. These are
            // marked @hide, but since we only see the .class files we don't know that.
            if (classItem.simpleName().startsWith("I") &&
                classItem.isFromClassPath() &&
                clz.interfaces.any { it.qualifiedName == "android.os.IInterface" }
            ) {
                classItem.hidden = true
            }
        }

        if (classItem.classType == ClassType.TYPE_PARAMETER) {
            // Don't put PsiTypeParameter classes into the registry; e.g. when we're visiting
            //  java.util.stream.Stream<R>
            // we come across "R" and would try to place it here.
            classItem.containingPackage = emptyPackage
            classItem.finishInitialization()
            return classItem
        }
        val qualifiedName: String = clz.qualifiedName ?: clz.name!!
        classMap[qualifiedName] = classItem

        // TODO: Cache for adjacent files!
        val packageName = getPackageName(clz)
        registerPackageClass(packageName, classItem)

        if (!initializing) {
            classItem.emit = false
            classItem.finishInitialization()
            val pkgName = getPackageName(clz)
            val pkg = findPackage(pkgName)
            if (pkg == null) {
                // val packageHtml: String? = packageDocs?.packageDocs!![pkgName]
                // dynamically discovered packages should NOT be included
                // val packageHtml = "/** @hide */"
                val packageHtml = null
                val psiPackage = JavaPsiFacade.getInstance(project).findPackage(pkgName)
                if (psiPackage != null) {
                    val packageItem = registerPackage(psiPackage, null, packageHtml, pkgName)
                    // Don't include packages from API that isn't directly included in the API
                    if (options.hideClasspathClasses) {
                        packageItem.emit = false
                    }
                    packageItem.addClass(classItem)
                }
            } else {
                pkg.addClass(classItem)
            }
        }

        return classItem
    }

    override fun getPackages(): PackageList {
        // TODO: Sorting is probably not necessary here!
        return PackageList(this, packageMap.values.toMutableList().sortedWith(PackageItem.comparator))
    }

    override fun getPackageDocs(): PackageDocs? {
        return packageDocs
    }

    override fun size(): Int {
        return packageMap.size
    }

    override fun findPackage(pkgName: String): PsiPackageItem? {
        return packageMap[pkgName]
    }

    override fun findClass(className: String): PsiClassItem? {
        return classMap[className]
    }

    open fun findClass(psiClass: PsiClass): PsiClassItem? {
        val qualifiedName: String = psiClass.qualifiedName ?: psiClass.name!!
        return classMap[qualifiedName]
    }

    open fun findOrCreateClass(qualifiedName: String): PsiClassItem? {
        val finder = JavaPsiFacade.getInstance(project)
        val psiClass = finder.findClass(qualifiedName, GlobalSearchScope.allScope(project)) ?: return null
        return findOrCreateClass(psiClass)
    }

    open fun findOrCreateClass(psiClass: PsiClass): PsiClassItem {
        val existing = findClass(psiClass)
        if (existing != null) {
            return existing
        }

        var curr = psiClass.containingClass
        if (curr != null && findClass(curr) == null) {
            // Make sure we construct outer/top level classes first
            if (findClass(curr) == null) {
                while (true) {
                    val containing = curr?.containingClass
                    if (containing == null) {
                        break
                    } else {
                        curr = containing
                    }
                }
                curr!!
                createClass(curr) // this will also create inner classes, which should now be in the map
                val inner = findClass(psiClass)
                inner!! // should be there now
                return inner
            }
        }

        return existing ?: return createClass(psiClass)
    }

    fun findClass(psiType: PsiType): PsiClassItem? {
        if (psiType is PsiClassType) {
            val cls = psiType.resolve() ?: return null
            return findOrCreateClass(cls)
        } else if (psiType is PsiArrayType) {
            var componentType = psiType.componentType
            // We repeatedly get the component type because the array may have multiple dimensions
            while (componentType is PsiArrayType) {
                componentType = componentType.componentType
            }
            if (componentType is PsiClassType) {
                val cls = componentType.resolve() ?: return null
                return findOrCreateClass(cls)
            }
        }
        return null
    }

    fun getClassType(cls: PsiClass): PsiClassType = getFactory().createType(cls, PsiSubstitutor.EMPTY)

    fun getComment(string: String, parent: PsiElement? = null): PsiDocComment =
        getFactory().createDocCommentFromText(string, parent)

    fun getType(psiType: PsiType): PsiTypeItem {
        // Note: We do *not* cache these; it turns out that storing PsiType instances
        // in a map is bad for performance; it has a very expensive equals operation
        // for some type comparisons (and we sometimes end up with unexpected results,
        // e.g. where we fetch an "equals" type from the map but its representation
        // is slightly different than we intended
        return PsiTypeItem.create(this, psiType)
    }

    fun getType(psiClass: PsiClass): PsiTypeItem {
        return PsiTypeItem.create(this, getFactory().createType(psiClass))
    }

    private fun getPackageName(clz: PsiClass): String {
        var top: PsiClass? = clz
        while (top?.containingClass != null) {
            top = top.containingClass
        }
        top ?: return ""

        val name = top.name
        val fullName = top.qualifiedName ?: return ""

        if (name == fullName) {
            return ""
        }

        return fullName.substring(0, fullName.length - 1 - name!!.length)
    }

    fun findMethod(method: PsiMethod): PsiMethodItem {
        val containingClass = method.containingClass
        val cls = findOrCreateClass(containingClass!!)

        // Ensure initialized/registered via [#registerMethods]
        if (methodMap[cls] == null) {
            val map = HashMap<PsiMethod, PsiMethodItem>(40)
            registerMethods(cls.methods(), map)
            registerMethods(cls.constructors(), map)
            methodMap[cls] = map
        }

        val methods = methodMap[cls]!!
        val methodItem = methods[method]
        if (methodItem == null) {
            // Probably switched psi classes (e.g. used source PsiClass in registry but
            // found duplicate class in .jar library and we're now pointing to it; in that
            // case, find the equivalent method by signature
            val psiClass = cls.psiClass
            val updatedMethod = psiClass.findMethodBySignature(method, true)
            val result = methods[updatedMethod!!]
            if (result == null) {
                val extra = PsiMethodItem.create(this, cls, updatedMethod)
                methods[method] = extra
                methods[updatedMethod] = extra
                if (!initializing) {
                    extra.finishInitialization()
                }

                return extra
            }
            return result
        }

        return methodItem
    }

    fun findField(field: PsiField): Item? {
        val containingClass = field.containingClass ?: return null
        val cls = findOrCreateClass(containingClass)
        return cls.findField(field.name)
    }

    private fun registerMethods(methods: List<MethodItem>, map: MutableMap<PsiMethod, PsiMethodItem>) {
        for (method in methods) {
            val psiMethod = (method as PsiMethodItem).psiMethod
            map[psiMethod] = method
        }
    }

    /**
     * Returns a list of the top-level classes declared in the codebase's source (rather than on its
     * classpath).
     */
    fun getTopLevelClassesFromSource(): List<ClassItem> {
        return topLevelClassesFromSource
    }

    fun createReferenceFromText(s: String, parent: PsiElement? = null): PsiJavaCodeReferenceElement =
        getFactory().createReferenceFromText(s, parent)

    fun createPsiMethod(s: String, parent: PsiElement? = null): PsiMethod =
        getFactory().createMethodFromText(s, parent)

    fun createConstructor(s: String, parent: PsiElement? = null): PsiMethod =
        getFactory().createConstructor(s, parent)

    fun createPsiType(s: String, parent: PsiElement? = null): PsiType =
        getFactory().createTypeFromText(s, parent)

    fun createPsiAnnotation(s: String, parent: PsiElement? = null): PsiAnnotation =
        getFactory().createAnnotationFromText(s, parent)

    fun createDocTagFromText(s: String): PsiDocTag = getFactory().createDocTagFromText(s)

    private fun getFactory() = JavaPsiFacade.getElementFactory(project)

    private var nonNullAnnotationProvider: TypeAnnotationProvider? = null
    private var nullableAnnotationProvider: TypeAnnotationProvider? = null

    /** Type annotation provider which provides androidx.annotation.NonNull */
    fun getNonNullAnnotationProvider(): TypeAnnotationProvider {
        return nonNullAnnotationProvider ?: run {
            val provider = TypeAnnotationProvider.Static.create(arrayOf(createPsiAnnotation("@$ANDROIDX_NONNULL")))
            nonNullAnnotationProvider
            provider
        }
    }

    /** Type annotation provider which provides androidx.annotation.Nullable */
    fun getNullableAnnotationProvider(): TypeAnnotationProvider {
        return nullableAnnotationProvider ?: run {
            val provider = TypeAnnotationProvider.Static.create(arrayOf(createPsiAnnotation("@$ANDROIDX_NULLABLE")))
            nullableAnnotationProvider
            provider
        }
    }

    override fun createAnnotation(
        source: String,
        context: Item?,
        mapName: Boolean
    ): PsiAnnotationItem {
        val psiAnnotation = createPsiAnnotation(source, context?.psi())
        return PsiAnnotationItem.create(this, psiAnnotation)
    }

    override fun supportsDocumentation(): Boolean = true

    override fun toString(): String = description

    fun registerClass(cls: PsiClassItem) {
        assert(classMap[cls.qualifiedName()] == null || classMap[cls.qualifiedName()] == cls)

        classMap[cls.qualifiedName()] = cls
    }
}
