PROG       = str
CXXFLAGS   += -Wall -std=c++03 -I$(STAGING_DIR_HOST)/usr/include-uapi
LDFLAGS    += -s -pthread -lasound

SRCS       = main.cpp streamer.cpp audio.cpp video.cpp socket.cpp rtsp.cpp rtp_stream.cpp helpers.cpp parameters.cpp
STREAMER_OBJS = $(SRCS:.cpp=.o)

INSTALL    = install
INSTMODE   = 0755
INSTDOCS   = 0644
OWN        = -o root -g root

SYSCONFDIR = /etc/
BINDIR     = /usr/bin/
WWW_PAGES  = /www/pages
IMAGEDIR   = $(WWW_PAGES)/images

all: $(PROG)

$(PROG): $(STREAMER_OBJS)
	$(CXX) -o $@ $(STREAMER_OBJS) $(LDFLAGS)

%.o: %.cpp
	 $(CXX) $(CXXFLAGS) -c -o $@ $<

install: $(PROG)
	$(INSTALL) $(OWN) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m $(INSTMODE) $(OWN) $(PROG) $(DESTDIR)$(BINDIR)

clean:
	rm -f *.o *.ver *.mp *~ core*  .depend $(PROG)

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^ > ./.depend;

include .depend