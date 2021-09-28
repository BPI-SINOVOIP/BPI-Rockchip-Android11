package com.android.vts.util;

import com.google.auth.oauth2.ServiceAccountCredentials;
import com.google.cloud.storage.Storage;
import com.google.cloud.storage.StorageOptions;

import java.io.IOException;
import java.io.InputStream;
import java.util.Optional;
import java.util.logging.Level;
import java.util.logging.Logger;

/** GcsHelper, a helper class for interacting with Google Cloud Storage. */
public class GcsHelper {
    private static final Logger logger = Logger.getLogger(GcsHelper.class.getName());

    /** Google Cloud Storage project ID */
    private static String GCS_PROJECT_ID;

    public static void setGcsProjectId(String gcsProjectId) {
        GCS_PROJECT_ID = gcsProjectId;
    }

    /**
     * Get GCS storage from Key file input stream parameter.
     */
    public static Optional<Storage> getStorage(InputStream keyFileInputStream) {

        if (keyFileInputStream == null) {
            logger.log(Level.SEVERE, "Error GCS key file is not exiting. Check key file!");
            return Optional.empty();
        } else {
            try {
                Storage storage =
                        StorageOptions.newBuilder()
                                .setProjectId(GCS_PROJECT_ID)
                                .setCredentials(
                                        ServiceAccountCredentials.fromStream(keyFileInputStream))
                                .build()
                                .getService();
                return Optional.of(storage);
            } catch (IOException e) {
                logger.log(Level.SEVERE, "Error on creating storage instance!");
                return Optional.empty();
            }
        }
    }
}
