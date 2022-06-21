DEBUG = y

OBJS = protocol.o addr.o
LIBS = -lssl -lcrypto

ifeq ($(DEBUG), y)
	CFLAGS += -g -DDEBUG
else
	CFLAGS += -O2
endif

all: server client

server: $(OBJS) $(LIBS) common.h
	cc $(OBJS) $(LIBS) server.c -o server

client: $(OBJS) $(LIBS) common.h
	cc $(OBJS) $(LIBS) client.c -o client

protocol.o: protocol.h

addr.o: addr.h

certs:
	openssl req -newkey rsa:4096 -x509 -sha256 -days 3650 -nodes -out cert.pem -keyout key.pem

.PHONY : clean
clean:
	rm -f server client $(OBJS)
