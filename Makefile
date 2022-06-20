DEBUG = y

OBJS = dtls.o protocol.o addr.o
LIBS = -lssl -lcrypto

ifeq ($(DEBUG), y)
	CFLAGS += -g -DDEBUG
else
	CFLAGS += -O2
endif

all: server client

server: dtls.h $(OBJS) $(LIBS)

client: dtls.h $(OBJS) $(LIBS)

dtls.o: dtls.h

protocol.o: protocol.h

addr.o: addr.h

certs:
	openssl req -newkey rsa:4096 -x509 -sha256 -days 3650 -nodes -out cert.pem -keyout key.pem

.PHONY : clean
clean:
	rm -f server client $(OBJS)
