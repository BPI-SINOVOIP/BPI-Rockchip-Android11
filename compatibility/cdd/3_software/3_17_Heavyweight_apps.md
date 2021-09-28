## 3.17\. Heavyweight Apps

If device implementations declare the feature [`FEATURE_CANT_SAVE_STATE`](
https://developer.android.com/reference/android/content/pm/PackageManager.html#FEATURE_CANT_SAVE_STATE),
then they:

*    [C-1-1] MUST have only one installed app that specifies
     [`cantSaveState`](https://developer.android.com/reference/android/R.attr#cantSaveState)
     running in the system at a time. If the user
     leaves such an app without explicitly exiting it (for example by pressing
     home while leaving an active activity the system, instead of pressing back
     with no remaining active activities in the system), then
     device implementations MUST prioritize that app in RAM as they do for other
     things that are expected to remain running, such as foreground services.
     While such an app is in the background, the system can still apply power
     management features to it, such as limiting CPU and network access.
*    [C-1-2] MUST provide a UI affordance to chose the app that won't
     participate in the normal state save/restore mechanism once the user
     launches a second app declared with [`cantSaveState`](https://developer.android.com/reference/android/R.attr#cantSaveState)
     attribute.
*    [C-1-3] MUST NOT apply other changes in policy to apps that specify
     [`cantSaveState`](https://developer.android.com/reference/android/R.attr#cantSaveState),
     such as changing CPU performance or changing scheduling prioritization.

If device implementations don't declare the feature [`FEATURE_CANT_SAVE_STATE`](
https://developer.android.com/reference/android/content/pm/PackageManager.html#FEATURE_CANT_SAVE_STATE),
then they:

*    [C-1-1] MUST ignore the [`cantSaveState`](https://developer.android.com/reference/android/R.attr#cantSaveState)
     attribute set by apps and MUST NOT change the app behavior based on that
     attribute.
