package org.robolectric.shadows;

import static org.robolectric.shadow.api.Shadow.invokeConstructor;
import static org.robolectric.util.ReflectionHelpers.ClassParameter.from;

import android.annotation.SuppressLint;
import android.os.ParcelFileDescriptor;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.lang.reflect.Constructor;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.util.ReflectionHelpers;

@Implements(ParcelFileDescriptor.class)
@SuppressLint("NewApi")
public class ShadowParcelFileDescriptor {
  // TODO: consider removing this shadow in favor of shadowing file operations at the libcore.os
  // level
  private static final String PIPE_TMP_DIR = "ShadowParcelFileDescriptor";
  private static final String PIPE_FILE_NAME = "pipe";
  private RandomAccessFile file;
  @RealObject ParcelFileDescriptor realParcelFd;

  private @RealObject ParcelFileDescriptor realObject;

  @Implementation
  protected void __constructor__(ParcelFileDescriptor wrapped) {
    invokeConstructor(
        ParcelFileDescriptor.class, realObject, from(ParcelFileDescriptor.class, wrapped));
    if (wrapped != null) {
      ShadowParcelFileDescriptor shadowParcelFileDescriptor = Shadow.extract(wrapped);
      this.file = shadowParcelFileDescriptor.file;
    }
  }

  @Implementation
  protected static ParcelFileDescriptor open(File file, int mode) throws FileNotFoundException {
    ParcelFileDescriptor pfd;
    try {
      Constructor<ParcelFileDescriptor> constructor =
          ParcelFileDescriptor.class.getDeclaredConstructor(FileDescriptor.class);
      pfd = constructor.newInstance(new FileDescriptor());
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
    ShadowParcelFileDescriptor shadowParcelFileDescriptor = Shadow.extract(pfd);
    shadowParcelFileDescriptor.file = new RandomAccessFile(file, getFileMode(mode));
    return pfd;
  }

  private static String getFileMode(int mode) {
    if ((mode & ParcelFileDescriptor.MODE_CREATE) != 0) {
      return "rw";
    }
    switch (mode & ParcelFileDescriptor.MODE_READ_WRITE) {
      case ParcelFileDescriptor.MODE_READ_ONLY:
        return "r";
      case ParcelFileDescriptor.MODE_WRITE_ONLY:
      case ParcelFileDescriptor.MODE_READ_WRITE:
        return "rw";
    }

    // TODO: this probably should be an error that we reach here, but default to 'rw' for now
    return "rw";
  }

  @Implementation
  protected static ParcelFileDescriptor[] createPipe() throws IOException {
    File file =
        new File(
            RuntimeEnvironment.getTempDirectory().create(PIPE_TMP_DIR).toFile(), PIPE_FILE_NAME);
    if (!file.createNewFile()) {
      throw new IOException("Cannot create pipe file: " + file.getAbsolutePath());
    }
    ParcelFileDescriptor readSide = open(file, ParcelFileDescriptor.MODE_READ_ONLY);
    ParcelFileDescriptor writeSide = open(file, ParcelFileDescriptor.MODE_READ_WRITE);
    file.deleteOnExit();
    return new ParcelFileDescriptor[] {readSide, writeSide};
  }

  @Implementation
  protected FileDescriptor getFileDescriptor() {
    try {
      return file.getFD();
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Implementation
  protected long getStatSize() {
    try {
      return file.length();
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Implementation
  protected int getFd() {
    try {
      return ReflectionHelpers.getField(file.getFD(), "fd");
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @Implementation
  protected void close() throws IOException {
    file.close();
  }
}
