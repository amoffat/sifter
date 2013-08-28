package com.threadless.sifter;

import android.app.Activity;
import android.graphics.Point;
import android.os.Bundle;
import android.view.Display;
import android.widget.TextView;


public class BaseActivity extends Activity {
    public Point getScreenSize() {
        Display display = getWindowManager().getDefaultDisplay();
        Point size = new Point();
        display.getSize(size);
        return size;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // TODO Auto-generated method stub
        super.onCreate(savedInstanceState);
        
        TextView tv = (TextView)findViewById(R.id.sifter_logo);
        if (tv != null) {
            Typefaces.threadlessFont(this, tv);
        }
    }
}
