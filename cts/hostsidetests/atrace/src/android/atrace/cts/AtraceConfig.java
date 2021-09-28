package android.atrace.cts;

public final class AtraceConfig {

    // Collection of all userspace tags, and 'sched'
    public static final String[] RequiredCategories = {
            "sched",
            "gfx",
            "input",
            "view",
            "webview",
            "wm",
            "am",
            "sm",
            "audio",
            "video",
            "camera",
            "hal",
            "res",
            "dalvik",
            "rs",
            "bionic",
            "power"
    };

    // Categories to use when capturing a trace with otherwise no categories specified
    public static final String[] DefaultCategories = {
            "view"
    };

    private AtraceConfig() {}
}
