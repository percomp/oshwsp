# Copyright (c) 2014 Cesanta Software
# All rights reserved

PROG = ii5

CFLAGS = -DCMAKE_BUILD  -O3 -fvisibility=hidden  -I. -pthread -g $(CFLAGS_EXTRA)
LDFLAGS = -fvisibility=hidden -rdynamic libwebsockets.a -lz -lssl -lcrypto -ldl -lm
#CFLAGS = -W -Wall -I.. -pthread -g -O0 $(CFLAGS_EXTRA)
SOURCES = $(PROG).c 

$(PROG): $(SOURCES)
	$(CC) -o $(PROG) $(SOURCES) $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf $(PROG) *.exe *.dSYM *.obj *.exp .*o *.lib



#enlazado:
#/usr/bin/cc  -Wall -Werror -O3 -fvisibility=hidden     CMakeFiles/ii4.dir/test-server/ii4.c.o  -o bin/libwebsockets-ii4 -rdynamic lib/libwebsockets.a -lz -lssl -lcrypto -ldl -lm

#compilado:
#/usr/bin/cc  -DCMAKE_BUILD -DINSTALL_DATADIR=\"/usr/local/share\" -Wall -Werror -O3 -fvisibility=hidden  -I/home/cesar/websockets/pika/build -I/home/cesar/websockets/pika/lib    -o CMakeFiles/ii4.dir/test-server/ii4.c.o   -c /home/cesar/websockets/pika/test-server/ii4.c
