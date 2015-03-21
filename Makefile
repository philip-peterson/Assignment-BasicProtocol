all: client server

client: client.c
	gcc -g -o client -Wno-format-security client.c

server: server.c
	gcc -g -o server -Wno-format-security server.c

.PHONY clean:
	rm server
	rm client
