# need this for Doxygen integration
LIBS = -pthread -L/usr/local/lib -lzmq -lczmq
AM_CFLAGS = -Wall -g -O2 -I${srcdir}/include
# AM_CFLAGS = -Wall -g -O0 -I${srcdir}/include -DDEBUG
bin_PROGRAMS = inn

inn_SOURCES = src/client.c src/sequencer.c src/mixer.c src/heartbeat.c src/collector.c src/replayer.c \
              src/lib/responder.c src/lib/publisher.c src/lib/subscriber.c src/lib/requester.c src/lib/candidate.c \
              src/lib/queue.c src/lib/util.c src/lib/ev.c src/lib/ezxml.c src/lib/rbtree.c src/lib/record.c src/inn.c

clean-local:
	$(RM) $(DX_CLEANFILES)