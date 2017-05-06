package com.threadless.sifter;

import java.io.IOException;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.media.MediaPlayer;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;


public class ScannerView extends View {

    public interface DoneScanningCallback {
        void doneScanning();
    }

    private static final String TAG = "ScannerView";
    private Paint linePaint;
    private int lineColor;
    private boolean scanning = false;
    private DoneScanningCallback waitingForGoodStoppingPoint = null;

    /*
     * how many seconds a full top-to-bottom scan takes
     */
    private float scanRate = 2.0f;
    private float timestep = 1.0f / 30.0f;
    private int lastPosition = 0;
    private float strokeWidth = 1.0f;
    private int direction = 1;
    
    /*
     * this fraction of the scanning complete (or beginning), the volume of the
     * scanning noise will fade out (or in)
     */
    private float volumePadding = 0.25f;

    public ScannerView(Context context) {
        super(context);
        init();
    }

    public ScannerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public ScannerView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init();
    }

    private void init() {
        linePaint = new Paint();
        lineColor = getResources().getColor(R.color.cyan);
        linePaint.setStrokeWidth(strokeWidth);
        linePaint.setStyle(Paint.Style.STROKE);
        linePaint.setDither(true);
        linePaint.setAntiAlias(true);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        setMeasuredDimension(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
    }

    synchronized public void startScan() {
        scanning = true;
        lastPosition = 0;
        invalidate();
    }

    synchronized public void stopScanAtGoodPoint(DoneScanningCallback cb) {
        waitingForGoodStoppingPoint = cb;
    }

    synchronized public void stopScan() {
        scanning = false;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (scanning) {
            int totalHeight = canvas.getHeight();
            int totalWidth = canvas.getWidth();

            int newPos = (int)(lastPosition + direction
                * (totalHeight * timestep) / scanRate);

            
            
            boolean goodStoppingPoint = false;

            if (newPos > totalHeight) {
                newPos = totalHeight;
                direction = -1;
                goodStoppingPoint = true;
            }
            else if (newPos < 0) {
                newPos = 0;
                direction = 1;
                goodStoppingPoint = true;
            }
            lastPosition = newPos;
            float progress = newPos / (float)totalHeight;
            
            
            float volume = 1.0f;
            float fromStartVolume = Math.min(progress / volumePadding, volume);
            float fromEndVolume = Math.max((1.0f - progress) / volumePadding, 0);
            volume = Math.min(fromStartVolume, fromEndVolume);
            //scanningSound.setVolume(1.0f, volume);
            

            /*
             * are we stopping?
             */
            if (goodStoppingPoint && waitingForGoodStoppingPoint != null) {
                waitingForGoodStoppingPoint.doneScanning();
                waitingForGoodStoppingPoint = null;
                stopScan();
            }
            /*
             * or are we drawing?
             */
            else {
    
                /*
                 * draw our initial bright line
                 */
                linePaint.setAlpha(255);
                linePaint.setColor(getResources().getColor(R.color.white));
                linePaint.setStrokeWidth(strokeWidth * 20);
                canvas.drawLine(0, newPos, totalWidth, newPos, linePaint);
    
                linePaint.setColor(lineColor);
                linePaint.setStrokeWidth(strokeWidth);
                int numLines = 200;
                for (int i = 0; i < numLines; i++) {
                    int y = (int)(newPos - direction * (i * strokeWidth));
                    float alpha = 1.0f - ((float)i) / numLines;
                    linePaint.setAlpha((int)(255 * alpha));
                    canvas.drawLine(0, y, totalWidth, y, linePaint);
                }
    
                invalidate();
            }
        }
    }

}
