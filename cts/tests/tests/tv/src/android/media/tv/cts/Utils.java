package android.media.tv.cts;

import android.content.Context;
import android.content.pm.PackageManager;
import android.util.Log;

import org.junit.Assume;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

public class Utils {

    private Utils() {
    }

    public static boolean hasTvInputFramework(Context context) {
        return context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LIVE_TV);
    }

}
