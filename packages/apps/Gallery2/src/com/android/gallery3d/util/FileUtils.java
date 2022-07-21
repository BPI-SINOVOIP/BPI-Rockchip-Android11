package com.android.gallery3d.util;

import android.content.Context;
import android.net.Uri;
import androidx.core.content.FileProvider;

import java.io.File;

public class FileUtils {
    public static String FILE_PROVIDER_SCHEME = "com.android.gallery3d.fileprovider";

    public static Uri adjustFileUri(Context context, Uri oldUri) {
        if (null == oldUri || null == oldUri.toString()) {
            return oldUri;
        }
        try{
            if (oldUri.toString().startsWith("file:///storage")) {
                Uri uri = FileProvider.getUriForFile(context,
                        FILE_PROVIDER_SCHEME,
                        new File(oldUri.getPath()));
                return uri;
            }
        } catch(Exception e) {
            e.printStackTrace();
        }
        return oldUri;
    }
}
