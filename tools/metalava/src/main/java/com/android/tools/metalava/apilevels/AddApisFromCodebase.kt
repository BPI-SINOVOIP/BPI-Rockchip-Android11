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

package com.android.tools.metalava.apilevels

import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Codebase
import com.android.tools.metalava.model.FieldItem
import com.android.tools.metalava.model.MethodItem
import com.android.tools.metalava.model.visitors.ApiVisitor

/** Visits the API codebase and inserts into the [Api] the classes, methods and fields */
fun addApisFromCodebase(api: Api, apiLevel: Int, codebase: Codebase) {
    codebase.accept(object : ApiVisitor(
        visitConstructorsAsMethods = true,
        nestInnerClasses = false
    ) {

        var currentClass: ApiClass? = null

        override fun afterVisitClass(cls: ClassItem) {
            currentClass = null
        }

        override fun visitClass(cls: ClassItem) {
            val newClass = api.addClass(cls.internalName(), apiLevel, cls.deprecated)
            currentClass = newClass

            if (cls.isClass()) {
                // The jar files historically contain package private parents instead of
                // the real API so we need to correct the data we've already read in

                val filteredSuperClass = cls.filteredSuperclass(filterReference)
                val superClass = cls.superClass()
                if (filteredSuperClass != superClass && filteredSuperClass != null) {
                    val existing = newClass.superClasses.firstOrNull()?.name
                    val superInternalName = superClass?.internalName()
                    if (existing == superInternalName) {
                        // The bytecode used to point to the old hidden super class. Point
                        // to the real one (that the signature files referenced) instead.
                        val removed = newClass.removeSuperClass(superInternalName)
                        val since = removed?.since ?: apiLevel
                        val entry = newClass.addSuperClass(filteredSuperClass.internalName(), since)
                        // Show that it's also seen here
                        entry.update(apiLevel)

                        // Also inherit the interfaces from that API level, unless it was added later
                        val superClassEntry = api.findClass(superInternalName)
                        if (superClassEntry != null) {
                            for (interfaceType in superClass!!.filteredInterfaceTypes(filterReference)) {
                                val interfaceClass = interfaceType.asClass() ?: return
                                var mergedSince = since
                                val interfaceName = interfaceClass.internalName()
                                for (itf in superClassEntry.interfaces) {
                                    val currentInterface = itf.name
                                    if (interfaceName == currentInterface) {
                                        mergedSince = itf.since
                                        break
                                    }
                                }
                                newClass.addInterface(interfaceClass.internalName(), mergedSince)
                            }
                        }
                    } else {
                        newClass.addSuperClass(filteredSuperClass.internalName(), apiLevel)
                    }
                } else if (superClass != null) {
                    newClass.addSuperClass(superClass.internalName(), apiLevel)
                }
            } else if (cls.isInterface()) {
                val superClass = cls.superClass()
                if (superClass != null && !superClass.isJavaLangObject()) {
                    newClass.addInterface(superClass.internalName(), apiLevel)
                }
            } else if (cls.isEnum()) {
                // Implicit super class; match convention from bytecode
                if (newClass.name != "java/lang/Enum") {
                    newClass.addSuperClass("java/lang/Enum", apiLevel)
                }

                // Mimic doclava enum methods
                newClass.addMethod("valueOf(Ljava/lang/String;)L" + newClass.name + ";", apiLevel, false)
                newClass.addMethod("values()[L" + newClass.name + ";", apiLevel, false)
            } else if (cls.isAnnotationType()) {
                // Implicit super class; match convention from bytecode
                if (newClass.name != "java/lang/annotation/Annotation") {
                    newClass.addSuperClass("java/lang/Object", apiLevel)
                    newClass.addInterface("java/lang/annotation/Annotation", apiLevel)
                }
            }

            // Ensure we don't end up with
            //    -  <extends name="java/lang/Object"/>
            //    +  <extends name="java/lang/Object" removed="29"/>
            // which can happen because the bytecode always explicitly contains extends java.lang.Object
            // but in the source code we don't see it, and the lack of presence of this shouldn't be
            // taken as a sign that we no longer extend object. But only do this if the class didn't
            // previously extend object and now extends something else.
            if ((cls.isClass() || cls.isInterface()) &&
                newClass.superClasses.size == 1 &&
                newClass.superClasses[0].name == "java/lang/Object") {
                newClass.addSuperClass("java/lang/Object", apiLevel)
            }

            for (interfaceType in cls.filteredInterfaceTypes(filterReference)) {
                val interfaceClass = interfaceType.asClass() ?: return
                newClass.addInterface(interfaceClass.internalName(), apiLevel)
            }
        }

        override fun visitMethod(method: MethodItem) {
            if (method.isPrivate || method.isPackagePrivate) {
                return
            }
            val name = method.internalName() +
                // Use "V" instead of the type of the constructor for backwards compatibility
                // with the older bytecode
                method.internalDesc(voidConstructorTypes = true)
            currentClass?.addMethod(name, apiLevel, method.deprecated)
        }

        override fun visitField(field: FieldItem) {
            if (field.isPrivate || field.isPackagePrivate) {
                return
            }

            currentClass?.addField(field.internalName(), apiLevel, field.deprecated)
        }
    })
}