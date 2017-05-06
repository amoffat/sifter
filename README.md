# Sifter

Sifter is a side-project I wrote in late 2013 in my free time on weekends and
evenings, during my first year at Threadless.  It allows you to capture the
image of any Threadless tee (from the 2000-2013 designs), and it identifies the
design, title, and artist.

![sifter demo](https://github.com/amoffat/sifter/blob/master/demo.gif)

It consists of 3 components:
* The sifter engine
* The Android app, tested on JellyBean
* An embedded web server, [mongoose](https://github.com/cesanta/mongoose)

The Android app captures the shirt images using the phone's camera, then makes
an HTTP call to the web server, POSTing the image data.  The embedded web server
calls to the sifter engine, which computes scale and rotationally-invariant
feature descriptors.  These descriptors are then searched against a database of
design descriptors, and the best match is yielded to the Android app.

# Installing

Build the docker image:

`docker build -t sifter .`

Then start the container:

`docker run -it --rm -p 8084:8080 -v /path/to/data:/home/sifter/data sifter`

You'll need a valid `/path/to/data` which contains all of the designs, feature
descriptors for the designs, and database of artist and design metadata.  I've
opted not to include this into the repo due to size constraints, but reach out
to me directly if you wish to try it out.
