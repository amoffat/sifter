CC = gcc
CPP = g++

INC = ../include
INC_DIRS = -I $(INC) -I /home/amoffat/include

CPPFLAGS := -std=c++11 -DLOGGING $(INC_DIRS) $(shell pkg-config --cflags glib-2.0)
CFLAGS = -DLOGGING $(INC_DIRS)

LIBBOOST = \
	-l:libboost_program_options.so.1.54.0\
	-l:libboost_filesystem.so.1.54.0\
	-l:libboost_system.so.1.54.0 
LIBOPENCV = \
	-lopencv_core\
	-lopencv_highgui\
	-lopencv_nonfree\
	-lopencv_features2d
LIBTBB = \
	 -ltbb
LDLIBS := \
	-L/home/amoffat/lib\
	-L/usr/local/lib\
	-ldl\
	-lpthread\
	$(LIBBOOST)\
	$(LIBOPENCV)\
	$(LIBTBB)\
	$(shell pkg-config --libs glib-2.0)

sifter: sifter.o web_server.o mongoose.o logging.o
	$(CPP) -o $@ $^ $(LDLIBS)

mongoose.o: mongoose.c $(INC)/mongoose.h
	$(CC) $(CFLAGS) -c -o $@ $<

logging.o: logging.cpp $(INC)/logging.h
	$(CPP) $(CPPFLAGS) -c -o $@ $<

sifter.o: sifter.cpp $(INC)/web_server.h $(INC)/logging.h

web_server.o: web_server.cpp $(INC)/mongoose.h $(INC)/logging.h

.PHONY: clean
clean:
	-rm *.o
	-rm sifter
