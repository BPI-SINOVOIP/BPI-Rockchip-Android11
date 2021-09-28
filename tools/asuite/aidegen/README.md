AIDEgen aims to automate the project setup process for developers to work on
Java project in popular IDE environment. Developers no longer need to
manually configure an IntelliJ project, such as all the project dependencies.
It's a **command line tool** that offers the following features:

* Configure Intellij or Android Studio project files with the relevant module
  dependencies resolved.

* Launch IDE for a specified sub-project or build target, i.e. frameworks/base
  or Settings.

* Launch IDE for a specified folder which contains build targets, i.e. cts.

* Auto configure JDK and Android coding style for Intellij.

## 1. Prerequisites:

    IDE installed and run $ '. build/envsetup.sh && lunch' under Android source
    root folder.

## 2. Execution:

    $ 'aidegen <module_name>... <module_path>...'
      Example to generate and launch IntelliJ project for framework and
      Settings:
        $ aidegen Settings framework
        $ aidegen packages/apps/Settings frameworks/base
        $ aidegen packages/apps/Settings framework

    $ 'aidegen <module> -i s'
      Example to generate and launch Android Studio project for framework:
        $ aidegen framework -i s

## 3. More argument:

    $ aidegen --help

## 4. FAQ:

    1. Q: If I already have an IDE project file, and I run command AIDEGen to
          generate the same project file again, what’ll happen?
       A: The former IDEA project file will be overwritten by the newly
          generated one from the aidegen command.

    2. Q: When do I need to re-run AIDEGen?
       A: Re-run AIDEGen after repo sync.

    3. Q: Does AIDEGen support debug log dump?
       A: Use aidegen -v to get more debug information.

    4. Q: After the aidegen command run locally, if there’s no IDEA with
          project shown up, what can I do ?
       A: Basic steps to do troubleshooting:
          - Make sure development environment is set up, please refer to
            prerequisites section.
          - Check error message in the aidegen command output.

# Hint
    1. In Intellij, uses [File] > [Invalidate Caches / Restart…] to force
       project panel updated when your IDE didn't sync.

    2. If you run aidegen on a remote desktop, make sure there is no IntelliJ
       running in a different desktop session.
