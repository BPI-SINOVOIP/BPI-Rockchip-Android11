package android.gwpasan;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class Utils {

    public static boolean isGwpAsanEnabled() throws IOException {
      BufferedReader reader = new BufferedReader(new FileReader("proc/self/maps"));
      String line;
      while ((line = reader.readLine()) != null) {
        if (line.contains("GWP-ASan Guard Page")) {
          return true;
        }
      }
      return false;
    }
}
