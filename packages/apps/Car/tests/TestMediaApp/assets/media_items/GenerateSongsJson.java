import java.io.IOException;
import java.util.Random;
import java.io.FileOutputStream;

/*
 * This class will generate a json file containing 200 songs by default.
 * Defaults:
 * 1.) No. of songs - 200
 * 2.) Seed - 200
 * 3.) File name - "hundres_songs.json"
 *
 * If you want to change any of the default values pass them as arguments in the following:
 * arg #1 No. of songs (Integer)
 * arg #2 Seed (Integer)
 * arg #3 File name (String) e.g <some-name>.json
 */

public class GenerateSongsJson {

  private static final StringBuilder sb = new StringBuilder();

  public static void main(String []args) throws IOException {
    int numOfSongs = 200;
    int seed = 200;
    String fileName = "hundres_songs.json";

    if (args.length != 0) {
      try {
        numOfSongs = Integer.parseInt(args[0]);
        seed = Integer.parseInt(args[1]);
        fileName = args[2];
      } catch (NumberFormatException e) {
        System.out.println("Please enter the args as mentioned in the comment above.");
        throw (e);
      }
    }

    Random randomNum = new Random(seed);
    FileOutputStream outputStream = new FileOutputStream(fileName);

    sb.append("{ \n");
    sb.append("  \"FLAGS\": \"browsable\", \n\n");
    sb.append("  \"METADATA\": { \n");
    sb.append("	\"MEDIA_ID\": \"hundreds_songs\", \n");
    sb.append("    \"DISPLAY_TITLE\": \"More songs\" \n");
    sb.append("  },\n\n");
    sb.append("  \"CHILDREN\": [ \n");

    for (int i = 1; i <= numOfSongs; i++) {
      int num1 = randomNum.nextInt(10000);
      int num2 = num1/1000;
      sb.append("    { \n  ");
      sb.append("    \"FLAGS\": \"playable\",\n");
      sb.append("      \"METADATA\": { \n");
      sb.append("     	\"MEDIA_ID\": \"hundreds_songs normal " + num2 + "s song" + i +"\",\n");
      sb.append(" 		\"DISPLAY_TITLE\": \"Normal " + num2 + "s song" + i + "\",\n");
      sb.append(" 		\"DURATION\": " + num1 + "\n");
      sb.append("      } \n");
      sb.append("    },\n");
    }
    sb.deleteCharAt(sb.length() - 2);
    sb.append("  ]\n");
    sb.append("}");
    byte[] strToBytes = sb.toString().getBytes();
    outputStream.write(strToBytes);
    outputStream.close();
  }
}
