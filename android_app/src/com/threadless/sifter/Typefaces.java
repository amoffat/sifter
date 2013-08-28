package com.threadless.sifter;

import java.util.Hashtable;

import android.content.Context;
import android.graphics.Typeface;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;


/*
 * https://code.google.com/p/android/issues/detail?id=9904#c7
 */
public class Typefaces {
    private static final String TAG = "Typefaces";
    private static final String THREADLESS_FONT = "futura-bold.ttf";

    private static final Hashtable<String, Typeface> cache = new Hashtable<String, Typeface>();

    public static Typeface get(Context c, String assetPath) {
        synchronized (cache) {
            if (!cache.containsKey(assetPath)) {
                try {
                    Typeface t = Typeface.createFromAsset(c.getAssets(),
                        assetPath);
                    cache.put(assetPath, t);
                }
                catch (Exception e) {
                    Log.e(TAG, "Could not get typeface '" + assetPath
                        + "' because " + e.getMessage());
                    return null;
                }
            }
            return cache.get(assetPath);
        }
    }
    
    public static void threadlessFont(Context c, TextView view) {
        view.setTypeface(get(c, THREADLESS_FONT));
    }
    
    public static void threadlessFont(Context c, Button view) {
        view.setTypeface(get(c, THREADLESS_FONT));
    }
}
