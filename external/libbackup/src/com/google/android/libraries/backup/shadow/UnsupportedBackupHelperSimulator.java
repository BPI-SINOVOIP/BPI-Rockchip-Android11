package com.google.android.libraries.backup.shadow;

import android.app.backup.BackupHelper;
import android.content.Context;
import android.util.Log;

/**
 * No-op backup helper representing all an unsupported backup helpers.
 *
 * <p>{@see BackupAgentHelperShadow}
 */
public class UnsupportedBackupHelperSimulator extends BackupHelperSimulator {
  private static final String TAG = "UnsupportedBckupHlprSim";
  private final BackupHelper mHelper;

  UnsupportedBackupHelperSimulator(String keyPrefix, BackupHelper helper) {
    super(keyPrefix);
    mHelper = helper;
  }

  @Override
  public Object backup(Context context) {
    return new UnsupportedBackupHelperOutput(mHelper);
  }

  @Override
  public void restore(Context context, Object data) {
    if (!(data instanceof UnsupportedBackupHelperOutput)) {
      throw new IllegalArgumentException(
          "Invalid type of data to restore in unsupported helper \""
              + keyPrefix
              + "\": "
              + (data == null ? null : data.getClass()));
    }
    Log.w(
        TAG,
        "Attempt to restore from an unsupported backup helper: " + mHelper.getClass().getName());
  }

  public static class UnsupportedBackupHelperOutput {
    public final BackupHelper wrappedHelper;

    public UnsupportedBackupHelperOutput(BackupHelper helper) {
      wrappedHelper = helper;
    }

    @Override
    public boolean equals(Object obj) {
      return obj instanceof UnsupportedBackupHelperOutput
          && wrappedHelper.equals(((UnsupportedBackupHelperOutput) obj).wrappedHelper);
    }

    @Override
    public int hashCode() {
      return wrappedHelper.hashCode();
    }
  }
}
