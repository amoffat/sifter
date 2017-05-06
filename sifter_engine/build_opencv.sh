#!/bin/bash

NUM_CORES=$(grep -c ^processor /proc/cpuinfo)

cmake \
    -DBUILD_DOCS=OFF\
    \
    -DBUILD_opencv_apps=OFF\
    -DBUILD_opencv_contrib=OFF\
    -DBUILD_opencv_gpu=OFF\
    -DBUILD_opencv_legacy=OFF\
    -DBUILD_opencv_ml=OFF\
    -DBUILD_opencv_objdetect=OFF\
    -DBUILD_opencv_ocl=OFF\
    -DBUILD_opencv_photo=OFF\
    -DBUILD_opencv_python=OFF\
    -DBUILD_opencv_stitching=OFF\
    -DBUILD_opencv_superres=OFF\
    -DBUILD_opencv_ts=OFF\
    -DBUILD_opencv_video=OFF\
    -DBUILD_opencv_videostab=OFF\
    -DBUILD_opencv_world=OFF\
    \
    -DBUILD_opencv_calib3d=ON\
    -DBUILD_opencv_flann=ON\
    -DBUILD_opencv_imgproc=ON\
    -DBUILD_opencv_core=ON\
    -DBUILD_opencv_features2d=ON\
    -DBUILD_opencv_highgui=ON\
    -DBUILD_opencv_nonfree=ON\
    \
    -DWITH_CUDA=OFF\
    -DWITH_CUBLAS=OFF\
    -DWITH_CUFFT=OFF\
    -DWITH_EIGEN=OFF\
    -DWITH_FFMPEG=OFF\
    -DWITH_GSTREAMER=OFF\
    -DWITH_GTK=OFF\
    -DWITH_IPP=OFF\
    -DWITH_LIBV4L=OFF\
    -DWITH_OPENCL=OFF\
    -DWITH_OPENCLAMDBLAS=OFF\
    -DWITH_OPENCLAMDFFT=OFF\
    -DWITH_OPENEXR=OFF\
    -DWITH_V4L=OFF\
    \
    -DWITH_TBB=ON\
    ..

make -j $NUM_CORES
