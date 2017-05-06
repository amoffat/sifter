package com.threadless.sifter;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.net.URI;
import java.util.Random;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.TransitionDrawable;
import android.hardware.Camera;
import android.hardware.Camera.PictureCallback;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.view.Display;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;

import com.loopj.android.http.AsyncHttpClient;
import com.loopj.android.http.AsyncHttpResponseHandler;
import com.loopj.android.http.RequestParams;


public class MainActivity extends BaseActivity implements PictureCallback {

    private SifterCamera cameraPreview;
    private Button shutter;
    private MediaPlayer shutterSound;
    
    // local sifter engine, for testing
    private String serverHostname = "192.168.1.2";
    private int serverPort = 8084;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setContentView(R.layout.activity_main);
        super.onCreate(savedInstanceState);
        
        
        shutterSound = MediaPlayer.create(MainActivity.this, R.raw.camera_shutter);

        cameraPreview = new SifterCamera(this, 0,
            SifterCamera.LayoutMode.FitToParent);
        FrameLayout cameraFrame = (FrameLayout)findViewById(R.id.camera_preview);
        cameraFrame.addView(cameraPreview);

        cameraPreview.setJpegCallback(this);

        shutter = (Button)findViewById(R.id.capture_button);
        Typefaces.threadlessFont(this, shutter);


        shutter.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                shutter.setEnabled(false);
                shutter.setText(R.string.scanning);
                shutterSound.start();
                cameraPreview.takePicture();
            }
        });
    }

    @Override
    public void onPictureTaken(byte[] data, Camera camera) {
        Bitmap original = BitmapFactory.decodeByteArray(data, 0, data.length);

        Matrix rotateMat = new Matrix();
        rotateMat.postRotate(90);
        Bitmap rotated = Bitmap.createBitmap(original, 0, 0,
            original.getWidth(), original.getHeight(), rotateMat, true);

        final ImageView capturedPreview = (ImageView)findViewById(R.id.captured_image);

        ImageView teeImage = (ImageView)findViewById(R.id.imageView1);
        int[] location = new int[2];
        teeImage.getLocationInWindow(location);

        Display display = getWindowManager().getDefaultDisplay();
        Point size = new Point();
        display.getSize(size);

        /*
         * these two values were added much later.  the original version of the app was designed
         * for a galaxy s3, and so the coordinates that follow were based off of the s3's camera
         * size.  these normalize values make the positioning camera-resolution independent
         */
        float normalizeX = (float) (720.0 / original.getHeight());
        float normalizeY = (float) (1280.0 / original.getWidth());

        /*
         * these magic numbers correspond to the subsection of the image that
         * contains the majority of the shirt design that we want to send off to
         * scanning. they are width and height independent
         */
        int upperLeftX = (int)((teeImage.getWidth() * normalizeX) * 0.2325) + location[0];
        int upperLeftY = (int)((teeImage.getHeight() * normalizeY) * 0.10269576379974327) + location[1];
        int width = (int)(teeImage.getWidth() * normalizeX * 0.5575);
        int height = (int)(teeImage.getHeight() * normalizeY * 0.8356867779204108);

        float scaleBy = 1.0f;
        if (width > 300) {
            scaleBy = 300.0f / width;
        }

        Matrix matrix = new Matrix();
        matrix.postScale(scaleBy, scaleBy);
        Bitmap cropped = Bitmap.createBitmap(rotated, upperLeftX, upperLeftY,
            width, height, matrix, true);

        Bitmap teeMask = BitmapFactory.decodeResource(getResources(),
            R.drawable.tee_mask);
        Bitmap mutableMask = teeMask.copy(teeMask.getConfig(), true);
        Matrix translateMat = new Matrix();
        float scale = ((float)teeMask.getWidth()) / ((float)rotated.getWidth());
        translateMat.postScale(scale, scale);
        translateMat.postTranslate(-location[0] * scale * normalizeX, (-upperLeftY * scale) * normalizeX);

        final Canvas canvas = new Canvas();
        canvas.setBitmap(mutableMask);
        Paint paint = new Paint();
        paint.setXfermode(new PorterDuffXfermode(Mode.SRC_IN));
        canvas.drawBitmap(rotated, translateMat, paint);

        capturedPreview.setImageBitmap(mutableMask);
        fadeInModalBackground(250);

        ByteArrayOutputStream outStream = new ByteArrayOutputStream();
        cropped.compress(Bitmap.CompressFormat.JPEG, 90, outStream);

        final ScannerView scanner = (ScannerView)findViewById(R.id.scanner);
        scanner.startScan();

        /*
         * if we don't do a random string for our file upload name, we run the
         * risk of multiple threads clobbering the same temporary file on the
         * server end
         */
        String tempFileName = randomString(8) + ".jpg";
        RequestParams params = new RequestParams();
        params.put("image", new ByteArrayInputStream(outStream.toByteArray()),
            tempFileName);

        AsyncHttpClient client = new AsyncHttpClient();
        client.post("http://"+serverHostname+":"+serverPort+"/match", params,
            new AsyncHttpResponseHandler() {
                @Override
                public void onSuccess(final String jsonResponse) {
                    File outputDir = MainActivity.this.getCacheDir();
                    File outputFile = null;
                    FileWriter writer = null;
                    try {
                        outputFile = File.createTempFile("match_results", ".json", outputDir);
                        writer = new FileWriter(outputFile, true);
                        writer.write(jsonResponse);
                        writer.flush();
                        writer.close();
                    }
                    catch (IOException e) {
                        // TODO Auto-generated catch block
                        e.printStackTrace();
                    }
                    
                    final String jsonFilePath = outputFile.getPath();
                    
                    /*
                     * stop the scanner at the end of one of its cycles, then
                     * switch activites
                     */
                    scanner
                        .stopScanAtGoodPoint(new ScannerView.DoneScanningCallback() {

                            @Override
                            public void doneScanning() {
                                Intent intent = new Intent(MainActivity.this,
                                    MatchResults.class);
                                intent.putExtra("json_file", jsonFilePath);
                                startActivity(intent);
                            }
                        });

                }
            });
    }

    @Override
    protected void onResume() {
        super.onResume();

        shutter.setEnabled(true);
        shutter.setText(R.string.capture);
        cameraPreview.startCamera();
        resetModalBackground();
        ImageView capturedPreview = (ImageView)findViewById(R.id.captured_image);
        capturedPreview.setImageBitmap(null);
    }

    @Override
    protected void onPause() {
        super.onPause();
        cameraPreview.stopCamera();
    }

    public static String randomString(int length) {
        Random generator = new Random();
        StringBuilder randomStringBuilder = new StringBuilder();
        char tempChar;
        for (int i = 0; i < length; i++) {
            tempChar = (char)(generator.nextInt(26) + 'a');
            randomStringBuilder.append(tempChar);
        }
        return randomStringBuilder.toString();
    }

    private void resetModalBackground() {
        FrameLayout modalBackground = (FrameLayout)findViewById(R.id.fade_in_black);
        TransitionDrawable background = (TransitionDrawable)modalBackground
            .getBackground();
        background.resetTransition();
    }

    private void fadeInModalBackground(int time) {
        FrameLayout modalBackground = (FrameLayout)findViewById(R.id.fade_in_black);
        TransitionDrawable background = (TransitionDrawable)modalBackground
            .getBackground();
        background.startTransition(time);
    }
}
