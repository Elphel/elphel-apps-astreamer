PROG       = str
TEST_PROG1  = test_socket_udp_sender
TEST_PROG2  = test_socket_udp_receiver

CXXFLAGS   += -Wall -std=c++03 -I$(STAGING_DIR_HOST)/usr/include-uapi
LDFLAGS    += -pthread -lasound

SRCS       = main.cpp streamer.cpp audio.cpp video.cpp socket.cpp rtsp.cpp rtp_stream.cpp helpers.cpp parameters.cpp
TEST_SRC1 = test_socket_udp_sender.c
TEST_SRC2 = test_socket_udp_receiver.c

STREAMER_OBJS = $(SRCS:.cpp=.o)

INSTALL    = install
INSTMODE   = 0755
INSTDOCS   = 0644
OWN        = -o root -g root

SYSCONFDIR = /etc/
BINDIR     = /usr/bin/
WWW_PAGES  = /www/pages
IMAGEDIR   = $(WWW_PAGES)/images

all: $(PROG) $(TEST_PROG1) $(TEST_PROG2)

$(PROG): $(STREAMER_OBJS)
	$(CXX) -o $@ $(STREAMER_OBJS) $(LDFLAGS)

$(TEST_PROG1): $(TEST_SRC1:.c=.o)
$(TEST_PROG2): $(TEST_SRC2:.c=.o)

%.o: %.cpp
	 $(CXX) $(CXXFLAGS) -c -o $@ $<

install: $(PROG)
	$(INSTALL) $(OWN) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m $(INSTMODE) $(OWN) $(PROG) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m $(INSTMODE) $(OWN) $(TEST_PROG1) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m $(INSTMODE) $(OWN) $(TEST_PROG2) $(DESTDIR)$(BINDIR)

clean:
	rm -f *.o *.ver *.mp *~ core*  .depend $(PROG)

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CXXFLAGS) -MM $^ > ./.depend;

include .depend
