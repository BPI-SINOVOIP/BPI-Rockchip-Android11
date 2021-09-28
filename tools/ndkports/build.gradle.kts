import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

plugins {
    kotlin("jvm") version "1.3.50"
    application
}

group = "com.android"
version = "1.0.0-SNAPSHOT"

repositories {
    mavenCentral()
    jcenter()
    google()
    maven(url = "https://dl.bintray.com/s1m0nw1/KtsRunner")
}

dependencies {
    implementation(kotlin("stdlib", "1.3.50"))
    implementation(kotlin("reflect", "1.3.50"))

    implementation("com.google.prefab:api:1.0.0-alpha2")

    implementation("com.github.ajalt:clikt:2.2.0")
    implementation("com.squareup.okhttp3:okhttp:4.2.2")
    implementation("de.swirtz:ktsRunner:0.0.7")
    implementation("org.apache.maven:maven-core:3.6.2")
    implementation("org.redundent:kotlin-xml-builder:1.5.3")

    testImplementation("org.jetbrains.kotlin:kotlin-test")
    testImplementation("org.jetbrains.kotlin:kotlin-test-junit")
    testImplementation("org.junit.jupiter:junit-jupiter-api:5.6.0-M1")
    testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine:5.6.0-M1")
}

application {
    // Define the main class for the application.
    mainClassName = "com.android.ndkports.CliKt"
}

tasks.withType<KotlinCompile> {
    kotlinOptions.jvmTarget = "1.8"
    kotlinOptions.freeCompilerArgs += listOf(
        "-progressive",
        "-Xuse-experimental=kotlinx.serialization.ImplicitReflectionSerializer"
    )
}

// Can be specified in ~/.gradle/gradle.properties:
//
//     ndkPath=/path/to/ndk
//
// Or on the command line:
//
//     ./gradlew -PndkPath=/path/to/ndk run
val ndkPath: String by project
tasks.named<JavaExec>("run") {
    val allPorts = File("ports").listFiles()!!.map { it.name }
    args = listOf("--ndk", ndkPath, "-o", "out") + allPorts
}