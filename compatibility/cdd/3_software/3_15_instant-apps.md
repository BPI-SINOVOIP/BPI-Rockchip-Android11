## 3.15\. Instant Apps

Device implementations MUST satisfy the following requirements:

*   [C-0-1] Instant Apps MUST only be granted permissions that have the
    [`android:protectionLevel`](
    https://developer.android.com/reference/android/R.attr#protectionLevel)
    set to `"instant"`.
*   [C-0-2] Instant Apps MUST NOT interact with installed apps via [implicit intents](https://developer.android.com/reference/android/content/Intent.html)
    unless one of the following is true:
    *   The component's intent pattern filter is exposed and has CATEGORY_BROWSABLE
    *   The action is one of ACTION_SEND, ACTION_SENDTO, ACTION_SEND_MULTIPLE
    *   The target is explicitly exposed with [android:visibleToInstantApps](https://developer.android.com/reference/android/R.attr.html#visibleToInstantApps)
*   [C-0-3] Instant Apps MUST NOT interact explicitly with installed apps unless the
    component is exposed via android:visibleToInstantApps.
*   [C-0-4] Installed Apps MUST NOT see details about Instant Apps on the
    device unless the Instant App explicitly connects to the
    installed application.
*   Device implementations MUST provide the following user affordances for
    interacting with Instant Apps. The AOSP meets the requirements with the
    default System UI, Settings, and Launcher. Device implementations:
    *   [C-0-5] MUST provide a user affordance to view and delete Instant Apps
        locally cached for each individual app package.
    *   [C-0-6] MUST provide a persistent user notification that can be
        collapsed while an Instant App is running in the foreground. This user
        notification MUST include that Instant Apps do not require installation
        and provide a user affordance that directs the user to the application
        info screen in Settings. For Instant Apps launched via web intents, as
        defined by using an intent with action set to `Intent.ACTION_VIEW` and
        with a scheme of "http" or "https", an additional user affordance
        SHOULD allow the user not to launch the Instant App and
        launch the associated link with the configured web browser, if a browser
        is available on the device.
    *   [C-0-7] MUST allow running Instant Apps to be accessed from the Recents
        function if the Recents function is available on the device.

If device implementations support Instant Apps, they:

*   [C-1-1] MUST preload one or more applications or service components
    with an intent handler for the intents listed in the SDK [here](https://developer.android.com/topic/google-play-instant/instant-app-intents)
    and make the intents visible for Instant Apps.