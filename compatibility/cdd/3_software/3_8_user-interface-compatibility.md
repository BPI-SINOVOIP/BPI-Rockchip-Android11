## 3.8\. User Interface Compatibility

### 3.8.1\. Launcher (Home Screen)

Android includes a launcher application (home screen) and support for
third-party applications to replace the device launcher (home screen).

If device implementations allow third-party applications to replace the device
home screen, they:

*   [C-1-1] MUST declare the platform feature `android.software.home_screen`.
*   [C-1-2] MUST return the [`AdaptiveIconDrawable`](
    https://developer.android.com/reference/android/graphics/drawable/AdaptiveIconDrawable.html)
    object when the third-party application use `<adaptive-icon>` tag to provide
    their icon, and the [`PackageManager`](
    https://developer.android.com/reference/android/content/pm/PackageManager.html)
    methods to retrieve icons are called.

If device implementations include a default launcher that supports in-app
pinning of shortcuts, they:

*   [C-2-1] MUST report `true` for
    [`ShortcutManager.isRequestPinShortcutSupported()`](
    https://developer.android.com/reference/android/content/pm/ShortcutManager.html#isRequestPinShortcutSupported%28%29).
*   [C-2-2] MUST have user affordance asking the user before adding a shortcut requested
    by apps via the [`ShortcutManager.requestPinShortcut()`](
    https://developer.android.com/reference/android/content/pm/ShortcutManager.html#requestPinShortcut%28android.content.pm.ShortcutInfo, android.content.IntentSender%29)
    API method.
*   [C-2-3] MUST support pinned shortcuts and dynamic and static
    shortcuts as documented on the [App Shortcuts page](
    https://developer.android.com/guide/topics/ui/shortcuts.html).

Conversely, if device implementations do not support in-app pinning of
shortcuts, they:

*   [C-3-1] MUST report `false` for
    [`ShortcutManager.isRequestPinShortcutSupported()`](
    https://developer.android.com/reference/android/content/pm/ShortcutManager.html#isRequestPinShortcutSupported%28%29).

If device implementations implement a default launcher that provides quick
access to the additional shortcuts provided by third-party apps through the
[ShortcutManager](
https://developer.android.com/reference/android/content/pm/ShortcutManager.html)
API, they:

*   [C-4-1] MUST support all documented shortcut features (e.g. static and
    dynamic shortcuts, pinning shortcuts) and fully implement the APIs of the
    [`ShortcutManager`](
    https://developer.android.com/reference/android/content/pm/ShortcutManager.html)
    API class.

If device implementations include a default launcher app that shows badges for
the app icons, they:

*   [C-5-1] MUST respect the [`NotificationChannel.setShowBadge()`](
    https://developer.android.com/reference/android/app/NotificationChannel.html#setShowBadge%28boolean%29)
    API method.
    In other words, show a visual affordance associated with the app icon if the
    value is set as `true`, and do not show any app icon badging scheme when all
    of the app's notification channels have set the value as `false`.
*   MAY override the app icon badges with their proprietary badging scheme when
    third-party applications indicate support of the proprietary badging scheme
    through the use of proprietary APIs, but SHOULD use the resources and values
    provided through the notification badges APIs described in [the SDK](
    https://developer.android.com/preview/features/notification-badges.html)
    , such as the [`Notification.Builder.setNumber()`](
    http://developer.android.com/reference/android/app/Notification.Builder.html#setNumber%28int%29)
    and the [`Notification.Builder.setBadgeIconType()`](
    http://developer.android.com/reference/android/app/Notification.Builder.html#setBadgeIconType%28int%29)
    API.

### 3.8.2\. Widgets

Android supports third-party app widgets by defining a component type and
corresponding API and lifecycle that allows applications to expose an
[“AppWidget”](http://developer.android.com/guide/practices/ui_guidelines/widget_design.html)
to the end user.

If device implementations support third-party app widgets, they:

*   [C-1-1] MUST declare support for platform feature
    `android.software.app_widgets`.
*   [C-1-2] MUST include built-in support for AppWidgets and expose
    user interface affordances to add, configure, view, and remove AppWidgets
    directly within the Launcher.
*   [C-1-3] MUST be capable of rendering widgets that are 4 x 4
    in the standard grid size. See the [App Widget DesignGuidelines](
    http://developer.android.com/guide/practices/ui_guidelines/widget_design.html)
    in the Android SDK documentation for details.
*   MAY support application widgets on the lock screen.

If device implementations support third-party app widgets and in-app
pinning of shortcuts, they:

*   [C-2-1] MUST report `true` for
    [`AppWidgetManager.html.isRequestPinAppWidgetSupported()`](
    https://developer.android.com/reference/android/appwidget/AppWidgetManager.html#isRequestPinAppWidgetSupported%28%29).
*   [C-2-2] MUST have user affordance asking the user before adding a shortcut requested
    by apps via the [`AppWidgetManager.requestPinAppWidget()`](
    https://developer.android.com/reference/android/appwidget/AppWidgetManager.html#requestPinAppWidget%28android.content.ComponentName,android.os.Bundle, android.app.PendingIntent%29)
    API method.

### 3.8.3\. Notifications

Android includes [`Notification`](
https://developer.android.com/reference/android/app/Notification.html) and
[`NotificationManager`](
https://developer.android.com/reference/android/app/NotificationManager.html)
APIs that allow third-party app developers to notify users of notable events and
attract users' attention using the hardware components (e.g. sound, vibration
and light) and software features (e.g. notification shade, system bar) of the
device.

#### 3.8.3.1\. Presentation of Notifications

If device implementations allow third-party apps to [notify users of notable events](
http://developer.android.com/guide/topics/ui/notifiers/notifications.html), they:

*   [C-1-1] MUST support notifications that use hardware features, as described in
    the SDK documentation, and to the extent possible with the device implementation
    hardware. For instance, if a device implementation includes a vibrator, it MUST
    correctly implement the vibration APIs. If a device implementation lacks
    hardware, the corresponding APIs MUST be implemented as no-ops. This behavior is
    further detailed in [section 7](#7_hardware_compatibility).
*   [C-1-2]  MUST correctly render all [resources](
    https://developer.android.com/guide/topics/resources/available-resources.html)
    (icons, animation files, etc.) provided for in the APIs, or in the
    Status/System Bar [icon style guide](
    http://developer.android.com/design/style/iconography.html), although they
    MAY provide an alternative user experience for notifications than that
    provided by the reference Android Open Source implementation.
*   [C-1-3] MUST honor and implement properly the behaviors described for
    [the APIs](
    https://developer.android.com/guide/topics/ui/notifiers/notifications.html#Managing)
    to update, remove and group notifications.
*   [C-1-4] MUST provide the full behavior of the [NotificationChannel](
    https://developer.android.com/reference/android/app/NotificationChannel.html)
    API documented in the SDK.
*   [C-1-5] MUST provide a user affordance to block and modify a certain
    third-party app's notification per each channel and app package level.
*   [C-1-6] MUST also provide a user affordance to display deleted notification
    channels.
*   [C-1-7] MUST correctly render all resources (images, stickers, icons, etc.)
    provided through [Notification.MessagingStyle](
    https://developer.android.com/reference/android/app/Notification.MessagingStyle)
    alongside the notification text without additional user interaction. For
    example, MUST show all resources including icons provided through
    [android.app.Person](https://developer.android.com/reference/android/app/Person)
    in a group conversation that is set through [setGroupConversation](
    https://developer.android.com/reference/android/app/Notification.MessagingStyle.html?hl=es-AR#setGroupConversation%28boolean%29).
*   [C-SR] Are STRONGLY RECOMMENDED to automatically surface a user affordance
    to block a certain third-party app's notification per each channel and app
    package level after the user dismisses that notification multiple times.
*   SHOULD support rich notifications.
*   SHOULD present some higher priority notifications as heads-up notifications.
*   SHOULD have a user affordance to snooze notifications.
*   MAY only manage the visibility and timing of when third-party apps can notify
    users of notable events to mitigate safety issues such as driver distraction.

Android 11 introduces support for conversation notifications, which are
notifications that use [MessagingStyle](https://developer.android.com/reference/android/app/Notification.MessagingStyle.html)
and provides a published [People](https://developer.android.com/reference/android/app/Person) Shortcut ID.

Device implementations:

*   [C-SR] Are STRONGLY RECOMMENDED to group and display
    [`conversation notifications`](https://developer.android.com/preview/features/conversations#api-notifications)
    ahead of non conversation notifications with the exception of
    ongoing foreground service notifications and [`importance:high`](https://developer.android.com/reference/android/app/NotificationManager#IMPORTANCE_HIGH)
    notifications.

If device implementations support [`conversation notifications`](https://developer.android.com/preview/features/conversations#api-notifications)
and the app provides the required data for
[`bubbles`](https://developer.android.com/guide/topics/ui/bubbles), they:

*   [C-SR] Are STRONGLY RECOMMENDED to display this conversation as a bubble.
    The AOSP implementation meets these requirements with the default System UI,
    Settings, and Launcher.

If device implementations support rich notifications, they:

*   [C-2-1] MUST use the exact resources as
    provided through the [`Notification.Style`](
    https://developer.android.com/reference/android/app/Notification.Style.html)
    API class and its subclasses for the presented resource elements.
*   SHOULD present each and every resource element (e.g.
    icon, title and summary text) defined in the [`Notification.Style`](
    https://developer.android.com/reference/android/app/Notification.Style.html)
    API class and its subclasses.

If device implementations support heads-up notifications: they:

*   [C-3-1] MUST use the heads-up notification view and resources
    as described in the [`Notification.Builder`](
    https://developer.android.com/reference/android/app/Notification.Builder.html)
    API class when heads-up notifications are presented.
*   [C-3-2] MUST display the actions provided through
    [`Notification.Builder.addAction()`](
    https://developer.android.com/reference/android/app/Notification.Builder#addAction%28android.app.Notification.Action%29)
    together with the notification content without additional user interaction
    as described in [the SDK](
    https://developer.android.com/guide/topics/ui/notifiers/notifications.html#Heads-up).

#### 3.8.3.2\. Notification Listener Service

Android includes the [`NotificationListenerService`](
https://developer.android.com/reference/android/service/notification/NotificationListenerService.html)
APIs that allow apps (once explicitly enabled by the user) to receive a copy of
all notifications as they are posted or updated.

Device implementations:

*   [C-0-1] MUST correctly and promptly update notifications in their entirety
    to all such installed and user-enabled listener services, including any and
    all metadata attached to the Notification object.
*   [C-0-2] MUST respect the [`snoozeNotification()`](
    https://developer.android.com/reference/android/service/notification/NotificationListenerService.html#snoozeNotification%28java.lang.String, long%29)
    API call, and dismiss the notification and make a callback after the snooze
    duration that is set in the API call.

If device implementations have a user affordance to snooze notifications, they:

*   [C-1-1] MUST reflect the snoozed notification status properly
    through the standard APIs such as
    [`NotificationListenerService.getSnoozedNotifications()`](
    https://developer.android.com/reference/android/service/notification/NotificationListenerService.html#getSnoozedNotifications%28%29).
*   [C-1-2] MUST make this user affordance available to snooze notifications
    from each installed third-party app's, unless they are from
    persistent/foreground services.

#### 3.8.3.3\. DND (Do not Disturb)

If device implementations support the DND feature, they:

*   [C-1-1] MUST, for when the device implementation has provided a means for the user
    to grant or deny third-party apps to access the DND policy configuration,
    display [Automatic DND rules](
    https://developer.android.com/reference/android/app/NotificationManager.html#addAutomaticZenRule%28android.app.AutomaticZenRule%29)
    created by applications alongside the user-created and pre-defined rules.
*   [C-1-3] MUST honor the [`suppressedVisualEffects`](https://developer.android.com/reference/android/app/NotificationManager.Policy.html#suppressedVisualEffects)
    values passed along the [`NotificationManager.Policy`](https://developer.android.com/reference/android/app/NotificationManager.Policy.html#NotificationManager.Policy%28int, int, int, int%29)
    and if an app has set any of the SUPPRESSED_EFFECT_SCREEN_OFF or
    SUPPRESSED_EFFECT_SCREEN_ON flags, it SHOULD indicate to the user that the
    visual effects are suppressed in the DND settings menu.

### 3.8.4\. Search

Android includes APIs that allow developers to
[incorporate search](http://developer.android.com/reference/android/app/SearchManager.html)
into their applications and expose their application’s data into the global
system search. Generally speaking, this functionality consists of a single,
system-wide user interface that allows users to enter queries, displays
suggestions as users type, and displays results. The Android APIs allow
developers to reuse this interface to provide search within their own apps and
allow developers to supply results to the common global search user interface.

*   Android device implementations SHOULD include global search, a single, shared,
system-wide search user interface capable of real-time suggestions in response
to user input.

If device implementations implement the global search interface, they:

*   [C-1-1] MUST implement the APIs that allow third-party applications to add
    suggestions to the search box when it is run in global search mode.

If no third-party applications are installed that make use of the global search:

*   The default behavior SHOULD be to display web search engine results and
    suggestions.

Android also includes the [Assist APIs](
https://developer.android.com/reference/android/app/assist/package-summary.html)
to allow applications to elect how much information of the current context is
shared with the assistant on the device.

If device implementations support the Assist action, they:

*   [C-2-1] MUST indicate clearly to the end user when the context is shared, by
    either:
    *   Each time the assist app accesses the context, displaying a white
    light around the edges of the screen that meet or exceed the duration and
    brightness of the Android Open Source Project implementation.
    *   For the preinstalled assist app, providing a user affordance less
    than two navigations away from
    [the default voice input and assistant app settings menu](#3_2_3_5_default_app_settings),
    and only sharing the context when the assist app is explicitly invoked by
    the user through a hotword or assist navigation key input.
*   [C-2-2] The designated interaction to launch the assist app as described
    in [section 7.2.3](#7_2_3_navigation_keys) MUST launch the user-selected
    assist app, in other words the app that implements `VoiceInteractionService`,
    or an activity handling the `ACTION_ASSIST` intent.

### 3.8.5\. Alerts and Toasts

Applications can use the [`Toast`](
http://developer.android.com/reference/android/widget/Toast.html)
API to display short non-modal strings to the end user that disappear after a
brief period of time, and use the [`TYPE_APPLICATION_OVERLAY`](
http://developer.android.com/reference/android/view/WindowManager.LayoutParams.html#TYPE_APPLICATION_OVERLAY)
window type API to display alert windows as an overlay over other apps.

If device implementations include a screen or video output, they:

*   [C-1-1] MUST provide a user affordance to block an app from displaying alert
windows that use the [`TYPE_APPLICATION_OVERLAY`](
http://developer.android.com/reference/android/view/WindowManager.LayoutParams.html#TYPE_APPLICATION_OVERLAY)
. The AOSP implementation meets this requirement by having controls in the notification shade.

*   [C-1-2] MUST honor the Toast API and display Toasts from applications to end users in some highly
visible manner.

### 3.8.6\. Themes

Android provides “themes” as a mechanism for applications to apply styles across
an entire Activity or application.

Android includes a “Holo” and "Material" theme family as a set of defined styles
for application developers to use if they want to match the
[Holo theme look and feel](http://developer.android.com/guide/topics/ui/themes.html)
as defined by the Android SDK.

If device implementations include a screen or video output, they:

*   [C-1-1] MUST NOT alter any of the [Holo theme attributes](
    http://developer.android.com/reference/android/R.style.html) exposed to
    applications.
*   [C-1-2] MUST support the “Material” theme family and MUST NOT alter any of
    the [Material theme attributes](
    http://developer.android.com/reference/android/R.style.html#Theme_Material)
    or their assets exposed to applications.
*   [C-1-3] MUST use the [Roboto version 2.x](https://github.com/google/roboto)
    fonts as the out-of-box Sans-serif font family for languages that Roboto
    supports.

Android also includes a “Device Default” theme family as a set of defined styles
for application developers to use if they want to match the look and feel of the
device theme as defined by the device implementer.

*   Device implementations MAY modify the [Device Default theme attributes](
    http://developer.android.com/reference/android/R.style.html) exposed to
    applications.

Android supports a variant theme with translucent system bars, which allows
application developers to fill the area behind the status and navigation bar
with their app content. To enable a consistent developer experience in this
configuration, it is important the status bar icon style is maintained across
different device implementations.

If device implementations include a system status bar, they:

*   [C-2-1] MUST use white for system status icons (such as signal strength and
    battery level) and notifications issued by the system, unless the icon is
    indicating a problematic status or an app requests a light status bar using
    the SYSTEM_UI_FLAG_LIGHT_STATUS_BAR flag.
*   [C-2-2] Android device implementations MUST change the color of the system
    status icons to black (for details, refer to [R.style](
    http://developer.android.com/reference/android/R.style.html)) when an app
    requests a light status bar.

### 3.8.7\. Live Wallpapers

Android defines a component type and corresponding API and lifecycle that allows
applications to expose one or more
[“Live Wallpapers”](http://developer.android.com/reference/android/service/wallpaper/WallpaperService.html)
to the end user. Live wallpapers are animations, patterns, or similar images
with limited input capabilities that display as a wallpaper, behind other
applications.

Hardware is considered capable of reliably running live wallpapers if it can run
all live wallpapers, with no limitations on functionality, at a reasonable frame
rate with no adverse effects on other applications. If limitations in the
hardware cause wallpapers and/or applications to crash, malfunction, consume
excessive CPU or battery power, or run at unacceptably low frame rates, the
hardware is considered incapable of running live wallpaper. As an example, some
live wallpapers may use an OpenGL 2.0 or 3.x context to render their content.
Live wallpaper will not run reliably on hardware that does not support multiple
OpenGL contexts because the live wallpaper use of an OpenGL context may conflict
with other applications that also use an OpenGL context.

*   Device implementations capable of running live wallpapers reliably as described
above SHOULD implement live wallpapers.

If device implementations implement live wallpapers, they:

*   [C-1-1] MUST report the platform feature flag android.software.live_wallpaper.

### 3.8.8\. Activity Switching

The upstream Android source code includes the
[overview screen](https://developer.android.com/guide/components/activities/recents.html), a
system-level user interface for task switching and displaying recently accessed
activities and tasks using a thumbnail image of the application’s graphical
state at the moment the user last left the application.

Device implementations
including the recents function navigation key as detailed in
[section 7.2.3](#7_2_3_navigation_keys) MAY alter the interface.

If device implementations including the recents function navigation key as detailed in
[section 7.2.3](#7_2_3_navigation_keys) alter the interface, they:

*   [C-1-1] MUST support at least up to 7 displayed activities.
*   SHOULD at least display the title of 4 activities at a time.
*   [C-1-2] MUST implement the [screen pinning behavior](http://developer.android.com/about/versions/android-5.0.html#ScreenPinning)
    and provide the user with a settings menu to toggle the feature.
*   SHOULD display highlight color, icon, screen title in recents.
*   SHOULD display a closing affordance ("x") but MAY delay this until user interacts with screens.
*   SHOULD implement a shortcut to switch easily to the previous activity.
*   SHOULD trigger the fast-switch action between the two most recently used
    apps, when the recents function key is tapped twice.
*   SHOULD trigger the split-screen multiwindow-mode, if supported, when the
    recents functions key is long pressed.
*   MAY display affiliated recents as a group that moves together.
*   [SR] Are STRONGLY RECOMMENDED to use the upstream Android user
interface (or a similar thumbnail-based interface) for the overview screen.

### 3.8.9\. Input Management

Android includes support for
[Input Management](http://developer.android.com/guide/topics/text/creating-input-method.html)
and support for third-party input method editors.

If device implementations allow users to use third-party input methods on the
device, they:

*   [C-1-1] MUST declare the platform feature android.software.input_methods and
    support IME APIs as defined in the Android SDK documentation.


### 3.8.10\. Lock Screen Media Control

The Remote Control Client API is deprecated from Android 5.0 in favor of the
[Media Notification Template](http://developer.android.com/reference/android/app/Notification.MediaStyle.html)
that allows media applications to integrate with playback controls that are
displayed on the lock screen.


### 3.8.11\. Screen savers (previously Dreams)

See [section 3.2.3.5](#3_2_3_5_conditional_application_intents) for settings
intent to congfigure screen savers.

### 3.8.12\. Location

If device implementations include a hardware sensor (e.g. GPS) that is capable
of providing the location coordinates, they

*   [C-1-2] MUST display the [current status of location](
    https://developer.android.com/reference/android/location/LocationManager.html#isLocationEnabled%28%29)
    in the Location menu within Settings.
*   [C-1-3] MUST NOT display [location modes](
    https://developer.android.com/reference/android/provider/Settings.Secure.html#LOCATION_MODE)
    in the Location menu within Settings.

### 3.8.13\. Unicode and Font

Android includes support for the emoji characters defined in
[Unicode 10.0](http://www.unicode.org/versions/Unicode10.0.0/).

If device implementations include a screen or video output, they:

*   [C-1-1] MUST be capable of rendering these emoji characters in color glyph.
*   [C-1-2] MUST include support for:
     *   Roboto 2 font with different weights—sans-serif-thin, sans-serif-light,
       sans-serif-medium, sans-serif-black, sans-serif-condensed,
       sans-serif-condensed-light for the languages available on the device.
     *   Full Unicode 7.0 coverage of Latin, Greek, and Cyrillic, including the
       Latin Extended A, B, C, and D ranges, and all glyphs in the currency
       symbols block of Unicode 7.0.
*   SHOULD support the skin tone and diverse family emojis as specified in the
    [Unicode Technical Report #51](http://unicode.org/reports/tr51).


If device implementations include an IME, they:

*   SHOULD provide an input method to the user for these emoji characters.

Android includes support to render Myanmar fonts. Myanmar has several
non-Unicode compliant fonts, commonly known as “Zawgyi,” for rendering Myanmar
languages.

If device implementations include support for Burmese, they:

    * [C-2-1] MUST render text with Unicode compliant font as default;
      non-Unicode compliant font MUST NOT be set as default font unless the user
      chooses it in the language picker.
    * [C-2-2] MUST support a Unicode font and a non-Unicode compliant font if a
      non-Unicode compliant font is supported on the device.  Non-Unicode
      compliant font MUST NOT remove or overwrite the Unicode font.
    * [C-2-3] MUST render text with non-Unicode compliant font ONLY IF a
      language code with [script code Qaag](
      http://unicode.org/reports/tr35/#unicode_script_subtag_validity) is
      specified (e.g. my-Qaag). No other ISO language or region codes (whether
      assigned, unassigned, or reserved) can be used to refer to non-Unicode
      compliant font for Myanmar. App developers and web page authors can
      specify my-Qaag as the designated language code as they would for any
      other language.

### 3.8.14\. Multi-windows

If device implementations have the capability to display multiple activities at
the same time, they:

*   [C-1-1] MUST implement such multi-window mode(s) in accordance with the
    application behaviors and APIs described in the Android SDK
    [multi-window mode support documentation](
    https://developer.android.com/guide/topics/ui/multi-window.html) and meet
    the following requirements:
*   [C-1-2] MUST honor [`android:resizeableActivity`](
    https://developer.android.com/reference/android/R.attr.html#resizeableActivity)
    that is set by an app in the `AndroidManifest.xml` file as described in
    [this SDK](https://developer.android.com/guide/topics/manifest/application-element#resizeableActivity).
*   [C-1-3] MUST NOT offer split-screen or freeform mode if
    the screen height is less than 440 dp and the screen width is less than 440
    dp.
*   [C-1-4] An activity MUST NOT be resized to a size smaller than 220dp in
    multi-window modes other than Picture-in-Picture.
*   Device implementations with screen size `xlarge` SHOULD support freeform
    mode.

If device implementations support multi-window mode(s), and the split screen
mode, they:

*   [C-2-1] MUST preload a [resizeable](
    https://developer.android.com/guide/topics/ui/multi-window.html#configuring)
    launcher as the default.
*   [C-2-2] MUST crop the docked activity of a split-screen multi-window but
    SHOULD show some content of it, if the Launcher app is the focused window.
*   [C-2-3] MUST honor the declared [`AndroidManifestLayout_minWidth`](
    https://developer.android.com/reference/android/R.styleable.html#AndroidManifestLayout_minWidth)
    and [`AndroidManifestLayout_minHeight`](
    https://developer.android.com/reference/android/R.styleable.html#AndroidManifestLayout_minHeight)
    values of the third-party launcher application and not override these values
    in the course of showing some content of the docked activity.


If device implementations support multi-window mode(s) and Picture-in-Picture
multi-window mode, they:

*   [C-3-1] MUST launch activities in picture-in-picture multi-window mode
    when the app is:
        *   Targeting API level 26 or higher and declares
        [`android:supportsPictureInPicture`](https://developer.android.com/reference/android/R.attr.html#supportsPictureInPicture)
        *   Targeting API level 25 or lower and declares both [`android:resizeableActivity`](https://developer.android.com/reference/android/R.attr.html#resizeableActivity)
        and [`android:supportsPictureInPicture`](https://developer.android.com/reference/android/R.attr.html#supportsPictureInPicture).
*   [C-3-2] MUST expose the actions in their SystemUI as
    specified by the current PIP activity through the [`setActions()`](
    https://developer.android.com/reference/android/app/PictureInPictureParams.Builder.html#setActions%28java.util.List<android.app.RemoteAction>%29)
    API.
*   [C-3-3] MUST support aspect ratios greater than or equal to
    1:2.39 and less than or equal to 2.39:1, as specified by the PIP activity through
    the [`setAspectRatio()`](
    https://developer.android.com/reference/android/app/PictureInPictureParams.Builder.html#setAspectRatio%28android.util.Rational%29)
    API.
*   [C-3-4] MUST use [`KeyEvent.KEYCODE_WINDOW`](
    https://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_WINDOW)
    to control the PIP window; if PIP mode is not implemented, the key MUST be
    available to the foreground activity.
*   [C-3-5] MUST provide a user affordance to block an app from displaying in
    PIP mode; the AOSP implementation meets this requirement by having
    controls in the notification shade.
*   [C-3-6] MUST allocate the following minimum width and height for the PIP
    window when an application does not declare any value for
    [`AndroidManifestLayout_minWidth`](
    https://developer.android.com/reference/android/R.styleable.html#AndroidManifestLayout_minWidth)
    and [`AndroidManifestLayout_minHeight`](
    https://developer.android.com/reference/android/R.styleable.html#AndroidManifestLayout_minHeight):

    *   Devices with the Configuration.uiMode that is set other than
    [`UI_MODE_TYPE_TELEVISION`](
    https://developer.android.com/reference/android/content/res/Configuration.html#UI_MODE_TYPE_TELEVISION)
    MUST allocate a minimum width and height of 108 dp.
    *   Devices with the Configuration.uiMode that is set to
    [`UI_MODE_TYPE_TELEVISION`](
    https://developer.android.com/reference/android/content/res/Configuration.html#UI_MODE_TYPE_TELEVISION)
    MUST allocate a minimum width of 240 dp and a minimum height of 135 dp.

### 3.8.15\. Display Cutout

Android supports a Display Cutout as described
in the SDK document. The [`DisplayCutout`](
https://developer.android.com/reference/android/view/DisplayCutout) API defines
an area on the edge of the display that may not be functional for an application
due to a display cutout or curved display on the edge(s).

If device implementations include display cutout(s), they:

*   [C-1-5] MUST NOT have cutout(s) if the device's aspect ratio is 1.0(1:1).
*   [C-1-2] MUST NOT have more than one cutout per edge.
*   [C-1-3] MUST honor the display cutout flags set by the app through the
[`WindowManager.LayoutParams`](
https://developer.android.com/reference/android/view/WindowManager.LayoutParams)
API as described in the SDK.
*   [C-1-4] MUST report correct values for all cutout metrics defined in the
[`DisplayCutout`](
https://developer.android.com/reference/android/view/DisplayCutout) API.

### 3.8.16\. Device Controls

Android includes [`ControlsProviderService`](https://developer.android.com/reference/android/service/controls/ControlsProviderService)
and [`Control`](https://developer.android.com/reference/android/service/controls/Control)
APIs to allow third-party applications to publish device controls for quick
status and action for users.

See Section [2_2_3](#2_2_3_software) for device-specific requirements.