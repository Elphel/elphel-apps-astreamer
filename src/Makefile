AXIS_USABLE_LIBS = UCLIBC GLIBC
AXIS_AUTO_DEPEND = yes
-include $(AXIS_TOP_DIR)/tools/build/Rules.axis
INSTALL ?= install_elinux
INSTALL_PATH = $(prefix)/usr/local
INCDIR       = $(AXIS_KERNEL_DIR)/include

LDFLAGS     = -Wl -s -L$(prefix)/lib -lpthread -lasound
CXXFLAGS      = -Wall -O2 -I$(INCDIR)

STREAMER_OBJS   = main.o streamer.o audio.o video.o socket.o rtsp.o rtp_stream.o helpers.o parameters.o
SRCS   = main.cpp streamer.cpp audio.cpp video.cpp socket.cpp rtsp.cpp rtp_stream.cpp helpers.cpp parameters.cpp

PROG    = str
all: dependency $(PROG)

clean:
	rm -f *.o *.ver *.mp *~ core*  $(PROG)

$(PROG): $(STREAMER_OBJS)
	$(CXX) $(LDFLAGS) -o $@  $(STREAMER_OBJS) -lpthread -asound

%.o: %.cpp
	 $(CXX) $(CXXFLAGS) -c -o $@ $<

#install:  $(PROG)
install: clean $(PROG)
	$(INSTALL) $(PROG) $(INSTALL_PATH)/bin/$(PROG)

configsubs:
