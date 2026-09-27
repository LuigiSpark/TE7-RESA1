CFLAGS=-Wall -Iinclude
#LDFLAGS=-lpthread

COMMON = src/protocol.c src/user_list.c

CLIENT_SRC = src/client.c $(COMMON)
SERVER_SRC = src/server.c $(COMMON)

all: client server

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $^ -o $@
server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f client server

debug: $(SERVER_SRC)
	clang -O0 -Iinclude -fsanitize=undefined,address,leak -fsanitize-trap=undefined -g -Wall -Werror $^ -o server
	lldb server 1234

.PHONY: clean debug

