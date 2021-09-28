## 3.14\. Media UI


If device implementations include non-voice-activated applications (the Apps) that interact with
third-party applications through [`MediaBrowser`](http://developer.android.com/reference/android/media/browse/MediaBrowser.html)
or [`MediaSession`](http://developer.android.com/reference/android/media/session/MediaSession.html),
the Apps:

*    [C-1-2] MUST clearly display icons obtained via getIconBitmap() or getIconUri() and titles
     obtained via getTitle() as described in [`MediaDescription`](http://developer.android.com/reference/android/media/MediaDescription.html).
     May shorten titles to comply with safety regulations (e.g. driver distraction).

*    [C-1-3] MUST show the third-party application icon whenever displaying content provided by
     this third-party application.

*    [C-1-4] MUST allow the user to interact with the entire [`MediaBrowser`](http://developer.android.com/reference/android/media/browse/MediaBrowser.html)
     hierarchy. MAY restrict the access to part of the hierarchy to comply with safety regulations
     (e.g. driver distraction), but MUST NOT give preferential treatment based on content or
     content provider.

*    [C-1-5] MUST consider double tap of [`KEYCODE_HEADSETHOOK`](
     https://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_HEADSETHOOK)
     or [`KEYCODE_MEDIA_PLAY_PAUSE`](https://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_MEDIA_PLAY_PAUSE)
     as [`KEYCODE_MEDIA_NEXT`](https://developer.android.com/reference/android/view/KeyEvent.html#KEYCODE_MEDIA_NEXT)
     for [`MediaSession.Callback#onMediaButtonEvent`](https://developer.android.com/reference/android/media/session/MediaSession.Callback.html#onMediaButtonEvent%28android.content.Intent%29).
