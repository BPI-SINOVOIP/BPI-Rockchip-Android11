/**
 *
 */
package com.android.tv.settings;

import android.app.Activity;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.android.tv.settings.R;

import android.util.Log;

/**
 * @author GaoFei
 */
public abstract class BaseInputActivity extends Activity {
    protected TextView mTextTitle;
    protected TextView mTextSummary;
    protected LinearLayout mLayoutContent;
    private View mContentView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.i("BaseInputActivity", "onCreate");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_base_input);
        baseInit();
        init();
    }

    public void baseInit() {
        mTextTitle = (TextView) findViewById(R.id.text_title);
        mTextSummary = (TextView) findViewById(R.id.text_summary);
        mTextTitle.setText(getInputTitle());
        mTextSummary.setText(getInputSummary());
        mLayoutContent = (LinearLayout) findViewById(R.id.layout_content);
        mContentView = LayoutInflater.from(this).inflate(getContentLayoutRes(), null);
        mLayoutContent.addView(mContentView,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
    }

    public abstract void init();

    public abstract int getContentLayoutRes();

    public abstract String getInputTitle();

    public String getInputSummary() {
        return "";
    }

    public View getContentView() {
        return mContentView;
    }

    public void setInputTitle(String title) {
        mTextTitle.setText(title);
    }
}
