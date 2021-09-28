# **Asuite IntelliJ plugin**

## **Development**

#### How to build/install the plugin

**Build:** `$./gradlew jar`

The artifact will be generated at build/libs/asuite_plugin-1.0.jar.

**Install:**  Place the asuite_plugin-1.0.jar into the IntelliJ/plugins
 directory. The typical path of IntelliJ is /opt/intellij.

**Debug in IntelliJ:** Edit configurations -> use `+` to add a Gradle configuration -> fill

gradle project:`Absolute path of asuite_plugin`

Tasks: `:runIde`

**Quick run in IntelliJ sandbox:** `$./gradlew :runIde`

#### Quick start

1. Click Atest button, the Atest tool window shall show up.
2. Fill in the test module.
    * Enter a target module, e.g. aidegen_unittests.
    * Or fill target path with check test_mapping checkbox, E.g.
    tools/tradefederation/core.
3. Click Run, the test result will be shown in Atest tool window.

