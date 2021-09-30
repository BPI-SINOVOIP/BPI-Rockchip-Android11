# AndroidX (formerly known as Android Support) libraries have moved
`frameworks/support` is no longer part of the platform checkout. Android platform now uses prebuilts of AndroidX for everything. AndroidX is being developed as an unbundled project.

# Next steps
- For source-code for your IDE, please use `*-sources.jar`s in `prebuilts/sdk/current/androidx`
- To contribute, check out AOSP AndroidX repository ([guide here](https://android.googlesource.com/platform/frameworks/support/+/androidx-master-dev/README.md)).
    - `repo init -u https://android.googlesource.com/platform/manifest -b androidx-master-dev`
- For secret platform work, you can use internal `ub-supportlib-master` repo.
    - `repo init -u persistent-https://googleplex-android.git.corp.google.com/platform/manifest -b ub-supportlib-master`
