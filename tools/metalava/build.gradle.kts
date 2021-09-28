import com.github.jengelman.gradle.plugins.shadow.tasks.ShadowJar
import org.jetbrains.kotlin.gradle.tasks.KotlinCompile
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.util.Properties

buildscript {
    repositories {
        jcenter()
    }
    dependencies {
        classpath("com.github.jengelman.gradle.plugins:shadow:4.0.4")
    }
}

buildDir = getBuildDirectory()

defaultTasks = listOf("installDist", "test", "shadowJar", "createArchive", "ktlint")

repositories {
    google()
    jcenter()
}

plugins {
    kotlin("jvm") version "1.3.20"
    id("application")
    id("java")
    id("com.github.johnrengelman.shadow") version "4.0.4"
    id("maven-publish")
}

group = "com.android"
version = getMetalavaVersion()

application {
    mainClassName = "com.android.tools.metalava.Driver"
    applicationDefaultJvmArgs = listOf("-ea", "-Xms2g", "-Xmx4g")
}

java {
    sourceCompatibility = JavaVersion.VERSION_1_8
    targetCompatibility = JavaVersion.VERSION_1_8
}

tasks.withType(KotlinCompile::class.java) {
    sourceCompatibility = "1.8"
    targetCompatibility = "1.8"

    kotlinOptions {
        jvmTarget = "1.8"
        apiVersion = "1.3"
        languageVersion = "1.3"
    }
}

val studioVersion: String = "26.5.0"
val kotlinVersion: String = "1.3.20"

dependencies {
    implementation("com.android.tools.external.org-jetbrains:uast:$studioVersion")
    implementation("com.android.tools.external.com-intellij:intellij-core:$studioVersion")
    implementation("com.android.tools.lint:lint-api:$studioVersion")
    implementation("com.android.tools.lint:lint-checks:$studioVersion")
    implementation("com.android.tools.lint:lint-gradle:$studioVersion")
    implementation("com.android.tools.lint:lint:$studioVersion")
    implementation("org.jetbrains.kotlin:kotlin-stdlib-jdk8:$kotlinVersion")
    implementation("org.jetbrains.kotlin:kotlin-reflect:$kotlinVersion")
    testImplementation("com.android.tools.lint:lint-tests:$studioVersion")
    testImplementation("junit:junit:4.11")
}

tasks.withType(ShadowJar::class.java) {
    archiveBaseName.set("metalava-full-${project.version}")
    archiveClassifier.set(null as String?)
    archiveVersion.set(null as String?)
    setZip64(true)
    destinationDirectory.set(getDistributionDirectory())
}

tasks.withType(Test::class.java) {
    val zipTask = project.tasks.register("zipResultsOf${name.capitalize()}", Zip::class.java) {
        destinationDirectory.set(File(getDistributionDirectory(), "host-test-reports"))
        archiveFileName.set("metalava-tests.zip")
    }
    if (isBuildingOnServer()) ignoreFailures = true
    finalizedBy(zipTask)
    doFirst {
        zipTask.configure {
            from(reports.junitXml.destination)
        }
    }
}

fun getMetalavaVersion(): Any {
    val versionPropertyFile = File(projectDir, "src/main/resources/version.properties")
    if (versionPropertyFile.canRead()) {
        val versionProps = Properties()
        versionProps.load(FileInputStream(versionPropertyFile))
        val metalavaVersion = versionProps["metalavaVersion"]
            ?: throw IllegalStateException("metalava version was not set in ${versionPropertyFile.absolutePath}")
        return if (isBuildingOnServer()) {
            metalavaVersion
        } else {
            // Local builds are not public release candidates.
            "$metalavaVersion-SNAPSHOT"
        }
    } else {
        throw FileNotFoundException("Could not read ${versionPropertyFile.absolutePath}")
    }
}

fun getBuildDirectory(): File {
    return if (System.getenv("OUT_DIR") != null) {
        File(System.getenv("OUT_DIR"), "host/common/metalava")
    } else {
        File("../../out/host/common")
    }
}

/**
 * The build server will copy the contents of the distribution directory and make it available for
 * download.
 */
fun getDistributionDirectory(): File {
    return if (System.getenv("DIST_DIR") != null) {
        File(System.getenv("DIST_DIR"))
    } else {
        File("../../out/dist")
    }
}

fun isBuildingOnServer(): Boolean {
    return System.getenv("OUT_DIR") != null && System.getenv("DIST_DIR") != null
}

/**
 * @return build id string for current build
 *
 * The build server does not pass the build id so we infer it from the last folder of the
 * distribution directory name.
 */
fun getBuildId(): String {
    return if (System.getenv("DIST_DIR") != null) File(System.getenv("DIST_DIR")).name else "0"
}

// KtLint: https://github.com/shyiko/ktlint

fun Project.getKtlintConfiguration(): Configuration {
    return configurations.findByName("ktlint") ?: configurations.create("ktlint") {
        val dependency = project.dependencies.create("com.pinterest:ktlint:0.33.0")
        dependencies.add(dependency)
    }
}

tasks.register("ktlint", JavaExec::class.java) {
    description = "Check Kotlin code style."
    group = "Verification"
    classpath = getKtlintConfiguration()
    main = "com.pinterest.ktlint.Main"
    args = listOf("src/**/*.kt", "build.gradle.kts")
}

tasks.register("ktlintFormat", JavaExec::class.java) {
    description = "Fix Kotlin code style deviations."
    group = "formatting"
    classpath = getKtlintConfiguration()
    main = "com.pinterest.ktlint.Main"
    args = listOf("-F", "src/**/*.kt", "build.gradle.kts")
}

val libraryName = "Metalava"
val repositoryName = "Dist"

publishing {
    publications {
        create<MavenPublication>(libraryName) {
            from(components["java"])
            pom {
                licenses {
                    license {
                        name.set("The Apache License, Version 2.0")
                        url.set("http://www.apache.org/licenses/LICENSE-2.0.txt")
                    }
                }
                developers {
                    developer {
                        name.set("The Android Open Source Project")
                    }
                }
                scm {
                    connection.set("scm:git:https://android.googlesource.com/platform/tools/metalava")
                    url.set("https://android.googlesource.com/platform/tools/metalava/")
                }
            }
        }
    }

    repositories {
        maven {
            name = repositoryName
            url = uri("file://${getDistributionDirectory().canonicalPath}/repo/m2repository")
        }
    }
}

tasks.register("createArchive", Zip::class.java) {
    description = "Create a zip of the library in a maven format"
    group = "publishing"

    from("${getDistributionDirectory().canonicalPath}/repo")
    archiveFileName.set("top-of-tree-m2repository-all-${getBuildId()}.zip")
    destinationDirectory.set(getDistributionDirectory())
    dependsOn("publish${libraryName}PublicationTo${repositoryName}Repository")
}
