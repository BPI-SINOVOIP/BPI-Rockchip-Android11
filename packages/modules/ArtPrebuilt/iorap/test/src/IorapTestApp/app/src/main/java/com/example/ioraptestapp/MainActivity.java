package com.example.ioraptestapp;

import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

public class MainActivity extends AppCompatActivity {

    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        try {
            LoadText(R.raw.testfile);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        TextView view = this.findViewById(R.id.textView);
        String text = "Version: " + BuildConfig.VERSION_CODE;
        view.setText(text);

    }

    @RequiresApi(api = Build.VERSION_CODES.KITKAT)
    public void LoadText(int resourceId) throws InterruptedException {
        // The InputStream opens the resourceId and sends it to the buffer
        InputStream is = this.getResources().openRawResource(resourceId);
        BufferedReader br = new BufferedReader(new InputStreamReader(is));
        String readLine = null;

        try {
            // While the BufferedReader readLine is not null
            while ((readLine = br.readLine()) != null) {
                Log.d("TEXT", readLine);
            }

            // Close the InputStream and BufferedReader
            is.close();
            br.close();

        } catch (IOException e) {
            e.printStackTrace();
        }
        Thread.sleep(100);
        this.reportFullyDrawn();
    }
}
