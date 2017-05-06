FROM ubuntu:trusty
EXPOSE 8080

RUN locale-gen en_US.UTF-8  
ENV LANG en_US.UTF-8  
ENV LANGUAGE en_US:en  
ENV LC_ALL en_US.UTF-8 

RUN apt-get -y update
RUN apt-get -y install\
        curl\
        libtbb-dev\
        libboost-system1.54-dev\
        libboost-program-options1.54-dev\
        libboost-filesystem1.54-dev\
        cmake\
        pkg-config\
        libglib2.0-dev\
        build-essential

RUN useradd -m sifter
USER sifter
WORKDIR /home/sifter
ENV HOME /home/sifter
ENV opencv_build_dir /home/sifter/opencv-2.4.13.2/build

RUN curl -L https://github.com/opencv/opencv/archive/2.4.13.2.tar.gz | tar -xzv
RUN mkdir $opencv_build_dir

USER root
COPY sifter_engine/build_opencv.sh $opencv_build_dir
RUN chown sifter:sifter $opencv_build_dir/build_opencv.sh

USER sifter
RUN cd $opencv_build_dir && bash build_opencv.sh

USER root
RUN cd $opencv_build_dir && make install

COPY sifter_engine/src $HOME/src
COPY sifter_engine/include $HOME/include
RUN chown sifter:sifter -R src include

RUN echo "/usr/local/lib" >> /etc/ld.so.conf.d/opencv.conf && ldconfig

USER sifter
RUN cd $HOME/src && make clean && make && mv sifter $HOME

ENTRYPOINT ./sifter --base ./data/ --port 8080
