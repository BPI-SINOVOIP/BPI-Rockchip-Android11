package com.rockchip.graphics;

import android.content.Context;
import android.content.res.AssetManager;

import java.io.File;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.io.LineNumberReader;

public class CubicLutUtil {

    private static int floatLutToIntLut(float value) {
        int ret = (int) (4096 * value + 0.5);
        if (ret >= 4096)
            ret = 4095;
        return ret;
    }

    public static int[][] get3DLutFromFile(String path) {
        int[][] ret = null;
        try {
            File file = new File(path);
            if(!file.exists()){
                return null;
            }
            FileReader fileReader = new FileReader(file);
            LineNumberReader reader = new LineNumberReader(fileReader);
            String txt = "";
            String[] temp = null;
            int lines = 0;
            int i = 0;
            boolean is3DLutData = false;
            while (txt != null) {
                lines++;
                txt = reader.readLine();
                if (txt.startsWith("Length")) {
                    int length = Integer.parseInt(txt.replaceAll("\\s+", "").replaceAll("Length", ""));
                    ret = new int[3][length * length * length];
                    continue;
                }
                if (txt.equals("{")) {
                    is3DLutData = true;
                    continue;
                } else if (txt.equals("}")) {
                    is3DLutData = false;
                    break;
                }
                if (is3DLutData && ret != null) {
                    temp = txt.trim().split(" ");
                    ret[0][i] = floatLutToIntLut(Float.parseFloat(temp[0]));
                    ret[1][i] = floatLutToIntLut(Float.parseFloat(temp[1]));
                    ret[2][i] = floatLutToIntLut(Float.parseFloat(temp[2]));
                    i++;
                }
            }
            reader.close();
            fileReader.close();
            if(ret != null && i != ret[0].length){
                ret = null;
            }
        } catch (Exception e) {
            e.printStackTrace();
            ret = null;
        }
        return ret;
    }

    public static int[][] get3DLutFromAsset(Context context, String fileName) {
        int[][] ret = null;
        try {
            AssetManager am = context.getResources().getAssets();
            InputStreamReader inputReader = new InputStreamReader(am.open(fileName));
            LineNumberReader reader = new LineNumberReader(inputReader);
            String txt = "";
            String[] temp = null;
            int lines = 0;
            int i = 0;
            boolean is3DLutData = false;
            while (txt != null) {
                lines++;
                txt = reader.readLine();
                if (txt.startsWith("Length")) {
                    int length = Integer.parseInt(txt.replaceAll("\\s+", "").replaceAll("Length", ""));
                    ret = new int[3][length * length * length];
                    continue;
                }
                if (txt.equals("{")) {
                    is3DLutData = true;
                    continue;
                } else if (txt.equals("}")) {
                    is3DLutData = false;
                    break;
                }
                if (is3DLutData) {
                    temp = txt.trim().split(" ");
                    ret[0][i] = floatLutToIntLut(Float.parseFloat(temp[0]));
                    ret[1][i] = floatLutToIntLut(Float.parseFloat(temp[1]));
                    ret[2][i] = floatLutToIntLut(Float.parseFloat(temp[2]));
                    i++;
                }
            }
            reader.close();
            inputReader.close();
            if(ret != null && i != ret[0].length){
                ret = null;
            }
        } catch (Exception e) {
            e.printStackTrace();
            ret = null;
        }
        return ret;
    }
}
