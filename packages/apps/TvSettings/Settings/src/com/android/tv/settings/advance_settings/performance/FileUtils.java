package com.android.tv.settings.advance_settings.performance;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;

import android.content.Context;

public class FileUtils {

        /**
         * 读取文件
         * @param file
         * @return 字符串
         */
        public static String readFromFile(File file) {
        if((file != null) && file.exists()) {
            try {
                FileInputStream fin= new FileInputStream(file);
                BufferedReader reader= new BufferedReader(new InputStreamReader(fin));
                String value = reader.readLine();
                fin.close();
                return value;
            } catch(IOException e) {
                return null;
            }
        }
        return null;
    }

        /**
         * 文件中写入字符串
         * @param file
         * @param enabled
         */
        public static boolean write2File(File file, String value) {
        if((file == null) || (!file.exists())) return false;
        try {
            FileOutputStream fout = new FileOutputStream(file);
            PrintWriter pWriter = new PrintWriter(fout);
            pWriter.println(value);
            pWriter.flush();
            pWriter.close();
            fout.close();
            return true;
        } catch(IOException re) {
                return false;
        }
    }


        /**
         * 读取文件内容
         * @param file
         * @return
         */
        public static byte[] readFileContent(File file){
                InputStream fin = null;
                try {
                        fin = new FileInputStream(file);
                        byte[] readBuffer = new byte[20480];
                        int readLen = 0;
                        ByteArrayOutputStream contentBuf = new ByteArrayOutputStream();
                        while((readLen=fin.read(readBuffer))>0){
                                contentBuf.write(readBuffer, 0, readLen);
                        }
                        return contentBuf.toByteArray();
                } catch (Exception e) {
                } finally{
                        if(fin!=null){
                                try {
                                        fin.close();
                                } catch (IOException e) {
                                        e.printStackTrace();
                                }
                        }
                }
                return null;
        }
}
