DEBUG = y

OBJS = protocol.o addr.o power.o
LIBS = -lssl -lcrypto

ifeq ($(DEBUG), y)
	CFLAGS += -g -DDEBUG
else
	CFLAGS += -O2
endif

all: server client

server: $(OBJS) $(LIBS) common.h
	cc $(CFLAGS) $(OBJS) $(LIBS) server.c -o server

client: $(OBJS) $(LIBS) common.h
	cc $(CFLAGS) $(OBJS) $(LIBS) client.c -o client

power.o: power.h

protocol.o: protocol.h

addr.o: addr.h

certs:
	openssl ecparam -genkey -name secp384r1 -noout -out pvtkey.pem
	openssl ec -in pvtkey.pem -pubout -out pubkey.pem

.PHONY : clean
clean:
	rm -f server client $(OBJS)
