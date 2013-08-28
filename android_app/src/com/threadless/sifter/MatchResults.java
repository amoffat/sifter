package com.threadless.sifter;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.net.Uri;
import android.os.Bundle;
import android.util.Base64;
import android.view.Display;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;


public class MatchResults extends BaseActivity {

    private class Result {
        public String title;
        public String artist;
        public String artistUrl;
        public String designUrl;
        public int id;
        public int width;
        public int height;
        public float confidence;
        public Bitmap image;

        Result(String jsonResponse) {
            JSONObject json = null;
            try {
                json = new JSONObject(jsonResponse);
            }
            catch (JSONException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

            if (json == null) {
                return;
            }

            try {
                id = json.getInt("id");
                title = json.getString("title");
                artist = json.getString("artist");
                confidence = (float)json.getDouble("confidence");
                artistUrl = json.getString("artist_url");
                designUrl = json.getString("design_url");
                width = json.getInt("width");
                height = json.getInt("height");
            }
            catch (JSONException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }

            try {
                byte[] imageData = Base64.decode(
                    json.getString("thumbnail"), Base64.DEFAULT);
                image = BitmapFactory.decodeByteArray(imageData, 0,
                    imageData.length);
            }
            catch (JSONException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }
    }

    private Result match;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setContentView(R.layout.activity_match_results);
        super.onCreate(savedInstanceState);

        String jsonLocation = getIntent().getStringExtra("json_file");
        Uri jsonResponseUri = Uri.parse("file:" + jsonLocation);
        InputStream jsonStream = null;
        try {
            jsonStream = getContentResolver().openInputStream(jsonResponseUri);
        }
        catch (FileNotFoundException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        
        /*
         * read our json data from the cache file we created in the previous
         * activity
         */
        BufferedReader br = new BufferedReader(new InputStreamReader(jsonStream));
        StringBuilder builder = new StringBuilder();
        String line;
        try {
            while ((line = br.readLine()) != null) {
                builder.append(line);
            }
            br.close();
            jsonStream.close();
        }
        catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        
        /*
         * clean up our cache file.  these can be a few hundred kb, so it's
         * good to not leave them around
         */
        File jsonFile = new File(jsonLocation);
        jsonFile.delete();
        
        
        match = new Result(builder.toString());

        
        /*
         * now that we have our match object, we can go ahead and start
         * populating elements in this activity's view
         */
        final ImageView design = (ImageView)findViewById(R.id.imageView1);
        LayoutParams params = design.getLayoutParams();
        Display display = getWindowManager().getDefaultDisplay();
        Point size = new Point();
        display.getSize(size);
        params.height = (int)(size.x * ((float)(match.height) / match.width));
        design.setLayoutParams(params);

        Button openDesign = (Button)findViewById(R.id.open_design);
        Typefaces.threadlessFont(this, openDesign);

        TextView matchTitle = (TextView)findViewById(R.id.match_title);
        Typefaces.threadlessFont(this, matchTitle);

        Button artistProfile = (Button)findViewById(R.id.artist_profile);
        Typefaces.threadlessFont(this, artistProfile);

        matchTitle.setText(match.title);
        ((TextView)findViewById(R.id.match_artist)).setText(match.artist);

        design.setImageBitmap(match.image);

        artistProfile.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse(match.artistUrl));
                startActivity(intent);
            }
        });

        openDesign.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setData(Uri.parse(match.designUrl));
                startActivity(intent);
            }
        });
    }
}
